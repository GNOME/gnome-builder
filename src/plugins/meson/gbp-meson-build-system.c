/* gbp-meson-build-system.c
 *
 * Copyright 2017 Christian Hergert <chergert@redhat.com>
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

#define G_LOG_DOMAIN "gbp-meson-build-system"

#include <glib/gi18n.h>
#include <json-glib/json-glib.h>

#include "gbp-meson-build-system.h"
#include "gbp-meson-build-target.h"
#include "gbp-meson-toolchain.h"

struct _GbpMesonBuildSystem
{
  IdeObject           parent_instance;
  GFile              *project_file;
  IdeCompileCommands *compile_commands;
  GFileMonitor       *monitor;
};

static void async_initable_iface_init (GAsyncInitableIface     *iface);
static void build_system_iface_init   (IdeBuildSystemInterface *iface);

G_DEFINE_TYPE_WITH_CODE (GbpMesonBuildSystem, gbp_meson_build_system, IDE_TYPE_OBJECT,
                         G_IMPLEMENT_INTERFACE (G_TYPE_ASYNC_INITABLE, async_initable_iface_init)
                         G_IMPLEMENT_INTERFACE (IDE_TYPE_BUILD_SYSTEM, build_system_iface_init))

enum {
  PROP_0,
  PROP_PROJECT_FILE,
  N_PROPS
};

static GParamSpec *properties [N_PROPS];

static void
gbp_meson_build_system_ensure_config_cb (GObject      *object,
                                         GAsyncResult *result,
                                         gpointer      user_data)
{
  IdeBuildManager *build_manager = (IdeBuildManager *)object;
  g_autoptr(IdeTask) task = user_data;
  g_autoptr(GError) error = NULL;

  g_assert (IDE_IS_BUILD_MANAGER (build_manager));
  g_assert (G_IS_ASYNC_RESULT (result));
  g_assert (IDE_IS_TASK (task));

  if (!ide_build_manager_execute_finish (build_manager, result, &error))
    ide_task_return_error (task, g_steal_pointer (&error));
  else
    ide_task_return_boolean (task, TRUE);
}

static void
gbp_meson_build_system_ensure_config_async (GbpMesonBuildSystem *self,
                                            GCancellable        *cancellable,
                                            GAsyncReadyCallback  callback,
                                            gpointer             user_data)
{
  g_autoptr(IdeTask) task = NULL;
  IdeBuildManager *build_manager;
  IdeContext *context;

  g_assert (GBP_IS_MESON_BUILD_SYSTEM (self));
  g_assert (!cancellable || G_IS_CANCELLABLE (cancellable));

  task = ide_task_new (self, cancellable, callback, user_data);
  ide_task_set_source_tag (task, gbp_meson_build_system_ensure_config_async);
  ide_task_set_priority (task, G_PRIORITY_LOW);

  context = ide_object_get_context (IDE_OBJECT (self));
  build_manager = ide_context_get_build_manager (context);

  ide_build_manager_execute_async (build_manager,
                                   IDE_BUILD_PHASE_CONFIGURE,
                                   cancellable,
                                   gbp_meson_build_system_ensure_config_cb,
                                   g_steal_pointer (&task));
}

static gboolean
gbp_meson_build_system_ensure_config_finish (GbpMesonBuildSystem  *self,
                                             GAsyncResult         *result,
                                             GError              **error)
{
  g_assert (GBP_IS_MESON_BUILD_SYSTEM (self));
  g_assert (IDE_IS_TASK (result));

  return ide_task_propagate_boolean (IDE_TASK (result), error);
}

static void
gbp_meson_build_system_monitor_changed (GbpMesonBuildSystem *self,
                                        GFile               *file,
                                        GFile               *other_file,
                                        GFileMonitorEvent    event,
                                        GFileMonitor        *monitor)
{
  IDE_ENTRY;

  g_assert (GBP_IS_MESON_BUILD_SYSTEM (self));
  g_assert (!file || G_IS_FILE (file));
  g_assert (!other_file || G_IS_FILE (other_file));
  g_assert (G_IS_FILE_MONITOR (monitor));

  /* Release our previous compile commands */
  g_clear_object (&self->compile_commands);
  g_file_monitor_cancel (monitor);
  g_clear_object (&self->monitor);

  IDE_EXIT;
}

static void
gbp_meson_build_system_monitor (GbpMesonBuildSystem *self,
                                GFile               *file)
{
  g_autoptr(GFileMonitor) monitor = NULL;

  g_assert (GBP_IS_MESON_BUILD_SYSTEM (self));
  g_assert (G_IS_FILE (file));

  monitor = g_file_monitor_file (file,
                                 G_FILE_MONITOR_NONE,
                                 NULL,
                                 NULL);
  g_signal_connect_object (monitor,
                           "changed",
                           G_CALLBACK (gbp_meson_build_system_monitor_changed),
                           self,
                           G_CONNECT_SWAPPED);
  g_set_object (&self->monitor, monitor);
}

static void
gbp_meson_build_system_load_commands_load_cb (GObject      *object,
                                              GAsyncResult *result,
                                              gpointer      user_data)
{
  IdeCompileCommands *compile_commands = (IdeCompileCommands *)object;
  GbpMesonBuildSystem *self;
  g_autoptr(IdeTask) task = user_data;
  g_autoptr(GError) error = NULL;

  g_assert (IDE_IS_COMPILE_COMMANDS (compile_commands));
  g_assert (G_IS_ASYNC_RESULT (result));
  g_assert (IDE_IS_TASK (task));

  self = ide_task_get_source_object (task);

  g_assert (GBP_IS_MESON_BUILD_SYSTEM (self));

  if (!ide_compile_commands_load_finish (compile_commands, result, &error))
    {
      ide_task_return_error (task, g_steal_pointer (&error));
      return;
    }

  g_set_object (&self->compile_commands, compile_commands);
  ide_task_return_pointer (task, g_object_ref (compile_commands), g_object_unref);
}

static void
gbp_meson_build_system_load_commands_config_cb (GObject      *object,
                                                GAsyncResult *result,
                                                gpointer      user_data)
{
  GbpMesonBuildSystem *self = (GbpMesonBuildSystem *)object;
  g_autoptr(IdeCompileCommands) compile_commands = NULL;
  g_autoptr(IdeTask) task = user_data;
  g_autoptr(GError) error = NULL;
  g_autoptr(GFile) file = NULL;
  g_autofree gchar *path = NULL;
  IdeBuildManager *build_manager;
  IdeBuildPipeline *pipeline;
  GCancellable *cancellable;
  IdeContext *context;

  g_assert (GBP_IS_MESON_BUILD_SYSTEM (self));
  g_assert (G_IS_ASYNC_RESULT (result));
  g_assert (IDE_IS_TASK (task));

  if (!gbp_meson_build_system_ensure_config_finish (self, result, &error))
    {
      ide_task_return_error (task, g_steal_pointer (&error));
      return;
    }

  context = ide_object_get_context (IDE_OBJECT (self));
  build_manager = ide_context_get_build_manager (context);
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

  path = ide_build_pipeline_build_builddir_path (pipeline, "compile_commands.json", NULL);

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
                                   gbp_meson_build_system_load_commands_load_cb,
                                   g_steal_pointer (&task));

  gbp_meson_build_system_monitor (self, file);
}

static void
gbp_meson_build_system_load_commands_async (GbpMesonBuildSystem *self,
                                            GCancellable        *cancellable,
                                            GAsyncReadyCallback  callback,
                                            gpointer             user_data)
{
  g_autoptr(IdeTask) task = NULL;
  g_autofree gchar *path = NULL;
  IdeBuildManager *build_manager;
  IdeBuildPipeline *pipeline;
  IdeContext *context;

  g_assert (GBP_IS_MESON_BUILD_SYSTEM (self));
  g_assert (!cancellable || G_IS_CANCELLABLE (cancellable));

  task = ide_task_new (self, cancellable, callback, user_data);
  ide_task_set_source_tag (task, gbp_meson_build_system_load_commands_async);
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
  build_manager = ide_context_get_build_manager (context);
  pipeline = ide_build_manager_get_pipeline (build_manager);

  /*
   * Because we're accessing the pipeline directly, we need to be careful
   * here about whether or not it is setup fully. It may be delayed due
   * to device initialization.
   */
  if (pipeline == NULL || !ide_build_pipeline_is_ready (pipeline))
    {
      ide_task_return_new_error (task,
                                 G_IO_ERROR,
                                 G_IO_ERROR_NOT_INITIALIZED,
                                 "The pipeline is not yet ready to handle requests");
      return;
    }

  path = ide_build_pipeline_build_builddir_path (pipeline, "compile_commands.json", NULL);

  if (g_file_test (path, G_FILE_TEST_IS_REGULAR))
    {
      g_autoptr(IdeCompileCommands) compile_commands = NULL;
      g_autoptr(GFile) file = NULL;

      compile_commands = ide_compile_commands_new ();
      file = g_file_new_for_path (path);

      ide_compile_commands_load_async (compile_commands,
                                       file,
                                       cancellable,
                                       gbp_meson_build_system_load_commands_load_cb,
                                       g_steal_pointer (&task));

      gbp_meson_build_system_monitor (self, file);

      return;
    }

  /*
   * It looks like we need to ensure the build pipeline advances to the the
   * CONFIGURE phase so that meson has generated a new compile_commands.json
   * that we can load.
   */

  gbp_meson_build_system_ensure_config_async (self,
                                              cancellable,
                                              gbp_meson_build_system_load_commands_config_cb,
                                              g_steal_pointer (&task));
}

static IdeCompileCommands *
gbp_meson_build_system_load_commands_finish (GbpMesonBuildSystem  *self,
                                             GAsyncResult         *result,
                                             GError              **error)
{
  g_assert (GBP_IS_MESON_BUILD_SYSTEM (self));
  g_assert (IDE_IS_TASK (result));

  return ide_task_propagate_pointer (IDE_TASK (result), error);
}

static void
gbp_meson_build_system_finalize (GObject *object)
{
  GbpMesonBuildSystem *self = (GbpMesonBuildSystem *)object;

  g_clear_object (&self->project_file);
  g_clear_object (&self->compile_commands);
  g_clear_object (&self->monitor);

  G_OBJECT_CLASS (gbp_meson_build_system_parent_class)->finalize (object);
}

static void
gbp_meson_build_system_get_property (GObject    *object,
                                     guint       prop_id,
                                     GValue     *value,
                                     GParamSpec *pspec)
{
  GbpMesonBuildSystem *self = GBP_MESON_BUILD_SYSTEM (object);

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
gbp_meson_build_system_set_property (GObject      *object,
                                     guint         prop_id,
                                     const GValue *value,
                                     GParamSpec   *pspec)
{
  GbpMesonBuildSystem *self = GBP_MESON_BUILD_SYSTEM (object);

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
gbp_meson_build_system_class_init (GbpMesonBuildSystemClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = gbp_meson_build_system_finalize;
  object_class->get_property = gbp_meson_build_system_get_property;
  object_class->set_property = gbp_meson_build_system_set_property;

  properties [PROP_PROJECT_FILE] =
    g_param_spec_object ("project-file",
                         "Project File",
                         "The primary meson.build for the project",
                         G_TYPE_FILE,
                         (G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, N_PROPS, properties);
}

static void
gbp_meson_build_system_init (GbpMesonBuildSystem *self)
{
}

static gchar *
gbp_meson_build_system_get_id (IdeBuildSystem *build_system)
{
  return g_strdup ("meson");
}

static gchar *
gbp_meson_build_system_get_display_name (IdeBuildSystem *build_system)
{
  return g_strdup (_("Meson"));
}

static gint
gbp_meson_build_system_get_priority (IdeBuildSystem *build_system)
{
  return -400;
}

static void
gbp_meson_build_system_get_build_flags_for_files_cb (GObject      *object,
                                                     GAsyncResult *result,
                                                     gpointer      user_data)
{
  GbpMesonBuildSystem *self = (GbpMesonBuildSystem *)object;
  g_autoptr(IdeCompileCommands) compile_commands = NULL;
  g_autoptr(IdeTask) task = user_data;
  g_autoptr(GError) error = NULL;
  g_autoptr(GHashTable) ret = NULL;
  g_auto(GStrv) system_includes = NULL;
  IdeConfigurationManager *config_manager;
  IdeConfiguration *config;
  IdeContext *context;
  IdeRuntime *runtime;
  GPtrArray *files;

  g_assert (GBP_IS_MESON_BUILD_SYSTEM (self));
  g_assert (G_IS_ASYNC_RESULT (result));
  g_assert (IDE_IS_TASK (task));

  if (!(compile_commands = gbp_meson_build_system_load_commands_finish (self, result, &error)))
    {
      ide_task_return_error (task, g_steal_pointer (&error));
      return;
    }

  files = ide_task_get_task_data (task);
  g_assert (files != NULL);

  /* Get non-standard system includes */
  context = ide_object_get_context (IDE_OBJECT (self));
  config_manager = ide_context_get_configuration_manager (context);
  config = ide_configuration_manager_get_current (config_manager);
  if (NULL != (runtime = ide_configuration_get_runtime (config)))
    system_includes = ide_runtime_get_system_include_dirs (runtime);

  ret = g_hash_table_new_full ((GHashFunc)ide_file_hash,
                               (GEqualFunc)ide_file_equal,
                               g_object_unref,
                               (GDestroyNotify)g_strfreev);

  for (guint i = 0; i < files->len; i++)
    {
      IdeFile *file = g_ptr_array_index (files, i);
      GFile *gfile = ide_file_get_file (file);
      g_auto(GStrv) flags = NULL;

      flags = ide_compile_commands_lookup (compile_commands, gfile,
                                           (const gchar * const *)system_includes,
                                           NULL, NULL);
      g_hash_table_insert (ret, g_object_ref (file), g_steal_pointer (&flags));
    }

  ide_task_return_pointer (task, g_steal_pointer (&ret), (GDestroyNotify)g_hash_table_unref);
}

static void
gbp_meson_build_system_get_build_flags_cb (GObject      *object,
                                           GAsyncResult *result,
                                           gpointer      user_data)
{
  GbpMesonBuildSystem *self = (GbpMesonBuildSystem *)object;
  g_autoptr(IdeCompileCommands) compile_commands = NULL;
  g_autoptr(IdeTask) task = user_data;
  g_autoptr(GError) error = NULL;
  g_autoptr(GFile) directory = NULL;
  g_auto(GStrv) system_includes = NULL;
  g_auto(GStrv) ret = NULL;
  IdeConfigurationManager *config_manager;
  IdeContext *context;
  IdeConfiguration *config;
  IdeRuntime *runtime;
  GFile *file;

  g_assert (GBP_IS_MESON_BUILD_SYSTEM (self));
  g_assert (G_IS_ASYNC_RESULT (result));
  g_assert (IDE_IS_TASK (task));

  compile_commands = gbp_meson_build_system_load_commands_finish (self, result, &error);

  if (compile_commands == NULL)
    {
      ide_task_return_error (task, g_steal_pointer (&error));
      return;
    }

  file = ide_task_get_task_data (task);
  g_assert (G_IS_FILE (file));

  /* Get non-standard system includes */
  context = ide_object_get_context (IDE_OBJECT (self));
  config_manager = ide_context_get_configuration_manager (context);
  config = ide_configuration_manager_get_current (config_manager);
  if (NULL != (runtime = ide_configuration_get_runtime (config)))
    system_includes = ide_runtime_get_system_include_dirs (runtime);

  ret = ide_compile_commands_lookup (compile_commands,
                                     file,
                                     (const gchar * const *)system_includes,
                                     &directory,
                                     &error);

  if (ret == NULL)
    ide_task_return_error (task, g_steal_pointer (&error));
  else
    ide_task_return_pointer (task, g_steal_pointer (&ret), (GDestroyNotify)g_strfreev);
}

static void
gbp_meson_build_system_get_build_flags_async (IdeBuildSystem      *build_system,
                                              IdeFile             *file,
                                              GCancellable        *cancellable,
                                              GAsyncReadyCallback  callback,
                                              gpointer             user_data)
{
  GbpMesonBuildSystem *self = (GbpMesonBuildSystem *)build_system;
  g_autoptr(IdeTask) task = NULL;
  GFile *gfile;

  IDE_ENTRY;

  g_assert (GBP_IS_MESON_BUILD_SYSTEM (self));
  g_assert (IDE_IS_FILE (file));
  g_assert (!cancellable || G_IS_CANCELLABLE (cancellable));

  gfile = ide_file_get_file (file);

  task = ide_task_new (self, cancellable, callback, user_data);
  ide_task_set_priority (task, G_PRIORITY_LOW);
  ide_task_set_source_tag (task, gbp_meson_build_system_get_build_flags_async);
  ide_task_set_task_data (task, g_object_ref (gfile), g_object_unref);

  gbp_meson_build_system_load_commands_async (self,
                                              cancellable,
                                              gbp_meson_build_system_get_build_flags_cb,
                                              g_steal_pointer (&task));

  IDE_EXIT;
}

static gchar **
gbp_meson_build_system_get_build_flags_finish (IdeBuildSystem  *build_system,
                                               GAsyncResult    *result,
                                               GError         **error)
{
  g_assert (GBP_IS_MESON_BUILD_SYSTEM (build_system));
  g_assert (IDE_IS_TASK (result));

  return ide_task_propagate_pointer (IDE_TASK (result), error);
}

static void
gbp_meson_build_system_get_build_flags_for_files_async (IdeBuildSystem      *build_system,
                                                        GPtrArray           *files,
                                                        GCancellable        *cancellable,
                                                        GAsyncReadyCallback  callback,
                                                        gpointer             user_data)
{
  GbpMesonBuildSystem *self = (GbpMesonBuildSystem *)build_system;
  g_autoptr(IdeTask) task = NULL;
  g_autoptr(GPtrArray) copy = NULL;

  IDE_ENTRY;

  g_assert (GBP_IS_MESON_BUILD_SYSTEM (self));
  g_assert (files != NULL);
  g_assert (!cancellable || G_IS_CANCELLABLE (cancellable));

  task = ide_task_new (self, cancellable, callback, user_data);
  ide_task_set_source_tag (task, gbp_meson_build_system_get_build_flags_async);
  ide_task_set_priority (task, G_PRIORITY_LOW);

  /* Make our own copy of the array */
  copy = g_ptr_array_new_with_free_func (g_object_unref);
  for (guint i = 0; i < files->len; i++)
    g_ptr_array_add (copy, g_object_ref (g_ptr_array_index (files, i)));
  ide_task_set_task_data (task, g_steal_pointer (&copy), (GDestroyNotify)g_ptr_array_unref);

  gbp_meson_build_system_load_commands_async (self,
                                              cancellable,
                                              gbp_meson_build_system_get_build_flags_for_files_cb,
                                              g_steal_pointer (&task));

  IDE_EXIT;
}

static GHashTable *
gbp_meson_build_system_get_build_flags_for_files_finish (IdeBuildSystem  *build_system,
                                                         GAsyncResult    *result,
                                                         GError         **error)
{
  g_assert (GBP_IS_MESON_BUILD_SYSTEM (build_system));
  g_assert (IDE_IS_TASK (result));

  return ide_task_propagate_pointer (IDE_TASK (result), error);
}

static gchar *
gbp_meson_build_system_get_builddir (IdeBuildSystem   *build_system,
                                     IdeBuildPipeline *pipeline)
{
  GbpMesonBuildSystem *self = (GbpMesonBuildSystem *)build_system;
  IdeConfiguration *config;
  IdeBuildLocality locality;

  g_assert (GBP_IS_MESON_BUILD_SYSTEM (self));
  g_assert (IDE_IS_BUILD_PIPELINE (pipeline));

  /*
   * If the build configuration requires that we do an in tree build (yuck),
   * then use "_build" as our build directory to build in-tree.
   */

  config = ide_build_pipeline_get_configuration (pipeline);
  locality = ide_configuration_get_locality (config);

  if ((locality & IDE_BUILD_LOCALITY_OUT_OF_TREE) == 0)
    {
      g_autoptr(GFile) parent = g_file_get_parent (self->project_file);
      g_autofree gchar *path = g_file_get_path (parent);
      g_autofree gchar *builddir = g_build_filename (path, "_build", NULL);

      return g_steal_pointer (&builddir);
    }

  return NULL;
}

static gboolean
gbp_meson_build_system_supports_toolchain (IdeBuildSystem *self,
                                           IdeToolchain   *toolchain)
{
  g_assert (GBP_IS_MESON_BUILD_SYSTEM (self));
  g_assert (IDE_IS_TOOLCHAIN (toolchain));

  if (GBP_IS_MESON_TOOLCHAIN (toolchain))
    return TRUE;

  return FALSE;
}

static void
build_system_iface_init (IdeBuildSystemInterface *iface)
{
  iface->get_id = gbp_meson_build_system_get_id;
  iface->get_display_name = gbp_meson_build_system_get_display_name;
  iface->get_priority = gbp_meson_build_system_get_priority;
  iface->get_build_flags_async = gbp_meson_build_system_get_build_flags_async;
  iface->get_build_flags_finish = gbp_meson_build_system_get_build_flags_finish;
  iface->get_build_flags_for_files_async = gbp_meson_build_system_get_build_flags_for_files_async;
  iface->get_build_flags_for_files_finish = gbp_meson_build_system_get_build_flags_for_files_finish;
  iface->get_builddir = gbp_meson_build_system_get_builddir;
  iface->supports_toolchain = gbp_meson_build_system_supports_toolchain;
}

static void
gbp_meson_build_system_notify_pipeline (GbpMesonBuildSystem *self,
                                        GParamSpec          *pspec,
                                        IdeBuildManager     *build_manager)
{
  IDE_ENTRY;

  g_assert (GBP_IS_MESON_BUILD_SYSTEM (self));
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
gbp_meson_build_system_init_worker (IdeTask      *task,
                                    gpointer      source_object,
                                    gpointer      task_data,
                                    GCancellable *cancellable)
{
  GFile *project_file = task_data;
  g_autofree gchar *name = NULL;

  IDE_ENTRY;

  g_assert (GBP_IS_MESON_BUILD_SYSTEM (source_object));
  g_assert (G_IS_FILE (project_file));
  g_assert (!cancellable || G_IS_CANCELLABLE (cancellable));

  name = g_file_get_basename (project_file);

  if (dzl_str_equal0 (name, "meson.build"))
    {
      ide_task_return_pointer (task, g_object_ref (project_file), g_object_unref);
      IDE_EXIT;
    }

  if (g_file_query_file_type (project_file, 0, cancellable) == G_FILE_TYPE_DIRECTORY)
    {
      g_autoptr(GFile) meson_build = g_file_get_child (project_file, "meson.build");

      if (g_file_query_exists (meson_build, cancellable))
        {
          ide_task_return_pointer (task, g_object_ref (meson_build), g_object_unref);
          IDE_EXIT;
        }
    }

  ide_task_return_new_error (task,
                             G_IO_ERROR,
                             G_IO_ERROR_NOT_SUPPORTED,
                             "%s is not supported by the meson plugin",
                             name);

  IDE_EXIT;
}

static void
gbp_meson_build_system_init_async (GAsyncInitable      *initable,
                                   gint                 io_priority,
                                   GCancellable        *cancellable,
                                   GAsyncReadyCallback  callback,
                                   gpointer             user_data)
{
  GbpMesonBuildSystem *self = (GbpMesonBuildSystem *)initable;
  g_autoptr(IdeTask) task = NULL;
  IdeBuildManager *build_manager;
  IdeContext *context;

  IDE_ENTRY;

  g_assert (GBP_IS_MESON_BUILD_SYSTEM (self));
  g_assert (G_IS_FILE (self->project_file));
  g_assert (!cancellable || G_IS_CANCELLABLE (cancellable));

  context = ide_object_get_context (IDE_OBJECT (self));
  g_assert (IDE_IS_CONTEXT (context));

  build_manager = ide_context_get_build_manager (context);
  g_assert (IDE_IS_BUILD_MANAGER (build_manager));

  task = ide_task_new (self, cancellable, callback, user_data);
  ide_task_set_source_tag (task, gbp_meson_build_system_init_async);
  ide_task_set_priority (task, io_priority);
  ide_task_set_task_data (task, g_object_ref (self->project_file), g_object_unref);

  /*
   * We want to be notified of any changes to the current build manager.
   * This will let us invalidate our compile_commands.json when it changes.
   */
  g_signal_connect_object (build_manager,
                           "notify::pipeline",
                           G_CALLBACK (gbp_meson_build_system_notify_pipeline),
                           self,
                           G_CONNECT_SWAPPED);

  ide_task_run_in_thread (task, gbp_meson_build_system_init_worker);

  IDE_EXIT;
}

static gboolean
gbp_meson_build_system_init_finish (GAsyncInitable  *initable,
                                    GAsyncResult    *result,
                                    GError         **error)
{
  GbpMesonBuildSystem *self = (GbpMesonBuildSystem *)initable;
  g_autoptr(GFile) project_file = NULL;

  IDE_ENTRY;

  g_assert (GBP_IS_MESON_BUILD_SYSTEM (self));
  g_assert (IDE_IS_TASK (result));

  project_file = ide_task_propagate_pointer (IDE_TASK (result), error);
  if (g_set_object (&self->project_file, project_file))
    g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_PROJECT_FILE]);

  IDE_RETURN (project_file != NULL);
}

static void
async_initable_iface_init (GAsyncInitableIface *iface)
{
  iface->init_async = gbp_meson_build_system_init_async;
  iface->init_finish = gbp_meson_build_system_init_finish;
}
