/* test-git.c
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
#include <stdlib.h>
#include <signal.h>
#include <unistd.h>

#include "ipc-git-change-monitor.h"
#include "ipc-git-config.h"
#include "ipc-git-progress.h"
#include "ipc-git-repository.h"
#include "ipc-git-service.h"
#include "ipc-git-types.h"

#define PROGRESS_PATH "/org/gnome/Builder/Git/Progress/1"

static GMainLoop *main_loop;

static void
notify_fraction_cb (IpcGitProgress *progress)
{
  g_message ("Fraction = %lf", ipc_git_progress_get_fraction (progress));
}

static void
notify_message_cb (IpcGitProgress *progress)
{
  g_message ("Message = %s", ipc_git_progress_get_message (progress));
}

static void
test_clone_cb (GObject      *object,
               GAsyncResult *result,
               gpointer      user_data)
{
  g_autoptr(IpcGitProgress) progress = user_data;
  g_autofree char *location = NULL;
  g_autoptr(GError) error = NULL;

  if (!ipc_git_service_call_clone_finish (IPC_GIT_SERVICE (object), &location, NULL, result, &error))
    g_error ("Error cloning: %s", error->message);
  else
    g_printerr ("Cloning complete: %s\n", location);

  g_main_loop_quit (main_loop);
}

static void
test_clone (IpcGitService *service,
            const char    *url,
            const char    *path)
{
  g_autoptr(IpcGitProgress) progress = NULL;
  g_autoptr(IpcGitRepository) repository = NULL;
  g_autoptr(IpcGitChangeMonitor) monitor = NULL;
  g_autoptr(IpcGitConfig) config = NULL;
  g_autoptr(GUnixFDList) fd_list = NULL;
  g_autoptr(GError) error = NULL;
  g_autoptr(GVariant) files = NULL;
  g_autofree gchar *location = NULL;
  GVariantDict opts;
  GDBusConnection *conn;
  int fd;

  g_assert (IPC_IS_GIT_SERVICE (service));

  g_variant_dict_init (&opts, NULL);

  g_message ("Creating local progress object");
  conn = g_dbus_proxy_get_connection (G_DBUS_PROXY (service));
  progress = ipc_git_progress_skeleton_new ();
  g_signal_connect (progress, "notify::fraction", G_CALLBACK (notify_fraction_cb), NULL);
  g_signal_connect (progress, "notify::message", G_CALLBACK (notify_message_cb), NULL);
  g_dbus_interface_skeleton_export (G_DBUS_INTERFACE_SKELETON (progress), conn, PROGRESS_PATH, &error);
  g_assert_no_error (error);

  fd = open ("test-output.log", O_RDWR, 0666);
  fd_list = g_unix_fd_list_new ();
  g_unix_fd_list_append (fd_list, fd, NULL);
  close (fd);

  ipc_git_service_call_clone (service, url, path, "", g_variant_dict_end (&opts), PROGRESS_PATH,
                              g_variant_new_handle (0), fd_list,
                              NULL, test_clone_cb, g_object_ref (progress));
}

gint
main (gint argc,
      gchar *argv[])
{
  g_autoptr(GError) error = NULL;
  g_autoptr(GIOStream) stream = NULL;
  g_autoptr(GInputStream) stdout_stream = NULL;
  g_autoptr(GOutputStream) stdin_stream = NULL;
  g_autoptr(GDBusConnection) connection = NULL;
  g_autoptr(GSubprocess) subprocess = NULL;
  g_autoptr(GSubprocessLauncher) launcher = NULL;
  g_autoptr(IpcGitService) service = NULL;

  if (argc < 3)
    {
      g_printerr ("usage: %s URL PATH\n", argv[0]);
      return EXIT_FAILURE;
    }

  launcher = g_subprocess_launcher_new (G_SUBPROCESS_FLAGS_STDIN_PIPE | G_SUBPROCESS_FLAGS_STDOUT_PIPE);
  subprocess = g_subprocess_launcher_spawn (launcher, &error, "./gnome-builder-git", NULL);

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

  service = ipc_git_service_proxy_new_sync (connection, 0, NULL, "/org/gnome/Builder/Git", NULL, &error);
  g_assert_no_error (error);
  g_assert_true (IPC_IS_GIT_SERVICE (service));

  g_dbus_proxy_set_default_timeout (G_DBUS_PROXY (service), G_MAXINT);

  test_clone (service, argv[1], argv[2]);

  g_main_loop_run (main_loop);

  while (g_main_context_pending (NULL))
    g_main_context_iteration (NULL, FALSE);

  return EXIT_SUCCESS;
}
