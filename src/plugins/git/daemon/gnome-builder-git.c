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
                                         (G_DBUS_CONNECTION_FLAGS_DELAY_MESSAGE_PROCESSING |
                                          G_DBUS_CONNECTION_FLAGS_AUTHENTICATION_CLIENT),
                                         NULL, NULL, error)))
    {
      g_dbus_connection_set_exit_on_close (ret, FALSE);
      g_signal_connect_swapped (ret, "closed", G_CALLBACK (g_main_loop_quit), main_loop);
    }

  return ret;
}

gint
main (gint argc,
      gchar *argv[])
{
  g_autoptr(GSocketConnection) stream = NULL;
  g_autoptr(GDBusConnection) connection = NULL;
  g_autoptr(IpcGitService) service = NULL;
  g_autoptr(GMainLoop) main_loop = NULL;
  g_autoptr(GSocket) socket = NULL;
  g_autoptr(GError) error = NULL;

  g_set_prgname ("gnome-builder-git");
  g_set_application_name ("gnome-builder-git");

#ifdef __linux__
  prctl (PR_SET_PDEATHSIG, SIGTERM);
#elif defined(__FreeBSD__)
  procctl (P_PID, 0, PROC_PDEATHSIG_CTL, &(int){ SIGTERM });
#else
# warning "Please submit a patch to support parent-death signal on your OS"
#endif

  signal (SIGPIPE, SIG_IGN);

  ggit_init ();

  main_loop = g_main_loop_new (NULL, FALSE);

  g_unix_set_fd_nonblocking (3, TRUE, NULL);

  if (!(socket = g_socket_new_from_fd (3, &error)))
    g_error ("Given something other than a socket: %s", error->message);

  stream = g_socket_connection_factory_create_connection (socket);

  if (!(connection = create_connection (G_IO_STREAM (stream), main_loop, &error)))
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
