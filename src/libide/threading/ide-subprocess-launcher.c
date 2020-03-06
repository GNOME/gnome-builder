/* ide-subprocess-launcher.c
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

#define G_LOG_DOMAIN "ide-subprocess-launcher"

#include "config.h"

#include <errno.h>
#include <fcntl.h>
#include <libide-core.h>
#include <signal.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#ifdef __linux__
# include <sys/prctl.h>
#endif
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include "ide-environment.h"
#include "ide-environment-variable.h"
#include "ide-flatpak-subprocess-private.h"
#include "ide-simple-subprocess-private.h"
#include "ide-subprocess-launcher.h"

#define is_flatpak() (ide_get_process_kind() == IDE_PROCESS_KIND_FLATPAK)

typedef struct
{
  GSubprocessFlags  flags;

  GPtrArray        *argv;
  gchar            *cwd;
  gchar           **environ;
  GArray           *fd_mapping;
  gchar            *stdout_file_path;

  gint              stdin_fd;
  gint              stdout_fd;
  gint              stderr_fd;

  guint             run_on_host : 1;
  guint             clear_env : 1;
} IdeSubprocessLauncherPrivate;

typedef struct
{
  gint source_fd;
  gint dest_fd;
} FdMapping;

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

#ifdef __linux__
  /*
   * If we were spawned from the main thread, then we can setup the
   * PR_SET_PDEATHSIG and know that when this thread exits that the
   * child will get a kill sig.
   */
  if (data != NULL)
    prctl (PR_SET_PDEATHSIG, SIGKILL);
#endif

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

  IDE_ENTRY;

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

  IDE_EXIT;
#else
# error "Your platform is not yet supported"
#endif
}

static void
ide_subprocess_launcher_kill_host_process (GCancellable  *cancellable,
                                           IdeSubprocess *subprocess)
{
  IDE_ENTRY;

  g_assert (G_IS_CANCELLABLE (cancellable));
  g_assert (IDE_IS_FLATPAK_SUBPROCESS (subprocess));

  g_signal_handlers_disconnect_by_func (cancellable,
                                        G_CALLBACK (ide_subprocess_launcher_kill_host_process),
                                        subprocess);

  ide_subprocess_force_exit (subprocess);

  IDE_EXIT;
}

IdeSubprocessLauncher *
ide_subprocess_launcher_new (GSubprocessFlags flags)
{
  return g_object_new (IDE_TYPE_SUBPROCESS_LAUNCHER,
                       "flags", flags,
                       NULL);
}

static gboolean
should_use_flatpak_process (IdeSubprocessLauncher *self)
{
  IdeSubprocessLauncherPrivate *priv = ide_subprocess_launcher_get_instance_private (self);

  g_assert (IDE_IS_SUBPROCESS_LAUNCHER (self));

  if (g_getenv ("IDE_USE_FLATPAK_SUBPROCESS") != NULL)
    return TRUE;

  if (!priv->run_on_host)
    return FALSE;

  return is_flatpak ();
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
  g_autoptr(GArray) fds = NULL;
  gint stdin_fd = -1;
  gint stdout_fd = -1;
  gint stderr_fd = -1;

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

  fds = g_steal_pointer (&priv->fd_mapping);

  if (priv->stdin_fd != -1)
    stdin_fd = dup (priv->stdin_fd);

  if (priv->stdout_fd != -1)
    stdout_fd = dup (priv->stdout_fd);
  else if (priv->stdout_file_path != NULL)
    stdout_fd = open (priv->stdout_file_path, O_WRONLY);

  if (priv->stderr_fd != -1)
    stderr_fd = dup (priv->stderr_fd);

  process = _ide_flatpak_subprocess_new (priv->cwd,
                                          (const gchar * const *)priv->argv->pdata,
                                          (const gchar * const *)priv->environ,
                                          priv->flags,
                                          priv->clear_env,
                                          stdin_fd,
                                          stdout_fd,
                                          stderr_fd,
                                          fds ? (gpointer)fds->data : NULL,
                                          fds ? fds->len : 0,
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
  gpointer child_data = NULL;

  IDE_ENTRY;

  g_return_if_fail (IDE_IS_SUBPROCESS_LAUNCHER (self));

  if (IDE_IS_MAIN_THREAD ())
    child_data = GUINT_TO_POINTER (TRUE);

  {
    g_autofree gchar *str = NULL;
    g_autofree gchar *env = NULL;

    str = g_strjoinv (" ", (gchar **)priv->argv->pdata);
    env = priv->environ ? g_strjoinv (" ", priv->environ) : g_strdup ("");

    g_debug ("Launching '%s' from directory '%s' with environment %s %s parent environment",
             str, priv->cwd, env, priv->clear_env ? "clearing" : "inheriting");
  }

  launcher = g_subprocess_launcher_new (priv->flags);
  g_subprocess_launcher_set_child_setup (launcher, child_setup_func, child_data, NULL);
  g_subprocess_launcher_set_cwd (launcher, priv->cwd);

  if (priv->stdout_file_path != NULL)
    g_subprocess_launcher_set_stdout_file_path (launcher, priv->stdout_file_path);

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

  if (priv->fd_mapping != NULL)
    {
      g_autoptr(GArray) ar = g_steal_pointer (&priv->fd_mapping);

      for (guint i = 0; i < ar->len; i++)
        {
          const FdMapping *map = &g_array_index (ar, FdMapping, i);

          g_subprocess_launcher_take_fd (launcher, map->source_fd, map->dest_fd);
        }
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
  IdeSubprocessLauncherPrivate *priv = ide_subprocess_launcher_get_instance_private (self);
  g_autoptr(GTask) task = NULL;
  IdeSubprocess *ret;
  GError *local_error = NULL;

  g_assert (IDE_IS_SUBPROCESS_LAUNCHER (self));
  g_assert (!cancellable || G_IS_CANCELLABLE (cancellable));

  task = g_task_new (self, cancellable, NULL, NULL);
  g_task_set_source_tag (task, ide_subprocess_launcher_real_spawn);

  if (priv->clear_env || (is_flatpak () && priv->run_on_host))
    {
      /*
       * Many things break without at least PATH, HOME, etc. being set.
       * The GbpFlatpakSubprocessLauncher will also try to set PATH so
       * that it can get /app/bin too. Since it chains up to us, we wont
       * overwrite PATH in that case (which is what we want).
       */
      ide_subprocess_launcher_setenv (self, "PATH", SAFE_PATH, FALSE);
      ide_subprocess_launcher_setenv (self, "HOME", g_get_home_dir (), FALSE);
      ide_subprocess_launcher_setenv (self, "USER", g_get_user_name (), FALSE);
      ide_subprocess_launcher_setenv (self, "LANG", g_getenv ("LANG"), FALSE);
    }

  if (should_use_flatpak_process (self))
    ide_subprocess_launcher_spawn_host_worker (task, self, NULL, cancellable);
  else
    ide_subprocess_launcher_spawn_worker (task, self, NULL, cancellable);

  ret = g_task_propagate_pointer (task, &local_error);

  if (!ret && !local_error)
    local_error = g_error_new (G_IO_ERROR,
                               G_IO_ERROR_FAILED,
                               "An unkonwn error occurred while spawning");

  if (local_error != NULL)
    g_propagate_error (error, g_steal_pointer (&local_error));

  return g_steal_pointer (&ret);
}

static void
ide_subprocess_launcher_finalize (GObject *object)
{
  IdeSubprocessLauncher *self = (IdeSubprocessLauncher *)object;
  IdeSubprocessLauncherPrivate *priv = ide_subprocess_launcher_get_instance_private (self);

  if (priv->fd_mapping != NULL)
    {
      for (guint i = 0; i < priv->fd_mapping->len; i++)
        {
          FdMapping *map = &g_array_index (priv->fd_mapping, FdMapping, i);

          if (map->source_fd != -1)
            close (map->source_fd);
        }

      g_clear_pointer (&priv->fd_mapping, g_array_unref);
    }

  g_clear_pointer (&priv->argv, g_ptr_array_unref);
  g_clear_pointer (&priv->cwd, g_free);
  g_clear_pointer (&priv->environ, g_strfreev);
  g_clear_pointer (&priv->stdout_file_path, g_free);

  if (priv->stdin_fd != -1)
    {
      close (priv->stdin_fd);
      priv->stdin_fd = -1;
    }

  if (priv->stdout_fd != -1)
    {
      close (priv->stdout_fd);
      priv->stdout_fd = -1;
    }

  if (priv->stderr_fd != -1)
    {
      close (priv->stderr_fd);
      priv->stderr_fd = -1;
    }

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

/**
 * ide_subprocess_launcher_set_environ:
 * @self: an #IdeSubprocessLauncher
 * @environ_: (array zero-terminated=1) (element-type utf8) (nullable): the list
 * of environment variables to set
 *
 * Replace the environment variables by a new list of variables.
 *
 * Since: 3.32
 */
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

  if (value != NULL)
    priv->environ = g_environ_setenv (priv->environ, key, value, replace);
  else
    priv->environ = g_environ_unsetenv (priv->environ, key);
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
 * Returns: (transfer full): an #IdeSubprocess or %NULL upon error.
 *
 * Since: 3.32
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
 * @self: an #IdeSubprocessLauncher
 * @args: (array zero-terminated=1) (element-type utf8) (nullable): the arguments
 *
 * This function is semantically identical to calling ide_subprocess_launcher_push_argv()
 * for each element of @args.
 *
 * If @args is %NULL, this function does nothing.
 *
 * Since: 3.32
 */
void
ide_subprocess_launcher_push_args (IdeSubprocessLauncher *self,
                                   const gchar * const   *args)
{
  g_return_if_fail (IDE_IS_SUBPROCESS_LAUNCHER (self));

  if (args != NULL)
    {
      for (guint i = 0; args [i] != NULL; i++)
        ide_subprocess_launcher_push_argv (self, args [i]);
    }
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
 *
 * Since: 3.32
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
 *
 * Since: 3.32
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

/**
 * ide_subprocess_launcher_set_argv:
 * @self: an #IdeSubprocessLauncher
 * @args: (array zero-terminated=1) (element-type utf8) (transfer none): a
 *   %NULL terminated array of strings.
 *
 * Clears the previous arguments and copies @args as the new argument array for
 * the subprocess.
 */
void
ide_subprocess_launcher_set_argv (IdeSubprocessLauncher  *self,
                                  gchar                 **args)
{
  IdeSubprocessLauncherPrivate *priv = ide_subprocess_launcher_get_instance_private (self);

  g_return_if_fail (IDE_IS_SUBPROCESS_LAUNCHER (self));

  g_ptr_array_remove_range (priv->argv, 0, priv->argv->len);

  if (args != NULL)
    {
      for (guint i = 0; args[i] != NULL; i++)
        g_ptr_array_add (priv->argv, g_strdup (args[i]));
    }

  g_ptr_array_add (priv->argv, NULL);
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

void
ide_subprocess_launcher_take_fd (IdeSubprocessLauncher *self,
                                 gint                   source_fd,
                                 gint                   dest_fd)
{
  IdeSubprocessLauncherPrivate *priv = ide_subprocess_launcher_get_instance_private (self);
  FdMapping map = {
    .source_fd = source_fd,
    .dest_fd = dest_fd
  };

  g_return_if_fail (IDE_IS_SUBPROCESS_LAUNCHER (self));
  g_return_if_fail (source_fd > -1);
  g_return_if_fail (dest_fd > -1);

  if (priv->fd_mapping == NULL)
    priv->fd_mapping = g_array_new (FALSE, FALSE, sizeof (FdMapping));

  g_array_append_val (priv->fd_mapping, map);
}

void
ide_subprocess_launcher_set_stdout_file_path (IdeSubprocessLauncher *self,
                                              const gchar           *stdout_file_path)
{
  IdeSubprocessLauncherPrivate *priv = ide_subprocess_launcher_get_instance_private (self);

  g_return_if_fail (IDE_IS_SUBPROCESS_LAUNCHER (self));

  if (g_strcmp0 (priv->stdout_file_path, stdout_file_path) != 0)
    {
      g_free (priv->stdout_file_path);
      priv->stdout_file_path = g_strdup (stdout_file_path);
    }
}

void
ide_subprocess_launcher_append_path (IdeSubprocessLauncher *self,
                                     const gchar           *path)
{
  const gchar *old_path;

  g_return_if_fail (IDE_IS_SUBPROCESS_LAUNCHER (self));

  if (path == NULL)
    return;

  old_path = ide_subprocess_launcher_getenv (self, "PATH");

  if (old_path != NULL)
    {
      g_autofree gchar *new_path = g_strdup_printf ("%s:%s", old_path, path);
      ide_subprocess_launcher_setenv (self, "PATH", new_path, TRUE);
    }
  else
    {
      ide_subprocess_launcher_setenv (self, "PATH", path, TRUE);
    }
}

gboolean
ide_subprocess_launcher_get_needs_tty (IdeSubprocessLauncher *self)
{
  IdeSubprocessLauncherPrivate *priv = ide_subprocess_launcher_get_instance_private (self);

  g_return_val_if_fail (IDE_IS_SUBPROCESS_LAUNCHER (self), FALSE);

  if ((priv->stdin_fd != -1 && isatty (priv->stdin_fd)) ||
      (priv->stdout_fd != -1 && isatty (priv->stdout_fd)) ||
      (priv->stderr_fd != -1 && isatty (priv->stderr_fd)))
    return TRUE;

  if (priv->fd_mapping != NULL)
    {
      for (guint i = 0; i < priv->fd_mapping->len; i++)
        {
          const FdMapping *fdmap = &g_array_index (priv->fd_mapping, FdMapping, i);

          switch (fdmap->dest_fd)
            {
            case STDIN_FILENO:
            case STDOUT_FILENO:
            case STDERR_FILENO:
              if (isatty (fdmap->source_fd))
                return TRUE;
              break;

            default:
              break;
            }
        }
    }

  return FALSE;
}

/**
 * ide_subprocess_launcher_get_max_fd:
 * @self: a #IdeSubprocessLauncher
 *
 * Gets the hightest number of FD that has been mapped into the
 * subprocess launcher.
 *
 * This will always return a value >= 2 (to indicate stdin/stdout/stderr).
 *
 * Returns: an integer for the max-fd
 *
 * Since: 3.34
 */
gint
ide_subprocess_launcher_get_max_fd (IdeSubprocessLauncher *self)
{
  IdeSubprocessLauncherPrivate *priv = ide_subprocess_launcher_get_instance_private (self);
  gint max_fd = 2;

  g_return_val_if_fail (IDE_IS_SUBPROCESS_LAUNCHER (self), 2);

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

const gchar *
ide_subprocess_launcher_get_arg (IdeSubprocessLauncher *self,
                                 guint                  pos)
{
  IdeSubprocessLauncherPrivate *priv = ide_subprocess_launcher_get_instance_private (self);

  g_return_val_if_fail (IDE_IS_SUBPROCESS_LAUNCHER (self), NULL);

  if (pos < priv->argv->len)
    return g_ptr_array_index (priv->argv, pos);

  return NULL;
}

void
ide_subprocess_launcher_join_args_for_sh_c (IdeSubprocessLauncher *self,
                                            guint                  start_pos)
{
  IdeSubprocessLauncherPrivate *priv = ide_subprocess_launcher_get_instance_private (self);
  const gchar * const *argv;
  GString *str;

  g_return_if_fail (IDE_IS_SUBPROCESS_LAUNCHER (self));
  g_return_if_fail (start_pos < priv->argv->len - 1);

  str = g_string_new (NULL);
  argv = ide_subprocess_launcher_get_argv (self);

  for (guint i = start_pos; argv[i] != NULL; i++)
    {
      g_autofree gchar *quoted_string = g_shell_quote (argv[i]);

      if (i > 0)
        g_string_append_c (str, ' ');
      g_string_append (str, quoted_string);
    }

  g_ptr_array_remove_range (priv->argv, start_pos, priv->argv->len - start_pos);
  g_ptr_array_add (priv->argv, g_string_free (g_steal_pointer (&str), FALSE));
  g_ptr_array_add (priv->argv, NULL);
}
