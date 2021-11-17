/* test-install.c
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
#include <stdlib.h>
#include <signal.h>
#include <unistd.h>

#include "ipc-flatpak-service.h"
#include "ipc-flatpak-transfer.h"

static gboolean
handle_confirm (IpcFlatpakTransfer    *transfer,
                GDBusMethodInvocation *invocation,
                const char * const    *refs)
{
  ipc_flatpak_transfer_complete_confirm (transfer, invocation);
  return TRUE;
}

static void
install_cb (GObject      *object,
            GAsyncResult *result,
            gpointer      user_data)
{
  IpcFlatpakService *service = (IpcFlatpakService *)object;
  g_autoptr(GMainLoop) main_loop = user_data;
  g_autoptr(GError) error = NULL;
  gboolean ret;

  ret = ipc_flatpak_service_call_install_finish (service, result, &error);
  g_assert_no_error (error);
  g_assert_true (ret);
  g_message ("Installed.");

  g_main_loop_quit (main_loop);
}

static void
print_info (IpcFlatpakTransfer *transfer,
            GParamSpec         *pspec,
            gpointer            user_data)
{
  g_print ("%s: %lf\n",
           ipc_flatpak_transfer_get_message (transfer) ?: "",
           ipc_flatpak_transfer_get_fraction (transfer));
}

gint
main (gint argc,
      gchar *argv[])
{
  static const char *transfer_path = "/org/gnome/Builder/Flatpak/Transfer/0";
  g_autoptr(GError) error = NULL;
  g_autoptr(GIOStream) stream = NULL;
  g_autoptr(GInputStream) stdout_stream = NULL;
  g_autoptr(GOutputStream) stdin_stream = NULL;
  g_autoptr(GDBusConnection) connection = NULL;
  g_autoptr(GSubprocess) subprocess = NULL;
  g_autoptr(GSubprocessLauncher) launcher = NULL;
  g_autoptr(IpcFlatpakService) service = NULL;
  g_autoptr(IpcFlatpakTransfer) transfer = NULL;
  g_autoptr(GPtrArray) all = g_ptr_array_new ();
  GMainLoop *main_loop;
  gboolean ret;

  if (argc < 2)
    {
      g_printerr ("usage: %s REF [REF..]\n", argv[0]);
      return EXIT_FAILURE;
    }

  for (guint i = 1; i < argc; i++)
    g_ptr_array_add (all, argv[i]);
  g_ptr_array_add (all, NULL);

  launcher = g_subprocess_launcher_new (G_SUBPROCESS_FLAGS_STDIN_PIPE | G_SUBPROCESS_FLAGS_STDOUT_PIPE);
  subprocess = g_subprocess_launcher_spawn (launcher, &error,
#if 0
                                            "valgrind", "--quiet",
#endif
                                            "./gnome-builder-flatpak", NULL);

  if (subprocess == NULL)
    g_error ("%s", error->message);

  main_loop = g_main_loop_new (NULL, FALSE);
  stdin_stream = g_subprocess_get_stdin_pipe (subprocess);
  stdout_stream = g_subprocess_get_stdout_pipe (subprocess);
  stream = g_simple_io_stream_new (stdout_stream, stdin_stream);
  connection = g_dbus_connection_new_sync (stream,
                                           NULL,
                                           G_DBUS_CONNECTION_FLAGS_DELAY_MESSAGE_PROCESSING,
                                           NULL,
                                           NULL,
                                           &error);
  g_assert_no_error (error);
  g_assert_true (G_IS_DBUS_CONNECTION (connection));

  g_dbus_connection_set_exit_on_close (connection, FALSE);
  g_dbus_connection_start_message_processing (connection);

  g_message ("Creating flatpak service proxy");
  service = ipc_flatpak_service_proxy_new_sync (connection, 0, NULL, "/org/gnome/Builder/Flatpak", NULL, &error);
  g_assert_no_error (error);
  g_assert_true (IPC_IS_FLATPAK_SERVICE (service));

  g_dbus_proxy_set_default_timeout (G_DBUS_PROXY (service), G_MAXINT);

  transfer = ipc_flatpak_transfer_skeleton_new ();
  g_signal_connect (transfer, "handle-confirm", G_CALLBACK (handle_confirm), NULL);
  g_signal_connect (transfer, "notify::message", G_CALLBACK (print_info), NULL);
  g_signal_connect (transfer, "notify::fraction", G_CALLBACK (print_info), NULL);
  ret = g_dbus_interface_skeleton_export (G_DBUS_INTERFACE_SKELETON (transfer), connection, transfer_path, &error);
  g_assert_no_error (error);
  g_assert_true (ret);

  g_message ("Installing %s", argv[1]);
  ipc_flatpak_service_call_install (service,
                                    (const char * const *)all->pdata,
                                    FALSE,
                                    transfer_path,
                                    "",
                                    NULL,
                                    install_cb,
                                    g_main_loop_ref (main_loop));

  g_main_loop_run (main_loop);

  return EXIT_SUCCESS;
}
