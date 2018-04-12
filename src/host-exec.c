/* host-exec.c
 *
 * Copyright 2018 Christian Hergert <chergert@redhat.com>
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

#include <ide.h>
#include <stdlib.h>
#include <unistd.h>

#include "threading/ide-thread-pool.h"

static gint exit_code;

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
  g_autofree gchar *argv0 = NULL;
  g_autoptr(GError) error = NULL;
  g_auto(GStrv) env = NULL;

  g_log_set_default_handler (log_func, NULL);

  main_loop = g_main_loop_new (NULL, FALSE);

  if (!(bus = g_bus_get_sync (G_BUS_TYPE_SESSION, NULL, &error)))
    g_error ("Failed to connect to session bus: %s", error->message);

  launcher = ide_subprocess_launcher_new (G_SUBPROCESS_FLAGS_STDOUT_PIPE |
                                          G_SUBPROCESS_FLAGS_STDIN_PIPE |
  	                                       G_SUBPROCESS_FLAGS_STDERR_PIPE);

  argv0 = g_path_get_basename (argv[0]);
  ide_subprocess_launcher_push_argv (launcher, argv0);
  for (guint i = 1; i < argc; i++)
    ide_subprocess_launcher_push_argv (launcher, argv[i]);

  env = g_get_environ ();

  ide_subprocess_launcher_set_cwd (launcher, g_get_current_dir ());
  ide_subprocess_launcher_set_environ (launcher, (const gchar * const *)env);
  ide_subprocess_launcher_set_run_on_host (launcher, TRUE);
  ide_subprocess_launcher_take_stdin_fd (launcher, dup (STDIN_FILENO));
  ide_subprocess_launcher_take_stdout_fd (launcher, dup (STDOUT_FILENO));
  ide_subprocess_launcher_take_stderr_fd (launcher, dup (STDERR_FILENO));

  if (!(subprocess = ide_subprocess_launcher_spawn (launcher, NULL, &error)))
    g_error ("ERROR: %s", error->message);

  ide_subprocess_wait_async (subprocess,
                             NULL,
                             (GAsyncReadyCallback)wait_cb,
                             main_loop);

  g_main_loop_run (main_loop);

  return exit_code;
}
