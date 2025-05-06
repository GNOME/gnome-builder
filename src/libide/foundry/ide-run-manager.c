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

#include <unistd.h>

#include <glib/gi18n.h>

#include <gtk/gtk.h>

#include <libpeas.h>

#include <libide-core.h>
#include <libide-plugins.h>
#include <libide-threading.h>
#include <libide-vcs.h>

#include "ide-marshal.h"
#include "ide-private.h"

#include "ide-build-manager.h"
#include "ide-build-system.h"
#include "ide-deploy-strategy.h"
#include "ide-device-manager.h"
#include "ide-foundry-compat.h"
#include "ide-no-tool-private.h"
#include "ide-run-command.h"
#include "ide-run-command-provider.h"
#include "ide-run-context.h"
#include "ide-run-manager-private.h"
#include "ide-run-manager.h"
#include "ide-run-tool-private.h"
#include "ide-runtime.h"

struct _IdeRunManager
{
  IdeObject                parent_instance;

  IdeSettings             *project_settings;

  GCancellable            *cancellable;
  IdeNotification         *notif;
  IdeExtensionSetAdapter  *run_command_providers;
  IdeExtensionSetAdapter  *run_tools;
  IdeRunTool              *run_tool;

  IdeSubprocess           *current_subprocess;
  IdeRunCommand           *current_run_command;

  /* Keep track of last change sequence from the file monitor
   * so that we can maybe skip past install phase and make
   * secondary execution time faster.
   */
  guint64                  last_change_seq;
  guint64                  pending_last_change_seq;

  guint                    busy;

  guint                    has_installed_once : 1;
  guint                    sent_signal : 1;
};

static void initable_iface_init                         (GInitableIface *iface);
static void ide_run_manager_actions_restart             (IdeRunManager  *self,
                                                         GVariant       *param);
static void ide_run_manager_actions_run                 (IdeRunManager  *self,
                                                         GVariant       *param);
static void ide_run_manager_actions_run_with_handler    (IdeRunManager  *self,
                                                         GVariant       *param);
static void ide_run_manager_actions_stop                (IdeRunManager  *self,
                                                         GVariant       *param);
static void ide_run_manager_actions_color_scheme        (IdeRunManager  *self,
                                                         GVariant       *param);
static void ide_run_manager_actions_accent_color        (IdeRunManager  *self,
                                                         GVariant       *param);
static void ide_run_manager_actions_adaptive_preview    (IdeRunManager  *self,
                                                         GVariant       *param);
static void ide_run_manager_actions_high_contrast       (IdeRunManager  *self,
                                                         GVariant       *param);
static void ide_run_manager_actions_text_direction      (IdeRunManager  *self,
                                                         GVariant       *param);
static void ide_run_manager_actions_interactive         (IdeRunManager  *self,
                                                         GVariant       *param);
static void ide_run_manager_actions_renderer            (IdeRunManager  *self,
                                                         GVariant       *param);

IDE_DEFINE_ACTION_GROUP (IdeRunManager, ide_run_manager, {
  { "restart", ide_run_manager_actions_restart },
  { "run", ide_run_manager_actions_run },
  { "run-with-handler", ide_run_manager_actions_run_with_handler, "s" },
  { "stop", ide_run_manager_actions_stop },
  { "color-scheme", ide_run_manager_actions_color_scheme, "s", "'follow'" },
  { "accent-color", ide_run_manager_actions_accent_color, "s", "'system'" },
  { "renderer", ide_run_manager_actions_renderer, "s", "'default'" },
  { "adaptive-preview", ide_run_manager_actions_adaptive_preview, NULL, "false" },
  { "high-contrast", ide_run_manager_actions_high_contrast, NULL, "false" },
  { "text-direction", ide_run_manager_actions_text_direction, "s", "''" },
  { "interactive", ide_run_manager_actions_interactive, NULL, "false" },
})

G_DEFINE_TYPE_EXTENDED (IdeRunManager, ide_run_manager, IDE_TYPE_OBJECT, G_TYPE_FLAG_FINAL,
                        G_IMPLEMENT_INTERFACE (G_TYPE_INITABLE, initable_iface_init)
                        G_IMPLEMENT_INTERFACE (G_TYPE_ACTION_GROUP, ide_run_manager_init_action_group))

enum {
  PROP_0,
  PROP_BUSY,
  PROP_ICON_NAME,
  PROP_RUN_TOOL,
  N_PROPS
};

enum {
  RUN,
  STARTED,
  STOPPED,
  N_SIGNALS
};

static GParamSpec *properties [N_PROPS];
static guint signals [N_SIGNALS];

static IdeRunTool *
ide_run_manager_get_run_tool (IdeRunManager *self)
{
  g_assert (IDE_IS_RUN_MANAGER (self));
  g_assert (IDE_IS_RUN_TOOL (self->run_tool));

  return self->run_tool;
}

void
ide_run_manager_set_run_tool_from_plugin_info (IdeRunManager  *self,
                                               PeasPluginInfo *plugin_info)
{
  g_autoptr(IdeRunTool) no_tool = NULL;
  GObject *exten = NULL;

  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (IDE_IS_RUN_MANAGER (self));

  if (plugin_info != NULL)
    exten = ide_extension_set_adapter_get_extension (self->run_tools, plugin_info);

  if (exten == NULL)
    {
      if (IDE_IS_NO_TOOL (self->run_tool))
        return;
      no_tool = ide_no_tool_new ();
      exten = (GObject *)no_tool;
    }

  if (g_set_object (&self->run_tool, IDE_RUN_TOOL (exten)))
    {
      g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_RUN_TOOL]);
      g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_ICON_NAME]);
    }
}

static void
ide_run_manager_set_run_tool_from_module_name (IdeRunManager *self,
                                               const char    *name)
{
  PeasPluginInfo *plugin_info = NULL;

  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (IDE_IS_RUN_MANAGER (self));

  g_debug ("Looking for run-tool from module %s", name);

  ide_object_message (IDE_OBJECT (self),
                      /* translators: %s is replaced with the name of the run tool such as "valgrind" */
                      _("User requested run tool “%s”"),
                      name);

  if (!ide_str_empty0 (name))
    plugin_info = peas_engine_get_plugin_info (peas_engine_get_default (), name);

  ide_run_manager_set_run_tool_from_plugin_info (self, plugin_info);
}

static void
ide_run_manager_actions_adaptive_preview (IdeRunManager *self,
                                          GVariant      *param)
{
  GVariant *state;

  g_assert (IDE_IS_RUN_MANAGER (self));

  state = ide_run_manager_get_action_state (self, "adaptive-preview");
  ide_run_manager_set_action_state (self,
                                    "adaptive-preview",
                                    g_variant_new_boolean (!g_variant_get_boolean (state)));
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
ide_run_manager_actions_interactive (IdeRunManager *self,
                                     GVariant      *param)
{
  GVariant *state;

  g_assert (IDE_IS_RUN_MANAGER (self));

  state = ide_run_manager_get_action_state (self, "interactive");
  ide_run_manager_set_action_state (self,
                                    "interactive",
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
ide_run_manager_actions_accent_color (IdeRunManager *self,
                                      GVariant      *param)
{
  const char *str;

  g_assert (IDE_IS_RUN_MANAGER (self));
  g_assert (param != NULL);
  g_assert (g_variant_is_of_type (param, G_VARIANT_TYPE_STRING));

  str = g_variant_get_string (param, NULL);
  if (!g_strv_contains (IDE_STRV_INIT ("system", "blue", "teal", "green", "yellow", "orange",
                                       "red", "pink", "purple", "slate"), str))
    {
      str = "system";
    }

  ide_run_manager_set_action_state (self,
                                    "accent-color",
                                    g_variant_new_string (str));
}

static void
ide_run_manager_actions_renderer (IdeRunManager *self,
                                  GVariant      *param)
{
  const char *str;

  g_assert (IDE_IS_RUN_MANAGER (self));
  g_assert (param != NULL);
  g_assert (g_variant_is_of_type (param, G_VARIANT_TYPE_STRING));

  str = g_variant_get_string (param, NULL);
  if (!g_strv_contains (IDE_STRV_INIT ("default", "gl", "ngl", "vulkan", "cairo"), str))
    str = "default";

  ide_run_manager_set_action_state (self,
                                    "renderer",
                                    g_variant_new_string (str));
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

  ide_run_manager_set_action_enabled (self, "restart",
                                      self->busy && can_build);
  ide_run_manager_set_action_enabled (self, "run",
                                      !self->busy && can_build);
  ide_run_manager_set_action_enabled (self, "run-with-handler",
                                      !self->busy && can_build);
  ide_run_manager_set_action_enabled (self, "stop", !!self->busy);
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
ide_run_manager_destroy (IdeObject *object)
{
  IdeRunManager *self = (IdeRunManager *)object;

  g_clear_object (&self->project_settings);
  g_clear_object (&self->cancellable);
  g_clear_object (&self->current_run_command);
  g_clear_object (&self->current_subprocess);
  g_clear_object (&self->run_tool);

  ide_clear_and_destroy_object (&self->run_command_providers);
  ide_clear_and_destroy_object (&self->run_tools);

  IDE_OBJECT_CLASS (ide_run_manager_parent_class)->destroy (object);
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

const char *
ide_run_manager_get_icon_name (IdeRunManager *self)
{
  g_assert (IDE_IS_RUN_MANAGER (self));

  if (self->run_tool == NULL)
    return NULL;

  return ide_run_tool_get_icon_name (self->run_tool);
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

  self->project_settings = ide_context_ref_settings (context,
                                                     "org.gnome.builder.project");

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

  self->run_tools = ide_extension_set_adapter_new (IDE_OBJECT (self),
                                                   peas_engine_get_default (),
                                                   IDE_TYPE_RUN_TOOL,
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

    case PROP_ICON_NAME:
      g_value_set_string (value, ide_run_manager_get_icon_name (self));
      break;

    case PROP_RUN_TOOL:
      g_value_set_object (value, ide_run_manager_get_run_tool (self));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
ide_run_manager_class_init (IdeRunManagerClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  IdeObjectClass *i_object_class = IDE_OBJECT_CLASS (klass);

  object_class->get_property = ide_run_manager_get_property;

  i_object_class->destroy = ide_run_manager_destroy;

  properties [PROP_BUSY] =
    g_param_spec_boolean ("busy", NULL, NULL,
                          FALSE,
                          (G_PARAM_READABLE | G_PARAM_STATIC_STRINGS));

  properties [PROP_ICON_NAME] =
    g_param_spec_string ("icon-name", NULL, NULL,
                         NULL,
                         (G_PARAM_READABLE | G_PARAM_STATIC_STRINGS));

  properties [PROP_RUN_TOOL] =
    g_param_spec_object ("run-tool", NULL, NULL,
                         IDE_TYPE_RUN_TOOL,
                         (G_PARAM_READABLE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, N_PROPS, properties);

  /**
   * IdeRunManager::run:
   * @self: An #IdeRunManager
   * @run_context: An #IdeRunContext
   *
   * This signal is emitted to allow plugins to add additional settings to a
   * run context before a launcher is created.
   *
   * Generally this can only be used in certain situations and you probably
   * want to modify the run context in another way such as a deploy strategry,
   * runtime, or similar.
   */
  signals [RUN] =
    g_signal_new_class_handler ("run",
                                G_TYPE_FROM_CLASS (klass),
                                G_SIGNAL_RUN_LAST,
                                NULL,
                                NULL,
                                NULL,
                                ide_marshal_VOID__OBJECT,
                                G_TYPE_NONE,
                                1,
                                IDE_TYPE_RUN_CONTEXT);
  g_signal_set_va_marshaller (signals [RUN],
                              G_TYPE_FROM_CLASS (klass),
                              ide_marshal_VOID__OBJECTv);

  /**
   * IdeRunManager::started:
   *
   * This signal is emitted when the run manager has spawned a new subprocess.
   */
  signals [STARTED] =
    g_signal_new ("started",
                  G_TYPE_FROM_CLASS (klass),
                  G_SIGNAL_RUN_LAST,
                  0,
                  NULL, NULL,
                  ide_marshal_VOID__VOID,
                  G_TYPE_NONE, 0);
  g_signal_set_va_marshaller (signals [STARTED],
                              G_TYPE_FROM_CLASS (klass),
                              ide_marshal_VOID__VOIDv);

  /**
   * IdeRunManager::stopped:
   *
   * This signal is emitted when the run manager has detected the running
   * subprocess has exited.
   */
  signals [STOPPED] =
    g_signal_new ("stopped",
                  G_TYPE_FROM_CLASS (klass),
                  G_SIGNAL_RUN_LAST,
                  0,
                  NULL, NULL,
                  ide_marshal_VOID__VOID,
                  G_TYPE_NONE, 0);
  g_signal_set_va_marshaller (signals [STOPPED],
                              G_TYPE_FROM_CLASS (klass),
                              ide_marshal_VOID__VOIDv);
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
apply_messages_debug (IdeRunContext *run_context,
                      gboolean       messages_debug_all)
{
  IDE_ENTRY;

  g_assert (IDE_IS_RUN_CONTEXT (run_context));

  if (messages_debug_all)
    ide_run_context_setenv (run_context, "G_MESSAGES_DEBUG", "all");

  IDE_EXIT;
}

static void
apply_renderer (IdeRunContext *run_context,
                const char    *renderer)
{
  IDE_ENTRY;

  g_assert (IDE_IS_RUN_CONTEXT (run_context));
  g_assert (renderer != NULL);

  g_debug ("Applying renderer \"%s\"", renderer);

  if (ide_str_equal0 (renderer, "default"))
    ide_run_context_unsetenv (run_context, "GSK_RENDERER");
  else
    ide_run_context_setenv (run_context, "GSK_RENDERER", renderer);

  IDE_EXIT;
}

static void
apply_color_scheme (IdeRunContext *run_context,
                    const char    *color_scheme)
{
  IDE_ENTRY;

  g_assert (IDE_IS_RUN_CONTEXT (run_context));
  g_assert (color_scheme != NULL);

  g_debug ("Applying color-scheme \"%s\"", color_scheme);

  if (ide_str_equal0 (color_scheme, "follow"))
    {
      ide_run_context_unsetenv (run_context, "ADW_DEBUG_COLOR_SCHEME");
      ide_run_context_unsetenv (run_context, "HDY_DEBUG_COLOR_SCHEME");
    }
  else if (ide_str_equal0 (color_scheme, "force-light"))
    {
      ide_run_context_setenv (run_context, "ADW_DEBUG_COLOR_SCHEME", "prefer-light");
      ide_run_context_setenv (run_context, "HDY_DEBUG_COLOR_SCHEME", "prefer-light");
    }
  else if (ide_str_equal0 (color_scheme, "force-dark"))
    {
      ide_run_context_setenv (run_context, "ADW_DEBUG_COLOR_SCHEME", "prefer-dark");
      ide_run_context_setenv (run_context, "HDY_DEBUG_COLOR_SCHEME", "prefer-dark");
    }
  else
    {
      g_warn_if_reached ();
    }

  IDE_EXIT;
}

static void
apply_accent_color (IdeRunContext *run_context,
                    const char    *accent_color)
{
  IDE_ENTRY;

  g_assert (IDE_IS_RUN_CONTEXT (run_context));
  g_assert (accent_color != NULL);

  g_debug ("Applying accent-color \"%s\"", accent_color);

  if (ide_str_equal0 (accent_color, "system"))
    ide_run_context_unsetenv (run_context, "ADW_DEBUG_ACCENT_COLOR");
  else
    ide_run_context_setenv (run_context, "ADW_DEBUG_ACCENT_COLOR", accent_color);

  IDE_EXIT;
}

static void
apply_adaptive_preview (IdeRunContext *run_context,
                        gboolean       adaptive_preview)
{
  IDE_ENTRY;

  g_assert (IDE_IS_RUN_CONTEXT (run_context));

  g_debug ("Applying adaptive-preview %d", adaptive_preview);

  if (adaptive_preview)
    {
      ide_run_context_setenv (run_context, "ADW_DEBUG_ADAPTIVE_PREVIEW", "1");
    }
  else
    {
      ide_run_context_unsetenv (run_context, "ADW_DEBUG_ADAPTIVE_PREVIEW");
    }

  IDE_EXIT;
}

static void
apply_high_contrast (IdeRunContext *run_context,
                     gboolean       high_contrast)
{
  IDE_ENTRY;

  g_assert (IDE_IS_RUN_CONTEXT (run_context));

  g_debug ("Applying high-contrast %d", high_contrast);

  if (high_contrast)
    {
      ide_run_context_setenv (run_context, "ADW_DEBUG_HIGH_CONTRAST", "1");
      ide_run_context_setenv (run_context, "HDY_DEBUG_HIGH_CONTRAST", "1");
    }
  else
    {
      ide_run_context_unsetenv (run_context, "ADW_DEBUG_HIGH_CONTRAST");
      ide_run_context_unsetenv (run_context, "HDY_DEBUG_HIGH_CONTRAST");
    }

  IDE_EXIT;
}

static void
apply_gtk_debug (IdeRunContext *run_context,
                 const char    *text_dir_str,
                 gboolean       interactive)
{
  g_autoptr(GString) str = NULL;
  GtkTextDirection dir;

  IDE_ENTRY;

  g_assert (IDE_IS_RUN_CONTEXT (run_context));

  str = g_string_new (NULL);

  if (ide_str_equal0 (text_dir_str, "rtl"))
    dir = GTK_TEXT_DIR_RTL;
  else if (ide_str_equal0 (text_dir_str, "ltr"))
    dir = GTK_TEXT_DIR_LTR;
  else
    g_return_if_reached ();

  if (dir != gtk_widget_get_default_direction ())
    {
      if (str->len)
        g_string_append_c (str, ',');
      g_string_append (str, "invert-text-dir");
    }

  if (interactive)
    {
      if (str->len)
        g_string_append_c (str, ',');
      g_string_append (str, "interactive");
    }

  if (str->len > 0)
    ide_run_context_setenv (run_context, "GTK_DEBUG", str->str);

  IDE_EXIT;
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

  if (!ide_settings_get_boolean (self->project_settings, "install-before-run"))
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
ide_run_manager_run_subprocess_wait_check_cb (GObject      *object,
                                              GAsyncResult *result,
                                              gpointer      user_data)
{
  IdeSubprocess *subprocess = (IdeSubprocess *)object;
  g_autoptr(IdeTask) task = user_data;
  g_autoptr(GError) error = NULL;
  IdeRunManager *self;

  IDE_ENTRY;

  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (IDE_IS_SUBPROCESS (subprocess));
  g_assert (G_IS_ASYNC_RESULT (result));
  g_assert (IDE_IS_TASK (task));

  self = ide_task_get_source_object (task);
  g_assert (IDE_IS_RUN_MANAGER (self));

  if (self->notif != NULL)
    ide_notification_withdraw (self->notif);

  g_clear_object (&self->notif);
  g_clear_object (&self->current_subprocess);

  if (!ide_subprocess_wait_check_finish (subprocess, result, &error))
    ide_task_return_error (task, g_error_copy (error));
  else
    ide_task_return_boolean (task, TRUE);

  if (error != NULL)
    ide_object_message (IDE_OBJECT (self),
                        _("Application exited with error: %s"),
                        error->message);
  else
    ide_object_message (IDE_OBJECT (self), _("Application exited"));

  if (self->run_tool != NULL)
    _ide_run_tool_emit_stopped (self->run_tool);

  g_signal_emit (self, signals[STOPPED], 0);

  IDE_EXIT;
}

static void
ide_run_manager_prepare_run_context (IdeRunManager *self,
                                     IdeRunContext *run_context,
                                     IdeRunCommand *run_command,
                                     IdePipeline   *pipeline)
{
  g_auto(GStrv) environ = NULL;

  IDE_ENTRY;

  g_assert (IDE_IS_RUN_MANAGER (self));
  g_assert (IDE_IS_RUN_CONTEXT (run_context));
  g_assert (IDE_IS_RUN_COMMAND (run_command));
  g_assert (IDE_IS_PIPELINE (pipeline));
  g_assert (IDE_IS_RUN_TOOL (self->run_tool));

  g_debug ("Preparing run context using run tool %s",
           G_OBJECT_TYPE_NAME (self->run_tool));

  /* The very first thing we need to do is allow the current run tool
   * to inject any command wrapper it needs. This might be something like
   * gdb, or valgrind, etc.
   */
  ide_run_tool_prepare_to_run (self->run_tool, pipeline, run_command, run_context);

  /* Now push a new layer so that we can keep those values separate from
   * what is configured in the run command. We use an expansion layer so
   * that we can expand common variables at this layer and not allow them
   * to be visible at lower layers.
   */
  environ = g_environ_setenv (environ, "BUILDDIR", ide_pipeline_get_builddir (pipeline), TRUE);
  environ = g_environ_setenv (environ, "SRCDIR", ide_pipeline_get_srcdir (pipeline), TRUE);
  environ = g_environ_setenv (environ, "HOME", g_get_home_dir (), TRUE);
  environ = g_environ_setenv (environ, "USER", g_get_user_name (), TRUE);
  ide_run_context_push_expansion (run_context, (const char * const *)environ);

  /* Setup working directory */
  {
    const char *cwd = ide_run_command_get_cwd (run_command);

    if (cwd != NULL)
      ide_run_context_set_cwd (run_context, cwd);
  }

  /* Setup command arguments */
  {
    const char * const *argv = ide_run_command_get_argv (run_command);

    if (argv != NULL)
      ide_run_context_append_args (run_context, argv);
  }

  /* Setup command environment */
  {
    const char * const *env = ide_run_command_get_environ (run_command);

    if (env != NULL && env[0] != NULL)
      ide_run_context_add_environ (run_context, env);
  }

  /* Now overlay runtime-tweaks as needed. Put this in a layer so that
   * we can debug where things are set/changed to help us when we need
   * to track down bugs in handlers/runtimes/devices/etc. All of our
   * changes will get persisted to the lower layer when merging anyway.
   *
   * TODO: These could probably be moved into a plugin rather than in
   * the foundry itself. That way they can be disabled by users who are
   * doing nothing with GTK/GNOME applications.
   */
  ide_run_context_push (run_context, NULL, NULL, NULL);
  apply_color_scheme (run_context, get_action_state_string (self, "color-scheme"));
  apply_accent_color (run_context, get_action_state_string (self, "accent-color"));
  apply_adaptive_preview (run_context, get_action_state_bool (self, "adaptive-preview"));
  apply_high_contrast (run_context, get_action_state_bool (self, "high-contrast"));
  apply_renderer (run_context, get_action_state_string (self, "renderer"));
  apply_gtk_debug (run_context,
                   get_action_state_string (self, "text-direction"),
                   get_action_state_bool (self, "interactive"));
  apply_messages_debug (run_context, ide_settings_get_boolean (self->project_settings, "verbose-logging"));

#if 0
/* TODO: Probably want to inherit these when running, but we should have
 * a toggle to turn inherit locale on/off.
 */
static const char * const copy[] = {
  "LANG",
  "LANGUAGE",
  "LC_ALL",
  "LC_ADDRESS",
  "LC_COLLATE",
  "LC_CTYPE",
  "LC_IDENTIFICATION",
  "LC_MEASUREMENT",
  "LC_MESSAGES",
  "LC_MONETARY",
  "LC_NAME",
  "LC_NUMERIC",
  "LC_PAPER",
  "LC_TELEPHONE",
  "LC_TIME",
};
#endif

  /* Allow plugins to track anything in the mix. For example the
   * terminal plugin will attach a PTY here for stdin/stdout/stderr.
   */
  g_signal_emit (self, signals [RUN], 0, run_context);

  IDE_EXIT;
}

static void
ide_run_manager_run_deploy_cb (GObject      *object,
                               GAsyncResult *result,
                               gpointer      user_data)
{
  IdeDeployStrategy *deploy_strategy = (IdeDeployStrategy *)object;
  g_autoptr(IdeSubprocess) subprocess = NULL;
  g_autoptr(IdeRunContext) run_context = NULL;
  g_autoptr(IdeTask) task = user_data;
  g_autoptr(GError) error = NULL;
  IdeNotification *notif;
  IdeRunManager *self;
  IdePipeline *pipeline;

  IDE_ENTRY;

  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (IDE_IS_DEPLOY_STRATEGY (deploy_strategy));
  g_assert (G_IS_ASYNC_RESULT (result));
  g_assert (IDE_IS_TASK (task));

  self = ide_task_get_source_object (task);
  pipeline = ide_task_get_task_data (task);
  notif = g_object_get_data (G_OBJECT (deploy_strategy), "PROGRESS");

  g_assert (IDE_IS_RUN_MANAGER (self));
  g_assert (IDE_IS_PIPELINE (pipeline));
  g_assert (IDE_IS_NOTIFICATION (notif));

  /* Withdraw our deploy notification */
  ide_notification_withdraw (notif);
  ide_object_destroy (IDE_OBJECT (notif));

  if (!ide_deploy_strategy_deploy_finish (deploy_strategy, result, &error))
    {
      ide_task_return_error (task, g_steal_pointer (&error));
      IDE_EXIT;
    }

  if (self->current_run_command == NULL)
    {
      ide_task_return_new_error (task,
                                 G_IO_ERROR,
                                 G_IO_ERROR_CANCELLED,
                                 "The operation was cancelled");
      IDE_EXIT;
    }

  /* Setup the run context */
  run_context = ide_run_context_new ();
  ide_deploy_strategy_prepare_run_context (deploy_strategy, pipeline, run_context);
  ide_run_manager_prepare_run_context (self, run_context, self->current_run_command, pipeline);

  /* Now spawn the subprocess or bail if there was a failure to build command */
  if (!(subprocess = ide_run_context_spawn (run_context, &error)))
    {
      ide_task_return_error (task, g_steal_pointer (&error));
      IDE_EXIT;
    }

  /* Keep subprocess around for send_signal/force_exit */
  g_set_object (&self->current_subprocess, subprocess);

  if (self->notif != NULL)
    ide_notification_withdraw (self->notif);

  /* Setup notification */
  {
    const char *name = ide_run_command_get_display_name (self->current_run_command);
    /* translators: %s is replaced with the name of the users run command */
    g_autofree char *title = g_strdup_printf (_("Running %s…"), name);

    g_clear_object (&self->notif);
    self->notif = g_object_new (IDE_TYPE_NOTIFICATION,
                                "id", "org.gnome.builder.run-manager.run",
                                "title", title,
                                NULL);
    ide_notification_attach (self->notif, IDE_OBJECT (self));
  }

  _ide_run_tool_emit_started (self->run_tool, subprocess);

  g_signal_emit (self, signals[STARTED], 0);

  /* Wait for the application to finish running */
  ide_subprocess_wait_check_async (subprocess,
                                   ide_task_get_cancellable (task),
                                   ide_run_manager_run_subprocess_wait_check_cb,
                                   g_object_ref (task));

  IDE_EXIT;
}

static void
ide_run_manager_run_discover_run_command_cb (GObject      *object,
                                             GAsyncResult *result,
                                             gpointer      user_data)
{
  IdeRunManager *self = (IdeRunManager *)object;
  g_autoptr(IdeNotification) notif = NULL;
  g_autoptr(IdeRunCommand) run_command = NULL;
  g_autoptr(IdeTask) task = user_data;
  g_autoptr(GError) error = NULL;
  IdeDeployStrategy *deploy_strategy;
  GCancellable *cancellable;
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

  cancellable = ide_task_get_cancellable (task);
  g_assert (!cancellable || G_IS_CANCELLABLE (cancellable));

  pipeline = ide_task_get_task_data (task);
  g_assert (IDE_IS_PIPELINE (pipeline));

  context = ide_object_get_context (IDE_OBJECT (pipeline));
  g_assert (IDE_IS_CONTEXT (context));

  deploy_strategy = ide_pipeline_get_deploy_strategy (pipeline);
  g_assert (IDE_IS_DEPLOY_STRATEGY (deploy_strategy));

  notif = g_object_new (IDE_TYPE_NOTIFICATION,
                        "id", "org.gnome.builder.run-manager.deploy",
                        "title", _("Deploying to device…"),
                        "icon-name", "package-x-generic-symbolic",
                        "has-progress", TRUE,
                        "progress-is-imprecise", FALSE,
                        NULL);
  ide_notification_attach (notif, IDE_OBJECT (context));
  g_object_set_data_full (G_OBJECT (deploy_strategy),
                          "PROGRESS",
                          g_object_ref (notif),
                          g_object_unref);

  ide_deploy_strategy_deploy_async (deploy_strategy,
                                    pipeline,
                                    ide_notification_file_progress_callback,
                                    g_object_ref (notif),
                                    g_object_unref,
                                    cancellable,
                                    ide_run_manager_run_deploy_cb,
                                    g_steal_pointer (&task));

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
  g_return_if_fail (!cancellable || G_IS_CANCELLABLE (cancellable));
  g_return_if_fail (!g_cancellable_is_cancelled (self->cancellable));

  if (cancellable == NULL)
    cancellable = local_cancellable = g_cancellable_new ();
  ide_cancellable_chain (cancellable, self->cancellable);

  self->sent_signal = FALSE;

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

static int
ide_run_manager_get_exit_signal (IdeRunManager *self)
{
  g_autofree char *stop_signal = NULL;
  int signum;

  g_assert (IDE_IS_RUN_MANAGER (self));

  stop_signal = ide_settings_get_string (self->project_settings, "stop-signal");

  if (0) {}
  else if (ide_str_equal0 (stop_signal, "SIGKILL")) signum = SIGKILL;
  else if (ide_str_equal0 (stop_signal, "SIGINT"))  signum = SIGINT;
  else if (ide_str_equal0 (stop_signal, "SIGHUP"))  signum = SIGHUP;
  else if (ide_str_equal0 (stop_signal, "SIGUSR1")) signum = SIGUSR1;
  else if (ide_str_equal0 (stop_signal, "SIGUSR2")) signum = SIGUSR2;
  else if (ide_str_equal0 (stop_signal, "SIGABRT")) signum = SIGABRT;
  else if (ide_str_equal0 (stop_signal, "SIGQUIT")) signum = SIGQUIT;
  else signum = SIGKILL;

  return signum;
}

void
ide_run_manager_cancel (IdeRunManager *self)
{
  IDE_ENTRY;

  g_return_if_fail (IDE_IS_MAIN_THREAD ());
  g_return_if_fail (IDE_IS_RUN_MANAGER (self));

  if (self->current_subprocess != NULL)
    {
      int exit_signal = ide_run_manager_get_exit_signal (self);

      if (!self->sent_signal)
        ide_run_tool_send_signal (self->run_tool, exit_signal);
      else
        ide_run_tool_force_exit (self->run_tool);

      self->sent_signal = TRUE;
    }

  /* Make sure tasks are cancelled too */
  if (self->cancellable != NULL)
    g_timeout_add (0, do_cancel_in_timeout, g_steal_pointer (&self->cancellable));
  self->cancellable = g_cancellable_new ();

  IDE_EXIT;
}

static void
ide_run_manager_run_action_cb (GObject      *object,
                               GAsyncResult *result,
                               gpointer      user_data)
{
  IdeRunManager *self = (IdeRunManager *)object;
  g_autoptr(GError) error = NULL;

  IDE_ENTRY;

  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (IDE_IS_RUN_MANAGER (self));
  g_assert (G_IS_ASYNC_RESULT (result));

  ide_run_manager_run_finish (self, result, &error);

  IDE_EXIT;
}

static void
ide_run_manager_actions_run (IdeRunManager *self,
                             GVariant      *param)
{
  IDE_ENTRY;

  g_assert (IDE_IS_RUN_MANAGER (self));

  ide_object_message (IDE_OBJECT (self),
                      _("User requested application to run"));

  ide_run_manager_run_async (self,
                             NULL,
                             ide_run_manager_run_action_cb,
                             NULL);

  IDE_EXIT;
}

static gboolean
ide_run_manager_actions_restart_from_timeout (IdeRunManager *self)
{
  IDE_ENTRY;

  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (IDE_IS_RUN_MANAGER (self));

  ide_run_manager_actions_run (self, NULL);

  IDE_RETURN (G_SOURCE_REMOVE);
}

static void
ide_run_manager_actions_restart (IdeRunManager *self,
                                 GVariant      *param)
{
  IDE_ENTRY;

  g_assert (IDE_IS_RUN_MANAGER (self));

  ide_object_message (IDE_OBJECT (self),
                      _("User requested application to restart"));

  ide_run_manager_actions_stop (self, NULL);

  g_timeout_add_full (G_PRIORITY_LOW,
                      100,
                      (GSourceFunc) ide_run_manager_actions_restart_from_timeout,
                      g_object_ref (self),
                      g_object_unref);

  IDE_EXIT;
}

static void
ide_run_manager_actions_run_with_handler (IdeRunManager *self,
                                          GVariant      *param)
{
  const char *name;

  IDE_ENTRY;

  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (IDE_IS_RUN_MANAGER (self));
  g_assert (g_variant_is_of_type (param, G_VARIANT_TYPE_STRING));

  name = g_variant_get_string (param, NULL);
  ide_run_manager_set_run_tool_from_module_name (self, name);

  ide_object_message (IDE_OBJECT (self),
                      /* translators: %s is replaced with the name of the tool */
                      _("User requested application to run with tool “%s”"),
                      name);

  ide_run_manager_run_async (self,
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

  ide_object_message (IDE_OBJECT (self),
                      _("User requested application to stop"));

  ide_run_manager_cancel (self);

  IDE_EXIT;
}

static void
ide_run_manager_init (IdeRunManager *self)
{
  GtkTextDirection text_dir;

  self->cancellable = g_cancellable_new ();
  self->run_tool = ide_no_tool_new ();

  /* Setup initial text direction state */
  text_dir = gtk_widget_get_default_direction ();
  if (text_dir == GTK_TEXT_DIR_LTR)
    ide_run_manager_set_action_state (self,
                                      "text-direction",
                                      text_dir == GTK_TEXT_DIR_LTR ?
                                        g_variant_new_string ("ltr") :
                                        g_variant_new_string ("rtl"));
}

void
_ide_run_manager_drop_caches (IdeRunManager *self)
{
  g_return_if_fail (IDE_IS_RUN_MANAGER (self));

  self->last_change_seq = 0;
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
                                          GObject          *exten,
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

      /* Don't allow using this as a default/fallback unless the command
       * is explicitely marked as capable of that. Otherwise, we risk things
       * like a "destroy my hard drive" commands in shellcmd being run as
       * the default run command.
       */
      if (!ide_run_command_get_can_default (run_command))
        continue;

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
  ide_task_set_task_data (task,
                          ide_settings_get_string (self->project_settings,
                                                   "default-run-command"),
                          g_free);

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

char *
_ide_run_manager_get_default_id (IdeRunManager *self)
{
  g_return_val_if_fail (IDE_IS_RUN_MANAGER (self), NULL);

  return ide_settings_get_string (self->project_settings, "default-run-command");
}

void
_ide_run_manager_set_default_id (IdeRunManager *self,
                                 const char    *run_command_id)
{
  g_return_if_fail (IDE_IS_RUN_MANAGER (self));

  if (run_command_id == NULL)
    run_command_id = "";

  g_debug ("Settinging default run command to \"%s\"", run_command_id);
  ide_settings_set_string (self->project_settings,
                           "default-run-command",
                           run_command_id);
}
