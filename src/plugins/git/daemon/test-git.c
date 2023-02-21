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
static gchar tmpdir[] = { "test-git-XXXXXX" };
static gchar tmpdir_push[] = { "test-git-bare-XXXXXX" };

static void
cleanup_dir (void)
{
  g_autofree gchar *command = NULL;
  command = g_strdup_printf ("rm -rf '%s' '%s'", tmpdir, tmpdir_push);
  g_message ("%s", command);
  if (system (command) != 0)
    g_warning ("Failed to execute command: %s", command);
}

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
do_test_config (IpcGitConfig *config)
{
  g_autoptr(GError) error = NULL;
  /* we need all of these to run test-git succesfully (for gpg) */
  static const gchar *keys[] = { "user.name", "user.email", "user.signingkey" };
  gboolean ret;

  g_assert (IPC_IS_GIT_CONFIG (config));

  g_message ("Checking for keys required by test-git");

  for (guint i = 0; i < G_N_ELEMENTS (keys); i++)
    {
      g_autofree gchar *value = NULL;

      g_message ("  Looking up key: %s", keys[i]);
      ret = ipc_git_config_call_read_key_sync (config, keys[i], &value, NULL, &error);

      if (error && strcmp (keys[i], "user.signingkey") == 0)
        g_error ("This test requires that you set user.signingkey for the user account");

      g_assert_no_error (error);
      g_assert_true (ret);

      g_message ("  %s = %s", keys[i], value);
    }

  g_message ("Closing config");
  ret = ipc_git_config_call_close_sync (config, NULL, &error);
  g_assert_no_error (error);
  g_assert_true (ret);
}

static void
test_config (IpcGitService *service)
{
  g_autofree gchar *config_path = NULL;
  g_autoptr(IpcGitConfig) config = NULL;
  g_autoptr(GError) error = NULL;
  /* we need all of these to run test-git succesfully (for gpg) */
  GDBusConnection *conn;
  gboolean ret;

  g_assert (IPC_IS_GIT_SERVICE (service));

  g_message ("Creating global config");
  ret = ipc_git_service_call_load_config_sync (service, &config_path, NULL, &error);
  g_assert_no_error (error);
  g_assert_true (ret);

  g_message ("Config created at %s", config_path);
  conn = g_dbus_proxy_get_connection (G_DBUS_PROXY (service));
  config = ipc_git_config_proxy_new_sync (conn, 0, NULL, config_path, NULL, &error);
  g_assert_no_error (error);
  g_assert (IPC_IS_GIT_CONFIG (config));

  do_test_config (config);
}

static void
test_push (IpcGitService    *service,
           IpcGitRepository *repository)
{
  g_autofree gchar *location = NULL;
  g_autofree gchar *url = NULL;
  g_autofree gchar *dir = NULL;
  g_autoptr(GError) error = NULL;
  static const gchar *ref_names[] = { "refs/heads/master:refs/heads/master", NULL };
  IpcGitPushFlags flags = IPC_GIT_PUSH_FLAGS_NONE;
  gboolean ret;

  g_message ("Creating bare repository for push");
  ret = ipc_git_service_call_create_sync (service, tmpdir_push, TRUE, &location, NULL, &error);
  g_assert_no_error (error);
  g_assert_true (ret);
  g_message ("Bare repository created at %s", location);

  dir = g_get_current_dir ();
  url = g_strdup_printf ("file://%s/%s", dir, tmpdir_push);

  g_message ("Pushing to %s", url);
  ret = ipc_git_repository_call_push_sync (repository, url, ref_names, flags, PROGRESS_PATH, NULL, &error);
  g_assert_no_error (error);
  g_assert_true (ret);
  g_message ("  Pushed");
}

static GVariant *
create_commit_details (const gchar *commit_msg)
{
  GVariantDict dict;

  g_variant_dict_init (&dict, NULL);
  g_variant_dict_insert (&dict, "AUTHOR_NAME", "s", "Me Myself");
  g_variant_dict_insert (&dict, "AUTHOR_EMAIL", "s", "me@localhost");
  g_variant_dict_insert (&dict, "COMMITTER_NAME", "s", "Me Myself");
  g_variant_dict_insert (&dict, "COMMITTER_EMAIL", "s", "me@localhost");
  g_variant_dict_insert (&dict, "COMMIT_MSG", "s", commit_msg ?: "");

  return g_variant_dict_end (&dict);
}

static void
test_clone (IpcGitService *service)
{
  g_autoptr(IpcGitProgress) progress = NULL;
  g_autoptr(IpcGitRepository) repository = NULL;
  g_autoptr(IpcGitChangeMonitor) monitor = NULL;
  g_autoptr(IpcGitConfig) config = NULL;
  g_autoptr(GError) error = NULL;
  g_autoptr(GVariant) files = NULL;
  g_autofree gchar *testfile = NULL;
  g_autofree gchar *location = NULL;
  g_autofree gchar *obj_path = NULL;
  g_autofree gchar *config_path = NULL;
  g_auto(GStrv) tags = NULL;
  g_auto(GStrv) branches = NULL;
  g_autoptr(GVariant) changes = NULL;
  g_autoptr(GUnixFDList) fd_list = NULL;
  g_autofree gchar *monitor_path = NULL;
  GVariantDict opts;
  GVariant *details;
  GDBusConnection *conn;
  GVariantIter iter;
  gboolean ret;
  int fd;

  g_assert (IPC_IS_GIT_SERVICE (service));

  g_variant_dict_init (&opts, NULL);
  g_variant_dict_insert (&opts, "user.name", "s", "Test User");
  g_variant_dict_insert (&opts, "user.email", "s", "Test Email");

  g_message ("Creating local progress object");
  conn = g_dbus_proxy_get_connection (G_DBUS_PROXY (service));
  progress = ipc_git_progress_skeleton_new ();
  g_signal_connect (progress, "notify::fraction", G_CALLBACK (notify_fraction_cb), NULL);
  g_signal_connect (progress, "notify::message", G_CALLBACK (notify_message_cb), NULL);
  g_dbus_interface_skeleton_export (G_DBUS_INTERFACE_SKELETON (progress),
                                    conn,
                                    PROGRESS_PATH,
                                    &error);
  g_assert_no_error (error);

  fd = open ("test-output.txt", O_RDWR, 0666);
  fd_list = g_unix_fd_list_new ();
  g_unix_fd_list_append (fd_list, fd, NULL);
  close (fd);

  g_message ("Cloning hello");
  ret = ipc_git_service_call_clone_sync (service,
                                         "https://gitlab.gnome.org/chergert/hello.git",
                                         tmpdir,
                                         "master",
                                         g_variant_dict_end (&opts),
                                         PROGRESS_PATH,
                                         g_variant_new_handle (0),
                                         fd_list,
                                         &location,
                                         NULL,
                                         NULL,
                                         &error);
  g_assert_no_error (error);
  g_assert (ret);

  g_message ("Cloned to %s", location);

  ret = ipc_git_service_call_open_sync (service, location, &obj_path, NULL, &error);
  g_assert_no_error (error);
  g_assert (ret);

  repository = ipc_git_repository_proxy_new_sync (conn, 0, NULL, obj_path, NULL, &error);
  g_assert_no_error (error);
  g_assert_true (IPC_IS_GIT_REPOSITORY (repository));

  g_message ("Initializing submodules");
  ret = ipc_git_repository_call_update_submodules_sync (repository, TRUE, PROGRESS_PATH, NULL, &error);
  g_assert_no_error (error);
  g_assert_true (ret);

  g_message ("Creating repository config");
  ret = ipc_git_repository_call_load_config_sync (repository, &config_path, NULL, &error);
  g_assert_no_error (error);
  g_assert_true (ret);

  g_message ("Repository config created at %s", config_path);
  conn = g_dbus_proxy_get_connection (G_DBUS_PROXY (service));
  config = ipc_git_config_proxy_new_sync (conn, 0, NULL, config_path, NULL, &error);
  g_assert_no_error (error);
  g_assert (IPC_IS_GIT_CONFIG (config));

  do_test_config (config);

  ret = ipc_git_repository_call_list_refs_by_kind_sync (repository, IPC_GIT_REF_BRANCH, &branches, NULL, &error);
  g_assert_no_error (error);
  g_assert_true (ret);

  g_message ("Branches:");
  for (guint i = 0; branches[i]; i++)
    g_message ("  %s", branches[i]);

  ret = ipc_git_repository_call_list_refs_by_kind_sync (repository, IPC_GIT_REF_TAG, &tags, NULL, &error);
  g_assert_no_error (error);
  g_assert_true (ret);

  g_message ("Tags:");
  for (guint i = 0; tags[i]; i++)
    g_message ("  %s", tags[i]);

  g_message ("Switching to branch %s", branches[0]);
  ret = ipc_git_repository_call_switch_branch_sync (repository, branches[0], NULL, &error);
  g_assert_no_error (error);
  g_assert_true (ret);

  testfile = g_build_filename (tmpdir, "foobar", NULL);
  g_message ("Creating empty file in tree '%s'", testfile);
  ret = g_file_set_contents (testfile, "test", 4, &error);
  g_assert_no_error (error);
  g_assert_true (ret);

  g_message ("Listing status");
  ret = ipc_git_repository_call_list_status_sync (repository, "", &files, NULL, &error);
  g_assert_no_error (error);
  g_assert_true (ret);

  if (g_variant_iter_init (&iter, files) > 0)
    {
      const gchar *path;
      guint32 state;

      while (g_variant_iter_next (&iter, "(&su)", &path, &state))
        g_message ("  %s: %u", path, state);
    }

  g_message ("Staging foobar");
  ret = ipc_git_repository_call_stage_file_sync (repository, "foobar", NULL, &error);
  g_assert_no_error (error);
  g_assert_true (ret);

  g_message ("Committing to local repository");
  details = create_commit_details ("My commit message");
  ret = ipc_git_repository_call_commit_sync (repository, details, 0, NULL, &error);
  g_assert_no_error (error);
  g_assert_true (ret);

  g_file_set_contents (testfile, "test test", 9, &error);
  g_assert_no_error (error);
  g_message ("Staging foobar");
  ret = ipc_git_repository_call_stage_file_sync (repository, "foobar", NULL, &error);
  g_assert_no_error (error);
  g_assert_true (ret);

  g_message ("Committing with gpg sign");
  details = create_commit_details ("My signed message");
  ret = ipc_git_repository_call_commit_sync (repository, details, IPC_GIT_COMMIT_FLAGS_GPG_SIGN, NULL, &error);
  g_assert_no_error (error);
  g_assert_true (ret);

  g_message ("Amending previous commit");
  details = create_commit_details ("My amended commit message");
  ret = ipc_git_repository_call_commit_sync (repository, details,
                                             IPC_GIT_COMMIT_FLAGS_AMEND |
                                             IPC_GIT_COMMIT_FLAGS_SIGNOFF,
                                             NULL, &error);
  g_assert_no_error (error);
  g_assert_true (ret);

  g_message ("Creating change monitor");
  ret = ipc_git_repository_call_create_change_monitor_sync (repository, "foobar", &monitor_path, NULL, &error);
  g_assert_no_error (error);
  g_assert_true (ret);

  g_message ("  Created at path %s", monitor_path);
  monitor = ipc_git_change_monitor_proxy_new_sync (conn, 0, NULL, monitor_path, NULL, &error);
  g_assert_no_error (error);
  g_assert_true (IPC_IS_GIT_CHANGE_MONITOR (monitor));

  g_message ("  Updating file contents");
  ret = ipc_git_change_monitor_call_update_content_sync (monitor, "this\nis\nsome\ntext\nhere", NULL, &error);
  g_assert_no_error (error);
  g_assert_true (ret);

  g_message ("  Listing file changes");
  ret = ipc_git_change_monitor_call_list_changes_sync (monitor, &changes, NULL, &error);
  g_assert_no_error (error);
  g_assert_true (ret);
  g_assert_nonnull (changes);
  g_assert_true (g_variant_is_of_type (changes, G_VARIANT_TYPE ("au")));

  {
    g_autofree gchar *str = g_variant_print (changes, TRUE);
    g_message ("    %s", str);
  }

  g_message ("Closing change monitor");
  ret = ipc_git_change_monitor_call_close_sync (monitor, NULL, &error);
  g_assert_no_error (error);
  g_assert_true (ret);

  test_push (service, repository);

  g_message ("Closing");
  ret = ipc_git_repository_call_close_sync (repository, NULL, &error);
  g_assert_no_error (error);
  g_assert_true (ret);

  cleanup_dir ();

  g_main_loop_quit (main_loop);
}

static void
open_cb (IpcGitService *service,
         GAsyncResult  *result,
         gpointer       user_data)
{
  g_autoptr(IpcGitRepository) repository = NULL;
  g_autoptr(GError) error = NULL;
  g_autofree gchar *path = NULL;
  GDBusConnection *connection;
  gboolean ignored = FALSE;
  gboolean ret;

  g_assert (IPC_IS_GIT_SERVICE (service));
  g_assert (G_IS_ASYNC_RESULT (result));

  ret = ipc_git_service_call_open_finish (service, &path, result, &error);
  g_assert_no_error (error);
  g_assert_true (ret);

  g_message ("Opened %s", path);

  connection = g_dbus_proxy_get_connection (G_DBUS_PROXY (service));
  repository = ipc_git_repository_proxy_new_sync (connection,
                                                  0,
                                                  NULL,
                                                  path,
                                                  NULL,
                                                  &error);
  g_assert_no_error (error);
  g_assert_true (IPC_IS_GIT_REPOSITORY (repository));

  g_message ("Branch: %s", ipc_git_repository_get_branch (repository));
  g_message ("Location: %s", ipc_git_repository_get_location (repository));

  ret = ipc_git_repository_call_path_is_ignored_sync (repository, "build", &ignored, NULL, &error);
  g_assert_no_error (error);
  g_assert_true (ret);

  g_message ("\"build\" ignored? %d", ignored);

  ret = ipc_git_repository_call_close_sync (repository, NULL, &error);
  g_assert_no_error (error);
  g_assert_true (ret);

  g_message ("Repository closed");

  cleanup_dir ();

  g_message ("Testing cloning");
  test_clone (service);
}

static void
discover_cb (IpcGitService *service,
             GAsyncResult  *result,
             gpointer       user_data)
{
  g_autoptr(GError) error = NULL;
  g_autofree gchar *location = NULL;
  gboolean ret;

  g_assert (IPC_IS_GIT_SERVICE (service));
  g_assert (G_IS_ASYNC_RESULT (result));

  ret = ipc_git_service_call_discover_finish (service, &location, result, &error);
  g_assert_no_error (error);
  g_assert_true (ret);

  g_message ("Discover => %s", location);

  ipc_git_service_call_open (service,
                             location,
                             NULL,
                             (GAsyncReadyCallback) open_cb,
                             NULL);
}

static void
create_cb (IpcGitService *service,
           GAsyncResult  *result,
           gpointer       user_data)
{
  g_autoptr(GError) error = NULL;
  g_autofree gchar *location = NULL;
  gboolean ret;

  g_assert (IPC_IS_GIT_SERVICE (service));
  g_assert (G_IS_ASYNC_RESULT (result));

  ret = ipc_git_service_call_create_finish (service, &location, result, &error);
  g_assert_no_error (error);
  g_assert_true (ret);

  g_message ("Created repository at %s", location);

  ipc_git_service_call_discover (service,
                                 location,
                                 NULL,
                                 (GAsyncReadyCallback) discover_cb,
                                 NULL);
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

  g_mkdtemp (tmpdir);
  g_mkdtemp (tmpdir_push);

  test_config (service);

  ipc_git_service_call_create (service,
                               tmpdir,
                               FALSE,
                               NULL,
                               (GAsyncReadyCallback) create_cb,
                               NULL);

  g_main_loop_run (main_loop);

  while (g_main_context_pending (NULL))
    g_main_context_iteration (NULL, FALSE);

  return EXIT_SUCCESS;
}
