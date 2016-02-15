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
  gint64  build_start;
};

static gint                  parallel = -1;
static IdeBuilderBuildFlags  flags;
static gchar                *configuration_id;
static gchar                *device_id;
static gchar                *runtime_id;

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
print_build_info (IdeContext *context,
                  IdeConfiguration *configuration)
{
  IdeProject *project;
  IdeBuildSystem *build_system;
  IdeDevice *device;
  IdeVcs *vcs;
  g_auto(GStrv) env = NULL;
  const gchar *dev_id;
  const gchar *project_name;
  const gchar *vcs_name;
  const gchar *build_system_name;
  const gchar *system_type;
  g_autofree gchar *build_date = NULL;
  GTimeVal tv;

  project = ide_context_get_project (context);
  project_name = ide_project_get_name (project);

  vcs = ide_context_get_vcs (context);
  vcs_name = g_type_name (G_TYPE_FROM_INSTANCE (vcs));

  build_system = ide_context_get_build_system (context);
  build_system_name = g_type_name (G_TYPE_FROM_INSTANCE (build_system));

  device = ide_configuration_get_device (configuration);
  dev_id = ide_device_get_id (device);
  system_type = ide_device_get_system_type (device);

  env = ide_configuration_get_environ (configuration);

  g_get_current_time (&tv);
  build_date = g_time_val_to_iso8601 (&tv);

  g_printerr (_("========================\n"));
  g_printerr (_("           Project Name: %s\n"), project_name);
  g_printerr (_(" Version Control System: %s\n"), vcs_name);
  g_printerr (_("           Build System: %s\n"), build_system_name);
  g_printerr (_("    Build Date and Time: %s\n"), build_date);
  g_printerr (_("    Building for Device: %s (%s)\n"), dev_id, system_type);

  if (env && env [0])
    {
      g_autofree gchar *envstr = g_strjoinv (" ", env);
      g_printerr (_("            Environment: %s\n"), envstr);
    }

  g_printerr (_("========================\n"));
}

static void
gbp_build_tool_build_cb (GObject      *object,
                         GAsyncResult *result,
                         gpointer      user_data)
{
  g_autoptr(GTask) task = user_data;
  g_autoptr(IdeBuildResult) build_result = NULL;
  GbpBuildTool *self;
  IdeBuilder *builder = (IdeBuilder *)object;
  GError *error = NULL;
  guint64 completed_at;
  guint64 total_usec;

  g_assert (G_IS_TASK (task));
  g_assert (IDE_IS_BUILDER (builder));

  self = g_task_get_source_object (task);
  completed_at = g_get_monotonic_time ();
  build_result = ide_builder_build_finish (builder, result, &error);

  total_usec = completed_at - self->build_start;

  if (build_result == NULL)
    {
      g_printerr (_("===============\n"));
      g_printerr (_(" Build Failure: %s\n"), error->message);
      g_printerr (_(" Build ran for: %"G_GUINT64_FORMAT".%"G_GUINT64_FORMAT" seconds\n"),
                  (total_usec / 1000000), ((total_usec % 1000000) / 1000));
      g_printerr (_("===============\n"));
      g_task_return_error (task, error);
      return;
    }

  /*
   * TODO: We should consider supporting packaging/xdg-app/deployment stuff
   *       here too. It would be nice if we could say, go build this project,
   *       for this device, and then deploy.
   */

  g_printerr (_("=================\n"));
  g_printerr (_(" Build Successful\n"));
  g_printerr (_("   Build ran for: %"G_GUINT64_FORMAT".%"G_GUINT64_FORMAT" seconds\n"),
              (total_usec / 1000000), ((total_usec % 1000000) / 1000));
  g_printerr (_("=================\n"));

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
  g_autoptr(IdeConfiguration) configuration = NULL;
  IdeConfigurationManager *configuration_manager;
  IdeBuildSystem *build_system;
  GbpBuildTool *self;
  GError *error = NULL;

  g_assert (G_IS_TASK (task));

  self = g_task_get_source_object (task);

  context = ide_context_new_finish (result, &error);

  if (context == NULL)
    {
      g_task_return_error (task, error);
      return;
    }

  configuration_manager = ide_context_get_configuration_manager (context);

  if (configuration_id != NULL)
    configuration = ide_configuration_manager_get_configuration (configuration_manager, configuration_id);
  else if (device_id && runtime_id)
    configuration = ide_configuration_new (context, "command-line-build", device_id, runtime_id);
  else if (device_id)
    configuration = ide_configuration_new (context, "command-line-build", device_id, "host");
  else if (runtime_id)
    configuration = ide_configuration_new (context, "command-line-build", "local", runtime_id);
  else
    configuration = ide_configuration_manager_get_current (configuration_manager);

  if (!ide_configuration_get_device (configuration))
    {
      /* TODO: Wait for devices to settle. */
      g_task_return_new_error (task,
                               G_IO_ERROR,
                               G_IO_ERROR_NOT_FOUND,
                               _("Failed to locate device \"%s\""),
                               device_id);
      return;
    }

  if (!ide_configuration_get_runtime (configuration))
    {
      g_task_return_new_error (task,
                               G_IO_ERROR,
                               G_IO_ERROR_NOT_FOUND,
                               _("Failed to locate runtime \"%s\""),
                               runtime_id);
      return;
    }

  if (parallel > -1)
    {
      /* TODO: put this into IdeConfiguration:parallel: */
      g_autofree gchar *str = g_strdup_printf ("%d", parallel);
      ide_configuration_setenv (configuration, "PARALLEL", str);
    }

  print_build_info (context, configuration);

  build_system = ide_context_get_build_system (context);
  builder = ide_build_system_get_builder (build_system, configuration, &error);

  if (builder == NULL)
    {
      g_task_return_error (task, error);
      return;
    }

  self->build_start = g_get_monotonic_time ();

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
  g_autoptr(GFile) project_file = NULL;
  g_autoptr(GOptionContext) opt_context = NULL;
  g_auto(GStrv) strv = NULL;
  gboolean clean = FALSE;
  GError *error = NULL;
  const GOptionEntry entries[] = {
    { "clean", 'c', 0, G_OPTION_ARG_NONE, &clean,
      N_("Clean the project") },
    { "device", 'd', 0, G_OPTION_ARG_STRING, &device_id,
      N_("The ID of the device to build for"),
      N_("local") },
    { "runtime", 'r', 0, G_OPTION_ARG_STRING, &runtime_id,
      N_("The runtime to use for building"),
      N_("host") },
    { "parallel", 'j', 0, G_OPTION_ARG_INT, &parallel,
      N_("Number of workers to use when building"),
      N_("N") },
    { "configuration", 't', 0, G_OPTION_ARG_STRING, &configuration_id,
      N_("The configuration to use from .buildconfig"),
      N_("CONFIG_ID") },
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

  if (clean)
    {
      flags |= IDE_BUILDER_BUILD_FLAGS_FORCE_CLEAN;
      flags |= IDE_BUILDER_BUILD_FLAGS_NO_BUILD;
    }

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
