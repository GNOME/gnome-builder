/* ide-subprocess-launcher.c
 *
 * Copyright (C) 2016 Christian Hergert <chergert@redhat.com>
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
 */

#define G_LOG_DOMAIN "ide-subprocess-launcher"

#include <errno.h>
#include <signal.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <unistd.h>

#include "ide-debug.h"
#include "ide-macros.h"

#include "buildsystem/ide-environment-variable.h"
#include "buildsystem/ide-environment.h"
#include "subprocess/ide-breakout-subprocess.h"
#include "subprocess/ide-breakout-subprocess-private.h"
#include "subprocess/ide-simple-subprocess.h"
#include "subprocess/ide-subprocess-launcher.h"
#include "util/ide-flatpak.h"

typedef struct
{
  GSubprocessFlags  flags;

  GPtrArray        *argv;
  gchar            *cwd;
  gchar           **environ;

  gint              stdin_fd;
  gint              stdout_fd;
  gint              stderr_fd;

  guint             run_on_host : 1;
  guint             clear_env : 1;
} IdeSubprocessLauncherPrivate;

G_DEFINE_TYPE_WITH_PRIVATE (IdeSubprocessLauncher, ide_subprocess_launcher, G_TYPE_OBJECT)

enum {
  PROP_0,
  PROP_CLEAR_ENV,
  PROP_CWD,
  PROP_ENVIRON,
  PROP_FLAGS,
  PROP_RUN_ON_HOST,
  N_PROPS
};

static GParamSpec *properties [N_PROPS];

static void
child_setup_func (gpointer data)
{
#ifdef G_OS_UNIX
  /*
   * TODO: Check on FreeBSD to see if the process group id is the same as
   *       the owning process. If not, our kill() signal might not work
   *       as expected.
   */

  setsid ();
  setpgid (0, 0);

  if (isatty (STDIN_FILENO))
    {
      if (ioctl (STDIN_FILENO, TIOCSCTTY, 0) != 0)
        g_warning ("Failed to setup TIOCSCTTY on stdin: %s",
                   g_strerror (errno));
    }
#endif
}

static void
ide_subprocess_launcher_kill_process_group (GCancellable *cancellable,
                                            GSubprocess  *subprocess)
{
#ifdef G_OS_UNIX
  const gchar *ident;
  pid_t pid;

  g_assert (G_IS_CANCELLABLE (cancellable));
  g_assert (G_IS_SUBPROCESS (subprocess));

  /*
   * This will send SIGKILL to all processes in the process group that
   * was created for our subprocess using setsid().
   */

  if (NULL != (ident = g_subprocess_get_identifier (subprocess)))
    {
      g_debug ("Killing process group %s due to cancellation", ident);
      pid = atoi (ident);
      kill (-pid, SIGKILL);
    }

  g_signal_handlers_disconnect_by_func (cancellable,
                                        G_CALLBACK (ide_subprocess_launcher_kill_process_group),
                                        subprocess);
#else
# error "Your platform is not yet supported"
#endif
}

static void
ide_subprocess_launcher_kill_host_process (GCancellable  *cancellable,
                                           IdeSubprocess *subprocess)
{
  g_assert (G_IS_CANCELLABLE (cancellable));
  g_assert (IDE_IS_BREAKOUT_SUBPROCESS (subprocess));

  g_signal_handlers_disconnect_by_func (cancellable,
                                        G_CALLBACK (ide_subprocess_launcher_kill_host_process),
                                        subprocess);

  ide_subprocess_force_exit (subprocess);
}

IdeSubprocessLauncher *
ide_subprocess_launcher_new (GSubprocessFlags flags)
{
  return g_object_new (IDE_TYPE_SUBPROCESS_LAUNCHER,
                       "flags", flags,
                       NULL);
}

static gboolean
should_use_breakout_process (IdeSubprocessLauncher *self)
{
  IdeSubprocessLauncherPrivate *priv = ide_subprocess_launcher_get_instance_private (self);

  g_assert (IDE_IS_SUBPROCESS_LAUNCHER (self));

  if (g_getenv ("IDE_USE_BREAKOUT_SUBPROCESS") != NULL)
    return TRUE;

  if (!priv->run_on_host)
    return FALSE;

  return ide_is_flatpak ();
}

static void
ide_subprocess_launcher_spawn_host_worker (GTask        *task,
                                           gpointer      source_object,
                                           gpointer      task_data,
                                           GCancellable *cancellable)
{
  IdeSubprocessLauncher *self = source_object;
  IdeSubprocessLauncherPrivate *priv = ide_subprocess_launcher_get_instance_private (self);
  g_autoptr(IdeSubprocess) process = NULL;
  g_autoptr(GError) error = NULL;

  IDE_ENTRY;

  g_return_if_fail (IDE_IS_SUBPROCESS_LAUNCHER (self));

#ifdef IDE_ENABLE_TRACE
  {
    g_autofree gchar *str = NULL;
    g_autofree gchar *env = NULL;
    str = g_strjoinv (" ", (gchar **)priv->argv->pdata);
    env = priv->environ ? g_strjoinv (" ", priv->environ) : g_strdup ("");
    IDE_TRACE_MSG ("Launching '%s' with environment %s %s parent environment",
                   str, env, priv->clear_env ? "clearing" : "inheriting");
  }
#endif

  process = _ide_breakout_subprocess_new (priv->cwd,
                                          (const gchar * const *)priv->argv->pdata,
                                          (const gchar * const *)priv->environ,
                                          priv->flags,
                                          priv->clear_env,
                                          priv->stdin_fd,
                                          priv->stdout_fd,
                                          priv->stderr_fd,
                                          cancellable,
                                          &error);

  if (process == NULL)
    {
      g_task_return_error (task, g_steal_pointer (&error));
      IDE_EXIT;
    }

  if (cancellable != NULL)
    {
      g_signal_connect_object (cancellable,
                               "cancelled",
                               G_CALLBACK (ide_subprocess_launcher_kill_host_process),
                               process,
                               0);
    }

  g_task_return_pointer (task, g_steal_pointer (&process), g_object_unref);

  IDE_EXIT;
}

static void
ide_subprocess_launcher_spawn_worker (GTask        *task,
                                      gpointer      source_object,
                                      gpointer      task_data,
                                      GCancellable *cancellable)
{
  IdeSubprocessLauncher *self = source_object;
  IdeSubprocessLauncherPrivate *priv = ide_subprocess_launcher_get_instance_private (self);
  g_autoptr(GSubprocessLauncher) launcher = NULL;
  g_autoptr(GSubprocess) real = NULL;
  g_autoptr(IdeSubprocess) wrapped = NULL;
  g_autoptr(GError) error = NULL;

  IDE_ENTRY;

  g_return_if_fail (IDE_IS_SUBPROCESS_LAUNCHER (self));

  /* Many things break without at least PATH, HOME, etc. being set */
  if (priv->clear_env)
    {
      ide_subprocess_launcher_setenv (self, "PATH", "/bin:/usr/bin", FALSE);
      ide_subprocess_launcher_setenv (self, "HOME", g_get_home_dir (), FALSE);
      ide_subprocess_launcher_setenv (self, "USER", g_get_user_name (), FALSE);
    }

#ifdef IDE_ENABLE_TRACE
  {
    g_autofree gchar *str = NULL;
    g_autofree gchar *env = NULL;
    str = g_strjoinv (" ", (gchar **)priv->argv->pdata);
    env = priv->environ ? g_strjoinv (" ", priv->environ) : g_strdup ("");
    IDE_TRACE_MSG ("Launching '%s' from directory %s with environment %s %s parent environment",
                   str, priv->cwd, env, priv->clear_env ? "clearing" : "inheriting");
  }
#endif

  launcher = g_subprocess_launcher_new (priv->flags);
  g_subprocess_launcher_set_child_setup (launcher, child_setup_func, NULL, NULL);
  g_subprocess_launcher_set_cwd (launcher, priv->cwd);

  if (priv->stdin_fd != -1)
    {
      g_subprocess_launcher_take_stdin_fd (launcher, priv->stdin_fd);
      priv->stdin_fd = -1;
    }

  if (priv->stdout_fd != -1)
    {
      g_subprocess_launcher_take_stdout_fd (launcher, priv->stdout_fd);
      priv->stdout_fd = -1;
    }

  if (priv->stderr_fd != -1)
    {
      g_subprocess_launcher_take_stderr_fd (launcher, priv->stderr_fd);
      priv->stderr_fd = -1;
    }

  /*
   * GSubprocessLauncher starts by inheriting the current environment.
   * So if clear-env is set, we need to unset those environment variables.
   * Simply setting the environ to NULL doesn't work, because glib uses
   * execv rather than execve in that case.
   */
  if (priv->clear_env)
    {
      gchar *envp[] = { NULL };
      g_subprocess_launcher_set_environ (launcher, envp);
    }

  /*
   * Now override any environment variables that were set using
   * ide_subprocess_launcher_setenv() or ide_subprocess_launcher_set_environ().
   */
  if (priv->environ != NULL)
    {
      for (guint i = 0; priv->environ[i] != NULL; i++)
        {
          const gchar *pair = priv->environ[i];
          const gchar *eq = strchr (pair, '=');
          g_autofree gchar *key = g_strndup (pair, eq - pair);
          const gchar *val = eq ? eq + 1 : NULL;

          g_subprocess_launcher_setenv (launcher, key, val, TRUE);
        }
    }

  real = g_subprocess_launcher_spawnv (launcher,
                                       (const gchar * const *)priv->argv->pdata,
                                       &error);

  if (real == NULL)
    {
      g_task_return_error (task, g_steal_pointer (&error));
      IDE_EXIT;
    }

  if (cancellable != NULL)
    {
      g_signal_connect_object (cancellable,
                               "cancelled",
                               G_CALLBACK (ide_subprocess_launcher_kill_process_group),
                               real,
                               0);
    }

  wrapped = ide_simple_subprocess_new (real);

  g_task_return_pointer (task, g_steal_pointer (&wrapped), g_object_unref);

  IDE_EXIT;
}

static IdeSubprocess *
ide_subprocess_launcher_real_spawn (IdeSubprocessLauncher  *self,
                                    GCancellable           *cancellable,
                                    GError                **error)
{
  g_autoptr(GTask) task = NULL;

  g_assert (IDE_IS_SUBPROCESS_LAUNCHER (self));
  g_assert (!cancellable || G_IS_CANCELLABLE (cancellable));

  task = g_task_new (self, cancellable, NULL, NULL);
  g_task_set_source_tag (task, ide_subprocess_launcher_real_spawn);

  if (should_use_breakout_process (self))
    g_task_run_in_thread_sync (task, ide_subprocess_launcher_spawn_host_worker);
  else
    g_task_run_in_thread_sync (task, ide_subprocess_launcher_spawn_worker);

  return g_task_propagate_pointer (task, error);
}

static void
ide_subprocess_launcher_finalize (GObject *object)
{
  IdeSubprocessLauncher *self = (IdeSubprocessLauncher *)object;
  IdeSubprocessLauncherPrivate *priv = ide_subprocess_launcher_get_instance_private (self);

  g_clear_pointer (&priv->argv, g_ptr_array_unref);
  g_clear_pointer (&priv->cwd, g_free);
  g_clear_pointer (&priv->environ, g_strfreev);

  if (priv->stdin_fd != -1)
    close (priv->stdin_fd);

  if (priv->stdout_fd != -1)
    close (priv->stdout_fd);

  if (priv->stderr_fd != -1)
    close (priv->stderr_fd);

  G_OBJECT_CLASS (ide_subprocess_launcher_parent_class)->finalize (object);
}

static void
ide_subprocess_launcher_get_property (GObject    *object,
                                      guint       prop_id,
                                      GValue     *value,
                                      GParamSpec *pspec)
{
  IdeSubprocessLauncher *self = IDE_SUBPROCESS_LAUNCHER (object);

  switch (prop_id)
    {
    case PROP_CLEAR_ENV:
      g_value_set_boolean (value, ide_subprocess_launcher_get_clear_env (self));
      break;

    case PROP_CWD:
      g_value_set_string (value, ide_subprocess_launcher_get_cwd (self));
      break;

    case PROP_FLAGS:
      g_value_set_flags (value, ide_subprocess_launcher_get_flags (self));
      break;

    case PROP_ENVIRON:
      g_value_set_boxed (value, ide_subprocess_launcher_get_environ (self));
      break;

    case PROP_RUN_ON_HOST:
      g_value_set_boolean (value, ide_subprocess_launcher_get_run_on_host (self));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
ide_subprocess_launcher_set_property (GObject      *object,
                                      guint         prop_id,
                                      const GValue *value,
                                      GParamSpec   *pspec)
{
  IdeSubprocessLauncher *self = IDE_SUBPROCESS_LAUNCHER (object);

  switch (prop_id)
    {
    case PROP_CLEAR_ENV:
      ide_subprocess_launcher_set_clear_env (self, g_value_get_boolean (value));
      break;

    case PROP_CWD:
      ide_subprocess_launcher_set_cwd (self, g_value_get_string (value));
      break;

    case PROP_FLAGS:
      ide_subprocess_launcher_set_flags (self, g_value_get_flags (value));
      break;

    case PROP_ENVIRON:
      ide_subprocess_launcher_set_environ (self, g_value_get_boxed (value));
      break;

    case PROP_RUN_ON_HOST:
      ide_subprocess_launcher_set_run_on_host (self, g_value_get_boolean (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
ide_subprocess_launcher_class_init (IdeSubprocessLauncherClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = ide_subprocess_launcher_finalize;
  object_class->get_property = ide_subprocess_launcher_get_property;
  object_class->set_property = ide_subprocess_launcher_set_property;

  klass->spawn = ide_subprocess_launcher_real_spawn;

  properties [PROP_CLEAR_ENV] =
    g_param_spec_boolean ("clean-env",
                          "Clear Environment",
                          "If the environment should be cleared before setting environment variables.",
                          FALSE,
                          (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  properties [PROP_CWD] =
    g_param_spec_string ("cwd",
                         "Current Working Directory",
                         "Current Working Directory",
                         NULL,
                         (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  properties [PROP_FLAGS] =
    g_param_spec_flags ("flags",
                        "Flags",
                        "Flags",
                        G_TYPE_SUBPROCESS_FLAGS,
                        G_SUBPROCESS_FLAGS_NONE,
                        (G_PARAM_READWRITE | G_PARAM_CONSTRUCT | G_PARAM_STATIC_STRINGS));

  properties [PROP_ENVIRON] =
    g_param_spec_boxed ("environ",
                        "Environ",
                        "Environ",
                        G_TYPE_STRV,
                        (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  properties [PROP_RUN_ON_HOST] =
    g_param_spec_boolean ("run-on-host",
                          "Run on Host",
                          "Run on Host",
                          FALSE,
                          (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, N_PROPS, properties);
}

static void
ide_subprocess_launcher_init (IdeSubprocessLauncher *self)
{
  IdeSubprocessLauncherPrivate *priv = ide_subprocess_launcher_get_instance_private (self);

  priv->clear_env = TRUE;

  priv->stdin_fd = -1;
  priv->stdout_fd = -1;
  priv->stderr_fd = -1;

  priv->argv = g_ptr_array_new_with_free_func (g_free);
  g_ptr_array_add (priv->argv, NULL);

  priv->cwd = g_strdup (".");
}

void
ide_subprocess_launcher_set_flags (IdeSubprocessLauncher *self,
                                   GSubprocessFlags       flags)
{
  IdeSubprocessLauncherPrivate *priv = ide_subprocess_launcher_get_instance_private (self);

  g_return_if_fail (IDE_IS_SUBPROCESS_LAUNCHER (self));

  if (flags != priv->flags)
    {
      priv->flags = flags;
      g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_FLAGS]);
    }
}

GSubprocessFlags
ide_subprocess_launcher_get_flags (IdeSubprocessLauncher *self)
{
  IdeSubprocessLauncherPrivate *priv = ide_subprocess_launcher_get_instance_private (self);

  g_return_val_if_fail (IDE_IS_SUBPROCESS_LAUNCHER (self), 0);

  return priv->flags;
}

const gchar * const *
ide_subprocess_launcher_get_environ (IdeSubprocessLauncher *self)
{
  IdeSubprocessLauncherPrivate *priv = ide_subprocess_launcher_get_instance_private (self);

  g_return_val_if_fail (IDE_IS_SUBPROCESS_LAUNCHER (self), NULL);

  return (const gchar * const *)priv->environ;
}

void
ide_subprocess_launcher_set_environ (IdeSubprocessLauncher *self,
                                     const gchar * const   *environ_)
{
  IdeSubprocessLauncherPrivate *priv = ide_subprocess_launcher_get_instance_private (self);

  g_return_if_fail (IDE_IS_SUBPROCESS_LAUNCHER (self));

  if (priv->environ != (gchar **)environ_)
    {
      g_strfreev (priv->environ);
      priv->environ = g_strdupv ((gchar **)environ_);
    }
}

const gchar *
ide_subprocess_launcher_getenv (IdeSubprocessLauncher *self,
                                const gchar           *key)
{
  IdeSubprocessLauncherPrivate *priv = ide_subprocess_launcher_get_instance_private (self);

  g_return_val_if_fail (IDE_IS_SUBPROCESS_LAUNCHER (self), NULL);
  g_return_val_if_fail (key != NULL, NULL);

  return g_environ_getenv (priv->environ, key);
}

void
ide_subprocess_launcher_setenv (IdeSubprocessLauncher *self,
                                const gchar           *key,
                                const gchar           *value,
                                gboolean               replace)
{
  IdeSubprocessLauncherPrivate *priv = ide_subprocess_launcher_get_instance_private (self);

  g_return_if_fail (IDE_IS_SUBPROCESS_LAUNCHER (self));
  g_return_if_fail (key != NULL);

  priv->environ = g_environ_setenv (priv->environ, key, value, replace);
}

void
ide_subprocess_launcher_push_argv (IdeSubprocessLauncher *self,
                                   const gchar           *argv)
{
  IdeSubprocessLauncherPrivate *priv = ide_subprocess_launcher_get_instance_private (self);

  g_return_if_fail (IDE_IS_SUBPROCESS_LAUNCHER (self));
  g_return_if_fail (argv != NULL);

  g_ptr_array_index (priv->argv, priv->argv->len - 1) = g_strdup (argv);
  g_ptr_array_add (priv->argv, NULL);
}

/**
 * ide_subprocess_launcher_spawn:
 *
 * Synchronously spawn a process using the internal state.
 *
 * Returns: (transfer full): A #IdeSubprocess or %NULL upon error.
 */
IdeSubprocess *
ide_subprocess_launcher_spawn (IdeSubprocessLauncher  *self,
                               GCancellable           *cancellable,
                               GError                **error)
{
  g_return_val_if_fail (IDE_IS_SUBPROCESS_LAUNCHER (self), NULL);
  g_return_val_if_fail (!cancellable || G_IS_CANCELLABLE (cancellable), NULL);

  return IDE_SUBPROCESS_LAUNCHER_GET_CLASS (self)->spawn (self, cancellable, error);
}

void
ide_subprocess_launcher_set_cwd (IdeSubprocessLauncher *self,
                                 const gchar           *cwd)
{
  IdeSubprocessLauncherPrivate *priv = ide_subprocess_launcher_get_instance_private (self);

  g_return_if_fail (IDE_IS_SUBPROCESS_LAUNCHER (self));

  if (ide_str_empty0 (cwd))
    cwd = ".";

  if (!ide_str_equal0 (priv->cwd, cwd))
    {
      g_free (priv->cwd);
      priv->cwd = g_strdup (cwd);
      g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_CWD]);
    }
}

const gchar *
ide_subprocess_launcher_get_cwd (IdeSubprocessLauncher *self)
{
  IdeSubprocessLauncherPrivate *priv = ide_subprocess_launcher_get_instance_private (self);

  g_return_val_if_fail (IDE_IS_SUBPROCESS_LAUNCHER (self), NULL);

  return priv->cwd;
}

void
ide_subprocess_launcher_overlay_environment (IdeSubprocessLauncher *self,
                                             IdeEnvironment        *environment)
{
  g_return_if_fail (IDE_IS_SUBPROCESS_LAUNCHER (self));
  g_return_if_fail (!environment || IDE_IS_ENVIRONMENT (environment));

  if (environment != NULL)
    {
      guint n_items = g_list_model_get_n_items (G_LIST_MODEL (environment));

      for (guint i = 0; i < n_items; i++)
        {
          g_autoptr(IdeEnvironmentVariable) var = NULL;
          const gchar *key;
          const gchar *value;

          var = g_list_model_get_item (G_LIST_MODEL (environment), i);
          key = ide_environment_variable_get_key (var);
          value = ide_environment_variable_get_value (var);

          if (!ide_str_empty0 (key))
            ide_subprocess_launcher_setenv (self, key, value ?: "", TRUE);
        }
    }
}

/**
 * ide_subprocess_launcher_push_args:
 * @args: (array zero-terminated=1) (element-type utf8): the arguments
 */
void
ide_subprocess_launcher_push_args (IdeSubprocessLauncher *self,
                                   const gchar * const   *args)
{
  g_return_if_fail (IDE_IS_SUBPROCESS_LAUNCHER (self));
  g_return_if_fail (args != NULL);

  for (guint i = 0; args [i] != NULL; i++)
    ide_subprocess_launcher_push_argv (self, args [i]);
}

gchar *
ide_subprocess_launcher_pop_argv (IdeSubprocessLauncher *self)
{
  IdeSubprocessLauncherPrivate *priv = ide_subprocess_launcher_get_instance_private (self);
  gchar *ret = NULL;

  g_return_val_if_fail (IDE_IS_SUBPROCESS_LAUNCHER (self), NULL);

  if (priv->argv->len > 1)
    {
      g_assert (g_ptr_array_index (priv->argv, priv->argv->len - 1) == NULL);

      ret = g_ptr_array_index (priv->argv, priv->argv->len - 2);
      g_ptr_array_index (priv->argv, priv->argv->len - 2) = NULL;
      g_ptr_array_set_size (priv->argv, priv->argv->len - 1);
    }

  return ret;
}

/**
 * ide_subprocess_launcher_get_run_on_host:
 *
 * Gets if the process should be executed on the host system. This might be
 * useful for situations where running in a contained environment is not
 * sufficient to perform the given task.
 *
 * Currently, only flatpak is supported for breaking out of the containment
 * zone and requires the application was built with --allow=devel.
 *
 * Returns: %TRUE if the process should be executed outside the containment zone.
 */
gboolean
ide_subprocess_launcher_get_run_on_host (IdeSubprocessLauncher *self)
{
  IdeSubprocessLauncherPrivate *priv = ide_subprocess_launcher_get_instance_private (self);

  g_return_val_if_fail (IDE_IS_SUBPROCESS_LAUNCHER (self), FALSE);

  return priv->run_on_host;
}

/**
 * ide_subprocess_launcher_set_run_on_host:
 *
 * Sets the #IdeSubprocessLauncher:run-on-host property. See
 * ide_subprocess_launcher_get_run_on_host() for more information.
 */
void
ide_subprocess_launcher_set_run_on_host (IdeSubprocessLauncher *self,
                                         gboolean               run_on_host)
{
  IdeSubprocessLauncherPrivate *priv = ide_subprocess_launcher_get_instance_private (self);

  run_on_host = !!run_on_host;

  if (priv->run_on_host != run_on_host)
    {
      priv->run_on_host = run_on_host;
      g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_RUN_ON_HOST]);
    }
}

gboolean
ide_subprocess_launcher_get_clear_env (IdeSubprocessLauncher *self)
{
  IdeSubprocessLauncherPrivate *priv = ide_subprocess_launcher_get_instance_private (self);

  g_return_val_if_fail (IDE_IS_SUBPROCESS_LAUNCHER (self), FALSE);

  return priv->clear_env;
}

void
ide_subprocess_launcher_set_clear_env (IdeSubprocessLauncher *self,
                                       gboolean               clear_env)
{
  IdeSubprocessLauncherPrivate *priv = ide_subprocess_launcher_get_instance_private (self);

  g_return_if_fail (IDE_IS_SUBPROCESS_LAUNCHER (self));

  clear_env = !!clear_env;

  if (priv->clear_env != clear_env)
    {
      priv->clear_env = clear_env;
      g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_CLEAR_ENV]);
    }
}

void
ide_subprocess_launcher_take_stdin_fd (IdeSubprocessLauncher *self,
                                       gint                   stdin_fd)
{
  IdeSubprocessLauncherPrivate *priv = ide_subprocess_launcher_get_instance_private (self);

  g_return_if_fail (IDE_IS_SUBPROCESS_LAUNCHER (self));

  if (priv->stdin_fd != stdin_fd)
    {
      if (priv->stdin_fd != -1)
        close (priv->stdin_fd);
      priv->stdin_fd = stdin_fd;
    }
}

void
ide_subprocess_launcher_take_stdout_fd (IdeSubprocessLauncher *self,
                                        gint                   stdout_fd)
{
  IdeSubprocessLauncherPrivate *priv = ide_subprocess_launcher_get_instance_private (self);

  g_return_if_fail (IDE_IS_SUBPROCESS_LAUNCHER (self));

  if (priv->stdout_fd != stdout_fd)
    {
      if (priv->stdout_fd != -1)
        close (priv->stdout_fd);
      priv->stdout_fd = stdout_fd;
    }
}

void
ide_subprocess_launcher_take_stderr_fd (IdeSubprocessLauncher *self,
                                        gint                   stderr_fd)
{
  IdeSubprocessLauncherPrivate *priv = ide_subprocess_launcher_get_instance_private (self);

  g_return_if_fail (IDE_IS_SUBPROCESS_LAUNCHER (self));

  if (priv->stderr_fd != stderr_fd)
    {
      if (priv->stderr_fd != -1)
        close (priv->stderr_fd);
      priv->stderr_fd = stderr_fd;
    }
}

const gchar * const *
ide_subprocess_launcher_get_argv (IdeSubprocessLauncher *self)
{
  IdeSubprocessLauncherPrivate *priv = ide_subprocess_launcher_get_instance_private (self);

  g_return_val_if_fail (IDE_IS_SUBPROCESS_LAUNCHER (self), NULL);

  return (const gchar * const *)priv->argv->pdata;
}

void
ide_subprocess_launcher_insert_argv (IdeSubprocessLauncher *self,
                                     guint                  index,
                                     const gchar           *arg)
{
  IdeSubprocessLauncherPrivate *priv = ide_subprocess_launcher_get_instance_private (self);

  g_return_if_fail (IDE_IS_SUBPROCESS_LAUNCHER (self));
  g_return_if_fail (priv->argv->len > 0);
  g_return_if_fail (index < (priv->argv->len - 1));
  g_return_if_fail (arg != NULL);

  g_ptr_array_insert (priv->argv, index, g_strdup (arg));
}

void
ide_subprocess_launcher_replace_argv (IdeSubprocessLauncher *self,
                                      guint                  index,
                                      const gchar           *arg)
{
  IdeSubprocessLauncherPrivate *priv = ide_subprocess_launcher_get_instance_private (self);
  gchar *old_arg;

  g_return_if_fail (IDE_IS_SUBPROCESS_LAUNCHER (self));
  g_return_if_fail (priv->argv->len > 0);
  g_return_if_fail (index < (priv->argv->len - 1));
  g_return_if_fail (arg != NULL);

  /* overwriting in place */
  old_arg = g_ptr_array_index (priv->argv, index);
  g_ptr_array_index (priv->argv, index) = g_strdup (arg);
  g_free (old_arg);
}
