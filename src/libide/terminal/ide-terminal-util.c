/* ide-terminal-util.c
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

#define G_LOG_DOMAIN "ide-terminal-util"

#include "config.h"

#include <fcntl.h>
#include <libide-io.h>
#include <libide-threading.h>
#include <stdlib.h>
#include <unistd.h>
#include <vte/vte.h>

#include "ide-terminal-private.h"
#include "ide-terminal-util.h"

static const gchar *user_shell = "/bin/sh";

gint
ide_vte_pty_create_slave (VtePty *pty)
{
  gint master_fd;

  g_return_val_if_fail (VTE_IS_PTY (pty), IDE_PTY_FD_INVALID);

  master_fd = vte_pty_get_fd (pty);
  if (master_fd == IDE_PTY_FD_INVALID)
    return IDE_PTY_FD_INVALID;

  return ide_pty_intercept_create_slave (master_fd, TRUE);
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
 *
 * Since: 3.32
 */
const gchar *
ide_get_user_shell (void)
{
  return user_shell;
}

static void
ide_guess_shell_communicate_cb (GObject      *object,
                                GAsyncResult *result,
                                gpointer      user_data)
{
  IdeSubprocess *subprocess = (IdeSubprocess *)object;
  g_autoptr(GError) error = NULL;
  g_autofree gchar *stdout_buf = NULL;

  g_assert (IDE_IS_SUBPROCESS (subprocess));
  g_assert (G_IS_ASYNC_RESULT (result));
  g_assert (user_data == NULL);

  if (!ide_subprocess_communicate_utf8_finish (subprocess, result, &stdout_buf, NULL, &error))
    {
      g_warning ("Failed to parse result from getent: %s", error->message);
      return;
    }

  if (stdout_buf != NULL)
    {
      g_strstrip (stdout_buf);

      if (stdout_buf[0] == '/')
        user_shell = g_steal_pointer (&stdout_buf);
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
  g_autofree gchar *shell = NULL;

  /*
   * First ask VTE to guess, so we can use that while we discover
   * the real shell asynchronously (and possibly outside the container).
   */
  if ((shell = vte_get_user_shell ()))
    user_shell = g_strdup (shell);

  command = g_strdup_printf ("sh -c 'getent passwd %s | head -n1 | cut -f 7 -d :'",
                             g_get_user_name ());

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
                                         NULL);
}
