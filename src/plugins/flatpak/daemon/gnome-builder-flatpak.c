/* gnome-builder-flatpak.c
 *
 * Copyright 2019 Christian Hergert <chergert@redhat.com>
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

#include <glib-unix.h>
#include <gio/gunixinputstream.h>
#include <gio/gunixoutputstream.h>
#include <flatpak/flatpak.h>
#include <stdlib.h>
#include <signal.h>
#include <sys/prctl.h>
#include <unistd.h>

#include "ipc-flatpak-service.h"
#include "ipc-flatpak-service-impl.h"

static void
log_func (const gchar    *log_domain,
          GLogLevelFlags  flags,
          const gchar    *message,
          gpointer        user_data)
{
  g_printerr ("gnome-builder-flatpak: %s\n", message);
}

gint
main (gint argc,
      gchar *argv[])
{
  g_autoptr(GDBusConnection) connection = NULL;
  g_autoptr(IpcFlatpakService) service = NULL;
  g_autoptr(GOutputStream) stdout_stream = NULL;
  g_autoptr(GInputStream) stdin_stream = NULL;
  g_autoptr(GIOStream) stream = NULL;
  g_autoptr(GMainLoop) main_loop = NULL;
  g_autoptr(GError) error = NULL;

  g_set_prgname ("gnome-builder-flatpak");
  g_set_application_name ("gnome-builder-flatpak");

  prctl (PR_SET_PDEATHSIG, SIGTERM);

  signal (SIGPIPE, SIG_IGN);

  g_log_set_handler (NULL, G_LOG_LEVEL_MASK, log_func, NULL);

  if (!g_unix_set_fd_nonblocking (STDIN_FILENO, TRUE, &error) ||
      !g_unix_set_fd_nonblocking (STDOUT_FILENO, TRUE, &error))
    goto error;

  main_loop = g_main_loop_new (NULL, FALSE);
  stdin_stream = g_unix_input_stream_new (STDIN_FILENO, FALSE);
  stdout_stream = g_unix_output_stream_new (STDOUT_FILENO, FALSE);
  stream = g_simple_io_stream_new (stdin_stream, stdout_stream);

  connection = g_dbus_connection_new_sync (stream, NULL,
                                           G_DBUS_CONNECTION_FLAGS_DELAY_MESSAGE_PROCESSING,
                                           NULL, NULL, &error);
  if (connection == NULL)
    goto error;

  g_dbus_connection_set_exit_on_close (connection, FALSE);
  g_signal_connect_swapped (connection, "closed", G_CALLBACK (g_main_loop_quit), main_loop);

  service = ipc_flatpak_service_impl_new ();

  if (!g_dbus_interface_skeleton_export (G_DBUS_INTERFACE_SKELETON (service),
                                         connection,
                                         "/org/gnome/Builder/Flatpak",
                                         &error))
    goto error;

  g_dbus_connection_start_message_processing (connection);

  g_main_loop_run (main_loop);

  g_dbus_interface_skeleton_unexport (G_DBUS_INTERFACE_SKELETON (service));
  g_dbus_connection_close_sync (connection, NULL, NULL);

  return EXIT_SUCCESS;

error:
  if (error != NULL)
    g_error ("%s", error->message);

  return EXIT_FAILURE;
}
