/* ide-subprocess-launcher.c
 *
 * Copyright 2016-2022 Christian Hergert <chergert@redhat.com>
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
#include "ide-subprocess-launcher-private.h"
#include "ide-unix-fd-map.h"

/* This comes from libide-io but we need access to it */
#include "../io/ide-shell.h"

#define is_flatpak() (ide_get_process_kind() == IDE_PROCESS_KIND_FLATPAK)

typedef struct
{
  GPtrArray        *argv;
  char             *cwd;
  char            **environ;
  char             *stdout_file_path;
  IdeUnixFDMap     *unix_fd_map;

  GSubprocessFlags  flags : 14;
  guint             run_on_host : 1;
  guint             clear_env : 1;
  guint             setup_tty : 1;
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

#define CHILD_PDEATHSIG (1<<0)
#define CHILD_SETUP_TTY (1<<1)

static void
child_setup_func (gpointer data)
{
  guint flags = GPOINTER_TO_UINT (data);

#ifdef G_OS_UNIX
  /*
   * TODO: Check on FreeBSD to see if the process group id is the same as
   *       the owning process. If not, our kill() signal might not work
   *       as expected.
   */

  setsid ();
  setpgid (0, 0);
#endif

#ifdef __linux__
  /*
   * If we were spawned from the main thread, then we can setup the
   * PR_SET_PDEATHSIG and know that when this thread exits that the
   * child will get a kill sig.
   */
  if (flags & CHILD_PDEATHSIG)
    prctl (PR_SET_PDEATHSIG, SIGKILL);
#endif

  if (flags & CHILD_SETUP_TTY)
    {
      if (isatty (STDIN_FILENO))
        ioctl (STDIN_FILENO, TIOCSCTTY, 0);
    }
}

static void
ide_subprocess_launcher_kill_process_group (GCancellable *cancellable,
                                            GSubprocess  *subprocess)
{
#ifdef G_OS_UNIX
  const char *ident;
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

  IDE_ENTRY;

  g_return_if_fail (IDE_IS_SUBPROCESS_LAUNCHER (self));

  {
    g_autofree char *str = NULL;
    g_autofree char *env = NULL;
    str = g_strjoinv (" ", (char **)priv->argv->pdata);
    env = priv->environ ? g_strjoinv (" ", priv->environ) : g_strdup ("");
    g_debug ("Launching %s [env %s] [directory %s] %s parent environment",
             str, env, priv->cwd, priv->clear_env ? "clearing" : "inheriting");
  }

  if (priv->stdout_file_path != NULL &&
      !ide_unix_fd_map_open_file (priv->unix_fd_map,
                                  priv->stdout_file_path, O_WRONLY, STDOUT_FILENO,
                                  &error))
    {
      g_task_return_error (task, g_steal_pointer (&error));
      IDE_EXIT;
    }

  if (!(process = _ide_flatpak_subprocess_new (priv->cwd,
                                               (const char * const *)priv->argv->pdata,
                                               (const char * const *)priv->environ,
                                               priv->flags,
                                               priv->clear_env,
                                               priv->unix_fd_map,
                                               cancellable,
                                               &error)))
    {
      g_task_return_error (task, g_steal_pointer (&error));
      IDE_EXIT;
    }

  if (cancellable != NULL)
    g_signal_connect_object (cancellable,
                             "cancelled",
                             G_CALLBACK (ide_subprocess_launcher_kill_host_process),
                             process,
                             0);

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
  guint flags = 0;
  guint length;

  IDE_ENTRY;

  g_return_if_fail (IDE_IS_SUBPROCESS_LAUNCHER (self));

  if (IDE_IS_MAIN_THREAD ())
    flags |= CHILD_PDEATHSIG;

  if (priv->setup_tty)
    flags |= CHILD_SETUP_TTY;

  {
    g_autofree char *str = NULL;
    g_autofree char *env = NULL;

    str = g_strjoinv (" ", (char **)priv->argv->pdata);
    env = priv->environ ? g_strjoinv (" ", priv->environ) : g_strdup ("");

    g_debug ("Launching %s [env %s] [directory %s] %s parent environment",
             str, env, priv->cwd, priv->clear_env ? "clearing" : "inheriting");
  }

  launcher = g_subprocess_launcher_new (priv->flags);
  g_subprocess_launcher_set_child_setup (launcher, child_setup_func, GUINT_TO_POINTER (flags), NULL);
  g_subprocess_launcher_set_cwd (launcher, priv->cwd);

  if (priv->stdout_file_path != NULL)
    g_subprocess_launcher_set_stdout_file_path (launcher, priv->stdout_file_path);

  length = ide_unix_fd_map_get_length (priv->unix_fd_map);
  for (guint i = 0; i < length; i++)
    {
      int source_fd;
      int dest_fd;

      if (-1 != (source_fd = ide_unix_fd_map_steal (priv->unix_fd_map, i, &dest_fd)))
        {
          if (dest_fd == STDIN_FILENO)
            g_subprocess_launcher_take_stdin_fd (launcher, source_fd);
          else if (dest_fd == STDOUT_FILENO)
            g_subprocess_launcher_take_stdout_fd (launcher, source_fd);
          else if (dest_fd == STDERR_FILENO)
            g_subprocess_launcher_take_stderr_fd (launcher, source_fd);
          else
            g_subprocess_launcher_take_fd (launcher, source_fd, dest_fd);
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
      static const char *envp[] = { NULL };
      g_subprocess_launcher_set_environ (launcher, (char **)envp);
    }

  /*
   * Now override any environment variables that were set using
   * ide_subprocess_launcher_setenv() or ide_subprocess_launcher_set_environ().
   */
  if (priv->environ != NULL)
    {
      for (guint i = 0; priv->environ[i]; i++)
        {
          g_autofree char *key = NULL;
          g_autofree char *value = NULL;

          if (ide_environ_parse (priv->environ[i], &key, &value))
            g_subprocess_launcher_setenv (launcher, key, value, TRUE);
        }
    }

  if (!(real = g_subprocess_launcher_spawnv (launcher,
                                             (const char * const *)priv->argv->pdata,
                                             &error)))
    {
      g_task_return_error (task, g_steal_pointer (&error));
      IDE_EXIT;
    }

  if (cancellable != NULL)
    g_signal_connect_object (cancellable,
                             "cancelled",
                             G_CALLBACK (ide_subprocess_launcher_kill_process_group),
                             real,
                             0);

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
      ide_subprocess_launcher_setenv (self, "PATH", ide_get_user_default_path (), FALSE);
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

  g_clear_object (&priv->unix_fd_map);
  g_clear_pointer (&priv->argv, g_ptr_array_unref);
  g_clear_pointer (&priv->cwd, g_free);
  g_clear_pointer (&priv->environ, g_strfreev);
  g_clear_pointer (&priv->stdout_file_path, g_free);

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
                          (G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS));

  properties [PROP_CWD] =
    g_param_spec_string ("cwd",
                         "Current Working Directory",
                         "Current Working Directory",
                         NULL,
                         (G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS));

  properties [PROP_FLAGS] =
    g_param_spec_flags ("flags",
                        "Flags",
                        "Flags",
                        G_TYPE_SUBPROCESS_FLAGS,
                        G_SUBPROCESS_FLAGS_NONE,
                        (G_PARAM_READWRITE | G_PARAM_CONSTRUCT | G_PARAM_STATIC_STRINGS));

  properties [PROP_ENVIRON] =
    g_param_spec_boxed ("environ",
                        "Environment",
                        "The environment variables for the subprocess",
                        G_TYPE_STRV,
                        (G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS));

  properties [PROP_RUN_ON_HOST] =
    g_param_spec_boolean ("run-on-host",
                          "Run on Host",
                          "Run on Host",
                          FALSE,
                          (G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, N_PROPS, properties);
}

static void
ide_subprocess_launcher_init (IdeSubprocessLauncher *self)
{
  IdeSubprocessLauncherPrivate *priv = ide_subprocess_launcher_get_instance_private (self);

  priv->clear_env = TRUE;
  priv->setup_tty = TRUE;

  priv->unix_fd_map = ide_unix_fd_map_new ();

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

const char * const *
ide_subprocess_launcher_get_environ (IdeSubprocessLauncher *self)
{
  IdeSubprocessLauncherPrivate *priv = ide_subprocess_launcher_get_instance_private (self);

  g_return_val_if_fail (IDE_IS_SUBPROCESS_LAUNCHER (self), NULL);

  return (const char * const *)priv->environ;
}

/**
 * ide_subprocess_launcher_set_environ:
 * @self: an #IdeSubprocessLauncher
 * @environ_: (array zero-terminated=1) (element-type utf8) (nullable): the list
 * of environment variables to set
 *
 * Replace the environment variables by a new list of variables.
 */
void
ide_subprocess_launcher_set_environ (IdeSubprocessLauncher *self,
                                     const char * const    *environ_)
{
  IdeSubprocessLauncherPrivate *priv = ide_subprocess_launcher_get_instance_private (self);

  g_return_if_fail (IDE_IS_SUBPROCESS_LAUNCHER (self));

  if (priv->environ == (char **)environ_)
    return;

  if ((priv->environ == NULL || environ_ == NULL) ||
      !g_strv_equal ((const char * const *)priv->environ, environ_))
    {
      g_strfreev (priv->environ);
      priv->environ = g_strdupv ((char **)environ_);
      g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_ENVIRON]);
    }
}

const char *
ide_subprocess_launcher_getenv (IdeSubprocessLauncher *self,
                                const char           *key)
{
  IdeSubprocessLauncherPrivate *priv = ide_subprocess_launcher_get_instance_private (self);

  g_return_val_if_fail (IDE_IS_SUBPROCESS_LAUNCHER (self), NULL);
  g_return_val_if_fail (key != NULL, NULL);

  return g_environ_getenv (priv->environ, key);
}

void
ide_subprocess_launcher_setenv (IdeSubprocessLauncher *self,
                                const char           *key,
                                const char           *value,
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
                                   const char           *argv)
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
                                 const char           *cwd)
{
  IdeSubprocessLauncherPrivate *priv = ide_subprocess_launcher_get_instance_private (self);

  g_return_if_fail (IDE_IS_SUBPROCESS_LAUNCHER (self));

  if (ide_str_empty0 (cwd))
    cwd = ".";

  if (g_set_str (&priv->cwd, cwd))
    {
      g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_CWD]);
    }
}

const char *
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
          const char *key;
          const char *value;

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
 */
void
ide_subprocess_launcher_push_args (IdeSubprocessLauncher *self,
                                   const char * const   *args)
{
  g_return_if_fail (IDE_IS_SUBPROCESS_LAUNCHER (self));

  if (args != NULL)
    {
      for (guint i = 0; args [i] != NULL; i++)
        ide_subprocess_launcher_push_argv (self, args [i]);
    }
}

char *
ide_subprocess_launcher_pop_argv (IdeSubprocessLauncher *self)
{
  IdeSubprocessLauncherPrivate *priv = ide_subprocess_launcher_get_instance_private (self);
  char *ret = NULL;

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
                                       int                    stdin_fd)
{
  IdeSubprocessLauncherPrivate *priv = ide_subprocess_launcher_get_instance_private (self);
  g_autoptr(GError) error = NULL;

  g_return_if_fail (IDE_IS_SUBPROCESS_LAUNCHER (self));

  ide_unix_fd_map_take (priv->unix_fd_map, stdin_fd, STDIN_FILENO);
}

void
ide_subprocess_launcher_take_stdout_fd (IdeSubprocessLauncher *self,
                                        int                    stdout_fd)
{
  IdeSubprocessLauncherPrivate *priv = ide_subprocess_launcher_get_instance_private (self);

  g_return_if_fail (IDE_IS_SUBPROCESS_LAUNCHER (self));

  ide_unix_fd_map_take (priv->unix_fd_map, stdout_fd, STDOUT_FILENO);
}

void
ide_subprocess_launcher_take_stderr_fd (IdeSubprocessLauncher *self,
                                        int                    stderr_fd)
{
  IdeSubprocessLauncherPrivate *priv = ide_subprocess_launcher_get_instance_private (self);

  g_return_if_fail (IDE_IS_SUBPROCESS_LAUNCHER (self));

  ide_unix_fd_map_take (priv->unix_fd_map, stderr_fd, STDERR_FILENO);
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
                                  const char * const    *args)
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

const char * const *
ide_subprocess_launcher_get_argv (IdeSubprocessLauncher *self)
{
  IdeSubprocessLauncherPrivate *priv = ide_subprocess_launcher_get_instance_private (self);

  g_return_val_if_fail (IDE_IS_SUBPROCESS_LAUNCHER (self), NULL);

  return (const char * const *)priv->argv->pdata;
}

void
ide_subprocess_launcher_insert_argv (IdeSubprocessLauncher *self,
                                     guint                  index,
                                     const char           *arg)
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
                                      const char           *arg)
{
  IdeSubprocessLauncherPrivate *priv = ide_subprocess_launcher_get_instance_private (self);
  char *old_arg;

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
                                 int                    source_fd,
                                 int                    dest_fd)
{
  IdeSubprocessLauncherPrivate *priv = ide_subprocess_launcher_get_instance_private (self);

  g_return_if_fail (IDE_IS_SUBPROCESS_LAUNCHER (self));
  g_return_if_fail (source_fd > -1);
  g_return_if_fail (dest_fd > -1);

  ide_unix_fd_map_take (priv->unix_fd_map, source_fd, dest_fd);
}

void
ide_subprocess_launcher_set_stdout_file_path (IdeSubprocessLauncher *self,
                                              const char           *stdout_file_path)
{
  IdeSubprocessLauncherPrivate *priv = ide_subprocess_launcher_get_instance_private (self);

  g_return_if_fail (IDE_IS_SUBPROCESS_LAUNCHER (self));

  g_set_str (&priv->stdout_file_path, stdout_file_path);
}

const char *
ide_subprocess_launcher_get_stdout_file_path (IdeSubprocessLauncher *self)
{
  IdeSubprocessLauncherPrivate *priv = ide_subprocess_launcher_get_instance_private (self);

  g_return_val_if_fail (IDE_IS_SUBPROCESS_LAUNCHER (self), NULL);

  return priv->stdout_file_path;
}

void
ide_subprocess_launcher_prepend_path (IdeSubprocessLauncher *self,
                                      const char           *path)
{
  const char *old_path;

  g_return_if_fail (IDE_IS_SUBPROCESS_LAUNCHER (self));

  if (path == NULL)
    return;

  old_path = ide_subprocess_launcher_getenv (self, "PATH");

  if (old_path != NULL)
    {
      g_autofree char *new_path = g_strdup_printf ("%s:%s", path, old_path);
      ide_subprocess_launcher_setenv (self, "PATH", new_path, TRUE);
    }
  else
    {
      ide_subprocess_launcher_setenv (self, "PATH", path, TRUE);
    }
}

void
ide_subprocess_launcher_append_path (IdeSubprocessLauncher *self,
                                     const char           *path)
{
  const char *old_path;

  g_return_if_fail (IDE_IS_SUBPROCESS_LAUNCHER (self));

  if (path == NULL)
    return;

  old_path = ide_subprocess_launcher_getenv (self, "PATH");

  if (old_path != NULL)
    {
      g_autofree char *new_path = g_strdup_printf ("%s:%s", old_path, path);
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
  g_return_val_if_fail (IDE_IS_UNIX_FD_MAP (priv->unix_fd_map), FALSE);

  return ide_unix_fd_map_stdin_isatty (priv->unix_fd_map) ||
         ide_unix_fd_map_stdout_isatty (priv->unix_fd_map) ||
         ide_unix_fd_map_stderr_isatty (priv->unix_fd_map);
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
 */
int
ide_subprocess_launcher_get_max_fd (IdeSubprocessLauncher *self)
{
  IdeSubprocessLauncherPrivate *priv = ide_subprocess_launcher_get_instance_private (self);

  g_return_val_if_fail (IDE_IS_SUBPROCESS_LAUNCHER (self), -1);
  g_return_val_if_fail (IDE_IS_UNIX_FD_MAP (priv->unix_fd_map), -1);

  return ide_unix_fd_map_get_max_dest_fd (priv->unix_fd_map);
}

const char *
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
  const char * const *argv;
  GString *str;

  g_return_if_fail (IDE_IS_SUBPROCESS_LAUNCHER (self));
  g_return_if_fail (start_pos < priv->argv->len - 1);

  str = g_string_new (NULL);
  argv = ide_subprocess_launcher_get_argv (self);

  for (guint i = start_pos; argv[i] != NULL; i++)
    {
      g_autofree char *quoted_string = g_shell_quote (argv[i]);

      if (i > 0)
        g_string_append_c (str, ' ');
      g_string_append (str, quoted_string);
    }

  g_ptr_array_remove_range (priv->argv, start_pos, priv->argv->len - start_pos);
  g_ptr_array_add (priv->argv, g_string_free (g_steal_pointer (&str), FALSE));
  g_ptr_array_add (priv->argv, NULL);
}

/**
 * ide_subprocess_launcher_push_argv_format: (skip)
 * @self: a #IdeSubprocessLauncher
 * @format: a printf-style format string
 *
 * Convenience function which allows combining a g_strdup_printf() and
 * call to ide_subprocess_launcher_push_argv() into one call.
 *
 * @format is used to build the argument string which is added using
 * ide_subprocess_launcher_push_argv() and then freed.
 */
void
ide_subprocess_launcher_push_argv_format (IdeSubprocessLauncher *self,
                                          const char            *format,
                                          ...)
{
  g_autofree char *arg = NULL;
  va_list args;

  g_return_if_fail (IDE_IS_SUBPROCESS_LAUNCHER (self));
  g_return_if_fail (format != NULL);

  va_start (args, format);
  arg = g_strdup_vprintf (format, args);
  va_end (args);

  g_return_if_fail (arg != NULL);

  ide_subprocess_launcher_push_argv (self, arg);
}

void
ide_subprocess_launcher_push_argv_parsed (IdeSubprocessLauncher *self,
                                          const char            *args_to_parse)
{
  g_return_if_fail (IDE_IS_SUBPROCESS_LAUNCHER (self));

  if (!ide_str_empty0 (args_to_parse))
    {
      g_autoptr(GError) error = NULL;
      g_auto(GStrv) argv = NULL;
      int argc;

      if (!g_shell_parse_argv (args_to_parse, &argc, &argv, &error))
        g_warning ("Failed to parse args: %s", error->message);
      else
        ide_subprocess_launcher_push_args (self, (const char * const *)argv);
    }
}

/**
 * ide_subprocess_launcher_set_setup_tty:
 * @self: a #IdeSubprocessLauncher
 * @setup_tty: if TTY should be prepared in subprocess
 *
 * Requests the controlling TTY be set on the subprocess.
 */
void
ide_subprocess_launcher_set_setup_tty (IdeSubprocessLauncher *self,
                                       gboolean               setup_tty)
{
  IdeSubprocessLauncherPrivate *priv = ide_subprocess_launcher_get_instance_private (self);

  g_return_if_fail (IDE_IS_SUBPROCESS_LAUNCHER (self));

  priv->setup_tty = !!setup_tty;
}
