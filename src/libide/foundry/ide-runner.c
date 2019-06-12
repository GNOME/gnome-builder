/* ide-runner.c
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

#define G_LOG_DOMAIN "ide-runner"

#include "config.h"

#include <dazzle.h>
#include <errno.h>
#include <glib/gi18n.h>
#include <libide-io.h>
#include <libide-threading.h>
#include <libpeas/peas.h>
#include <stdlib.h>
#include <unistd.h>

#include "ide-build-target.h"
#include "ide-config-manager.h"
#include "ide-config.h"
#include "ide-foundry-compat.h"
#include "ide-runner-addin.h"
#include "ide-runner.h"
#include "ide-runtime.h"

typedef struct
{
  PeasExtensionSet *addins;
  IdeEnvironment *env;
  IdeBuildTarget *build_target;

  GArray *fd_mapping;

  gchar *cwd;

  IdeSubprocess *subprocess;

  GQueue argv;

  GSubprocessFlags flags;

  VtePty *pty;
  gint child_fd;

  guint clear_env : 1;
  guint failed : 1;
  guint run_on_host : 1;
  guint disable_pty : 1;
} IdeRunnerPrivate;

typedef struct
{
  GSList *prehook_queue;
  GSList *posthook_queue;
} IdeRunnerRunState;

typedef struct
{
  gint source_fd;
  gint dest_fd;
} FdMapping;

enum {
  PROP_0,
  PROP_ARGV,
  PROP_BUILD_TARGET,
  PROP_CLEAR_ENV,
  PROP_CWD,
  PROP_DISABLE_PTY,
  PROP_ENV,
  PROP_FAILED,
  PROP_RUN_ON_HOST,
  N_PROPS
};

enum {
  EXITED,
  SPAWNED,
  N_SIGNALS
};

static void ide_runner_tick_posthook (IdeTask *task);
static void ide_runner_tick_prehook  (IdeTask *task);
static void ide_runner_tick_run      (IdeTask *task);

G_DEFINE_TYPE_WITH_PRIVATE (IdeRunner, ide_runner, IDE_TYPE_OBJECT)

static GParamSpec *properties [N_PROPS];
static guint signals [N_SIGNALS];

static IdeRunnerAddin *
pop_runner_addin (GSList **list)
{
  IdeRunnerAddin *ret;

  g_assert (list != NULL);
  g_assert (*list != NULL);

  ret = (*list)->data;

  *list = g_slist_delete_link (*list, *list);

  return ret;
}

static void
ide_runner_run_state_free (gpointer data)
{
  IdeRunnerRunState *state = data;

  g_slist_foreach (state->prehook_queue, (GFunc)g_object_unref, NULL);
  g_slist_free (state->prehook_queue);

  g_slist_foreach (state->posthook_queue, (GFunc)g_object_unref, NULL);
  g_slist_free (state->posthook_queue);

  g_slice_free (IdeRunnerRunState, state);
}

static void
ide_runner_run_wait_cb (GObject      *object,
                        GAsyncResult *result,
                        gpointer      user_data)
{
  IdeSubprocess *subprocess = (IdeSubprocess *)object;
  IdeRunnerPrivate *priv;
  g_autoptr(IdeTask) task = user_data;
  g_autoptr(GError) error = NULL;
  IdeRunner *self;

  IDE_ENTRY;

  g_assert (IDE_IS_SUBPROCESS (subprocess));
  g_assert (G_IS_ASYNC_RESULT (result));
  g_assert (IDE_IS_TASK (task));

  self = ide_task_get_source_object (task);
  priv = ide_runner_get_instance_private (self);

  g_assert (IDE_IS_RUNNER (self));

  g_clear_object (&priv->subprocess);

  g_signal_emit (self, signals [EXITED], 0);

  if (!ide_subprocess_wait_finish (subprocess, result, &error))
    {
      ide_task_return_error (task, g_steal_pointer (&error));
      IDE_EXIT;
    }

  if (ide_subprocess_get_if_exited (subprocess))
    {
      gint exit_code;

      exit_code = ide_subprocess_get_exit_status (subprocess);

      if (exit_code == EXIT_SUCCESS)
        {
          ide_task_return_boolean (task, TRUE);
          IDE_EXIT;
        }
    }

  ide_task_return_new_error (task,
                             G_IO_ERROR,
                             G_IO_ERROR_FAILED,
                             "%s",
                             _("Process quit unexpectedly"));

  IDE_EXIT;
}

static IdeSubprocessLauncher *
ide_runner_real_create_launcher (IdeRunner *self)
{
  IdeRunnerPrivate *priv = ide_runner_get_instance_private (self);
  IdeConfigManager *config_manager;
  IdeSubprocessLauncher *ret;
  IdeConfig *config;
  IdeContext *context;
  IdeRuntime *runtime;

  g_assert (IDE_IS_RUNNER (self));

  context = ide_object_get_context (IDE_OBJECT (self));
  config_manager = ide_config_manager_from_context (context);
  config = ide_config_manager_get_current (config_manager);
  runtime = ide_config_get_runtime (config);

  ret = ide_runtime_create_launcher (runtime, NULL);

  if (ret != NULL && priv->cwd != NULL)
    ide_subprocess_launcher_set_cwd (ret, priv->cwd);

  return ret;
}

static void
ide_runner_real_run_async (IdeRunner           *self,
                           GCancellable        *cancellable,
                           GAsyncReadyCallback  callback,
                           gpointer             user_data)
{
  IdeRunnerPrivate *priv = ide_runner_get_instance_private (self);
  g_autoptr(IdeTask) task = NULL;
  g_autoptr(IdeSubprocessLauncher) launcher = NULL;
  g_autoptr(IdeSubprocess) subprocess = NULL;
  IdeConfigManager *config_manager;
  IdeConfig *config;
  const gchar *identifier;
  IdeContext *context;
  IdeRuntime *runtime;
  g_autoptr(GError) error = NULL;

  IDE_ENTRY;

  g_assert (IDE_IS_RUNNER (self));
  g_assert (!cancellable || G_IS_CANCELLABLE (cancellable));

  task = ide_task_new (self, cancellable, callback, user_data);
  ide_task_set_source_tag (task, ide_runner_real_run_async);

  context = ide_object_get_context (IDE_OBJECT (self));
  config_manager = ide_config_manager_from_context (context);
  config = ide_config_manager_get_current (config_manager);
  runtime = ide_config_get_runtime (config);

  if (runtime != NULL)
    launcher = IDE_RUNNER_GET_CLASS (self)->create_launcher (self);

  if (launcher == NULL)
    launcher = ide_subprocess_launcher_new (0);

  ide_subprocess_launcher_set_flags (launcher, priv->flags);

  /*
   * If we have a tty_fd set, then we want to override our stdin,
   * stdout, and stderr fds with our TTY.
   */
  if (!priv->disable_pty && (priv->child_fd != -1 || priv->pty != NULL))
    {
      if (priv->child_fd == -1)
        {
          gint master_fd;
          gint tty_fd;

          g_assert (priv->pty != NULL);

          master_fd = vte_pty_get_fd (priv->pty);

          errno = 0;
          if (-1 == (tty_fd = ide_pty_intercept_create_slave (master_fd, TRUE)))
            g_critical ("Failed to create TTY device: %s", g_strerror (errno));

          g_assert (tty_fd == -1 || isatty (tty_fd));

          ide_runner_take_tty_fd (self, tty_fd);
        }

      if (!(priv->flags & G_SUBPROCESS_FLAGS_STDIN_PIPE))
        ide_subprocess_launcher_take_stdin_fd (launcher, dup (priv->child_fd));

      if (!(priv->flags & (G_SUBPROCESS_FLAGS_STDOUT_PIPE | G_SUBPROCESS_FLAGS_STDOUT_SILENCE)))
        ide_subprocess_launcher_take_stdout_fd (launcher, dup (priv->child_fd));

      if (!(priv->flags & (G_SUBPROCESS_FLAGS_STDERR_PIPE | G_SUBPROCESS_FLAGS_STDERR_SILENCE)))
        ide_subprocess_launcher_take_stderr_fd (launcher, dup (priv->child_fd));
    }

  /*
   * Now map in any additionally requested FDs.
   */
  if (priv->fd_mapping != NULL)
    {
      g_autoptr(GArray) ar = g_steal_pointer (&priv->fd_mapping);

      for (guint i = 0; i < ar->len; i++)
        {
          FdMapping *map = &g_array_index (ar, FdMapping, i);

          ide_subprocess_launcher_take_fd (launcher, map->source_fd, map->dest_fd);
        }
    }

  /*
   * We want the runners to run on the host so that we aren't captive to
   * our containing system (flatpak, jhbuild, etc).
   */
  ide_subprocess_launcher_set_run_on_host (launcher, priv->run_on_host);

  /*
   * We don't want the environment cleared because we need access to
   * things like DISPLAY, WAYLAND_DISPLAY, and DBUS_SESSION_BUS_ADDRESS.
   */
  ide_subprocess_launcher_set_clear_env (launcher, priv->clear_env);

  /*
   * Overlay the environment provided.
   */
  ide_subprocess_launcher_overlay_environment (launcher, priv->env);

  /*
   * Push all of our configured arguments in order.
   */
  for (const GList *iter = priv->argv.head; iter != NULL; iter = iter->next)
    ide_subprocess_launcher_push_argv (launcher, iter->data);

  /* Give the runner a final chance to mutate the launcher */
  if (IDE_RUNNER_GET_CLASS (self)->fixup_launcher)
    IDE_RUNNER_GET_CLASS (self)->fixup_launcher (self, launcher);

  subprocess = ide_subprocess_launcher_spawn (launcher, cancellable, &error);

  g_assert (subprocess == NULL || IDE_IS_SUBPROCESS (subprocess));

  if (subprocess == NULL)
    {
      ide_task_return_error (task, g_steal_pointer (&error));
      IDE_GOTO (failure);
    }

  priv->subprocess = g_object_ref (subprocess);

  identifier = ide_subprocess_get_identifier (subprocess);
  g_signal_emit (self, signals [SPAWNED], 0, identifier);

  ide_subprocess_wait_async (subprocess,
                             cancellable,
                             ide_runner_run_wait_cb,
                             g_steal_pointer (&task));

failure:
  IDE_EXIT;
}

static gboolean
ide_runner_real_run_finish (IdeRunner     *self,
                            GAsyncResult  *result,
                            GError       **error)
{
  g_assert (IDE_IS_RUNNER (self));
  g_assert (IDE_IS_TASK (result));
  g_assert (ide_task_is_valid (IDE_TASK (result), self));
  g_assert (ide_task_get_source_tag (IDE_TASK (result)) == ide_runner_real_run_async);

  return ide_task_propagate_boolean (IDE_TASK (result), error);
}

static GOutputStream *
ide_runner_real_get_stdin (IdeRunner *self)
{
  IdeRunnerPrivate *priv = ide_runner_get_instance_private (self);

  if (priv->subprocess)
    return g_object_ref (ide_subprocess_get_stdin_pipe (priv->subprocess));
  return NULL;
}

static GInputStream *
ide_runner_real_get_stdout (IdeRunner *self)
{
  IdeRunnerPrivate *priv = ide_runner_get_instance_private (self);

  if (priv->subprocess)
    return g_object_ref (ide_subprocess_get_stdout_pipe (priv->subprocess));
  return NULL;
}

static GInputStream *
ide_runner_real_get_stderr (IdeRunner *self)
{
  IdeRunnerPrivate *priv = ide_runner_get_instance_private (self);

  if (priv->subprocess)
    return g_object_ref (ide_subprocess_get_stderr_pipe (priv->subprocess));
  return NULL;
}

static void
ide_runner_real_force_quit (IdeRunner *self)
{
  IdeRunnerPrivate *priv = ide_runner_get_instance_private (self);

  IDE_ENTRY;

  g_assert (IDE_IS_RUNNER (self));

  if (priv->subprocess != NULL)
    ide_subprocess_force_exit (priv->subprocess);

  IDE_EXIT;
}

static void
ide_runner_extension_added (PeasExtensionSet *set,
                            PeasPluginInfo   *plugin_info,
                            PeasExtension    *exten,
                            gpointer          user_data)
{
  IdeRunnerAddin *addin = (IdeRunnerAddin *)exten;
  IdeRunner *self = user_data;

  g_assert (PEAS_IS_EXTENSION_SET (set));
  g_assert (plugin_info != NULL);
  g_assert (IDE_IS_RUNNER_ADDIN (addin));
  g_assert (IDE_IS_RUNNER (self));

  ide_runner_addin_load (addin, self);
}

static void
ide_runner_extension_removed (PeasExtensionSet *set,
                              PeasPluginInfo   *plugin_info,
                              PeasExtension    *exten,
                              gpointer          user_data)
{
  IdeRunnerAddin *addin = (IdeRunnerAddin *)exten;
  IdeRunner *self = user_data;

  g_assert (PEAS_IS_EXTENSION_SET (set));
  g_assert (plugin_info != NULL);
  g_assert (IDE_IS_RUNNER_ADDIN (addin));
  g_assert (IDE_IS_RUNNER (self));

  ide_runner_addin_unload (addin, self);
}

static void
ide_runner_constructed (GObject *object)
{
  IdeRunner *self = (IdeRunner *)object;
  IdeRunnerPrivate *priv = ide_runner_get_instance_private (self);

  G_OBJECT_CLASS (ide_runner_parent_class)->constructed (object);

  priv->addins = peas_extension_set_new (peas_engine_get_default (),
                                         IDE_TYPE_RUNNER_ADDIN,
                                         NULL);

  g_signal_connect (priv->addins,
                    "extension-added",
                    G_CALLBACK (ide_runner_extension_added),
                    self);

  g_signal_connect (priv->addins,
                    "extension-removed",
                    G_CALLBACK (ide_runner_extension_removed),
                    self);

  peas_extension_set_foreach (priv->addins,
                              ide_runner_extension_added,
                              self);
}

static void
ide_runner_finalize (GObject *object)
{
  IdeRunner *self = (IdeRunner *)object;
  IdeRunnerPrivate *priv = ide_runner_get_instance_private (self);

  g_queue_foreach (&priv->argv, (GFunc)g_free, NULL);
  g_queue_clear (&priv->argv);
  g_clear_object (&priv->env);
  g_clear_object (&priv->subprocess);
  g_clear_object (&priv->build_target);

  if (priv->child_fd != -1)
    {
      close (priv->child_fd);
      priv->child_fd = -1;
    }

  if (priv->fd_mapping != NULL)
    {
      for (guint i = 0; i < priv->fd_mapping->len; i++)
        {
          FdMapping *map = &g_array_index (priv->fd_mapping, FdMapping, i);

          if (map->source_fd != -1)
            {
              close (map->source_fd);
              map->source_fd = -1;
            }
        }
    }

  g_clear_pointer (&priv->fd_mapping, g_array_unref);
  g_clear_object (&priv->pty);

  G_OBJECT_CLASS (ide_runner_parent_class)->finalize (object);
}

static void
ide_runner_get_property (GObject    *object,
                         guint       prop_id,
                         GValue     *value,
                         GParamSpec *pspec)
{
  IdeRunner *self = IDE_RUNNER (object);

  switch (prop_id)
    {
    case PROP_ARGV:
      g_value_take_boxed (value, ide_runner_get_argv (self));
      break;

    case PROP_CLEAR_ENV:
      g_value_set_boolean (value, ide_runner_get_clear_env (self));
      break;

    case PROP_CWD:
      g_value_set_string (value, ide_runner_get_cwd (self));
      break;

    case PROP_DISABLE_PTY:
      g_value_set_boolean (value, ide_runner_get_disable_pty (self));
      break;

    case PROP_ENV:
      g_value_set_object (value, ide_runner_get_environment (self));
      break;

    case PROP_FAILED:
      g_value_set_boolean (value, ide_runner_get_failed (self));
      break;

    case PROP_RUN_ON_HOST:
      g_value_set_boolean (value, ide_runner_get_run_on_host (self));
      break;

    case PROP_BUILD_TARGET:
      g_value_set_object (value, ide_runner_get_build_target (self));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
ide_runner_set_property (GObject      *object,
                         guint         prop_id,
                         const GValue *value,
                         GParamSpec   *pspec)
{
  IdeRunner *self = IDE_RUNNER (object);

  switch (prop_id)
    {
    case PROP_ARGV:
      ide_runner_set_argv (self, g_value_get_boxed (value));
      break;

    case PROP_CLEAR_ENV:
      ide_runner_set_clear_env (self, g_value_get_boolean (value));
      break;

    case PROP_CWD:
      ide_runner_set_cwd (self, g_value_get_string (value));
      break;

    case PROP_DISABLE_PTY:
      ide_runner_set_disable_pty (self, g_value_get_boolean (value));
      break;

    case PROP_FAILED:
      ide_runner_set_failed (self, g_value_get_boolean (value));
      break;

    case PROP_RUN_ON_HOST:
      ide_runner_set_run_on_host (self, g_value_get_boolean (value));
      break;

    case PROP_BUILD_TARGET:
      ide_runner_set_build_target (self, g_value_get_object (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
ide_runner_class_init (IdeRunnerClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->constructed = ide_runner_constructed;
  object_class->finalize = ide_runner_finalize;
  object_class->get_property = ide_runner_get_property;
  object_class->set_property = ide_runner_set_property;

  klass->run_async = ide_runner_real_run_async;
  klass->run_finish = ide_runner_real_run_finish;
  klass->create_launcher = ide_runner_real_create_launcher;
  klass->get_stdin = ide_runner_real_get_stdin;
  klass->get_stdout = ide_runner_real_get_stdout;
  klass->get_stderr = ide_runner_real_get_stderr;
  klass->force_quit = ide_runner_real_force_quit;

  properties [PROP_ARGV] =
    g_param_spec_boxed ("argv",
                        "Argv",
                        "The argument list for the command",
                        G_TYPE_STRV,
                        (G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS));

  properties [PROP_CLEAR_ENV] =
    g_param_spec_boolean ("clear-env",
                          "Clear Env",
                          "If the environment should be cleared before applying overrides",
                          FALSE,
                          (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  properties [PROP_CWD] =
    g_param_spec_string ("cwd",
                         "Current Working Directory",
                         "The directory to use as the working directory for the process",
                         NULL,
                         (G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS));

  properties [PROP_DISABLE_PTY] =
    g_param_spec_boolean ("disable-pty",
                          "Disable PTY",
                          "If the pty should be disabled from use",
                          FALSE,
                          (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  properties [PROP_ENV] =
    g_param_spec_object ("environment",
                         "Environment",
                         "The environment variables for the command",
                         IDE_TYPE_ENVIRONMENT,
                         (G_PARAM_READABLE | G_PARAM_STATIC_STRINGS));

  /**
   * IdeRunner:failed:
   *
   * If the runner has "failed". This should be set if a plugin can determine
   * that the runner cannot be executed due to an external issue. One such
   * example might be a debugger plugin that cannot locate a suitable debugger
   * to run the program.
   *
   * Since: 3.32
   */
  properties [PROP_FAILED] =
    g_param_spec_boolean ("failed",
                          "Failed",
                          "If the runner has failed",
                          FALSE,
                          (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  /**
   * IdeRunner:run-on-host:
   *
   * The "run-on-host" property indicates the program should be run on the
   * host machine rather than inside the application sandbox.
   *
   * Since: 3.32
   */
  properties [PROP_RUN_ON_HOST] =
    g_param_spec_boolean ("run-on-host",
                          "Run on Host",
                          "Run on Host",
                          FALSE,
                          (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  /**
   * IdeRunner:build-target:
   *
   * The %IdeBuildTarget from which this %IdeRunner was constructed.
   *
   * This is useful to retrieve various properties related to the program
   * that will be launched, such as what programming language it uses,
   * or whether it's a graphical application, a command line tool or a test
   * program.
   *
   * Since: 3.32
   */
  properties [PROP_BUILD_TARGET] =
    g_param_spec_object ("build-target",
                         "Build Target",
                         "Build Target",
                         IDE_TYPE_BUILD_TARGET,
                         (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, N_PROPS, properties);

  signals [EXITED] =
    g_signal_new ("exited",
                  G_TYPE_FROM_CLASS (klass),
                  G_SIGNAL_RUN_LAST,
                  0,
                  NULL,
                  NULL,
                  NULL,
                  G_TYPE_NONE,
                  0);

  signals [SPAWNED] =
    g_signal_new ("spawned",
                  G_TYPE_FROM_CLASS (klass),
                  G_SIGNAL_RUN_LAST,
                  0, NULL, NULL,
                  g_cclosure_marshal_VOID__STRING,
                  G_TYPE_NONE, 1, G_TYPE_STRING | G_SIGNAL_TYPE_STATIC_SCOPE);
}

static void
ide_runner_init (IdeRunner *self)
{
  IdeRunnerPrivate *priv = ide_runner_get_instance_private (self);

  g_queue_init (&priv->argv);

  priv->env = ide_environment_new ();
  priv->child_fd = -1;
  priv->flags = 0;
}

/**
 * ide_runner_get_stdin:
 *
 * Returns: (nullable) (transfer full): An #GOutputStream or %NULL.
 *
 * Since: 3.32
 */
GOutputStream *
ide_runner_get_stdin (IdeRunner *self)
{
  g_return_val_if_fail (IDE_IS_RUNNER (self), NULL);

  return IDE_RUNNER_GET_CLASS (self)->get_stdin (self);
}

/**
 * ide_runner_get_stdout:
 *
 * Returns: (nullable) (transfer full): An #GOutputStream or %NULL.
 *
 * Since: 3.32
 */
GInputStream *
ide_runner_get_stdout (IdeRunner *self)
{
  g_return_val_if_fail (IDE_IS_RUNNER (self), NULL);

  return IDE_RUNNER_GET_CLASS (self)->get_stdout (self);
}

/**
 * ide_runner_get_stderr:
 *
 * Returns: (nullable) (transfer full): An #GOutputStream or %NULL.
 *
 * Since: 3.32
 */
GInputStream *
ide_runner_get_stderr (IdeRunner *self)
{
  g_return_val_if_fail (IDE_IS_RUNNER (self), NULL);

  return IDE_RUNNER_GET_CLASS (self)->get_stderr (self);
}

void
ide_runner_force_quit (IdeRunner *self)
{
  IDE_ENTRY;

  g_return_if_fail (IDE_IS_RUNNER (self));

  if (IDE_RUNNER_GET_CLASS (self)->force_quit)
    IDE_RUNNER_GET_CLASS (self)->force_quit (self);

  IDE_EXIT;
}

void
ide_runner_set_argv (IdeRunner           *self,
                     const gchar * const *argv)
{
  IdeRunnerPrivate *priv = ide_runner_get_instance_private (self);
  guint i;

  g_return_if_fail (IDE_IS_RUNNER (self));

  g_queue_foreach (&priv->argv, (GFunc)g_free, NULL);
  g_queue_clear (&priv->argv);

  if (argv != NULL)
    {
      for (i = 0; argv [i]; i++)
        g_queue_push_tail (&priv->argv, g_strdup (argv [i]));
    }

  g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_ARGV]);
}

/**
 * ide_runner_get_environment:
 *
 * Returns: (transfer none): The #IdeEnvironment the process launched uses.
 *
 * Since: 3.32
 */
IdeEnvironment *
ide_runner_get_environment (IdeRunner *self)
{
  IdeRunnerPrivate *priv = ide_runner_get_instance_private (self);

  g_return_val_if_fail (IDE_IS_RUNNER (self), NULL);

  return priv->env;
}

/**
 * ide_runner_get_argv:
 *
 * Gets the argument list as a newly allocated string array.
 *
 * Returns: (transfer full): A newly allocated string array that should
 *   be freed with g_strfreev().
 *
 * Since: 3.32
 */
gchar **
ide_runner_get_argv (IdeRunner *self)
{
  IdeRunnerPrivate *priv = ide_runner_get_instance_private (self);
  GPtrArray *ar;
  GList *iter;

  g_return_val_if_fail (IDE_IS_RUNNER (self), NULL);

  ar = g_ptr_array_new ();

  for (iter = priv->argv.head; iter != NULL; iter = iter->next)
    {
      const gchar *param = iter->data;

      g_ptr_array_add (ar, g_strdup (param));
    }

  g_ptr_array_add (ar, NULL);

  return (gchar **)g_ptr_array_free (ar, FALSE);
}

static void
ide_runner_collect_addins_cb (PeasExtensionSet *set,
                              PeasPluginInfo   *plugin_info,
                              PeasExtension    *exten,
                              gpointer          user_data)
{
  GSList **list = user_data;

  *list = g_slist_prepend (*list, exten);
}

static void
ide_runner_collect_addins (IdeRunner  *self,
                           GSList    **list)
{
  IdeRunnerPrivate *priv = ide_runner_get_instance_private (self);

  g_assert (IDE_IS_RUNNER (self));
  g_assert (list != NULL);

  peas_extension_set_foreach (priv->addins,
                              ide_runner_collect_addins_cb,
                              list);
}

static void
ide_runner_posthook_cb (GObject      *object,
                        GAsyncResult *result,
                        gpointer      user_data)
{
  IdeRunnerAddin *addin = (IdeRunnerAddin *)object;
  g_autoptr(IdeTask) task = user_data;
  g_autoptr(GError) error = NULL;

  g_assert (IDE_IS_RUNNER_ADDIN (addin));
  g_assert (G_IS_ASYNC_RESULT (result));

  if (!ide_runner_addin_posthook_finish (addin, result, &error))
    ide_task_return_error (task, g_steal_pointer (&error));
  else
    ide_runner_tick_posthook (task);
}

static void
ide_runner_tick_posthook (IdeTask *task)
{
  IdeRunnerRunState *state;

  g_assert (IDE_IS_TASK (task));

  state = ide_task_get_task_data (task);

  if (state->posthook_queue != NULL)
    {
      g_autoptr(IdeRunnerAddin) addin = NULL;

      addin = pop_runner_addin (&state->posthook_queue);
      ide_runner_addin_posthook_async (addin,
                                       ide_task_get_cancellable (task),
                                       ide_runner_posthook_cb,
                                       g_object_ref (task));
      return;
    }

  ide_task_return_boolean (task, TRUE);
}

static void
ide_runner_run_cb (GObject      *object,
                   GAsyncResult *result,
                   gpointer      user_data)
{
  IdeRunner *self = (IdeRunner *)object;
  g_autoptr(IdeTask) task = user_data;
  g_autoptr(GError) error = NULL;

  IDE_ENTRY;

  g_assert (IDE_IS_RUNNER (self));
  g_assert (G_IS_ASYNC_RESULT (result));
  g_assert (IDE_IS_TASK (task));

  if (!IDE_RUNNER_GET_CLASS (self)->run_finish (self, result, &error))
    ide_task_return_error (task, g_steal_pointer (&error));
  else
    ide_runner_tick_posthook (task);

  IDE_EXIT;
}

static void
ide_runner_tick_run (IdeTask *task)
{
  IdeRunner *self;

  IDE_ENTRY;

  g_assert (IDE_IS_TASK (task));
  self = ide_task_get_source_object (task);
  g_assert (IDE_IS_RUNNER (self));

  IDE_RUNNER_GET_CLASS (self)->run_async (self,
                                          ide_task_get_cancellable (task),
                                          ide_runner_run_cb,
                                          g_object_ref (task));

  IDE_EXIT;
}

static void
ide_runner_prehook_cb (GObject      *object,
                       GAsyncResult *result,
                       gpointer      user_data)
{
  IdeRunnerAddin *addin = (IdeRunnerAddin *)object;
  g_autoptr(IdeTask) task = user_data;
  g_autoptr(GError) error = NULL;

  IDE_ENTRY;

  g_assert (IDE_IS_RUNNER_ADDIN (addin));
  g_assert (G_IS_ASYNC_RESULT (result));
  g_assert (IDE_IS_TASK (task));

  if (!ide_runner_addin_prehook_finish (addin, result, &error))
    ide_task_return_error (task, g_steal_pointer (&error));
  else
    ide_runner_tick_prehook (task);

  IDE_EXIT;
}

static void
ide_runner_tick_prehook (IdeTask *task)
{
  IdeRunnerRunState *state;

  IDE_ENTRY;

  g_assert (IDE_IS_TASK (task));

  state = ide_task_get_task_data (task);

  if (state->prehook_queue != NULL)
    {
      g_autoptr(IdeRunnerAddin) addin = NULL;

      addin = pop_runner_addin (&state->prehook_queue);
      ide_runner_addin_prehook_async (addin,
                                      ide_task_get_cancellable (task),
                                      ide_runner_prehook_cb,
                                      g_object_ref (task));
      IDE_EXIT;
    }

  ide_runner_tick_run (task);

  IDE_EXIT;
}

void
ide_runner_run_async (IdeRunner           *self,
                      GCancellable        *cancellable,
                      GAsyncReadyCallback  callback,
                      gpointer             user_data)
{
  g_autoptr(IdeTask) task = NULL;
  IdeRunnerRunState *state;

  IDE_ENTRY;

  g_return_if_fail (IDE_IS_RUNNER (self));
  g_return_if_fail (!cancellable || G_IS_CANCELLABLE (cancellable));

  task = ide_task_new (self, cancellable, callback, user_data);
  ide_task_set_source_tag (task, ide_runner_run_async);
  ide_task_set_check_cancellable (task, FALSE);
  ide_task_set_priority (task, G_PRIORITY_LOW);

  /*
   * We need to run the prehook functions for each addin first before we
   * can call our IdeRunnerClass.run vfunc.  Since these are async, we
   * have to bring some state along with us.
   */
  state = g_slice_new0 (IdeRunnerRunState);
  ide_runner_collect_addins (self, &state->prehook_queue);
  ide_runner_collect_addins (self, &state->posthook_queue);
  ide_task_set_task_data (task, state, ide_runner_run_state_free);

  ide_runner_tick_prehook (task);

  IDE_EXIT;
}

gboolean
ide_runner_run_finish (IdeRunner     *self,
                       GAsyncResult  *result,
                       GError       **error)
{
  gboolean ret;

  IDE_ENTRY;

  g_return_val_if_fail (IDE_IS_RUNNER (self), FALSE);
  g_return_val_if_fail (IDE_IS_TASK (result), FALSE);

  ret = ide_task_propagate_boolean (IDE_TASK (result), error);

  IDE_RETURN (ret);
}

void
ide_runner_append_argv (IdeRunner   *self,
                        const gchar *param)
{
  IdeRunnerPrivate *priv = ide_runner_get_instance_private (self);

  g_return_if_fail (IDE_IS_RUNNER (self));
  g_return_if_fail (param != NULL);

  g_queue_push_tail (&priv->argv, g_strdup (param));
  g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_ARGV]);
}

void
ide_runner_prepend_argv (IdeRunner   *self,
                         const gchar *param)
{
  IdeRunnerPrivate *priv = ide_runner_get_instance_private (self);

  g_return_if_fail (IDE_IS_RUNNER (self));
  g_return_if_fail (param != NULL);

  g_queue_push_head (&priv->argv, g_strdup (param));
  g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_ARGV]);
}

IdeRunner *
ide_runner_new (IdeContext *context)
{
  g_return_val_if_fail (IDE_IS_CONTEXT (context), NULL);

  return g_object_new (IDE_TYPE_RUNNER,
                       NULL);
}

gboolean
ide_runner_get_run_on_host (IdeRunner *self)
{
  IdeRunnerPrivate *priv = ide_runner_get_instance_private (self);

  g_return_val_if_fail (IDE_IS_RUNNER (self), FALSE);

  return priv->run_on_host;
}

void
ide_runner_set_run_on_host (IdeRunner *self,
                            gboolean   run_on_host)
{
  IdeRunnerPrivate *priv = ide_runner_get_instance_private (self);

  run_on_host = !!run_on_host;

  if (run_on_host != priv->run_on_host)
    {
      priv->run_on_host = run_on_host;
      g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_RUN_ON_HOST]);
    }
}

GSubprocessFlags
ide_runner_get_flags (IdeRunner *self)
{
  IdeRunnerPrivate *priv = ide_runner_get_instance_private (self);

  g_return_val_if_fail (IDE_IS_RUNNER (self), 0);

  return priv->flags;
}

void
ide_runner_set_flags (IdeRunner        *self,
                      GSubprocessFlags  flags)
{
  IdeRunnerPrivate *priv = ide_runner_get_instance_private (self);

  g_return_if_fail (IDE_IS_RUNNER (self));

  priv->flags = flags;
}

gboolean
ide_runner_get_clear_env (IdeRunner *self)
{
  IdeRunnerPrivate *priv = ide_runner_get_instance_private (self);

  g_return_val_if_fail (IDE_IS_RUNNER (self), FALSE);

  return priv->clear_env;
}

void
ide_runner_set_clear_env (IdeRunner *self,
                          gboolean   clear_env)
{
  IdeRunnerPrivate *priv = ide_runner_get_instance_private (self);

  g_return_if_fail (IDE_IS_RUNNER (self));

  clear_env = !!clear_env;

  if (clear_env != priv->clear_env)
    {
      priv->clear_env = clear_env;
      g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_CLEAR_ENV]);
    }
}

/**
 * ide_runner_set_pty:
 * @self: a #IdeRunner
 * @pty: (nullable): a #VtePty or %NULL
 *
 * Sets the #VtePty to use for the runner.
 *
 * This is equivalent to calling ide_runner_set_tty() with the
 * result of vte_pty_get_fd().
 *
 * Since: 3.32
 */
void
ide_runner_set_pty (IdeRunner *self,
                    VtePty    *pty)
{
  IdeRunnerPrivate *priv = ide_runner_get_instance_private (self);

  g_return_if_fail (IDE_IS_RUNNER (self));
  g_return_if_fail (!pty || VTE_IS_PTY (pty));

  g_set_object (&priv->pty, pty);
}

/**
 * ide_runner_get_pty:
 * @self: a #IdeRunner
 *
 * Gets the #VtePty that was assigned.
 *
 * Returns: (nullable) (transfer none): a #VtePty or %NULL
 *
 * Since: 3.34
 */
VtePty *
ide_runner_get_pty (IdeRunner *self)
{
  IdeRunnerPrivate *priv = ide_runner_get_instance_private (self);

  g_return_val_if_fail (IDE_IS_RUNNER (self), NULL);

  return priv->pty;
}

static gint
sort_fd_mapping (gconstpointer a,
                 gconstpointer b)
{
  const FdMapping *map_a = a;
  const FdMapping *map_b = b;

  return map_a->dest_fd - map_b->dest_fd;
}

/**
 * ide_runner_take_fd:
 * @self: An #IdeRunner
 * @source_fd: the fd to map, this will be closed by #IdeRunner
 * @dest_fd: the target FD in the spawned process, or -1 for next available
 *
 * This will ensure that @source_fd is mapped into the new process as @dest_fd.
 * If @dest_fd is -1, then the next fd will be used and that value will be
 * returned. Note that this is not a valid fd in the calling process, only
 * within the destination process.
 *
 * Returns: @dest_fd or the FD or the next available dest_fd.
 *
 * Since: 3.32
 */
gint
ide_runner_take_fd (IdeRunner *self,
                    gint       source_fd,
                    gint       dest_fd)
{
  IdeRunnerPrivate *priv = ide_runner_get_instance_private (self);
  FdMapping map = { -1, -1 };

  g_return_val_if_fail (IDE_IS_RUNNER (self), -1);
  g_return_val_if_fail (source_fd > -1, -1);

  if (priv->fd_mapping == NULL)
    priv->fd_mapping = g_array_new (FALSE, FALSE, sizeof (FdMapping));

  /*
   * Quick and dirty hack to take the next FD, won't deal with people mapping
   * to 1024 well, but we can fix that when we come across it.
   */
  if (dest_fd < 0)
    {
      gint max_fd = 2;

      for (guint i = 0; i < priv->fd_mapping->len; i++)
        {
          FdMapping *entry = &g_array_index (priv->fd_mapping, FdMapping, i);

          if (entry->dest_fd > max_fd)
            max_fd = entry->dest_fd;
        }

      dest_fd = max_fd + 1;
    }

  map.source_fd = source_fd;
  map.dest_fd = dest_fd;

  g_array_append_val (priv->fd_mapping, map);
  g_array_sort (priv->fd_mapping, sort_fd_mapping);

  return dest_fd;
}

/**
 * ide_runner_get_runtime:
 * @self: An #IdeRuntime
 *
 * This function will get the #IdeRuntime that will be used to execute the
 * application. Consumers may want to use this to determine if a particular
 * program is available (such as gdb, perf, strace, etc).
 *
 * Returns: (nullable) (transfer full): An #IdeRuntime or %NULL.
 *
 * Since: 3.32
 */
IdeRuntime *
ide_runner_get_runtime (IdeRunner *self)
{
  IdeConfigManager *config_manager;
  IdeConfig *config;
  IdeContext *context;
  IdeRuntime *runtime;

  g_return_val_if_fail (IDE_IS_RUNNER (self), NULL);

  if (IDE_RUNNER_GET_CLASS (self)->get_runtime)
    return IDE_RUNNER_GET_CLASS (self)->get_runtime (self);

  context = ide_object_get_context (IDE_OBJECT (self));
  config_manager = ide_config_manager_from_context (context);
  config = ide_config_manager_get_current (config_manager);
  runtime = ide_config_get_runtime (config);

  return runtime != NULL ? g_object_ref (runtime) : NULL;
}

gboolean
ide_runner_get_failed (IdeRunner *self)
{
  IdeRunnerPrivate *priv = ide_runner_get_instance_private (self);

  g_return_val_if_fail (IDE_IS_RUNNER (self), FALSE);

  return priv->failed;
}

void
ide_runner_set_failed (IdeRunner *self,
                       gboolean   failed)
{
  IdeRunnerPrivate *priv = ide_runner_get_instance_private (self);

  IDE_ENTRY;

  g_return_if_fail (IDE_IS_RUNNER (self));

  failed = !!failed;

  if (failed != priv->failed)
    {
      priv->failed = failed;
      g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_FAILED]);
    }

  IDE_EXIT;
}

/**
 * ide_runner_get_cwd:
 * @self: a #IdeRunner
 *
 * Returns: (nullable): The current working directory, or %NULL.
 *
 * Since: 3.32
 */
const gchar *
ide_runner_get_cwd (IdeRunner *self)
{
  IdeRunnerPrivate *priv = ide_runner_get_instance_private (self);

  g_return_val_if_fail (IDE_IS_RUNNER (self), NULL);

  return priv->cwd;
}

/**
 * ide_runner_set_cwd:
 * @self: a #IdeRunner
 * @cwd: (nullable): The working directory or %NULL
 *
 * Sets the directory to use when spawning the runner.
 *
 * Since: 3.32
 */
void
ide_runner_set_cwd (IdeRunner   *self,
                    const gchar *cwd)
{
  IdeRunnerPrivate *priv = ide_runner_get_instance_private (self);

  g_return_if_fail (IDE_IS_RUNNER (self));

  if (!ide_str_equal0 (priv->cwd, cwd))
    {
      g_free (priv->cwd);
      priv->cwd = g_strdup (cwd);
      g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_CWD]);
    }
}

/**
 * ide_runner_push_args:
 * @self: a #IdeRunner
 * @args: (array zero-terminated=1) (element-type utf8) (nullable): the arguments
 *
 * Helper to call ide_runner_append_argv() for every argument
 * contained in @args.
 *
 * Since: 3.32
 */
void
ide_runner_push_args (IdeRunner           *self,
                      const gchar * const *args)
{
  g_return_if_fail (IDE_IS_RUNNER (self));

  if (args == NULL)
    return;

  for (guint i = 0; args[i] != NULL; i++)
    ide_runner_append_argv (self, args[i]);
}

/**
 * ide_runner_get_build_target:
 * @self: a #IdeRunner
 *
 * Returns: (nullable) (transfer none): The %IdeBuildTarget associated with this %IdeRunner, or %NULL.
 *   See #IdeRunner:build-target for details.
 *
 * Since: 3.32
 */
IdeBuildTarget *
ide_runner_get_build_target (IdeRunner *self)
{
  IdeRunnerPrivate *priv = ide_runner_get_instance_private (self);

  g_return_val_if_fail (IDE_IS_RUNNER (self), NULL);

  return priv->build_target;
}

/**
 * ide_runner_set_build_target:
 * @self: a #IdeRunner
 * @build_target: (nullable): The build target, or %NULL
 *
 * Sets the build target associated with this runner.
 *
 * Since: 3.32
 */
void
ide_runner_set_build_target (IdeRunner      *self,
                             IdeBuildTarget *build_target)
{
  IdeRunnerPrivate *priv = ide_runner_get_instance_private (self);

  g_return_if_fail (IDE_IS_RUNNER (self));

  if (g_set_object (&priv->build_target, build_target))
    g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_BUILD_TARGET]);
}

gboolean
ide_runner_get_disable_pty (IdeRunner *self)
{
  IdeRunnerPrivate *priv = ide_runner_get_instance_private (self);

  g_return_val_if_fail (IDE_IS_RUNNER (self), FALSE);

  return priv->disable_pty;
}

void
ide_runner_set_disable_pty (IdeRunner *self,
                            gboolean   disable_pty)
{
  IdeRunnerPrivate *priv = ide_runner_get_instance_private (self);

  g_return_if_fail (IDE_IS_RUNNER (self));

  disable_pty = !!disable_pty;

  if (disable_pty != priv->disable_pty)
    {
      priv->disable_pty = disable_pty;
      g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_DISABLE_PTY]);
    }
}

void
ide_runner_take_tty_fd (IdeRunner *self,
                        gint       tty_fd)
{
  IdeRunnerPrivate *priv = ide_runner_get_instance_private (self);

  g_return_if_fail (IDE_IS_RUNNER (self));
  g_return_if_fail (tty_fd > -1);

  if (priv->child_fd != -1)
    close (priv->child_fd);
  priv->child_fd = tty_fd;
}

gint
ide_runner_get_max_fd (IdeRunner *self)
{
  IdeRunnerPrivate *priv = ide_runner_get_instance_private (self);
  gint max_fd = 2;

  g_return_val_if_fail (IDE_IS_RUNNER (self), 2);

  if (priv->fd_mapping != NULL)
    {
      for (guint i = 0; i < priv->fd_mapping->len; i++)
        {
          const FdMapping *map = &g_array_index (priv->fd_mapping, FdMapping, i);

          if (map->dest_fd > max_fd)
            max_fd = map->dest_fd;
        }
    }

  return max_fd;
}
