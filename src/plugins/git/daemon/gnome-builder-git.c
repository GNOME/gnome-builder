/* gnome-builder-git.c
 *
 * Copyright 2019 Christian Hergert <chergert@redhat.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include <glib-unix.h>
#include <gio/gunixinputstream.h>
#include <gio/gunixoutputstream.h>
#include <libgit2-glib/ggit.h>
#include <stdlib.h>
#include <signal.h>
#include <unistd.h>

#ifdef __linux__
#include <sys/prctl.h>
#endif

#ifdef __FreeBSD__
#include <sys/procctl.h>
#endif

#include "ipc-git-service.h"
#include "ipc-git-service-impl.h"

static GDBusConnection *
create_connection (GIOStream  *stream,
                   GMainLoop  *main_loop,
                   GError    **error)
{
  GDBusConnection *ret;

  g_assert (G_IS_IO_STREAM (stream));
  g_assert (main_loop != NULL);
  g_assert (error != NULL);

  if ((ret = g_dbus_connection_new_sync (stream, NULL,
                                          G_DBUS_CONNECTION_FLAGS_DELAY_MESSAGE_PROCESSING,
                                          NULL, NULL, error)))
    {
      g_dbus_connection_set_exit_on_close (ret, FALSE);
      g_signal_connect_swapped (ret, "closed", G_CALLBACK (g_main_loop_quit), main_loop);
    }

  return ret;
}

static void
log_func (const gchar    *log_domain,
          GLogLevelFlags  flags,
          const gchar    *message,
          gpointer        user_data)
{
  g_printerr ("gnome-builder-git: %s\n", message);
}

gint
main (gint argc,
      gchar *argv[])
{
  g_autoptr(GDBusConnection) connection = NULL;
  g_autoptr(IpcGitService) service = NULL;
  g_autoptr(GOutputStream) stdout_stream = NULL;
  g_autoptr(GInputStream) stdin_stream = NULL;
  g_autoptr(GIOStream) stream = NULL;
  g_autoptr(GMainLoop) main_loop = NULL;
  g_autoptr(GError) error = NULL;

  g_set_prgname ("gnome-builder-git");
  g_set_application_name ("gnome-builder-git");

#ifdef __linux__
  prctl (PR_SET_PDEATHSIG, SIGTERM);
#elif defined(__FreeBSD__)
  procctl (P_PID, 0, PROC_PDEATHSIG_CTL, &(int){ SIGTERM });
#else
#error "Please submit a patch to support parent-death signal on your OS"
#endif

  signal (SIGPIPE, SIG_IGN);

  ggit_init ();

  g_log_set_handler (NULL, G_LOG_LEVEL_MASK, log_func, NULL);

  if (!g_unix_set_fd_nonblocking (STDIN_FILENO, TRUE, &error) ||
      !g_unix_set_fd_nonblocking (STDOUT_FILENO, TRUE, &error))
    goto error;

  main_loop = g_main_loop_new (NULL, FALSE);
  stdin_stream = g_unix_input_stream_new (STDIN_FILENO, FALSE);
  stdout_stream = g_unix_output_stream_new (STDOUT_FILENO, FALSE);
  stream = g_simple_io_stream_new (stdin_stream, stdout_stream);

  if (!(connection = create_connection (stream, main_loop, &error)))
    goto error;

  service = ipc_git_service_impl_new ();

  if (!g_dbus_interface_skeleton_export (G_DBUS_INTERFACE_SKELETON (service),
                                         connection,
                                         "/org/gnome/Builder/Git",
                                         &error))
    goto error;

  g_dbus_connection_start_message_processing (connection);
  g_main_loop_run (main_loop);

  return EXIT_SUCCESS;

error:
  if (error != NULL)
    g_printerr ("%s\n", error->message);

  return EXIT_FAILURE;
}
