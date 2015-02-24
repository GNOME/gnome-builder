/* ide-build.c
 *
 * Copyright (C) 2015 Christian Hergert <christian@hergert.me>
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
 */

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <glib.h>
#include <glib/gi18n.h>
#include <ide.h>
#include <stdlib.h>

static GMainLoop *gMainLoop;
static gchar *gDeviceId;
static gint gExitCode = EXIT_SUCCESS;
static IdeContext *gContext;
static guint gTimeout;
static gulong gAddedHandler;
static guint64 gBuildStart;
static gboolean gRebuild;

static void read_line_cb (GObject      *object,
                          GAsyncResult *result,
                          gpointer      user_data);

static void
quit (gint exit_code)
{
  gExitCode = exit_code;
  g_clear_object (&gContext);
  g_main_loop_quit (gMainLoop);
}

static void
build_cb (GObject      *object,
          GAsyncResult *result,
          gpointer      user_data)
{
  IdeBuilder *builder = (IdeBuilder *)object;
  g_autoptr(IdeBuildResult) build_result = NULL;
  g_autoptr(GError) error = NULL;
  guint64 completed_at;
  guint64 total_usec;

  completed_at = g_get_monotonic_time ();
  build_result = ide_builder_build_finish (builder, result, &error);

  total_usec = completed_at - gBuildStart;

  if (!build_result)
    {
      g_printerr (_("===============\n"));
      g_printerr (_(" Build Failure: %s\n"), error->message);
      g_printerr (_(" Build ran for: %"G_GUINT64_FORMAT".%"G_GUINT64_FORMAT" seconds\n"),
                  (total_usec / 1000000), ((total_usec % 1000000) / 1000));
      g_printerr (_("===============\n"));
      quit (EXIT_FAILURE);
      return;
    }

  g_printerr (_("=================\n"));
  g_printerr (_(" Build Successful\n"));
  g_printerr (_("   Build ran for: %"G_GUINT64_FORMAT".%"G_GUINT64_FORMAT" seconds\n"),
              (total_usec / 1000000), ((total_usec % 1000000) / 1000));
  g_printerr (_("=================\n"));

  quit (EXIT_SUCCESS);
}

static gboolean
delayed_read (gpointer data)
{
  g_autoptr(GDataInputStream) data_stream = data;

  g_data_input_stream_read_line_async (data_stream,
                                       G_PRIORITY_DEFAULT,
                                       NULL,
                                       read_line_cb,
                                       NULL);

  return G_SOURCE_REMOVE;
}

static void
read_line_cb (GObject      *object,
              GAsyncResult *result,
              gpointer      user_data)
{
  GDataInputStream *data_stream = (GDataInputStream *)object;
  g_autofree gchar *line = NULL;
  g_autoptr(GError) error = NULL;
  gsize length = 0;

  g_assert (G_IS_DATA_INPUT_STREAM (data_stream));

  line = g_data_input_stream_read_line_finish_utf8 (data_stream,
                                                    result,
                                                    &length,
                                                    &error);

  if (line)
    {
      g_print ("%s\n", line);
      g_data_input_stream_read_line_async (data_stream,
                                           G_PRIORITY_DEFAULT,
                                           NULL,
                                           read_line_cb,
                                           NULL);
    }
  else if (error && g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CLOSED))
    {
      g_print ("Stream closed.\n");
    }
  else if (!error)
    {
      /* delay next read a bit. */
      /* TODO: is there a better way to do this? should we alter the
       *       O_NONBLOCK on the pipe?
       */
      g_timeout_add (50, delayed_read, g_object_ref (data_stream));
    }
  else
    {
      g_print ("Stream failure: %s.\n", error->message);
      g_input_stream_close (G_INPUT_STREAM (data_stream), NULL, NULL);
    }
}

static void
log_dumper (GInputStream *stream)
{
  g_autoptr(GDataInputStream) data_stream = NULL;

  g_assert (G_IS_INPUT_STREAM (stream));

  data_stream = g_data_input_stream_new (stream);

  g_data_input_stream_read_line_async (data_stream,
                                       G_PRIORITY_DEFAULT,
                                       NULL,
                                       read_line_cb,
                                       NULL);
}

static void
print_build_info (IdeContext *context,
                  IdeDevice  *device)
{
  IdeProject *project;
  IdeBuildSystem *build_system;
  IdeVcs *vcs;
  const gchar *project_name;
  const gchar *vcs_name;
  const gchar *build_system_name;
  const gchar *device_id;
  const gchar *system_type;
  g_autofree gchar *build_date = NULL;
  GTimeVal tv;

  project = ide_context_get_project (context);
  project_name = ide_project_get_name (project);

  vcs = ide_context_get_vcs (context);
  vcs_name = g_type_name (G_TYPE_FROM_INSTANCE (vcs));

  build_system = ide_context_get_build_system (context);
  build_system_name = g_type_name (G_TYPE_FROM_INSTANCE (build_system));

  device_id = ide_device_get_id (device);
  system_type = ide_device_get_system_type (device);

  g_get_current_time (&tv);
  build_date = g_time_val_to_iso8601 (&tv);

  g_printerr (_("========================\n"));
  g_printerr (_("           Project Name: %s\n"), project_name);
  g_printerr (_(" Version Control System: %s\n"), vcs_name);
  g_printerr (_("           Build System: %s\n"), build_system_name);
  g_printerr (_("    Build Date and Time: %s\n"), build_date);
  g_printerr (_("    Building for Device: %s (%s)\n"), device_id, system_type);
  g_printerr (_("========================\n"));
}

static void
build_for_device (IdeContext *context,
                  IdeDevice  *device)
{
  g_autoptr(IdeBuilder) builder = NULL;
  g_autoptr(IdeBuildResult) build_result = NULL;
  g_autoptr(GError) error = NULL;
  IdeBuildSystem *build_system;
  GKeyFile *config;

  print_build_info (context, device);

  config = g_key_file_new ();

  if (gRebuild)
    g_key_file_set_boolean (config, "autotools", "rebuild", TRUE);

  build_system = ide_context_get_build_system (context);
  builder = ide_build_system_get_builder (build_system, config, device, &error);
  g_key_file_unref (config);

  if (!builder)
    {
      g_printerr ("%s\n", error->message);
      quit (EXIT_FAILURE);
      return;
    }

  gBuildStart = g_get_monotonic_time ();

  ide_builder_build_async (builder, &build_result, NULL, build_cb, NULL);

  if (build_result)
    {
      GInputStream *stderr_stream;
      GInputStream *stdout_stream;

      stderr_stream = ide_build_result_get_stderr_stream (build_result);
      stdout_stream = ide_build_result_get_stdout_stream (build_result);

      log_dumper (stderr_stream);
      log_dumper (stdout_stream);

      g_object_unref (stderr_stream);
      g_object_unref (stdout_stream);
    }
}

static void
device_added_cb (IdeDeviceManager  *device_manager,
                 IdeDeviceProvider *provider,
                 IdeDevice         *device,
                 gpointer           user_data)
{
  const gchar *device_id;

  device_id = ide_device_get_id (device);

  if (g_strcmp0 (device_id, gDeviceId) == 0)
    {
      build_for_device (gContext, device);

      if (gTimeout)
        {
          g_source_remove (gTimeout);
          gTimeout = 0;
        }

      g_signal_handler_disconnect (device_manager, gAddedHandler);
    }
}

static gboolean
timeout_cb (gpointer data)
{
  g_printerr (_("Timed out while waiting for devices to settle.\n"));
  quit (EXIT_FAILURE);
  return G_SOURCE_REMOVE;
}

static void
context_cb (GObject      *object,
            GAsyncResult *result,
            gpointer      user_data)
{
  g_autoptr(IdeContext) context = NULL;
  g_autoptr(GError) error = NULL;
  g_autoptr(IdeDevice) device = NULL;
  GPtrArray *devices;
  IdeDeviceManager *device_manager;
  guint i;

  context = ide_context_new_finish (result, &error);

  if (!context)
    {
      g_printerr ("%s\n", error->message);
      quit (EXIT_FAILURE);
      return;
    }

  gContext = g_object_ref (context);

  /*
   * Try to locate the device we are building for. If the device is not found,
   * we will wait for a timeout period while devices show up during device
   * settling.
   */

  device_manager = ide_context_get_device_manager (context);

  devices = ide_device_manager_get_devices (device_manager);
  for (i = 0; i < devices->len; i++)
    {
      IdeDevice *device;
      const gchar *device_id;

      device = g_ptr_array_index (devices, i);
      device_id = ide_device_get_id (device);

      if (g_strcmp0 (device_id, gDeviceId) == 0)
        {
          build_for_device (gContext, device);
          g_ptr_array_unref (devices);
          return;
        }
    }
  g_ptr_array_unref (devices);

  gAddedHandler = g_signal_connect (device_manager,
                                    "device-added",
                                    G_CALLBACK (device_added_cb),
                                    NULL);
  gTimeout = g_timeout_add_seconds (60, timeout_cb, NULL);
  g_printerr (_("Waiting up to 60 seconds for devices to settle. Ctrl+C to exit.\n"));
}

int
main (gint   argc,
      gchar *argv[])
{
  GOptionEntry entries[] = {
    { "device", 'd', 0, G_OPTION_ARG_STRING, &gDeviceId,
      N_("The target device we are building for."),
      N_("DEVICE_ID")
    },
    { "rebuild", 'r', 0, G_OPTION_ARG_NONE, &gRebuild,
      N_("Clean and rebuild the project.") },
    { NULL }
  };
  g_autoptr(GOptionContext) context = NULL;
  g_autoptr(GError) error = NULL;
  g_autoptr(GFile) project_file = NULL;
  const gchar *project_path = ".";

  ide_set_program_name ("gnome-builder");
  g_set_prgname ("ide-build");

  context = g_option_context_new (_("- Build the project."));
  g_option_context_add_main_entries (context, entries, GETTEXT_PACKAGE);

  if (!g_option_context_parse (context, &argc, &argv, &error))
    {
      g_printerr ("%s\n", error->message);
      return EXIT_FAILURE;
    }

  gMainLoop = g_main_loop_new (NULL, FALSE);

  if (argc > 1)
    project_path = argv [1];
  project_file = g_file_new_for_path (project_path);

  if (!gDeviceId)
    gDeviceId = g_strdup ("local");

  ide_context_new_async (project_file, NULL, context_cb, NULL);

  g_main_loop_run (gMainLoop);
  g_clear_pointer (&gMainLoop, g_main_loop_unref);

  return gExitCode;
}
