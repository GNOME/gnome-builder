/* test-flatpak.c
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

static void
on_runtime_added_cb (IpcFlatpakService *service,
                     GVariant          *info)
{
  const gchar *name;
  const gchar *arch;
  const gchar *branch;
  const gchar *sdk_name;
  const gchar *sdk_branch;
  gboolean sdk_extension;

  g_assert (IPC_IS_FLATPAK_SERVICE (service));
  g_assert (info != NULL);
  g_assert (g_variant_is_of_type (info, G_VARIANT_TYPE ("(sssssb)")));

  g_variant_get (info, "(&s&s&s&s&sb)",
                 &name, &arch, &branch, &sdk_name, &sdk_branch, &sdk_extension);

  if (!sdk_extension)
    g_message ("Runtime Added: %s/%s/%s with SDK %s//%s",
               name, arch, branch, sdk_name, sdk_branch);
  else
    g_message ("SDK Extension Added: %s/%s/%s with SDK %s//%s",
               name, arch, branch, sdk_name, sdk_branch);
}

static void
add_install_cb (GObject      *object,
                GAsyncResult *result,
                gpointer      user_data)
{
  IpcFlatpakService *service = (IpcFlatpakService *)object;
  g_autofree gchar *sizestr = NULL;
  g_autoptr(GMainLoop) main_loop = user_data;
  g_autoptr(GVariant) runtimes = NULL;
  g_autoptr(GError) error = NULL;
  GVariantIter iter;
  gboolean is_known = TRUE;
  gboolean ret;
  gint64 download_size = 0;

  ret = ipc_flatpak_service_call_add_installation_finish (service, result, &error);
  g_assert_no_error (error);
  g_assert_true (ret);
  g_message ("Installation added");

  g_message ("Listing runtimes");
  ret = ipc_flatpak_service_call_list_runtimes_sync (service, &runtimes, NULL, &error);
  g_assert_no_error (error);
  g_assert_true (ret);

  if (g_variant_iter_init (&iter, runtimes))
    {
      const gchar *name;
      const gchar *arch;
      const gchar *branch;
      const gchar *sdk_name;
      const gchar *sdk_branch;
      gboolean sdk_extension;

      while (g_variant_iter_next (&iter, "(&s&s&s&s&sb)", &name, &arch, &branch, &sdk_name, &sdk_branch, &sdk_extension))
        g_message ("  %s/%s/%s with SDK %s//%s (Extension: %d)",
                   name, arch, branch, sdk_name, sdk_branch, sdk_extension);
    }

  g_message ("Checking for a missing runtime");
  ret = ipc_flatpak_service_call_runtime_is_known_sync (service, "me.hergert.FooBar/x86_64/master", &is_known, &download_size, NULL, &error);
  g_assert_no_error (error);
  g_assert_true (ret);
  g_assert_false (is_known);
  g_message ("  Not found");

  g_message ("Checking if org.gnome.Sdk/x86_64/3.24 is known");
  ret = ipc_flatpak_service_call_runtime_is_known_sync (service, "org.gnome.Sdk/x86_64/3.24", &is_known, &download_size, NULL, &error);
  g_assert_no_error (error);
  g_assert_true (ret);
  g_assert_true (is_known);
  sizestr = g_format_size (download_size);
  g_message ("  Found, Download Size: <=%s", sizestr);

  g_main_loop_quit (main_loop);
}

gint
main (gint argc,
      gchar *argv[])
{
  g_autofree gchar *home_install = g_build_filename (g_get_home_dir (), ".local", "share", "flatpak", NULL);
  g_autoptr(GError) error = NULL;
  g_autoptr(GIOStream) stream = NULL;
  g_autoptr(GInputStream) stdout_stream = NULL;
  g_autoptr(GOutputStream) stdin_stream = NULL;
  g_autoptr(GDBusConnection) connection = NULL;
  g_autoptr(GSubprocess) subprocess = NULL;
  g_autoptr(GSubprocessLauncher) launcher = NULL;
  g_autoptr(IpcFlatpakService) service = NULL;
  GMainLoop *main_loop;

  launcher = g_subprocess_launcher_new (G_SUBPROCESS_FLAGS_STDIN_PIPE | G_SUBPROCESS_FLAGS_STDOUT_PIPE);
  subprocess = g_subprocess_launcher_spawn (launcher, &error, "./gnome-builder-flatpak", NULL);

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

  g_signal_connect (service,
                    "runtime-added",
                    G_CALLBACK (on_runtime_added_cb),
                    NULL);

  g_message ("Adding user installation to daemon");
  ipc_flatpak_service_call_add_installation (service,
                                             home_install,
                                             TRUE,
                                             NULL,
                                             add_install_cb,
                                             g_main_loop_ref (main_loop));

  g_main_loop_run (main_loop);

  return EXIT_SUCCESS;
}
