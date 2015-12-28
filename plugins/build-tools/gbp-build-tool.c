/* gbp-build-tool.c
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

#include <glib/gi18n.h>

#include "gbp-build-tool.h"

struct _GbpBuildTool
{
  GObject parent_instance;
};

static void application_tool_init (IdeApplicationToolInterface *iface);

G_DEFINE_TYPE_EXTENDED (GbpBuildTool, gbp_build_tool, G_TYPE_OBJECT, 0,
                        G_IMPLEMENT_INTERFACE (IDE_TYPE_APPLICATION_TOOL,
                                               application_tool_init))

static void
gbp_build_tool_class_init (GbpBuildToolClass *klass)
{
}

static void
gbp_build_tool_init (GbpBuildTool *self)
{
}

static void
gbp_build_tool_log (GbpBuildTool      *self,
                    IdeBuildResultLog  log,
                    const gchar       *message,
                    IdeBuildResult    *build_result)
{
  if (log == IDE_BUILD_RESULT_LOG_STDERR)
    g_printerr ("%s", message);
  else
    g_print ("%s", message);
}

static void
gbp_build_tool_build_cb (GObject      *object,
                         GAsyncResult *result,
                         gpointer      user_data)
{
  g_autoptr(GTask) task = user_data;
  IdeBuilder *builder = (IdeBuilder *)object;
  GError *error = NULL;

  g_assert (G_IS_TASK (task));
  g_assert (IDE_IS_BUILDER (builder));

  if (!ide_builder_build_finish (builder, result, &error))
    {
      g_task_return_error (task, error);
      return;
    }

  /*
   * TODO: We should consider supporting packaging/xdg-app/deployment stuff
   *       here too. It would be nice if we could say, go build this project,
   *       for this device, and then deploy.
   */

  g_print (_("Success.\n"));

  g_task_return_boolean (task, TRUE);
}

static void
gbp_build_tool_new_context_cb (GObject      *object,
                               GAsyncResult *result,
                               gpointer      user_data)
{
  g_autoptr(GTask) task = user_data;
  g_autoptr(IdeContext) context = NULL;
  g_autoptr(IdeBuilder) builder = NULL;
  g_autoptr(IdeBuildResult) build_result = NULL;
  g_autoptr(IdeDevice) device = NULL;
  IdeDeviceManager *device_manager;
  IdeBuildSystem *build_system;
  IdeBuilderBuildFlags flags;
  GKeyFile *config;
  const gchar *device_id;
  GError *error = NULL;

  g_assert (G_IS_TASK (task));

  context = ide_context_new_finish (result, &error);

  if (context == NULL)
    {
      g_task_return_error (task, error);
      return;
    }

  config = g_object_get_data (G_OBJECT (task), "CONFIG");
  flags = GPOINTER_TO_INT (g_object_get_data (G_OBJECT (task), "FLAGS"));

  device_id = g_object_get_data (G_OBJECT (task), "DEVICE_ID");
  device_manager = ide_context_get_device_manager (context);
  device = ide_device_manager_get_device (device_manager, device_id);

  if (device == NULL)
    {
      /* TODO: Wait for devices to settle. */
      g_task_return_new_error (task,
                               G_IO_ERROR,
                               G_IO_ERROR_NOT_FOUND,
                               _("Failed to locate device \"%s\""),
                               device_id);
      return;
    }

  /* TODO: Support custom configs */

  build_system = ide_context_get_build_system (context);
  builder = ide_build_system_get_builder (build_system, config, device, &error);

  if (builder == NULL)
    {
      g_task_return_error (task, error);
      return;
    }

  ide_builder_build_async (builder,
                           flags,
                           &build_result,
                           g_task_get_cancellable (task),
                           gbp_build_tool_build_cb,
                           g_object_ref (task));

  if (build_result != NULL)
    {
      /*
       * XXX: Technically we could lose some log lines unless we
       * guarantee that the build can't start until the main loop
       * is reached. (Which is probably reasonable).
       */
      g_signal_connect_object (build_result,
                               "log",
                               G_CALLBACK (gbp_build_tool_log),
                               g_task_get_source_object (task),
                               G_CONNECT_SWAPPED);
    }
}

static void
gbp_build_tool_run_async (IdeApplicationTool  *tool,
                          const gchar * const *arguments,
                          GCancellable        *cancellable,
                          GAsyncReadyCallback  callback,
                          gpointer             user_data)
{
  GbpBuildTool *self = (GbpBuildTool *)tool;
  g_autoptr(GTask) task = NULL;
  g_autofree gchar *project_path = NULL;
  g_autofree gchar *device_id = NULL;
  g_autoptr(GFile) project_file = NULL;
  g_autoptr(GOptionContext) opt_context = NULL;
  g_autoptr(GKeyFile) config = NULL;
  g_auto(GStrv) strv = NULL;
  gboolean clean = FALSE;
  gint parallel = -1;
  IdeBuilderBuildFlags flags = 0;
  GError *error = NULL;
  const GOptionEntry entries[] = {
    { "clean", 'c', 0, G_OPTION_ARG_NONE, &clean,
      N_("Clean the project") },
    { "device", 'd', 0, G_OPTION_ARG_STRING, &device_id,
      N_("The id of the device to build for"),
      N_("local") },
    { "parallel", 'j', 0, G_OPTION_ARG_INT, &parallel,
      N_("Number of workers to use when building"),
      N_("N") },
    { "project", 'p', 0, G_OPTION_ARG_FILENAME, &project_path,
      N_("Path to project file, defaults to current directory"),
      N_("PATH") },
    { NULL }
  };

  g_assert (GBP_IS_BUILD_TOOL (self));
  g_assert (arguments != NULL);
  g_assert (!cancellable || G_IS_CANCELLABLE (cancellable));

  task = g_task_new (self, cancellable, callback, user_data);

  opt_context = g_option_context_new ("build [OPTIONS]");
  g_option_context_add_main_entries (opt_context, entries, GETTEXT_PACKAGE);
  strv = g_strdupv ((gchar **)arguments);

  if (!g_option_context_parse_strv (opt_context, &strv, &error))
    {
      g_task_return_error (task, error);
      return;
    }

  if (project_path == NULL)
    project_path = g_strdup (".");

  project_file = g_file_new_for_commandline_arg (project_path);

  if (device_id == NULL)
    device_id = g_strdup ("local");

  config = g_key_file_new ();

  if (parallel >= -1)
    g_key_file_set_integer (config, "parallel", "workers", parallel);

  if (clean)
    flags |= IDE_BUILDER_BUILD_FLAGS_CLEAN;

  g_object_set_data_full (G_OBJECT (task), "DEVICE_ID", g_strdup (device_id), g_free);
  g_object_set_data_full (G_OBJECT (task), "CONFIG", g_key_file_ref (config), (GDestroyNotify)g_key_file_unref);
  g_object_set_data (G_OBJECT (task), "FLAGS", GINT_TO_POINTER (flags));

  ide_context_new_async (project_file,
                         cancellable,
                         gbp_build_tool_new_context_cb,
                         g_object_ref (task));
}

static gboolean
gbp_build_tool_run_finish (IdeApplicationTool  *tool,
                           GAsyncResult        *result,
                           GError             **error)
{
  g_assert (GBP_IS_BUILD_TOOL (tool));
  g_assert (G_IS_TASK (result));

  return g_task_propagate_boolean (G_TASK (result), error);
}

static void
application_tool_init (IdeApplicationToolInterface *iface)
{
  iface->run_async = gbp_build_tool_run_async;
  iface->run_finish = gbp_build_tool_run_finish;
}
