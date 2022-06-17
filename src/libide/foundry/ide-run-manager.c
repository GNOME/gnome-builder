/* ide-run-manager.c
 *
 * Copyright 2016-2019 Christian Hergert <chergert@redhat.com>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#define G_LOG_DOMAIN "ide-run-manager"

#include "config.h"

#include <glib/gi18n.h>
#include <gtk/gtk.h>

#include <libpeas/peas.h>
#include <libpeas/peas-autocleanups.h>

#include <libide-core.h>
#include <libide-plugins.h>
#include <libide-threading.h>
#include <libide-vcs.h>

#include "ide-private.h"

#include "ide-build-manager.h"
#include "ide-build-system.h"
#include "ide-build-target-provider.h"
#include "ide-build-target.h"
#include "ide-config-manager.h"
#include "ide-config.h"
#include "ide-deploy-strategy.h"
#include "ide-device-manager.h"
#include "ide-foundry-compat.h"
#include "ide-run-command.h"
#include "ide-run-command-provider.h"
#include "ide-run-manager-private.h"
#include "ide-run-manager.h"
#include "ide-runner.h"
#include "ide-runtime.h"

struct _IdeRunManager
{
  IdeObject                parent_instance;

  GCancellable            *cancellable;
  IdeBuildTarget          *build_target;
  IdeNotification         *notif;
  IdeExtensionSetAdapter  *run_command_providers;

  const IdeRunHandlerInfo *handler;
  GList                   *handlers;

  IdeRunner               *current_runner;
  IdeRunCommand           *current_run_command;

  /* Keep track of last change sequence from the file monitor
   * so that we can maybe skip past install phase and make
   * secondary execution time faster.
   */
  guint64                  last_change_seq;
  guint64                  pending_last_change_seq;

  char                    *default_run_command;

  guint                    busy;

  guint                    messages_debug_all : 1;
  guint                    has_installed_once : 1;
};

typedef struct
{
  GList     *providers;
  GPtrArray *results;
  guint      active;
} DiscoverState;

static void initable_iface_init                         (GInitableIface *iface);
static void ide_run_manager_actions_run                 (IdeRunManager  *self,
                                                         GVariant       *param);
static void ide_run_manager_actions_run_with_handler    (IdeRunManager  *self,
                                                         GVariant       *param);
static void ide_run_manager_actions_stop                (IdeRunManager  *self,
                                                         GVariant       *param);
static void ide_run_manager_actions_messages_debug_all  (IdeRunManager  *self,
                                                         GVariant       *param);
static void ide_run_manager_actions_default_run_command (IdeRunManager  *self,
                                                         GVariant       *param);
static void ide_run_manager_actions_color_scheme        (IdeRunManager  *self,
                                                         GVariant       *param);
static void ide_run_manager_actions_high_contrast       (IdeRunManager  *self,
                                                         GVariant       *param);
static void ide_run_manager_actions_text_direction      (IdeRunManager  *self,
                                                         GVariant       *param);

IDE_DEFINE_ACTION_GROUP (IdeRunManager, ide_run_manager, {
  { "run", ide_run_manager_actions_run },
  { "run-with-handler", ide_run_manager_actions_run_with_handler, "s" },
  { "stop", ide_run_manager_actions_stop },
  { "messages-debug-all", ide_run_manager_actions_messages_debug_all, NULL, "false" },
  { "default-run-command", ide_run_manager_actions_default_run_command, "s", "''" },
  { "color-scheme", ide_run_manager_actions_color_scheme, "s", "'follow'" },
  { "high-contrast", ide_run_manager_actions_high_contrast, NULL, "false" },
  { "text-direction", ide_run_manager_actions_text_direction, "s", "''" },
})

G_DEFINE_TYPE_EXTENDED (IdeRunManager, ide_run_manager, IDE_TYPE_OBJECT, G_TYPE_FLAG_FINAL,
                        G_IMPLEMENT_INTERFACE (G_TYPE_INITABLE, initable_iface_init)
                        G_IMPLEMENT_INTERFACE (G_TYPE_ACTION_GROUP, ide_run_manager_init_action_group))

enum {
  PROP_0,
  PROP_BUSY,
  PROP_HANDLER,
  PROP_BUILD_TARGET,
  N_PROPS
};

enum {
  RUN,
  STOPPED,
  N_SIGNALS
};

static GParamSpec *properties [N_PROPS];
static guint signals [N_SIGNALS];

static void
discover_state_free (gpointer data)
{
  DiscoverState *state = data;

  g_assert (state->active == 0);

  g_list_free_full (state->providers, g_object_unref);
  g_clear_pointer (&state->results, g_ptr_array_unref);
  g_slice_free (DiscoverState, state);
}

static void
ide_run_manager_actions_high_contrast (IdeRunManager *self,
                                       GVariant      *param)
{
  GVariant *state;

  g_assert (IDE_IS_RUN_MANAGER (self));

  state = ide_run_manager_get_action_state (self, "high-contrast");
  ide_run_manager_set_action_state (self,
                                    "high-contrast",
                                    g_variant_new_boolean (!g_variant_get_boolean (state)));
}

static void
ide_run_manager_actions_text_direction (IdeRunManager *self,
                                        GVariant      *param)
{
  const char *str;

  g_assert (IDE_IS_RUN_MANAGER (self));
  g_assert (param != NULL);
  g_assert (g_variant_is_of_type (param, G_VARIANT_TYPE_STRING));

  str = g_variant_get_string (param, NULL);
  if (g_strv_contains (IDE_STRV_INIT ("ltr", "rtl"), str))
    ide_run_manager_set_action_state (self,
                                      "text-direction",
                                      g_variant_new_string (str));
}

static void
ide_run_manager_actions_color_scheme (IdeRunManager *self,
                                      GVariant      *param)
{
  const char *str;

  g_assert (IDE_IS_RUN_MANAGER (self));
  g_assert (param != NULL);
  g_assert (g_variant_is_of_type (param, G_VARIANT_TYPE_STRING));

  str = g_variant_get_string (param, NULL);
  if (!g_strv_contains (IDE_STRV_INIT ("follow", "force-light", "force-dark"), str))
    str = "follow";

  ide_run_manager_set_action_state (self,
                                    "color-scheme",
                                    g_variant_new_string (str));
}

static void
ide_run_manager_actions_default_run_command (IdeRunManager *self,
                                             GVariant      *param)
{
  const char *str;

  g_assert (IDE_IS_RUN_MANAGER (self));
  g_assert (param != NULL);
  g_assert (g_variant_is_of_type (param, G_VARIANT_TYPE_STRING));

  str = g_variant_get_string (param, NULL);
  if (ide_str_empty0 (str))
    str = NULL;

  if (g_strcmp0 (str, self->default_run_command) != 0)
    {
      g_free (self->default_run_command);
      self->default_run_command = g_strdup (str);
      ide_run_manager_set_action_state (self,
                                        "default-run-command",
                                        g_variant_new_string (str ? str : ""));
    }
}

static void
ide_run_manager_real_run (IdeRunManager *self,
                          IdeRunner     *runner)
{
  IDE_ENTRY;

  g_assert (IDE_IS_RUN_MANAGER (self));
  g_assert (IDE_IS_RUNNER (runner));

  /* Setup G_MESSAGES_DEBUG environment variable if necessary */
  if (self->messages_debug_all)
    {
      IdeEnvironment *env = ide_runner_get_environment (runner);
      ide_environment_setenv (env, "G_MESSAGES_DEBUG", "all");
    }

  /*
   * If the current handler has a callback specified (our default "run" handler
   * does not), then we need to allow that handler to prepare the runner.
   */
  if (self->handler != NULL && self->handler->handler != NULL)
    self->handler->handler (self, runner, self->handler->handler_data);

  IDE_EXIT;
}

static void
ide_run_handler_info_free (gpointer data)
{
  IdeRunHandlerInfo *info = data;

  g_free (info->id);
  g_free (info->title);
  g_free (info->icon_name);

  if (info->handler_data_destroy)
    info->handler_data_destroy (info->handler_data);

  g_slice_free (IdeRunHandlerInfo, info);
}

static void
ide_run_manager_update_action_enabled (IdeRunManager *self)
{
  IdeBuildManager *build_manager;
  IdeContext *context;
  gboolean can_build;

  g_assert (IDE_IS_RUN_MANAGER (self));

  context = ide_object_get_context (IDE_OBJECT (self));
  build_manager = ide_build_manager_from_context (context);
  can_build = ide_build_manager_get_can_build (build_manager);

  ide_run_manager_set_action_enabled (self, "run",
                                      self->busy == 0 && can_build == TRUE);
  ide_run_manager_set_action_enabled (self, "run-with-handler",
                                      self->busy == 0 && can_build == TRUE);
  ide_run_manager_set_action_enabled (self, "stop", self->busy > 0);
}


static void
ide_run_manager_mark_busy (IdeRunManager *self)
{
  IDE_ENTRY;

  g_assert (IDE_IS_RUN_MANAGER (self));

  self->busy++;

  if (self->busy == 1)
    {
      g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_BUSY]);
      ide_run_manager_update_action_enabled (self);
    }

  IDE_EXIT;
}

static void
ide_run_manager_unmark_busy (IdeRunManager *self)
{
  IDE_ENTRY;

  g_assert (IDE_IS_RUN_MANAGER (self));

  self->busy--;

  if (self->busy == 0)
    {
      g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_BUSY]);
      ide_run_manager_update_action_enabled (self);
    }

  IDE_EXIT;
}

static void
ide_run_manager_dispose (GObject *object)
{
  IdeRunManager *self = (IdeRunManager *)object;

  self->handler = NULL;

  g_clear_pointer (&self->default_run_command, g_free);

  g_clear_object (&self->cancellable);
  ide_clear_and_destroy_object (&self->build_target);

  g_clear_object (&self->current_run_command);
  g_clear_object (&self->current_runner);

  ide_clear_and_destroy_object (&self->run_command_providers);

  g_list_free_full (self->handlers, ide_run_handler_info_free);
  self->handlers = NULL;

  G_OBJECT_CLASS (ide_run_manager_parent_class)->dispose (object);
}

static void
ide_run_manager_notify_can_build (IdeRunManager   *self,
                                  GParamSpec      *pspec,
                                  IdeBuildManager *build_manager)
{
  IDE_ENTRY;

  g_assert (IDE_IS_RUN_MANAGER (self));
  g_assert (G_IS_PARAM_SPEC (pspec));
  g_assert (IDE_IS_BUILD_MANAGER (build_manager));

  ide_run_manager_update_action_enabled (self);

  IDE_EXIT;
}

static gboolean
initable_init (GInitable     *initable,
               GCancellable  *cancellable,
               GError       **error)
{
  IdeRunManager *self = (IdeRunManager *)initable;
  IdeBuildManager *build_manager;
  IdeContext *context;

  IDE_ENTRY;

  g_assert (IDE_IS_RUN_MANAGER (self));
  g_assert (!cancellable || G_IS_CANCELLABLE (cancellable));

  context = ide_object_get_context (IDE_OBJECT (self));
  build_manager = ide_build_manager_from_context (context);

  g_signal_connect_object (build_manager,
                           "notify::can-build",
                           G_CALLBACK (ide_run_manager_notify_can_build),
                           self,
                           G_CONNECT_SWAPPED);

  ide_run_manager_update_action_enabled (self);

  self->run_command_providers = ide_extension_set_adapter_new (IDE_OBJECT (self),
                                                               peas_engine_get_default (),
                                                               IDE_TYPE_RUN_COMMAND_PROVIDER,
                                                               NULL, NULL);

  IDE_RETURN (TRUE);
}

static void
initable_iface_init (GInitableIface *iface)
{
  iface->init = initable_init;
}

static void
ide_run_manager_get_property (GObject    *object,
                              guint       prop_id,
                              GValue     *value,
                              GParamSpec *pspec)
{
  IdeRunManager *self = IDE_RUN_MANAGER (object);

  switch (prop_id)
    {
    case PROP_BUSY:
      g_value_set_boolean (value, ide_run_manager_get_busy (self));
      break;

    case PROP_HANDLER:
      g_value_set_string (value, ide_run_manager_get_handler (self));
      break;

    case PROP_BUILD_TARGET:
      g_value_set_object (value, ide_run_manager_get_build_target (self));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
ide_run_manager_set_property (GObject      *object,
                              guint         prop_id,
                              const GValue *value,
                              GParamSpec   *pspec)
{
  IdeRunManager *self = IDE_RUN_MANAGER (object);

  switch (prop_id)
    {
    case PROP_BUILD_TARGET:
      ide_run_manager_set_build_target (self, g_value_get_object (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
    }
}

static void
ide_run_manager_class_init (IdeRunManagerClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->dispose = ide_run_manager_dispose;
  object_class->get_property = ide_run_manager_get_property;
  object_class->set_property = ide_run_manager_set_property;

  properties [PROP_BUSY] =
    g_param_spec_boolean ("busy",
                          "Busy",
                          "Busy",
                          FALSE,
                          (G_PARAM_READABLE | G_PARAM_STATIC_STRINGS));

  properties [PROP_HANDLER] =
    g_param_spec_string ("handler",
                         "Handler",
                         "Handler",
                         "run",
                         (G_PARAM_READABLE | G_PARAM_STATIC_STRINGS));

  properties [PROP_BUILD_TARGET] =
    g_param_spec_object ("build-target",
                         "Build Target",
                         "The IdeBuildTarget that will be run",
                         IDE_TYPE_BUILD_TARGET,
                         (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, N_PROPS, properties);

  /**
   * IdeRunManager::run:
   * @self: An #IdeRunManager
   * @runner: An #IdeRunner
   *
   * This signal is emitted right before ide_runner_run_async() is called
   * on an #IdeRunner. It can be used by plugins to tweak things right
   * before the runner is executed.
   *
   * The current run handler (debugger, profiler, etc) is run as the default
   * handler for this function. So connect with %G_SIGNAL_AFTER if you want
   * to be nofied after the run handler has executed. It's unwise to change
   * things that the run handler might expect. Generally if you want to
   * change settings, do that before the run handler has exected.
   */
  signals [RUN] =
    g_signal_new_class_handler ("run",
                                G_TYPE_FROM_CLASS (klass),
                                G_SIGNAL_RUN_LAST,
                                G_CALLBACK (ide_run_manager_real_run),
                                NULL,
                                NULL,
                                NULL,
                                G_TYPE_NONE,
                                1,
                                IDE_TYPE_RUNNER);

  /**
   * IdeRunManager::stopped:
   *
   * This signal is emitted when the run manager has stopped the currently
   * executing inferior.
   */
  signals [STOPPED] =
    g_signal_new ("stopped",
                  G_TYPE_FROM_CLASS (klass),
                  G_SIGNAL_RUN_LAST,
                  0,
                  NULL,
                  NULL,
                  NULL,
                  G_TYPE_NONE,
                  0);
}

gboolean
ide_run_manager_get_busy (IdeRunManager *self)
{
  g_return_val_if_fail (IDE_IS_RUN_MANAGER (self), FALSE);

  return self->busy > 0;
}

static gboolean
ide_run_manager_check_busy (IdeRunManager  *self,
                            GError        **error)
{
  g_assert (IDE_IS_RUN_MANAGER (self));
  g_assert (error != NULL);

  if (ide_run_manager_get_busy (self))
    {
      g_set_error (error,
                   G_IO_ERROR,
                   G_IO_ERROR_BUSY,
                   "%s",
                   _("Cannot run target, another target is running"));
      return TRUE;
    }

  return FALSE;
}

static void
copy_builtin_envvars (IdeEnvironment *environment)
{
  static const gchar *copy_env[] = {
    "AT_SPI_BUS_ADDRESS",
    "COLORTERM",
    "DBUS_SESSION_BUS_ADDRESS",
    "DBUS_SYSTEM_BUS_ADDRESS",
    "DESKTOP_SESSION",
    "DISPLAY",
    "LANG",
    "SHELL",
    "SSH_AUTH_SOCK",
    "USER",
    "WAYLAND_DISPLAY",
    "XAUTHORITY",
    "XDG_CURRENT_DESKTOP",
    "XDG_MENU_PREFIX",
#if 0
    /* Can't copy these as they could mess up Flatpak */
    "XDG_DATA_DIRS",
    "XDG_RUNTIME_DIR",
#endif
    "XDG_SEAT",
    "XDG_SESSION_DESKTOP",
    "XDG_SESSION_ID",
    "XDG_SESSION_TYPE",
    "XDG_VTNR",
  };
  const gchar * const *host_environ = _ide_host_environ ();

  for (guint i = 0; i < G_N_ELEMENTS (copy_env); i++)
    {
      const gchar *key = copy_env[i];
      const gchar *val = g_environ_getenv ((gchar **)host_environ, key);

      if (val != NULL && ide_environment_getenv (environment, key) == NULL)
        ide_environment_setenv (environment, key, val);
    }
}

static void
apply_color_scheme (IdeEnvironment *env,
                    const char     *color_scheme)
{
  IDE_ENTRY;

  g_assert (IDE_IS_ENVIRONMENT (env));
  g_assert (color_scheme != NULL);

  g_debug ("Applying color-scheme \"%s\"", color_scheme);

  if (ide_str_equal0 (color_scheme, "follow"))
    {
      ide_environment_setenv (env, "ADW_DEBUG_COLOR_SCHEME", NULL);
      ide_environment_setenv (env, "HDY_DEBUG_COLOR_SCHEME", NULL);
    }
  else if (ide_str_equal0 (color_scheme, "force-light"))
    {
      ide_environment_setenv (env, "ADW_DEBUG_COLOR_SCHEME", "prefer-light");
      ide_environment_setenv (env, "HDY_DEBUG_COLOR_SCHEME", "prefer-light");
    }
  else if (ide_str_equal0 (color_scheme, "force-dark"))
    {
      ide_environment_setenv (env, "ADW_DEBUG_COLOR_SCHEME", "prefer-dark");
      ide_environment_setenv (env, "HDY_DEBUG_COLOR_SCHEME", "prefer-dark");
    }
  else g_warn_if_reached ();

  IDE_EXIT;
}

static void
apply_high_contrast (IdeEnvironment *env,
                     gboolean        high_contrast)
{
  IDE_ENTRY;

  g_assert (IDE_IS_ENVIRONMENT (env));

  g_debug ("Applying high-contrast %d", high_contrast);

  if (high_contrast)
    {
      ide_environment_setenv (env, "ADW_DEBUG_HIGH_CONTRAST", "1");
      ide_environment_setenv (env, "HDY_DEBUG_HIGH_CONTRAST", "1");
    }
  else
    {
      ide_environment_setenv (env, "ADW_DEBUG_HIGH_CONTRAST", NULL);
      ide_environment_setenv (env, "HDY_DEBUG_HIGH_CONTRAST", NULL);
    }

  IDE_EXIT;
}

static void
apply_text_direction (IdeEnvironment *env,
                      const char     *text_dir_str)
{
  g_autofree char *value = NULL;
  GtkTextDirection dir;
  const char *gtk_debug;

  if (ide_str_equal0 (text_dir_str, "rtl"))
    dir = GTK_TEXT_DIR_RTL;
  else if (ide_str_equal0 (text_dir_str, "ltr"))
    dir = GTK_TEXT_DIR_LTR;
  else
    g_return_if_reached ();

  if (dir == gtk_widget_get_default_direction ())
    return;

  if ((gtk_debug = ide_environment_getenv (env, "GTK_DEBUG")))
    value = g_strdup_printf ("%s:invert-text-dir", gtk_debug);
  else
    value = g_strdup ("invert-text-dir");

  ide_environment_setenv (env, "GTK_DEBUG", value);
}

static inline const char *
get_action_state_string (IdeRunManager *self,
                         const char    *action_name)
{
  GVariant *state = ide_run_manager_get_action_state (self, action_name);
  return g_variant_get_string (state, NULL);
}

static inline gboolean
get_action_state_bool (IdeRunManager *self,
                       const char    *action_name)
{
  GVariant *state = ide_run_manager_get_action_state (self, action_name);
  return g_variant_get_boolean (state);
}

static void
ide_run_manager_install_cb (GObject      *object,
                            GAsyncResult *result,
                            gpointer      user_data)
{
  IdeBuildManager *build_manager = (IdeBuildManager *)object;
  g_autoptr(IdeTask) task = user_data;
  g_autoptr(GError) error = NULL;

  IDE_ENTRY;

  g_assert (IDE_IS_BUILD_MANAGER (build_manager));
  g_assert (G_IS_ASYNC_RESULT (result));
  g_assert (IDE_IS_TASK (task));

  if (!ide_build_manager_build_finish (build_manager, result, &error))
    ide_task_return_error (task, g_steal_pointer (&error));
  else
    ide_task_return_boolean (task, TRUE);

  IDE_EXIT;
}

static void
ide_run_manager_install_async (IdeRunManager       *self,
                               GCancellable        *cancellable,
                               GAsyncReadyCallback  callback,
                               gpointer             user_data)
{
  g_autoptr(IdeContext) context = NULL;
  g_autoptr(GSettings) project_settings = NULL;
  g_autoptr(IdeTask) task = NULL;
  IdeBuildManager *build_manager;
  IdeVcsMonitor *monitor;
  guint64 sequence = 0;

  IDE_ENTRY;

  g_assert (IDE_IS_RUN_MANAGER (self));
  g_assert (!cancellable || G_IS_CANCELLABLE (cancellable));

  context = ide_object_ref_context (IDE_OBJECT (self));

  task = ide_task_new (self, cancellable, callback, user_data);
  ide_task_set_source_tag (task, ide_run_manager_install_async);

  project_settings = ide_context_ref_project_settings (context);
  if (!g_settings_get_boolean (project_settings, "install-before-run"))
    {
      ide_task_return_boolean (task, TRUE);
      IDE_EXIT;
    }

  monitor = ide_vcs_monitor_from_context (context);
  if (monitor != NULL)
    sequence = ide_vcs_monitor_get_sequence (monitor);

  if (self->has_installed_once && sequence == self->last_change_seq)
    {
      ide_task_return_boolean (task, TRUE);
      IDE_EXIT;
    }

  self->pending_last_change_seq = sequence;

  build_manager = ide_build_manager_from_context (context);
  ide_build_manager_build_async (build_manager,
                                 IDE_PIPELINE_PHASE_INSTALL,
                                 NULL,
                                 cancellable,
                                 ide_run_manager_install_cb,
                                 g_steal_pointer (&task));

  IDE_EXIT;
}

static gboolean
ide_run_manager_install_finish (IdeRunManager  *self,
                                GAsyncResult   *result,
                                GError        **error)
{
  gboolean ret;

  IDE_ENTRY;

  g_assert (IDE_IS_RUN_MANAGER (self));
  g_assert (IDE_IS_TASK (result));

  ret = ide_task_propagate_boolean (IDE_TASK (result), error);

  IDE_RETURN (ret);
}

static void
ide_run_manager_run_runner_run_cb (GObject      *object,
                                   GAsyncResult *result,
                                   gpointer      user_data)
{
  IdeRunner *runner = (IdeRunner *)object;
  g_autoptr(IdeTask) task = user_data;
  g_autoptr(GError) error = NULL;
  IdeRunManager *self;

  IDE_ENTRY;

  g_assert (IDE_IS_RUNNER (runner));
  g_assert (G_IS_ASYNC_RESULT (result));
  g_assert (IDE_IS_TASK (task));

  self = ide_task_get_source_object (task);

  g_assert (IDE_IS_RUN_MANAGER (self));

  if (self->notif != NULL)
    {
      ide_notification_withdraw (self->notif);
      g_clear_object (&self->notif);
    }

  g_clear_object (&self->current_runner);

  if (!ide_runner_run_finish (runner, result, &error))
    ide_task_return_error (task, g_steal_pointer (&error));
  else
    ide_task_return_boolean (task, TRUE);

  g_signal_emit (self, signals[STOPPED], 0);

  ide_object_destroy (IDE_OBJECT (runner));

  IDE_EXIT;
}

static gboolean
ide_run_manager_prepare_runner (IdeRunManager  *self,
                                IdeRunner      *runner,
                                GError        **error)
{
  g_autofree char *title = NULL;
  IdeConfigManager *config_manager;
  IdeEnvironment *environment;
  IdeContext *context;
  const char *color_scheme;
  const char *run_opts;
  const char *name;
  IdeConfig *config;

  IDE_ENTRY;

  g_assert (IDE_IS_RUN_MANAGER (self));
  g_assert (IDE_IS_RUNNER (runner));

  context = ide_object_get_context (IDE_OBJECT (self));
  config_manager = ide_config_manager_from_context (context);
  config = ide_config_manager_get_current (config_manager);

  /* Add our run arguments if specified in the config. */
  if (NULL != (run_opts = ide_config_get_run_opts (config)))
    {
      g_auto(GStrv) argv = NULL;
      gint argc;

      if (g_shell_parse_argv (run_opts, &argc, &argv, NULL))
        {
          for (gint i = 0; i < argc; i++)
            ide_runner_append_argv (runner, argv[i]);
        }
    }

  /* Add our runtime environment variables. */
  environment = ide_runner_get_environment (runner);
  copy_builtin_envvars (environment);
  ide_environment_copy_into (ide_config_get_runtime_environment (config), environment, TRUE);

  /* Add debugging overrides */
  color_scheme = get_action_state_string (self, "color-scheme");
  apply_color_scheme (environment, color_scheme);
  apply_high_contrast (environment, get_action_state_bool (self, "high-contrast"));
  apply_text_direction (environment, get_action_state_string (self, "text-direction"));

  g_signal_emit (self, signals [RUN], 0, runner);

  if (ide_runner_get_failed (runner))
    {
      g_set_error (error,
                   IDE_RUNTIME_ERROR,
                   IDE_RUNTIME_ERROR_SPAWN_FAILED,
                   "Failed to execute the application");
      IDE_RETURN (FALSE);
    }

  if (self->notif != NULL)
    {
      ide_notification_withdraw (self->notif);
      g_clear_object (&self->notif);
    }

  self->notif = ide_notification_new ();
  name = ide_run_command_get_display_name (self->current_run_command);
  /* translators: %s is replaced with the name of the users executable */
  title = g_strdup_printf (_("Running %s…"), name);
  ide_notification_set_title (self->notif, title);
  ide_notification_attach (self->notif, IDE_OBJECT (self));

  g_set_object (&self->current_runner, runner);

  IDE_RETURN (TRUE);
}

static void
ide_run_manager_run_create_runner_cb (GObject      *object,
                                      GAsyncResult *result,
                                      gpointer      user_data)
{
  IdeDeviceManager *device_manager = (IdeDeviceManager *)object;
  g_autoptr(IdeRunner) runner = NULL;
  g_autoptr(IdeTask) task = user_data;
  g_autoptr(GError) error = NULL;
  IdeRunManager *self;

  IDE_ENTRY;

  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (IDE_IS_DEVICE_MANAGER (device_manager));
  g_assert (G_IS_ASYNC_RESULT (result));
  g_assert (IDE_IS_TASK (task));

  self = ide_task_get_source_object (task);

  g_assert (IDE_IS_RUN_MANAGER (self));

  if (!(runner = ide_device_manager_create_runner_finish (device_manager, result, &error)) ||
      !ide_run_manager_prepare_runner (self, runner, &error))
    {
      ide_task_return_error (task, g_steal_pointer (&error));
      IDE_EXIT;
    }

  ide_runner_run_async (runner,
                        ide_task_get_cancellable (task),
                        ide_run_manager_run_runner_run_cb,
                        g_object_ref (task));

  IDE_EXIT;
}

static void
ide_run_manager_run_deploy_cb (GObject      *object,
                               GAsyncResult *result,
                               gpointer      user_data)
{
  IdeDeviceManager *device_manager = (IdeDeviceManager *)object;
  g_autoptr(IdeTask) task = user_data;
  g_autoptr(GError) error = NULL;
  IdePipeline *pipeline;

  IDE_ENTRY;

  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (IDE_IS_DEVICE_MANAGER (device_manager));
  g_assert (G_IS_ASYNC_RESULT (result));
  g_assert (IDE_IS_TASK (task));

  pipeline = ide_task_get_task_data (task);

  g_assert (IDE_IS_PIPELINE (pipeline));

  if (!ide_device_manager_deploy_finish (device_manager, result, &error))
    ide_task_return_error (task, g_steal_pointer (&error));
  else
    ide_device_manager_create_runner_async (device_manager,
                                            pipeline,
                                            ide_task_get_cancellable (task),
                                            ide_run_manager_run_create_runner_cb,
                                            g_object_ref (task));

  IDE_EXIT;
}

static void
ide_run_manager_run_discover_run_command_cb (GObject      *object,
                                             GAsyncResult *result,
                                             gpointer      user_data)
{
  IdeRunManager *self = (IdeRunManager *)object;
  g_autoptr(IdeRunCommand) run_command = NULL;
  g_autoptr(IdeTask) task = user_data;
  g_autoptr(GError) error = NULL;
  IdeDeviceManager *device_manager;
  IdePipeline *pipeline;
  IdeContext *context;

  IDE_ENTRY;

  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (IDE_IS_RUN_MANAGER (self));
  g_assert (G_IS_ASYNC_RESULT (result));
  g_assert (IDE_IS_TASK (task));

  if (!(run_command = ide_run_manager_discover_run_command_finish (self, result, &error)))
    {
      ide_task_return_error (task, g_steal_pointer (&error));
      IDE_EXIT;
    }

  g_set_object (&self->current_run_command, run_command);

  pipeline = ide_task_get_task_data (task);
  g_assert (IDE_IS_PIPELINE (pipeline));

  context = ide_object_get_context (IDE_OBJECT (pipeline));
  g_assert (IDE_IS_CONTEXT (context));

  device_manager = ide_device_manager_from_context (context);
  g_assert (IDE_IS_DEVICE_MANAGER (device_manager));

  ide_device_manager_deploy_async (device_manager,
                                   pipeline,
                                   ide_task_get_cancellable (task),
                                   ide_run_manager_run_deploy_cb,
                                   g_object_ref (task));

  IDE_EXIT;
}

static void
ide_run_manager_run_install_cb (GObject      *object,
                                GAsyncResult *result,
                                gpointer      user_data)
{
  IdeRunManager *self = (IdeRunManager *)object;
  g_autoptr(IdeTask) task = user_data;
  g_autoptr(GError) error = NULL;

  IDE_ENTRY;

  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (IDE_IS_RUN_MANAGER (self));
  g_assert (G_IS_ASYNC_RESULT (result));
  g_assert (IDE_IS_TASK (task));

  if (!ide_run_manager_install_finish (self, result, &error))
    ide_task_return_error (task, g_steal_pointer (&error));
  else
    ide_run_manager_discover_run_command_async (self,
                                                ide_task_get_cancellable (task),
                                                ide_run_manager_run_discover_run_command_cb,
                                                g_object_ref (task));

  IDE_EXIT;
}

void
ide_run_manager_run_async (IdeRunManager       *self,
                           IdeBuildTarget      *build_target,
                           GCancellable        *cancellable,
                           GAsyncReadyCallback  callback,
                           gpointer             user_data)
{
  g_autoptr(IdeTask) task = NULL;
  g_autoptr(GCancellable) local_cancellable = NULL;
  g_autoptr(GError) error = NULL;
  IdeBuildManager *build_manager;
  IdePipeline *pipeline;
  IdeContext *context;

  IDE_ENTRY;

  g_return_if_fail (IDE_IS_MAIN_THREAD ());
  g_return_if_fail (IDE_IS_RUN_MANAGER (self));
  g_return_if_fail (!build_target || IDE_IS_BUILD_TARGET (build_target));
  g_return_if_fail (!cancellable || G_IS_CANCELLABLE (cancellable));
  g_return_if_fail (!g_cancellable_is_cancelled (self->cancellable));

  if (cancellable == NULL)
    cancellable = local_cancellable = g_cancellable_new ();
  ide_cancellable_chain (cancellable, self->cancellable);

  task = ide_task_new (self, cancellable, callback, user_data);
  ide_task_set_source_tag (task, ide_run_manager_run_async);

  if (ide_task_return_error_if_cancelled (task))
    IDE_EXIT;

  if (ide_run_manager_check_busy (self, &error))
    {
      ide_task_return_error (task, g_steal_pointer (&error));
      IDE_EXIT;
    }

  ide_run_manager_mark_busy (self);
  g_signal_connect_object (task,
                           "notify::completed",
                           G_CALLBACK (ide_run_manager_unmark_busy),
                           self,
                           G_CONNECT_SWAPPED);

  context = ide_object_get_context (IDE_OBJECT (self));
  build_manager = ide_build_manager_from_context (context);
  pipeline = ide_build_manager_get_pipeline (build_manager);

  if (pipeline == NULL)
    {
      ide_task_return_new_error (task,
                                 G_IO_ERROR,
                                 G_IO_ERROR_NOT_FOUND,
                                 "A pipeline cannot be found");
      IDE_EXIT;
    }

  ide_task_set_task_data (task, g_object_ref (pipeline), g_object_unref);

  ide_run_manager_install_async (self,
                                 cancellable,
                                 ide_run_manager_run_install_cb,
                                 g_steal_pointer (&task));

  IDE_EXIT;
}

gboolean
ide_run_manager_run_finish (IdeRunManager  *self,
                            GAsyncResult   *result,
                            GError        **error)
{
  gboolean ret;

  IDE_ENTRY;

  g_return_val_if_fail (IDE_IS_RUN_MANAGER (self), FALSE);
  g_return_val_if_fail (IDE_IS_TASK (result), FALSE);

  ret = ide_task_propagate_boolean (IDE_TASK (result), error);

  IDE_RETURN (ret);
}

static gboolean
do_cancel_in_timeout (gpointer user_data)
{
  g_autoptr(GCancellable) cancellable = user_data;

  IDE_ENTRY;

  g_assert (G_IS_CANCELLABLE (cancellable));

  if (!g_cancellable_is_cancelled (cancellable))
    g_cancellable_cancel (cancellable);

  IDE_RETURN (G_SOURCE_REMOVE);
}

void
ide_run_manager_cancel (IdeRunManager *self)
{
  IDE_ENTRY;

  g_return_if_fail (IDE_IS_MAIN_THREAD ());
  g_return_if_fail (IDE_IS_RUN_MANAGER (self));

  /* If the runner is still active, we can just force_exit that instead
   * of cancelling a bunch of in-flight things. This is more useful since
   * it means that we can override the exit signal.
   */
  if (self->current_runner != NULL)
    {
      ide_runner_force_quit (self->current_runner);
      IDE_EXIT;
    }

  if (self->cancellable != NULL)
    g_timeout_add (0, do_cancel_in_timeout, g_steal_pointer (&self->cancellable));
  self->cancellable = g_cancellable_new ();

  IDE_EXIT;
}

void
ide_run_manager_set_handler (IdeRunManager *self,
                             const gchar   *id)
{
  g_return_if_fail (IDE_IS_RUN_MANAGER (self));

  self->handler = NULL;

  for (GList *iter = self->handlers; iter; iter = iter->next)
    {
      const IdeRunHandlerInfo *info = iter->data;

      if (g_strcmp0 (info->id, id) == 0)
        {
          self->handler = info;
          IDE_TRACE_MSG ("run handler set to %s", info->title);
          g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_HANDLER]);
          break;
        }
    }
}

void
ide_run_manager_add_handler (IdeRunManager  *self,
                             const gchar    *id,
                             const gchar    *title,
                             const gchar    *icon_name,
                             IdeRunHandler   run_handler,
                             gpointer        user_data,
                             GDestroyNotify  user_data_destroy)
{
  IdeRunHandlerInfo *info;

  g_return_if_fail (IDE_IS_RUN_MANAGER (self));
  g_return_if_fail (id != NULL);
  g_return_if_fail (title != NULL);

  info = g_slice_new0 (IdeRunHandlerInfo);
  info->id = g_strdup (id);
  info->title = g_strdup (title);
  info->icon_name = g_strdup (icon_name);
  info->handler = run_handler;
  info->handler_data = user_data;
  info->handler_data_destroy = user_data_destroy;

  self->handlers = g_list_append (self->handlers, info);

  if (self->handler == NULL)
    self->handler = info;
}

void
ide_run_manager_remove_handler (IdeRunManager *self,
                                const gchar   *id)
{
  g_return_if_fail (IDE_IS_RUN_MANAGER (self));
  g_return_if_fail (id != NULL);

  for (GList *iter = self->handlers; iter; iter = iter->next)
    {
      IdeRunHandlerInfo *info = iter->data;

      if (g_strcmp0 (info->id, id) == 0)
        {
          self->handlers = g_list_delete_link (self->handlers, iter);

          if (self->handler == info && self->handlers != NULL)
            self->handler = self->handlers->data;
          else
            self->handler = NULL;

          ide_run_handler_info_free (info);

          break;
        }
    }
}

/**
 * ide_run_manager_get_build_target:
 *
 * Gets the build target that will be executed by the run manager which
 * was either specified to ide_run_manager_run_async() or determined by
 * the build system.
 *
 * Returns: (transfer none): An #IdeBuildTarget or %NULL if no build target
 *   has been set.
 */
IdeBuildTarget *
ide_run_manager_get_build_target (IdeRunManager *self)
{
  g_return_val_if_fail (IDE_IS_RUN_MANAGER (self), NULL);

  return self->build_target;
}

void
ide_run_manager_set_build_target (IdeRunManager  *self,
                                  IdeBuildTarget *build_target)
{
  g_return_if_fail (IDE_IS_RUN_MANAGER (self));
  g_return_if_fail (!build_target || IDE_IS_BUILD_TARGET (build_target));

  if (build_target == self->build_target)
    return;

  if (self->build_target)
    ide_clear_and_destroy_object (&self->build_target);

  if (build_target)
    self->build_target = g_object_ref (build_target);

  g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_BUILD_TARGET]);
}

static gint
compare_targets (gconstpointer a,
                 gconstpointer b)
{
  const IdeBuildTarget * const *a_target = a;
  const IdeBuildTarget * const *b_target = b;

  return ide_build_target_compare (*a_target, *b_target);
}

static void
collect_extensions (PeasExtensionSet *set,
                    PeasPluginInfo   *plugin_info,
                    PeasExtension    *exten,
                    gpointer          user_data)
{
  DiscoverState *state = user_data;

  g_assert (state != NULL);
  g_assert (IDE_IS_BUILD_TARGET_PROVIDER (exten));

  state->providers = g_list_append (state->providers, g_object_ref (exten));
  state->active++;
}

static void
ide_run_manager_provider_get_targets_cb (GObject      *object,
                                         GAsyncResult *result,
                                         gpointer      user_data)
{
  IdeBuildTargetProvider *provider = (IdeBuildTargetProvider *)object;
  g_autoptr(IdeBuildTarget) first = NULL;
  g_autoptr(IdeTask) task = user_data;
  g_autoptr(GPtrArray) ret = NULL;
  g_autoptr(GError) error = NULL;
  IdeRunManager *self;
  DiscoverState *state;

  IDE_ENTRY;

  g_assert (IDE_IS_BUILD_TARGET_PROVIDER (provider));
  g_assert (G_IS_ASYNC_RESULT (result));
  g_assert (IDE_IS_TASK (task));

  self = ide_task_get_source_object (task);
  state = ide_task_get_task_data (task);

  g_assert (IDE_IS_RUN_MANAGER (self));
  g_assert (state != NULL);
  g_assert (state->active > 0);
  g_assert (g_list_find (state->providers, provider) != NULL);

  ret = ide_build_target_provider_get_targets_finish (provider, result, &error);
  IDE_PTR_ARRAY_SET_FREE_FUNC (ret, g_object_unref);

  if (ret != NULL)
    {
      for (guint i = 0; i < ret->len; i++)
        {
          IdeBuildTarget *target = g_ptr_array_index (ret, i);

          if (ide_object_is_root (IDE_OBJECT (target)))
            ide_object_append (IDE_OBJECT (self), IDE_OBJECT (target));

          g_ptr_array_add (state->results, g_object_ref (target));
        }
    }

  ide_object_destroy (IDE_OBJECT (provider));

  state->active--;

  if (state->active > 0)
    return;

  if (state->results->len == 0)
    {
      if (error != NULL)
        ide_task_return_error (task, g_steal_pointer (&error));
      else
        ide_task_return_new_error (task,
                                   IDE_RUNTIME_ERROR,
                                   IDE_RUNTIME_ERROR_TARGET_NOT_FOUND,
                                   _("Failed to locate a build target"));
      IDE_EXIT;
    }

  g_ptr_array_sort (state->results, compare_targets);

  /* Steal the first item so that it is not destroyed */
  first = ide_ptr_array_steal_index (state->results,
                                     0,
                                     (GDestroyNotify)ide_object_unref_and_destroy);
  ide_task_return_pointer (task,
                           IDE_OBJECT (g_steal_pointer (&first)),
                           ide_object_unref_and_destroy);

  IDE_EXIT;
}

void
ide_run_manager_discover_default_target_async (IdeRunManager       *self,
                                               GCancellable        *cancellable,
                                               GAsyncReadyCallback  callback,
                                               gpointer             user_data)
{
  g_autoptr(PeasExtensionSet) set = NULL;
  g_autoptr(IdeTask) task = NULL;
  DiscoverState *state;

  IDE_ENTRY;

  g_return_if_fail (IDE_IS_RUN_MANAGER (self));
  g_return_if_fail (!cancellable || G_IS_CANCELLABLE (cancellable));

  task = ide_task_new (self, cancellable, callback, user_data);
  ide_task_set_source_tag (task, ide_run_manager_discover_default_target_async);
  ide_task_set_priority (task, G_PRIORITY_LOW);

  set = peas_extension_set_new (peas_engine_get_default (),
                                IDE_TYPE_BUILD_TARGET_PROVIDER,
                                NULL);

  state = g_slice_new0 (DiscoverState);
  state->results = g_ptr_array_new_with_free_func ((GDestroyNotify)ide_object_unref_and_destroy);
  state->providers = NULL;
  state->active = 0;

  peas_extension_set_foreach (set, collect_extensions, state);

  for (const GList *iter = state->providers; iter; iter = iter->next)
    ide_object_append (IDE_OBJECT (self), IDE_OBJECT (iter->data));

  ide_task_set_task_data (task, state, discover_state_free);

  for (const GList *iter = state->providers; iter != NULL; iter = iter->next)
    {
      IdeBuildTargetProvider *provider = iter->data;

      ide_build_target_provider_get_targets_async (provider,
                                                   cancellable,
                                                   ide_run_manager_provider_get_targets_cb,
                                                   g_object_ref (task));
    }

  if (state->active == 0)
    ide_task_return_new_error (task,
                               IDE_RUNTIME_ERROR,
                               IDE_RUNTIME_ERROR_TARGET_NOT_FOUND,
                               _("Failed to locate a build target"));

  IDE_EXIT;
}

/**
 * ide_run_manager_discover_default_target_finish:
 *
 * Returns: (transfer full): An #IdeBuildTarget if successful; otherwise %NULL
 *   and @error is set.
 */
IdeBuildTarget *
ide_run_manager_discover_default_target_finish (IdeRunManager  *self,
                                                GAsyncResult   *result,
                                                GError        **error)
{
  IdeBuildTarget *ret;

  IDE_ENTRY;

  g_return_val_if_fail (IDE_IS_RUN_MANAGER (self), NULL);
  g_return_val_if_fail (IDE_IS_TASK (result), NULL);

  ret = ide_task_propagate_pointer (IDE_TASK (result), error);

  IDE_RETURN (ret);
}

const GList *
_ide_run_manager_get_handlers (IdeRunManager *self)
{
  g_return_val_if_fail (IDE_IS_RUN_MANAGER (self), NULL);

  return self->handlers;
}

const gchar *
ide_run_manager_get_handler (IdeRunManager *self)
{
  g_return_val_if_fail (IDE_IS_RUN_MANAGER (self), NULL);

  if (self->handler != NULL)
    return self->handler->id;

  return NULL;
}

static void
ide_run_manager_run_action_cb (GObject      *object,
                               GAsyncResult *result,
                               gpointer      user_data)
{
  IdeRunManager *self = (IdeRunManager *)object;
  IdeContext *context;
  g_autoptr(GError) error = NULL;

  g_assert (IDE_IS_RUN_MANAGER (self));
  g_assert (G_IS_ASYNC_RESULT (result));

  context = ide_object_get_context (IDE_OBJECT (self));

  /* Propagate the error to the context */
  if (!ide_run_manager_run_finish (self, result, &error))
    ide_context_warning (context, "%s", error->message);
}

static void
ide_run_manager_actions_run (IdeRunManager *self,
                             GVariant      *param)
{
  IDE_ENTRY;

  g_assert (IDE_IS_RUN_MANAGER (self));

  ide_run_manager_run_async (self,
                             NULL,
                             NULL,
                             ide_run_manager_run_action_cb,
                             NULL);

  IDE_EXIT;
}

static void
ide_run_manager_actions_run_with_handler (IdeRunManager *self,
                                          GVariant      *param)
{
  const gchar *handler = NULL;
  g_autoptr(GVariant) sunk = NULL;

  IDE_ENTRY;

  g_assert (IDE_IS_RUN_MANAGER (self));

  if (param != NULL)
  {
    handler = g_variant_get_string (param, NULL);
    if (g_variant_is_floating (param))
      sunk = g_variant_ref_sink (param);
  }

  /* Use specified handler, if provided */
  if (!ide_str_empty0 (handler))
    ide_run_manager_set_handler (self, handler);

  ide_run_manager_run_async (self,
                             NULL,
                             NULL,
                             ide_run_manager_run_action_cb,
                             NULL);

  IDE_EXIT;
}

static void
ide_run_manager_actions_stop (IdeRunManager *self,
                              GVariant      *param)
{
  IDE_ENTRY;

  g_assert (IDE_IS_RUN_MANAGER (self));

  ide_run_manager_cancel (self);

  IDE_EXIT;
}

static void
ide_run_manager_init (IdeRunManager *self)
{
  GtkTextDirection text_dir;

  self->cancellable = g_cancellable_new ();

  /* Setup initial text direction state */
  text_dir = gtk_widget_get_default_direction ();
  if (text_dir == GTK_TEXT_DIR_LTR)
    ide_run_manager_set_action_state (self,
                                      "text-direction",
                                      text_dir == GTK_TEXT_DIR_LTR ?
                                        g_variant_new_string ("ltr") :
                                        g_variant_new_string ("rtl"));

  ide_run_manager_add_handler (self,
                               "run",
                               _("Run"),
                               "builder-run-start-symbolic",
                               NULL,
                               NULL,
                               NULL);
}

void
_ide_run_manager_drop_caches (IdeRunManager *self)
{
  g_return_if_fail (IDE_IS_RUN_MANAGER (self));

  self->last_change_seq = 0;
}

static void
ide_run_manager_actions_messages_debug_all (IdeRunManager *self,
                                            GVariant      *param)
{
  IDE_ENTRY;

  g_assert (IDE_IS_RUN_MANAGER (self));

  self->messages_debug_all = !self->messages_debug_all;
  ide_run_manager_set_action_state (self,
                                    "messages-debug-all",
                                    g_variant_new_boolean (self->messages_debug_all));

  IDE_EXIT;
}

typedef struct
{
  GString *errors;
  GListStore *store;
  int n_active;
} ListCommands;

static void
list_commands_free (ListCommands *state)
{
  g_assert (state != NULL);
  g_assert (state->n_active == 0);

  g_string_free (state->errors, TRUE);
  state->errors = NULL;
  g_clear_object (&state->store);
  g_slice_free (ListCommands, state);
}

static void
ide_run_manager_list_commands_cb (GObject      *object,
                                  GAsyncResult *result,
                                  gpointer      user_data)
{
  IdeRunCommandProvider *provider = (IdeRunCommandProvider *)object;
  g_autoptr(GListModel) model = NULL;
  g_autoptr(IdeTask) task = user_data;
  g_autoptr(GError) error = NULL;
  ListCommands *state;

  g_assert (IDE_IS_RUN_COMMAND_PROVIDER (provider));
  g_assert (G_IS_ASYNC_RESULT (result));
  g_assert (IDE_IS_TASK (task));

  state = ide_task_get_task_data (task);

  g_assert (state != NULL);
  g_assert (state->n_active > 0);
  g_assert (G_IS_LIST_STORE (state->store));

  if (!(model = ide_run_command_provider_list_commands_finish (provider, result, &error)))
    {
      if (!ide_error_ignore (error))
        {
          if (state->errors->len > 0)
            g_string_append (state->errors, "; ");
          g_string_append (state->errors, error->message);
        }
    }
  else
    {
      g_list_store_append (state->store, model);
    }

  state->n_active--;

  if (state->n_active == 0)
    {
      if (state->errors->len > 0)
        ide_task_return_new_error (task,
                                   G_IO_ERROR,
                                   G_IO_ERROR_FAILED,
                                   "%s",
                                   state->errors->str);
      else
        ide_task_return_pointer (task,
                                 gtk_flatten_list_model_new (G_LIST_MODEL (g_steal_pointer (&state->store))),
                                 g_object_unref);
    }
}

static void
ide_run_manager_list_commands_foreach_cb (IdeExtensionSetAdapter *set,
                                          PeasPluginInfo         *plugin_info,
                                          PeasExtension          *exten,
                                          gpointer                user_data)
{
  IdeRunCommandProvider *provider = (IdeRunCommandProvider *)exten;
  IdeTask *task = user_data;
  ListCommands *state;

  g_assert (IDE_IS_EXTENSION_SET_ADAPTER (set));
  g_assert (plugin_info != NULL);
  g_assert (IDE_IS_RUN_COMMAND_PROVIDER (provider));
  g_assert (IDE_IS_TASK (task));

  state = ide_task_get_task_data (task);
  state->n_active++;

  ide_run_command_provider_list_commands_async (provider,
                                                ide_task_get_cancellable (task),
                                                ide_run_manager_list_commands_cb,
                                                g_object_ref (task));
}

void
ide_run_manager_list_commands_async (IdeRunManager       *self,
                                     GCancellable        *cancellable,
                                     GAsyncReadyCallback  callback,
                                     gpointer             user_data)
{
  g_autoptr(IdeTask) task = NULL;
  ListCommands *state;

  IDE_ENTRY;

  g_return_if_fail (IDE_IS_RUN_MANAGER (self));
  g_return_if_fail (!cancellable || G_IS_CANCELLABLE (cancellable));

  state = g_slice_new0 (ListCommands);
  state->store = g_list_store_new (G_TYPE_LIST_MODEL);
  state->errors = g_string_new (NULL);

  task = ide_task_new (self, cancellable, callback, user_data);
  ide_task_set_source_tag (task, ide_run_manager_list_commands_async);
  ide_task_set_task_data (task, state, list_commands_free);

  if (self->run_command_providers)
    ide_extension_set_adapter_foreach (self->run_command_providers,
                                       ide_run_manager_list_commands_foreach_cb,
                                       task);

  if (state->n_active == 0)
    ide_task_return_new_error (task,
                               G_IO_ERROR,
                               G_IO_ERROR_NOT_SUPPORTED,
                               "No run command providers available");

  IDE_EXIT;
}

/**
 * ide_run_manager_list_commands_finish:
 *
 * Returns: (transfer full): a #GListModel of #IdeRunCommand
 */
GListModel *
ide_run_manager_list_commands_finish (IdeRunManager  *self,
                                      GAsyncResult   *result,
                                      GError        **error)
{
  g_return_val_if_fail (IDE_IS_RUN_MANAGER (self), NULL);
  g_return_val_if_fail (IDE_IS_TASK (result), NULL);

  return ide_task_propagate_pointer (IDE_TASK (result), error);
}

static void
ide_run_manager_discover_run_command_cb (GObject      *object,
                                         GAsyncResult *result,
                                         gpointer      user_data)
{
  IdeRunManager *self = (IdeRunManager *)object;
  g_autoptr(IdeRunCommand) best = NULL;
  g_autoptr(GListModel) model = NULL;
  g_autoptr(IdeTask) task = user_data;
  g_autoptr(GError) error = NULL;
  const char *default_id;
  guint n_items;
  int best_priority = G_MAXINT;

  IDE_ENTRY;

  g_assert (IDE_IS_RUN_MANAGER (self));
  g_assert (G_IS_ASYNC_RESULT (result));
  g_assert (IDE_IS_TASK (task));

  if (!(model = ide_run_manager_list_commands_finish (self, result, &error)))
    {
      ide_task_return_error (task, g_steal_pointer (&error));
      IDE_EXIT;
    }

  default_id = ide_task_get_task_data (task);
  n_items = g_list_model_get_n_items (model);

  for (guint i = 0; i < n_items; i++)
    {
      g_autoptr(IdeRunCommand) run_command = g_list_model_get_item (model, i);
      const char *id;
      int priority;

      g_assert (IDE_IS_RUN_COMMAND (run_command));

      id = ide_run_command_get_id (run_command);
      priority = ide_run_command_get_priority (run_command);

      if (!ide_str_empty0 (id) &&
          !ide_str_empty0 (default_id) &&
          strcmp (default_id, id) == 0)
        {
          ide_task_return_pointer (task,
                                   g_steal_pointer (&run_command),
                                   g_object_unref);
          IDE_EXIT;
        }

      if (best == NULL || priority < best_priority)
        {
          g_set_object (&best, run_command);
          best_priority = priority;
        }
    }

  if (best != NULL)
    ide_task_return_pointer (task,
                             g_steal_pointer (&best),
                             g_object_unref);
  else
    ide_task_return_new_error (task,
                               G_IO_ERROR,
                               G_IO_ERROR_NOT_FOUND,
                               "No run command discovered. Set one manually.");

  IDE_EXIT;
}

void
ide_run_manager_discover_run_command_async (IdeRunManager       *self,
                                            GCancellable        *cancellable,
                                            GAsyncReadyCallback  callback,
                                            gpointer             user_data)
{
  g_autoptr(IdeTask) task = NULL;

  IDE_ENTRY;

  g_return_if_fail (IDE_IS_RUN_MANAGER (self));
  g_return_if_fail (!cancellable || G_IS_CANCELLABLE (cancellable));

  task = ide_task_new (self, cancellable, callback, user_data);
  ide_task_set_source_tag (task, ide_run_manager_discover_run_command_async);
  ide_task_set_task_data (task, g_strdup (self->default_run_command), g_free);

  ide_run_manager_list_commands_async (self,
                                       cancellable,
                                       ide_run_manager_discover_run_command_cb,
                                       g_steal_pointer (&task));

  IDE_EXIT;
}

/**
 * ide_run_manager_discover_run_command_finish:
 * @self: a #IdeRunManager
 *
 * Complete request to discover the default run command.
 *
 * Returns: (transfer full): an #IdeRunCommand if successful; otherwise
 *   %NULL and @error is set.
 */
IdeRunCommand *
ide_run_manager_discover_run_command_finish (IdeRunManager  *self,
                                             GAsyncResult   *result,
                                             GError        **error)
{
  IdeRunCommand *run_command;

  IDE_ENTRY;

  g_return_val_if_fail (IDE_IS_RUN_MANAGER (self), NULL);
  g_return_val_if_fail (IDE_IS_TASK (result), NULL);

  run_command = ide_task_propagate_pointer (IDE_TASK (result), error);

  g_return_val_if_fail (!run_command || IDE_IS_RUN_COMMAND (run_command), NULL);

  IDE_RETURN (run_command);
}
