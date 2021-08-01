/* gbp-cmake-build-system.c
 *
 * Copyright 2017-2019 Christian Hergert <chergert@redhat.com>
 * Copyright 2017 Martin Blanchard <tchaik@gmx.com>
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

#define G_LOG_DOMAIN "gbp-cmake-build-system"

#include <glib/gi18n.h>

#include "gbp-cmake-build-system.h"
#include "gbp-cmake-build-target.h"

struct _GbpCMakeBuildSystem
{
  IdeObject           parent_instance;

  GFile              *project_file;
  IdeCompileCommands *compile_commands;
  GFileMonitor       *monitor;
};

static void async_initable_iface_init (GAsyncInitableIface     *iface);
static void build_system_iface_init   (IdeBuildSystemInterface *iface);

G_DEFINE_FINAL_TYPE_WITH_CODE (GbpCMakeBuildSystem, gbp_cmake_build_system, IDE_TYPE_OBJECT,
                         G_IMPLEMENT_INTERFACE (G_TYPE_ASYNC_INITABLE, async_initable_iface_init)
                         G_IMPLEMENT_INTERFACE (IDE_TYPE_BUILD_SYSTEM, build_system_iface_init))

enum {
  PROP_0,
  PROP_PROJECT_FILE,
  N_PROPS
};

static GParamSpec *properties [N_PROPS];

static void
gbp_cmake_build_system_commands_file_changed (GbpCMakeBuildSystem *self,
                                              GFile               *file,
                                              GFile               *other_file,
                                              GFileMonitorEvent    event,
                                              GFileMonitor        *monitor)
{
  IDE_ENTRY;

  g_assert (GBP_IS_CMAKE_BUILD_SYSTEM (self));
  g_assert (!file || G_IS_FILE (file));
  g_assert (!other_file || G_IS_FILE (other_file));
  g_assert (G_IS_FILE_MONITOR (monitor));

  g_clear_object (&self->compile_commands);

  g_file_monitor_cancel (monitor);
  g_clear_object (&self->monitor);

  IDE_EXIT;
}

static void
gbp_cmake_build_system_monitor_commands_file (GbpCMakeBuildSystem *self,
                                              GFile               *file)
{
  g_autoptr(GFileMonitor) monitor = NULL;

  g_assert (GBP_IS_CMAKE_BUILD_SYSTEM (self));
  g_assert (G_IS_FILE (file));

  monitor = g_file_monitor_file (file,
                                 G_FILE_MONITOR_NONE,
                                 NULL,
                                 NULL);

  g_signal_connect_object (monitor,
                           "changed",
                           G_CALLBACK (gbp_cmake_build_system_commands_file_changed),
                           self,
                           G_CONNECT_SWAPPED);

  g_set_object (&self->monitor, monitor);
}

static void
gbp_cmake_build_system_ensure_config_cb (GObject      *object,
                                         GAsyncResult *result,
                                         gpointer      user_data)
{
  IdeBuildManager *build_manager = (IdeBuildManager *)object;
  g_autoptr(IdeTask) task = user_data;
  g_autoptr(GError) error = NULL;

  g_assert (IDE_IS_BUILD_MANAGER (build_manager));
  g_assert (G_IS_ASYNC_RESULT (result));
  g_assert (IDE_IS_TASK (task));

  if (!ide_build_manager_build_finish (build_manager, result, &error))
    ide_task_return_error (task, g_steal_pointer (&error));
  else
    ide_task_return_boolean (task, TRUE);
}

static void
gbp_cmake_build_system_ensure_config_async (GbpCMakeBuildSystem *self,
                                            GCancellable        *cancellable,
                                            GAsyncReadyCallback  callback,
                                            gpointer             user_data)
{
  g_autoptr(IdeTask) task = NULL;
  IdeBuildManager *build_manager;
  IdeContext *context;

  g_assert (GBP_IS_CMAKE_BUILD_SYSTEM (self));
  g_assert (!cancellable || G_IS_CANCELLABLE (cancellable));

  task = ide_task_new (self, cancellable, callback, user_data);
  ide_task_set_source_tag (task, gbp_cmake_build_system_ensure_config_async);
  ide_task_set_priority (task, G_PRIORITY_LOW);

  context = ide_object_get_context (IDE_OBJECT (self));
  build_manager = ide_build_manager_from_context (context);

  ide_build_manager_build_async (build_manager,
                                   IDE_PIPELINE_PHASE_CONFIGURE,
                                   NULL,
                                   cancellable,
                                   gbp_cmake_build_system_ensure_config_cb,
                                   g_steal_pointer (&task));
}

static gboolean
gbp_cmake_build_system_ensure_config_finish (GbpCMakeBuildSystem  *self,
                                             GAsyncResult         *result,
                                             GError              **error)
{
  g_assert (GBP_IS_CMAKE_BUILD_SYSTEM (self));
  g_assert (IDE_IS_TASK (result));

  return ide_task_propagate_boolean (IDE_TASK (result), error);
}

static void
gbp_cmake_build_system_load_commands_load_cb (GObject      *object,
                                              GAsyncResult *result,
                                              gpointer      user_data)
{
  IdeCompileCommands *compile_commands = (IdeCompileCommands *)object;
  GbpCMakeBuildSystem *self;
  g_autoptr(IdeTask) task = user_data;
  g_autoptr(GError) error = NULL;

  g_assert (IDE_IS_COMPILE_COMMANDS (compile_commands));
  g_assert (G_IS_ASYNC_RESULT (result));
  g_assert (IDE_IS_TASK (task));

  self = ide_task_get_source_object (task);
  g_assert (GBP_IS_CMAKE_BUILD_SYSTEM (self));

  if (!ide_compile_commands_load_finish (compile_commands, result, &error))
    {
      ide_task_return_error (task, g_steal_pointer (&error));
      return;
    }

  g_set_object (&self->compile_commands, compile_commands);

  ide_task_return_pointer (task, g_object_ref (compile_commands), g_object_unref);
}

static void
gbp_cmake_build_system_load_commands_config_cb (GObject      *object,
                                                GAsyncResult *result,
                                                gpointer      user_data)
{
  GbpCMakeBuildSystem *self = (GbpCMakeBuildSystem *)object;
  g_autoptr(IdeCompileCommands) compile_commands = NULL;
  g_autoptr(IdeTask) task = user_data;
  g_autoptr(GError) error = NULL;
  g_autoptr(GFile) file = NULL;
  g_autofree gchar *path = NULL;
  IdeBuildManager *build_manager;
  IdePipeline *pipeline;
  GCancellable *cancellable;
  IdeContext *context;

  g_assert (GBP_IS_CMAKE_BUILD_SYSTEM (self));
  g_assert (G_IS_ASYNC_RESULT (result));
  g_assert (IDE_IS_TASK (task));

  if (!gbp_cmake_build_system_ensure_config_finish (self, result, &error))
    {
      ide_task_return_error (task, g_steal_pointer (&error));
      return;
    }

  context = ide_object_get_context (IDE_OBJECT (self));
  build_manager = ide_build_manager_from_context (context);
  pipeline = ide_build_manager_get_pipeline (build_manager);

  if (pipeline == NULL)
    {
      /* Unlikely, but possible */
      ide_task_return_new_error (task,
                                 G_IO_ERROR,
                                 G_IO_ERROR_FAILED,
                                 "No build pipeline is available");
      return;
    }

  path = ide_pipeline_build_builddir_path (pipeline, "compile_commands.json", NULL);

  if (!g_file_test (path, G_FILE_TEST_IS_REGULAR))
    {
      /* Unlikely, but possible */
      ide_task_return_new_error (task,
                                 G_IO_ERROR,
                                 G_IO_ERROR_NOT_FOUND,
                                 "Failed to locate compile_commands.json");
      return;
    }

  compile_commands = ide_compile_commands_new ();
  file = g_file_new_for_path (path);
  cancellable = ide_task_get_cancellable (task);

  ide_compile_commands_load_async (compile_commands,
                                   file,
                                   cancellable,
                                   gbp_cmake_build_system_load_commands_load_cb,
                                   g_steal_pointer (&task));

  gbp_cmake_build_system_monitor_commands_file (self, file);
}

static void
gbp_cmake_build_system_load_commands_async (GbpCMakeBuildSystem *self,
                                            GCancellable        *cancellable,
                                            GAsyncReadyCallback  callback,
                                            gpointer             user_data)
{
  g_autoptr(IdeTask) task = NULL;
  IdeBuildManager *build_manager;
  IdePipeline *pipeline;
  IdeContext *context;

  g_assert (GBP_IS_CMAKE_BUILD_SYSTEM (self));
  g_assert (!cancellable || G_IS_CANCELLABLE (cancellable));

  task = ide_task_new (self, cancellable, callback, user_data);
  ide_task_set_source_tag (task, gbp_cmake_build_system_load_commands_async);
  ide_task_set_priority (task, G_PRIORITY_LOW);

  /*
   * If we've already load the compile commands database, use it and
   * short circuit as early as we can to avoid progressing the build
   * pipeline unnecessarily.
   */

  if (self->compile_commands != NULL)
    {
      ide_task_return_pointer (task,
                               g_object_ref (self->compile_commands),
                               g_object_unref);
      return;
    }

  /*
   * If the build pipeline has been previously configured, we might
   * already have a "compile_commands.json" file in the build directory
   * that we can reuse.
   */

  context = ide_object_get_context (IDE_OBJECT (self));
  build_manager = ide_build_manager_from_context (context);
  pipeline = ide_build_manager_get_pipeline (build_manager);

  if (pipeline != NULL)
    {
      g_autofree gchar *path = NULL;

      path = ide_pipeline_build_builddir_path (pipeline, "compile_commands.json", NULL);

      if (g_file_test (path, G_FILE_TEST_IS_REGULAR))
        {
          g_autoptr(IdeCompileCommands) compile_commands = NULL;
          g_autoptr(GFile) file = NULL;

          compile_commands = ide_compile_commands_new ();
          file = g_file_new_for_path (path);

          ide_compile_commands_load_async (compile_commands,
                                           file,
                                           cancellable,
                                           gbp_cmake_build_system_load_commands_load_cb,
                                           g_steal_pointer (&task));

          gbp_cmake_build_system_monitor_commands_file (self, file);

          return;
        }
    }

  /*
   * It looks like we need to ensure the build pipeline advances to the the
   * CONFIGURE phase so that cmake has generated a new compile_commands.json
   * that we can load.
   */

  gbp_cmake_build_system_ensure_config_async (self,
                                              cancellable,
                                              gbp_cmake_build_system_load_commands_config_cb,
                                              g_steal_pointer (&task));
}

static IdeCompileCommands *
gbp_cmake_build_system_load_commands_finish (GbpCMakeBuildSystem  *self,
                                             GAsyncResult         *result,
                                             GError              **error)
{
  g_assert (GBP_IS_CMAKE_BUILD_SYSTEM (self));
  g_assert (IDE_IS_TASK (result));

  return ide_task_propagate_pointer (IDE_TASK (result), error);
}

static void
gbp_cmake_build_system_finalize (GObject *object)
{
  GbpCMakeBuildSystem *self = (GbpCMakeBuildSystem *)object;

  g_clear_object (&self->project_file);
  g_clear_object (&self->compile_commands);
  g_clear_object (&self->monitor);

  G_OBJECT_CLASS (gbp_cmake_build_system_parent_class)->finalize (object);
}

static void
gbp_cmake_build_system_get_property (GObject    *object,
                                     guint       prop_id,
                                     GValue     *value,
                                     GParamSpec *pspec)
{
  GbpCMakeBuildSystem *self = GBP_CMAKE_BUILD_SYSTEM (object);

  switch (prop_id)
    {
    case PROP_PROJECT_FILE:
      g_value_set_object (value, self->project_file);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
gbp_cmake_build_system_set_property (GObject      *object,
                                     guint         prop_id,
                                     const GValue *value,
                                     GParamSpec   *pspec)
{
  GbpCMakeBuildSystem *self = GBP_CMAKE_BUILD_SYSTEM (object);

  switch (prop_id)
    {
    case PROP_PROJECT_FILE:
      self->project_file = g_value_dup_object (value);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
gbp_cmake_build_system_class_init (GbpCMakeBuildSystemClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = gbp_cmake_build_system_finalize;
  object_class->get_property = gbp_cmake_build_system_get_property;
  object_class->set_property = gbp_cmake_build_system_set_property;

  properties [PROP_PROJECT_FILE] =
    g_param_spec_object ("project-file",
                         "Project File",
                         "The primary CMakeLists.txt for the project",
                         G_TYPE_FILE,
                         (G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, N_PROPS, properties);
}

static void
gbp_cmake_build_system_init (GbpCMakeBuildSystem *self)
{
}

static gchar *
gbp_cmake_build_system_get_id (IdeBuildSystem *build_system)
{
  return g_strdup ("cmake");
}

static gchar *
gbp_cmake_build_system_get_display_name (IdeBuildSystem *build_system)
{
  return g_strdup (_("CMake"));
}

static gint
gbp_cmake_build_system_get_priority (IdeBuildSystem *build_system)
{
  return -300;
}

static void
gbp_cmake_build_system_get_build_flags_cb (GObject      *object,
                                           GAsyncResult *result,
                                           gpointer      user_data)
{
  GbpCMakeBuildSystem *self = (GbpCMakeBuildSystem *)object;
  g_autoptr(IdeCompileCommands) compile_commands = NULL;
  g_autoptr(IdeTask) task = user_data;
  g_autoptr(GError) error = NULL;
  g_autoptr(GFile) directory = NULL;
  g_auto(GStrv) system_includes = NULL;
  g_auto(GStrv) build_flags = NULL;
  IdeConfigManager *config_manager;
  IdeContext *context;
  IdeConfig *config;
  IdeRuntime *runtime;
  GFile *file;

  g_assert (GBP_IS_CMAKE_BUILD_SYSTEM (self));
  g_assert (G_IS_ASYNC_RESULT (result));
  g_assert (IDE_IS_TASK (task));

  compile_commands = gbp_cmake_build_system_load_commands_finish (self, result, &error);

  if (compile_commands == NULL)
    {
      ide_task_return_error (task, g_steal_pointer (&error));
      return;
    }

  file = ide_task_get_task_data (task);
  g_assert (G_IS_FILE (file));

  /* Get non-standard system includes */
  context = ide_object_get_context (IDE_OBJECT (self));
  config_manager = ide_config_manager_from_context (context);
  config = ide_config_manager_get_current (config_manager);
  if ((runtime = ide_config_get_runtime (config)))
    system_includes = ide_runtime_get_system_include_dirs (runtime);

  build_flags = ide_compile_commands_lookup (compile_commands,
                                             file,
                                             (const gchar * const *)system_includes,
                                             &directory,
                                             &error);

  if (build_flags == NULL)
    ide_task_return_error (task, g_steal_pointer (&error));
  else
    ide_task_return_pointer (task, g_steal_pointer (&build_flags), g_strfreev);
}

static void
gbp_cmake_build_system_get_build_flags_async (IdeBuildSystem      *build_system,
                                              GFile               *file,
                                              GCancellable        *cancellable,
                                              GAsyncReadyCallback  callback,
                                              gpointer             user_data)
{
  GbpCMakeBuildSystem *self = (GbpCMakeBuildSystem *)build_system;
  g_autoptr(IdeTask) task = NULL;

  IDE_ENTRY;

  g_assert (GBP_IS_CMAKE_BUILD_SYSTEM (self));
  g_assert (G_IS_FILE (file));
  g_assert (!cancellable || G_IS_CANCELLABLE (cancellable));

  task = ide_task_new (self, cancellable, callback, user_data);
  ide_task_set_priority (task, G_PRIORITY_LOW);
  ide_task_set_source_tag (task, gbp_cmake_build_system_get_build_flags_async);
  ide_task_set_task_data (task, g_object_ref (file), g_object_unref);

  gbp_cmake_build_system_load_commands_async (self,
                                              cancellable,
                                              gbp_cmake_build_system_get_build_flags_cb,
                                              g_steal_pointer (&task));

  IDE_EXIT;
}

static gchar **
gbp_cmake_build_system_get_build_flags_finish (IdeBuildSystem  *build_system,
                                               GAsyncResult    *result,
                                               GError         **error)
{
  gchar **build_flags;

  IDE_ENTRY;

  g_assert (GBP_IS_CMAKE_BUILD_SYSTEM (build_system));
  g_assert (IDE_IS_TASK (result));

  build_flags = ide_task_propagate_pointer (IDE_TASK (result), error);

  IDE_RETURN (build_flags);
}

static void
build_system_iface_init (IdeBuildSystemInterface *iface)
{
  iface->get_id = gbp_cmake_build_system_get_id;
  iface->get_display_name = gbp_cmake_build_system_get_display_name;
  iface->get_priority = gbp_cmake_build_system_get_priority;
  iface->get_build_flags_async = gbp_cmake_build_system_get_build_flags_async;
  iface->get_build_flags_finish = gbp_cmake_build_system_get_build_flags_finish;
}

static void
gbp_cmake_build_system_notify_pipeline (GbpCMakeBuildSystem *self,
                                        GParamSpec          *pspec,
                                        IdeBuildManager     *build_manager)
{
  IDE_ENTRY;

  g_assert (GBP_IS_CMAKE_BUILD_SYSTEM (self));
  g_assert (pspec != NULL);
  g_assert (IDE_IS_BUILD_MANAGER (build_manager));

  /*
   * We need to regenerate compile commands when the build pipeline
   * changes so that we get the updated commands.
   */
  g_clear_object (&self->compile_commands);

  IDE_EXIT;
}

static void
gbp_cmake_build_system_init_worker (IdeTask      *task,
                                    gpointer      source_object,
                                    gpointer      task_data,
                                    GCancellable *cancellable)
{
  GFile *project_file = task_data;
  g_autofree gchar *name = NULL;

  IDE_ENTRY;

  g_assert (GBP_IS_CMAKE_BUILD_SYSTEM (source_object));
  g_assert (G_IS_FILE (project_file));
  g_assert (!cancellable || G_IS_CANCELLABLE (cancellable));

  name = g_file_get_basename (project_file);

  if (ide_str_equal0 (name, "CMakeLists.txt"))
    {
      ide_task_return_pointer (task, g_object_ref (project_file), g_object_unref);
      IDE_EXIT;
    }

  if (g_file_query_file_type (project_file, 0, cancellable) == G_FILE_TYPE_DIRECTORY)
    {
      g_autoptr(GFile) cmake_file = g_file_get_child (project_file, "CMakeLists.txt");

      if (g_file_query_exists (cmake_file, cancellable))
        {
          ide_task_return_pointer (task, g_object_ref (cmake_file), g_object_unref);
          IDE_EXIT;
        }
    }

  ide_task_return_new_error (task,
                             G_IO_ERROR,
                             G_IO_ERROR_NOT_SUPPORTED,
                             "%s is not supported by the cmake plugin",
                             name);

  IDE_EXIT;
}

static void
gbp_cmake_build_system_init_async (GAsyncInitable      *initable,
                                   gint                 io_priority,
                                   GCancellable        *cancellable,
                                   GAsyncReadyCallback  callback,
                                   gpointer             user_data)
{
  GbpCMakeBuildSystem *self = (GbpCMakeBuildSystem *)initable;
  g_autoptr(IdeTask) task = NULL;
  IdeBuildManager *build_manager;
  IdeContext *context;

  IDE_ENTRY;

  g_assert (GBP_IS_CMAKE_BUILD_SYSTEM (self));
  g_assert (G_IS_FILE (self->project_file));
  g_assert (!cancellable || G_IS_CANCELLABLE (cancellable));

  context = ide_object_get_context (IDE_OBJECT (self));
  g_assert (IDE_IS_CONTEXT (context));

  build_manager = ide_build_manager_from_context (context);
  g_assert (IDE_IS_BUILD_MANAGER (build_manager));

  task = ide_task_new (self, cancellable, callback, user_data);
  ide_task_set_source_tag (task, gbp_cmake_build_system_init_async);
  ide_task_set_priority (task, io_priority);
  ide_task_set_task_data (task, g_object_ref (self->project_file), g_object_unref);

  /*
   * We want to be notified of any changes to the current build manager.
   * This will let us invalidate our compile_commands.json when it changes.
   */
  g_signal_connect_object (build_manager,
                           "notify::pipeline",
                           G_CALLBACK (gbp_cmake_build_system_notify_pipeline),
                           self,
                           G_CONNECT_SWAPPED);

  ide_task_run_in_thread (task, gbp_cmake_build_system_init_worker);

  IDE_EXIT;
}

static gboolean
gbp_cmake_build_system_init_finish (GAsyncInitable  *initable,
                                    GAsyncResult    *result,
                                    GError         **error)
{
  GbpCMakeBuildSystem *self = (GbpCMakeBuildSystem *)initable;
  g_autoptr(GFile) project_file = NULL;

  IDE_ENTRY;

  g_assert (GBP_IS_CMAKE_BUILD_SYSTEM (self));
  g_assert (IDE_IS_TASK (result));

  project_file = ide_task_propagate_pointer (IDE_TASK (result), error);
  if (g_set_object (&self->project_file, project_file))
    g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_PROJECT_FILE]);

  IDE_RETURN (project_file != NULL);
}

static void
async_initable_iface_init (GAsyncInitableIface *iface)
{
  iface->init_async = gbp_cmake_build_system_init_async;
  iface->init_finish = gbp_cmake_build_system_init_finish;
}
