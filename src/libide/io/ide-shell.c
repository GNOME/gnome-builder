/* ide-shell.c
 *
 * Copyright 2021 Christian Hergert <chergert@redhat.com>
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
         strcmp (shell, "fish") == 0 || g_str_has_suffix (shell, "/fish") ||
         strcmp (shell, "zsh") == 0 || g_str_has_suffix (shell, "/zsh") ||
         strcmp (shell, "sh") == 0 || g_str_has_suffix (shell, "/sh");
}

gboolean
ide_shell_supports_dash_login (const char *shell)
{
  if (shell == NULL)
    return FALSE;

  return strcmp (shell, "bash") == 0 || g_str_has_suffix (shell, "/bash") ||
         strcmp (shell, "fish") == 0 || g_str_has_suffix (shell, "/fish") ||
         strcmp (shell, "zsh") == 0 || g_str_has_suffix (shell, "/zsh") ||
         strcmp (shell, "sh") == 0 || g_str_has_suffix (shell, "/sh");
}

static void
ide_guess_shell_communicate_cb (GObject      *object,
                                GAsyncResult *result,
                                gpointer      user_data)
{
  IdeSubprocess *subprocess = (IdeSubprocess *)object;
  g_autoptr(GError) error = NULL;
  g_autofree gchar *stdout_buf = NULL;
  const char *key = user_data;

  g_assert (IDE_IS_SUBPROCESS (subprocess));
  g_assert (G_IS_ASYNC_RESULT (result));
  g_assert (key != NULL);

  if (!ide_subprocess_communicate_utf8_finish (subprocess, result, &stdout_buf, NULL, &error))
    {
      g_warning ("Failure to parse host information: %s", error->message);
      return;
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
}

void
_ide_guess_shell (void)
{
  g_autoptr(IdeSubprocessLauncher) launcher = NULL;
  g_autoptr(IdeSubprocess) subprocess = NULL;
  g_autofree gchar *command = NULL;
  g_autoptr(GError) error = NULL;
  g_auto(GStrv) argv = NULL;

#ifdef __APPLE__
  command = g_strdup_printf ("sh -c 'dscacheutil -q user -a name %s | grep ^shell: | cut -f 2 -d \" \"'",
                             g_get_user_name ());
#else
  command = g_strdup_printf ("sh -c 'getent passwd %s | head -n1 | cut -f 7 -d :'",
                             g_get_user_name ());
#endif

  if (!g_shell_parse_argv (command, NULL, &argv, &error))
    {
      g_warning ("Failed to parse command into argv: %s",
                 error ? error->message : "unknown error");
      return;
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
    {
      g_warning ("Failed to spawn getent: %s", error->message);
      return;
    }

  ide_subprocess_communicate_utf8_async (subprocess,
                                         NULL,
                                         NULL,
                                         ide_guess_shell_communicate_cb,
                                         (gpointer)"SHELL");
}

void
_ide_guess_user_path (void)
{
  g_autoptr(IdeSubprocessLauncher) launcher = NULL;
  g_autoptr(IdeSubprocess) subprocess = NULL;
  g_autofree gchar *command = NULL;
  g_autoptr(GError) error = NULL;
  g_auto(GStrv) argv = NULL;

  command = g_strdup_printf ("sh --login -c 'echo $PATH'");

  if (!g_shell_parse_argv (command, NULL, &argv, &error))
    {
      g_warning ("Failed to parse command into argv: %s",
                 error ? error->message : "unknown error");
      return;
    }

  launcher = ide_subprocess_launcher_new (G_SUBPROCESS_FLAGS_STDOUT_PIPE);
  ide_subprocess_launcher_set_run_on_host (launcher, TRUE);
  ide_subprocess_launcher_set_clear_env (launcher, FALSE);
  ide_subprocess_launcher_set_cwd (launcher, g_get_home_dir ());
  ide_subprocess_launcher_push_args (launcher, (const gchar * const *)argv);

  if (!(subprocess = ide_subprocess_launcher_spawn (launcher, NULL, &error)))
    {
      g_warning ("Failed to spawn getent: %s", error->message);
      return;
    }

  ide_subprocess_communicate_utf8_async (subprocess,
                                         NULL,
                                         NULL,
                                         ide_guess_shell_communicate_cb,
                                         (gpointer)"PATH");
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
