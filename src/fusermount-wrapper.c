/* fusermount-wrapper.c
 *
 * Copyright 2018-2019 Christian Hergert <chergert@redhat.com>
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

#include "config.h"

#include <errno.h>
#include <libide-threading.h>
#include <stdlib.h>
#include <unistd.h>

static gint exit_code;

static gboolean
parse_fd (const gchar *str,
          gint        *fd)
{
  gint64 v = g_ascii_strtoll (str, NULL, 10);

  *fd = -1;

  if (v < 0 || v > G_MAXINT)
    return FALSE;

  if (v == 0 && errno == EINVAL)
    return FALSE;

  *fd = v;

  return TRUE;
}

static void
wait_cb (IdeSubprocess *subprocess,
         GAsyncResult  *result,
         GMainLoop     *main_loop)
{
  g_autoptr(GError) error = NULL;

  g_assert (IDE_IS_SUBPROCESS (subprocess));
  g_assert (G_IS_ASYNC_RESULT (result));

  if (!ide_subprocess_wait_finish (subprocess, result, &error))
    g_error ("Subprocess wait failed: %s", error->message);

  if (ide_subprocess_get_if_signaled (subprocess))
    kill (getpid (), ide_subprocess_get_term_sig (subprocess));
  else
    exit (ide_subprocess_get_exit_status (subprocess));

  g_main_loop_quit (main_loop);
}

static void
log_func (const gchar    *log_domain,
          GLogLevelFlags  log_level,
          const gchar    *message,
          gpointer        user_data)
{
  if (log_level & G_LOG_FLAG_FATAL)
    g_printerr ("%s\n", message);
}

int
main (int   argc,
      char *argv[])
{
  g_autoptr(IdeSubprocessLauncher) launcher = NULL;
  g_autoptr(GDBusConnection) bus = NULL;
  g_autoptr(IdeSubprocess) subprocess = NULL;
  g_autoptr(GMainLoop) main_loop = NULL;
  g_autoptr(GError) error = NULL;
  const gchar *env;
  gint fd = -1;

  g_log_set_default_handler (log_func, NULL);

  main_loop = g_main_loop_new (NULL, FALSE);

  if (!(bus = g_bus_get_sync (G_BUS_TYPE_SESSION, NULL, &error)))
    g_error ("Failed to connect to session bus: %s", error->message);

  launcher = ide_subprocess_launcher_new (0);

  ide_subprocess_launcher_push_argv (launcher, "fusermount");
  for (guint i = 1; i < argc; i++)
    ide_subprocess_launcher_push_argv (launcher, argv[i]);

  ide_subprocess_launcher_set_cwd (launcher, g_get_current_dir ());
  ide_subprocess_launcher_set_run_on_host (launcher, TRUE);
  ide_subprocess_launcher_take_stdin_fd (launcher, dup (STDIN_FILENO));
  ide_subprocess_launcher_take_stdout_fd (launcher, dup (STDOUT_FILENO));
  ide_subprocess_launcher_take_stderr_fd (launcher, dup (STDERR_FILENO));

  if ((env = g_getenv ("_FUSE_COMMFD")) && parse_fd (env, &fd) && fd > 2)
    {
      ide_subprocess_launcher_setenv (launcher, "_FUSE_COMMFD", env, TRUE);
      ide_subprocess_launcher_take_fd (launcher, fd, fd);
    }

  if (!(subprocess = ide_subprocess_launcher_spawn (launcher, NULL, &error)))
    g_error ("ERROR: %s", error->message);

  ide_subprocess_wait_async (subprocess,
                             NULL,
                             (GAsyncReadyCallback)wait_cb,
                             main_loop);

  g_main_loop_run (main_loop);

  return exit_code;
}
