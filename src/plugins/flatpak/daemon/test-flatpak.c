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
#include "ipc-flatpak-util.h"

static void
rm_rf (const char *dir)
{
  g_autofree char *escaped = g_shell_quote (dir);
  g_autofree char *command = g_strdup_printf ("rm -rf %s", escaped);
  g_message ("Deleting test data-dir %s", dir);
  system (command);
}

static void
on_runtime_added_cb (IpcFlatpakService *service,
                     GVariant          *info)
{
  const gchar *name;
  const gchar *arch;
  const gchar *branch;
  const gchar *sdk_name;
  const gchar *sdk_branch;
  const gchar *deploy_dir;
  const gchar *metadata;
  gboolean sdk_extension;
  gboolean ret;

  g_assert (IPC_IS_FLATPAK_SERVICE (service));
  g_assert (info != NULL);

  ret = runtime_variant_parse (info, &name, &arch, &branch, &sdk_name, &sdk_branch, &deploy_dir, &metadata, &sdk_extension);
  g_assert_true (ret);

  if (!sdk_extension)
    g_message ("Runtime Added: %s/%s/%s with SDK %s//%s",
               name, arch, branch, sdk_name, sdk_branch);
  else
    g_message ("SDK Extension Added: %s/%s/%s with SDK %s//%s",
               name, arch, branch, sdk_name, sdk_branch);
}

static void
free_element (gpointer data)
{
  g_free (*(gpointer *)data);
}

static void
begin_test (IpcFlatpakService *service,
            GMainLoop         *main_loop)
{
  g_autofree gchar *sizestr = NULL;
  g_autofree gchar *resolved = NULL;
  g_autoptr(GVariant) runtimes = NULL;
  g_autoptr(GVariant) info = NULL;
  g_autoptr(GError) error = NULL;
  g_autoptr(GArray) runtime_names = NULL;
  GVariantIter iter;
  gboolean is_known = TRUE;
  gboolean ret;
  gint64 download_size = 0;

  runtime_names = g_array_new (TRUE, FALSE, sizeof (char*));
  g_array_set_clear_func (runtime_names, free_element);

  g_message ("Listing runtimes");
  ret = ipc_flatpak_service_call_list_runtimes_sync (service, &runtimes, NULL, &error);
  g_assert_no_error (error);
  g_assert_true (ret);

  if (g_variant_iter_init (&iter, runtimes))
    {
      GVariant *value;
      const gchar *name;
      const gchar *arch;
      const gchar *branch;
      const gchar *sdk_name;
      const gchar *sdk_branch;
      const gchar *deploy_dir;
      const gchar *metadata;
      gboolean sdk_extension;

      while ((value = g_variant_iter_next_value (&iter)))
        {
          char *id = NULL;

          ret = runtime_variant_parse (value, &name, &arch, &branch, &sdk_name, &sdk_branch, &deploy_dir, &metadata, &sdk_extension);
          g_assert_true (ret);

          g_message ("  %s/%s/%s with SDK %s//%s (Extension: %d) in directory %s",
                     name, arch, branch, sdk_name, sdk_branch, sdk_extension, deploy_dir);

          id = g_strdup_printf ("%s/%s/%s", name, arch, branch);
          g_array_append_val (runtime_names, id);

          g_variant_unref (value);
        }
    }

  g_message ("Checking for a missing runtime");
  ret = ipc_flatpak_service_call_runtime_is_known_sync (service, "me.hergert.FooBar/x86_64/master", &is_known, &download_size, NULL, &error);
  g_assert_no_error (error);
  g_assert_true (ret);
  g_assert_false (is_known);
  g_message ("  Not found");

  g_message ("Checking if org.gnome.Sdk/x86_64/master is known");
  ret = ipc_flatpak_service_call_runtime_is_known_sync (service, "org.gnome.Sdk/x86_64/master", &is_known, &download_size, NULL, &error);
  g_assert_no_error (error);
  g_assert_true (ret);
  g_assert_true (is_known);
  sizestr = g_format_size (download_size);
  g_message ("  Found, Download Size: <=%s", sizestr);

  for (guint i = 0; i < runtime_names->len; i++)
    {
      const char *id = g_array_index (runtime_names, const char *, i);
      g_message ("Getting runtime info for known runtime");
      ret = ipc_flatpak_service_call_get_runtime_sync (service, id, &info, NULL, &error);
      g_assert_no_error (error);
      g_assert_true (ret);
      g_message ("  Found");
    }

  g_message ("Resolving org.freedesktop.Sdk.Extension.rust-stable for runtime/org.gnome.Sdk/x86_64/40");
  ret = ipc_flatpak_service_call_resolve_extension_sync (service, "runtime/org.gnome.Sdk/x86_64/40", "org.freedesktop.Sdk.Extension.rust-stable", &resolved, NULL, &error);
  g_assert_no_error (error);
  g_assert_true (ret);
  g_message (" Resolved to %s", resolved);
  g_assert_cmpstr (resolved, ==, "org.freedesktop.Sdk.Extension.rust-stable/x86_64/20.08");
  g_clear_pointer (&resolved, g_free);

  g_message ("Resolving org.freedesktop.Sdk.Extension.rust-stable for runtime/org.gnome.Platform/x86_64/40");
  ret = ipc_flatpak_service_call_resolve_extension_sync (service, "runtime/org.gnome.Platform/x86_64/40", "org.freedesktop.Sdk.Extension.rust-stable", &resolved, NULL, &error);
  g_assert_false (ret);
  g_clear_pointer (&resolved, g_free);
  g_clear_error (&error);

  g_message ("Resolving org.freedesktop.Sdk.Extension.rust-stable for org.gnome.Sdk/x86_64/40");
  ret = ipc_flatpak_service_call_resolve_extension_sync (service, "org.gnome.Sdk/x86_64/40", "org.freedesktop.Sdk.Extension.rust-stable", &resolved, NULL, &error);
  g_assert_no_error (error);
  g_assert_true (ret);
  g_message (" Resolved to %s", resolved);
  g_assert_cmpstr (resolved, ==, "org.freedesktop.Sdk.Extension.rust-stable/x86_64/20.08");

  g_message ("Resolving org.freedesktop.Sdk.Extension.rust-stable for org.gnome.Sdk/aarch64/40");
  ret = ipc_flatpak_service_call_resolve_extension_sync (service, "org.gnome.Sdk/aarch64/40", "org.freedesktop.Sdk.Extension.rust-stable", &resolved, NULL, &error);
  g_assert_no_error (error);
  g_assert_true (ret);
  g_message (" Resolved to %s", resolved);
  g_assert_cmpstr (resolved, ==, "org.freedesktop.Sdk.Extension.rust-stable/aarch64/20.08");

  g_message ("Resolving org.freedesktop.Sdk.Extension.rust-stable for org.gnome.Sdk/aarch64/41beta");
  ret = ipc_flatpak_service_call_resolve_extension_sync (service, "org.gnome.Sdk/aarch64/41beta", "org.freedesktop.Sdk.Extension.rust-stable", &resolved, NULL, &error);
  g_assert_no_error (error);
  g_assert_true (ret);
  g_message (" Resolved to %s", resolved);
  g_assert_cmpstr (resolved, ==, "org.freedesktop.Sdk.Extension.rust-stable/aarch64/21.08");

  g_message ("Resolving org.freedesktop.Sdk.Extension.llvm12 for org.gnome.Sdk/x86_64/41beta");
  ret = ipc_flatpak_service_call_resolve_extension_sync (service, "org.gnome.Sdk/x86_64/41beta", "org.freedesktop.Sdk.Extension.llvm12", &resolved, NULL, &error);
  g_assert_no_error (error);
  g_assert_true (ret);
  g_message (" Resolved to %s", resolved);
  g_assert_cmpstr (resolved, ==, "org.freedesktop.Sdk.Extension.llvm12/x86_64/21.08");

  g_main_loop_quit (main_loop);
}

static void
add_install_cb (GObject      *object,
                GAsyncResult *result,
                gpointer      user_data)
{
  IpcFlatpakService *service = (IpcFlatpakService *)object;
  g_autoptr(GMainLoop) main_loop = user_data;
  g_autoptr(GError) error = NULL;
  gboolean ret;

  ret = ipc_flatpak_service_call_add_installation_finish (service, result, &error);
  g_assert_no_error (error);
  g_assert_true (ret);
  g_message ("Installation added");

  begin_test (service, main_loop);
}

static gboolean ignore_home;
static gboolean ignore_system;
static char *data_dir;
static GOptionEntry main_entries[] = {
  { "ignore-home", 'i', 0, G_OPTION_ARG_NONE, &ignore_home, "Ignore --user flatpak installation" },
  { "ignore-system", 's', 0, G_OPTION_ARG_NONE, &ignore_system, "Ignore --system flatpak installation" },
  { "data-dir", 'd', 0, G_OPTION_ARG_FILENAME, &data_dir, "Set the data directory to use" },
  { 0 }
};

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
  g_autoptr(GOptionContext) context = NULL;
  g_autoptr(GPtrArray) args = g_ptr_array_new ();
  GMainLoop *main_loop;
  gboolean data_dir_is_temp = FALSE;

  context = g_option_context_new ("- test gnome-builder-flatpak daemon");
  g_option_context_add_main_entries (context, main_entries, NULL);

  if (!g_option_context_parse (context, &argc, &argv, &error))
    {
      g_printerr ("%s\n", error->message);
      return EXIT_FAILURE;
    }

  g_ptr_array_add (args, (char *)"./gnome-builder-flatpak");

  if (!data_dir)
    {
      char template[] = "data-dir-XXXXXX";
      data_dir = g_strdup (g_mkdtemp (template));
      data_dir_is_temp = TRUE;
    }

  g_message ("Using %s for test data directory", data_dir);

  if (ignore_system)
    g_ptr_array_add (args, (char *)"--ignore-system");
  g_ptr_array_add (args, (char *)"--verbose");
  g_ptr_array_add (args, (char *)"--data-dir");
  g_ptr_array_add (args, data_dir);
  g_ptr_array_add (args, NULL);

  launcher = g_subprocess_launcher_new (G_SUBPROCESS_FLAGS_STDIN_PIPE | G_SUBPROCESS_FLAGS_STDOUT_PIPE);
  g_subprocess_launcher_unsetenv (launcher, "G_MESSAGES_DEBUG");
  subprocess = g_subprocess_launcher_spawnv (launcher, (const char * const *)args->pdata, &error);

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

  {
    const char *default_arch;
    g_message ("Checking DefaultArch property");
    default_arch = ipc_flatpak_service_get_default_arch (service);
#if defined(__x86_64__)
    g_assert_cmpstr (default_arch, ==, "x86_64");
#elif defined(__i386__)
    g_assert_cmpstr (default_arch, ==, "i386");
#elif defined(__aarch64__)
    g_assert_cmpstr (default_arch, ==, "aarch64");
#else
# warning "Please add your compiler to the test for default arch property!"
#endif
  }

  g_signal_connect (service,
                    "runtime-added",
                    G_CALLBACK (on_runtime_added_cb),
                    NULL);

  if (!ignore_home)
    {
      g_message ("Adding --user installation to daemon");
      ipc_flatpak_service_call_add_installation (service,
                                                 home_install,
                                                 TRUE,
                                                 NULL,
                                                 add_install_cb,
                                                 g_main_loop_ref (main_loop));
      g_main_loop_run (main_loop);
    }
  else
    {
      g_message ("Ignoring --user installation");
      begin_test (service, main_loop);
    }

  if (data_dir_is_temp)
    rm_rf (data_dir);

  return EXIT_SUCCESS;
}
