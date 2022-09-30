/* ide-shell.c
 *
 * Copyright 2021-2022 Christian Hergert <chergert@redhat.com>
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

#define G_LOG_DOMAIN "ide-shell"

#include "config.h"

#include <libide-threading.h>

#include "ide-shell-private.h"

static const char *user_shell = "/bin/sh";
static const char *user_default_path = SAFE_PATH;

gboolean
ide_shell_supports_dash_c (const char *shell)
{
  if (shell == NULL)
    return FALSE;

  return strcmp (shell, "bash") == 0 || g_str_has_suffix (shell, "/bash") ||
#if 0
         /* Fish does apparently support -l and -c in testing, but it is causing
          * issues with users, so we will disable it for now so that we fallback
          * to using `sh -l -c ''` instead.
          */
         strcmp (shell, "fish") == 0 || g_str_has_suffix (shell, "/fish") ||
#endif
         strcmp (shell, "zsh") == 0 || g_str_has_suffix (shell, "/zsh") ||
         strcmp (shell, "dash") == 0 || g_str_has_suffix (shell, "/dash") ||
         strcmp (shell, "tcsh") == 0 || g_str_has_suffix (shell, "/tcsh") ||
         strcmp (shell, "sh") == 0 || g_str_has_suffix (shell, "/sh");
}

/**
 * ide_shell_supports_dash_login:
 * @shell: the name of the shell, such as `sh` or `/bin/sh`
 *
 * Checks if the shell is known to support login semantics. Originally,
 * this meant `--login`, but now is meant to mean `-l` as more shells
 * support `-l` than `--login` (notably dash).
 *
 * Returns: %TRUE if @shell likely supports `-l`.
 */
gboolean
ide_shell_supports_dash_login (const char *shell)
{
  if (shell == NULL)
    return FALSE;

  return strcmp (shell, "bash") == 0 || g_str_has_suffix (shell, "/bash") ||
#if 0
         strcmp (shell, "fish") == 0 || g_str_has_suffix (shell, "/fish") ||
#endif
         strcmp (shell, "zsh") == 0 || g_str_has_suffix (shell, "/zsh") ||
         strcmp (shell, "dash") == 0 || g_str_has_suffix (shell, "/dash") ||
#if 0
         /* tcsh supports -l and -c but not combined! To do that, you'd have
          * to instead launch the login shell like `-tcsh -c 'command'`, which
          * is possible, but we lack the abstractions for that currently.
          */
         strcmp (shell, "tcsh") == 0 || g_str_has_suffix (shell, "/tcsh") ||
#endif
         strcmp (shell, "sh") == 0 || g_str_has_suffix (shell, "/sh");
}

static void
ide_guess_shell_communicate_cb (GObject      *object,
                                GAsyncResult *result,
                                gpointer      user_data)
{
  IdeSubprocess *subprocess = (IdeSubprocess *)object;
  g_autoptr(IdeTask) task = user_data;
  g_autoptr(GError) error = NULL;
  g_autofree gchar *stdout_buf = NULL;
  const char *key;

  IDE_ENTRY;

  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (IDE_IS_SUBPROCESS (subprocess));
  g_assert (G_IS_ASYNC_RESULT (result));
  g_assert (IDE_IS_TASK (task));

  key = ide_task_get_task_data (task);

  if (!ide_subprocess_communicate_utf8_finish (subprocess, result, &stdout_buf, NULL, &error))
    {
      ide_task_return_error (task, g_steal_pointer (&error));
      IDE_EXIT;
    }

  if (stdout_buf != NULL)
    g_strstrip (stdout_buf);

  g_debug ("Guessed %s as \"%s\"", key, stdout_buf);

  if (ide_str_equal0 (key, "SHELL"))
    {
      if (stdout_buf[0] == '/')
        user_shell = g_steal_pointer (&stdout_buf);
    }
  else if (ide_str_equal0 (key, "PATH"))
    {
      if (!ide_str_empty0 (stdout_buf))
        user_default_path = g_steal_pointer (&stdout_buf);
    }
  else
    {
      g_critical ("Unknown key %s", key);
    }

  ide_task_return_boolean (task, TRUE);

  IDE_EXIT;
}

static void
_ide_guess_shell (GCancellable        *cancellable,
                  GAsyncReadyCallback  callback,
                  gpointer             user_data)
{
  g_autoptr(IdeTask) task = NULL;
  g_autoptr(IdeSubprocessLauncher) launcher = NULL;
  g_autoptr(IdeSubprocess) subprocess = NULL;
  g_autofree gchar *command = NULL;
  g_autoptr(GError) error = NULL;
  g_auto(GStrv) argv = NULL;

  IDE_ENTRY;

  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (!cancellable || G_IS_CANCELLABLE (cancellable));

  task = ide_task_new (NULL, cancellable, callback, user_data);
  ide_task_set_task_data (task, g_strdup ("SHELL"), g_free);

#ifdef __APPLE__
  command = g_strdup_printf ("sh -c 'dscacheutil -q user -a name %s | grep ^shell: | cut -f 2 -d \" \"'",
                             g_get_user_name ());
#else
  command = g_strdup_printf ("sh -c 'getent passwd %s | head -n1 | cut -f 7 -d :'",
                             g_get_user_name ());
#endif

  if (!g_shell_parse_argv (command, NULL, &argv, &error))
    {
      ide_task_return_error (task, g_steal_pointer (&error));
      IDE_EXIT;
    }

  /*
   * We don't use the runtime shell here, because we want to know
   * what the host thinks the user shell should be.
   */
  launcher = ide_subprocess_launcher_new (G_SUBPROCESS_FLAGS_STDOUT_PIPE);
  ide_subprocess_launcher_set_run_on_host (launcher, TRUE);
  ide_subprocess_launcher_set_clear_env (launcher, FALSE);
  ide_subprocess_launcher_set_cwd (launcher, g_get_home_dir ());
  ide_subprocess_launcher_push_args (launcher, (const gchar * const *)argv);

  if (!(subprocess = ide_subprocess_launcher_spawn (launcher, NULL, &error)))
    ide_task_return_error (task, g_steal_pointer (&error));
  else
    ide_subprocess_communicate_utf8_async (subprocess,
                                           NULL,
                                           cancellable,
                                           ide_guess_shell_communicate_cb,
                                           g_steal_pointer (&task));

  IDE_EXIT;
}

static void
_ide_guess_user_path (GCancellable        *cancellable,
                      GAsyncReadyCallback  callback,
                      gpointer             user_data)
{
  g_autoptr(IdeSubprocessLauncher) launcher = NULL;
  g_autoptr(IdeSubprocess) subprocess = NULL;
  g_autoptr(IdeTask) task = NULL;
  g_autoptr(GError) error = NULL;

  IDE_ENTRY;

  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (!cancellable || G_IS_CANCELLABLE (cancellable));

  task = ide_task_new (NULL, cancellable, callback, user_data);
  ide_task_set_task_data (task, g_strdup ("PATH"), g_free);

  /* This works by running 'echo $PATH' on the host, preferably
   * through the user $SHELL we discovered.
   */
  launcher = ide_subprocess_launcher_new (G_SUBPROCESS_FLAGS_STDOUT_PIPE);
  ide_subprocess_launcher_set_run_on_host (launcher, TRUE);
  ide_subprocess_launcher_set_clear_env (launcher, FALSE);
  ide_subprocess_launcher_set_cwd (launcher, g_get_home_dir ());

  if (ide_shell_supports_dash_c (user_shell))
    {
      ide_subprocess_launcher_push_argv (launcher, user_shell);
      if (ide_shell_supports_dash_login (user_shell))
        ide_subprocess_launcher_push_argv (launcher, "-l");
      ide_subprocess_launcher_push_argv (launcher, "-c");
      ide_subprocess_launcher_push_argv (launcher, "echo $PATH");
    }
  else
    {
      ide_subprocess_launcher_push_args (launcher,
                                         IDE_STRV_INIT ("/bin/sh", "-l", "-c", "echo $PATH"));
    }

  if (!(subprocess = ide_subprocess_launcher_spawn (launcher, NULL, &error)))
    ide_task_return_error (task, g_steal_pointer (&error));
  else
    ide_subprocess_communicate_utf8_async (subprocess,
                                           NULL,
                                           NULL,
                                           ide_guess_shell_communicate_cb,
                                           g_steal_pointer (&task));

  IDE_EXIT;
}

/**
 * ide_get_user_shell:
 *
 * Gets the user preferred shell on the host.
 *
 * If the background shell discovery has not yet finished due to
 * slow or misconfigured getent on the host, this will provide a
 * sensible fallback.
 *
 * Returns: (not nullable): a shell such as "/bin/sh"
 */
const gchar *
ide_get_user_shell (void)
{
  return user_shell;
}

/**
 * ide_get_user_default_path:
 *
 * Gets the default `$PATH` on the system for the user on the host.
 *
 * This value is sniffed during startup and will default to `SAFE_PATH`
 * configured when building Builder until that value has been discovered.
 *
 * Returns: (not nullable): a string such as "/bin:/usr/bin"
 */
const char *
ide_get_user_default_path (void)
{
  return user_default_path;
}

static void
ide_shell_init_guess_path_cb (GObject      *object,
                              GAsyncResult *result,
                              gpointer      user_data)
{
  g_autoptr(GError) error = NULL;

  IDE_ENTRY;

  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (object == NULL);
  g_assert (IDE_IS_TASK (result));
  g_assert (user_data == NULL);

  if (!ide_task_propagate_boolean (IDE_TASK (result), &error))
    g_warning ("Failed to guess user $PATH using $SHELL %s: %s",
               user_shell, error->message);

  IDE_EXIT;
}

static void
ide_shell_init_guess_shell_cb (GObject      *object,
                               GAsyncResult *result,
                               gpointer      user_data)
{
  g_autoptr(GError) error = NULL;

  IDE_ENTRY;

  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (object == NULL);
  g_assert (IDE_IS_TASK (result));
  g_assert (user_data == NULL);

  if (!ide_task_propagate_boolean (IDE_TASK (result), &error))
    g_warning ("Failed to guess user $SHELL: %s", error->message);

  _ide_guess_user_path (NULL,
                        ide_shell_init_guess_path_cb,
                        NULL);

  IDE_EXIT;
}

void
_ide_shell_init (void)
{
  IDE_ENTRY;

  /* First we need to guess the user shell, so that we can potentially
   * get the path using that shell (instead of just /bin/sh which might
   * not include things like .bashrc).
   */
  _ide_guess_shell (NULL,
                    ide_shell_init_guess_shell_cb,
                    NULL);

  IDE_EXIT;
}
