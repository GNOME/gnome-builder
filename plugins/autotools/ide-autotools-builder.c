/* ide-autotools-builder.c
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

#include <glib/gi18n.h>
#include <ide.h>

#include "ide-autotools-build-task.h"
#include "ide-autotools-builder.h"

struct _IdeAutotoolsBuilder
{
  IdeBuilder parent_instance;
};

G_DEFINE_TYPE (IdeAutotoolsBuilder, ide_autotools_builder, IDE_TYPE_BUILDER)

static void
ide_autotools_builder_build_cb (GObject      *object,
                                GAsyncResult *result,
                                gpointer      user_data)
{
  g_autoptr(GTask) task = user_data;
  IdeAutotoolsBuildTask *build_result = (IdeAutotoolsBuildTask *)object;
  GError *error = NULL;

  g_return_if_fail (IDE_IS_AUTOTOOLS_BUILD_TASK (build_result));
  g_return_if_fail (G_IS_TASK (task));

  ide_build_result_set_running (IDE_BUILD_RESULT (build_result), FALSE);

  if (!ide_autotools_build_task_execute_finish (build_result, result, &error))
    {
      if (g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CANCELLED))
        ide_build_result_set_mode (IDE_BUILD_RESULT (build_result), _("Cancelled"));
      else
        ide_build_result_set_mode (IDE_BUILD_RESULT (build_result), _("Failed"));

      g_task_return_error (task, error);
      return;
    }

  ide_build_result_set_mode (IDE_BUILD_RESULT (build_result), _("Success"));

  g_task_return_pointer (task, g_object_ref (build_result), g_object_unref);
}

/**
 * ide_autotools_builder_get_build_directory:
 *
 * Gets the directory that will contain the generated makefiles and build root.
 *
 * Returns: (transfer full): A #GFile containing the build directory.
 */
GFile *
ide_autotools_builder_get_build_directory (IdeAutotoolsBuilder *self)
{
  g_autofree gchar *path = NULL;
  IdeConfiguration *configuration;
  IdeContext *context;
  IdeProject *project;
  IdeDevice *device;
  const gchar *root_build_dir;
  const gchar *project_name;
  const gchar *device_id;
  const gchar *system_type;

  g_return_val_if_fail (IDE_IS_AUTOTOOLS_BUILDER (self), NULL);

  context = ide_object_get_context (IDE_OBJECT (self));

  configuration = ide_builder_get_configuration (IDE_BUILDER (self));

  device = ide_configuration_get_device (configuration);
  device_id = ide_device_get_id (device);

  /*
   * If this is the local device, we have a special workaround for building within the project
   * tree. Generally we want to be doing out of tree builds, but a lot of people are going to
   * fire up their project from jhbuild or similar, and build in tree.
   *
   * This workaround will let us continue building their project in that location, with the
   * caveat that we will need to `make distclean` later if they want to build for another device.
   */
  if (0 == g_strcmp0 (device_id, "local"))
    {
      IdeVcs *vcs;
      GFile *working_directory;
      g_autoptr(GFile) makefile_file = NULL;
      g_autofree gchar *makefile_path = NULL;

      vcs = ide_context_get_vcs (context);
      working_directory = ide_vcs_get_working_directory (vcs);
      makefile_file = g_file_get_child (working_directory, "Makefile");
      makefile_path = g_file_get_path (makefile_file);

      /*
       * NOTE:
       *
       * It would be nice if this was done asynchronously, but if this isn't fast, we will have
       * stalled in so many other places that the app will probably be generally unusable. So
       * I'm going to cheat for now and make this function synchronous.
       */
      if (g_file_test (makefile_path, G_FILE_TEST_EXISTS))
        return g_object_ref (working_directory);
    }

  project = ide_context_get_project (context);
  root_build_dir = ide_context_get_root_build_dir (context);
  system_type = ide_device_get_system_type (device);
  project_name = ide_project_get_name (project);
  path = g_build_filename (root_build_dir, project_name, device_id, system_type, NULL);

  return g_file_new_for_path (path);
}

static void
ide_autotools_builder_build_async (IdeBuilder           *builder,
                                   IdeBuilderBuildFlags  flags,
                                   IdeBuildResult      **result,
                                   GCancellable         *cancellable,
                                   GAsyncReadyCallback   callback,
                                   gpointer              user_data)
{
  IdeAutotoolsBuilder *self = (IdeAutotoolsBuilder *)builder;
  g_autoptr(IdeAutotoolsBuildTask) build_result = NULL;
  g_autoptr(GTask) task = NULL;
  g_autoptr(GFile) directory = NULL;
  IdeConfiguration *configuration;
  IdeContext *context;

  g_return_if_fail (IDE_IS_AUTOTOOLS_BUILDER (builder));
  g_return_if_fail (IDE_IS_AUTOTOOLS_BUILDER (self));

  if (ide_autotools_builder_get_needs_bootstrap (self))
    flags |= IDE_BUILDER_BUILD_FLAGS_FORCE_BOOTSTRAP;

  task = g_task_new (self, cancellable, callback, user_data);

  context = ide_object_get_context (IDE_OBJECT (builder));
  configuration = ide_builder_get_configuration (IDE_BUILDER (self));
  directory = ide_autotools_builder_get_build_directory (self);
  build_result = g_object_new (IDE_TYPE_AUTOTOOLS_BUILD_TASK,
                               "context", context,
                               "configuration", configuration,
                               "directory", directory,
                               "mode", _("Building…"),
                               "running", TRUE,
                               NULL);

  if (result)
    *result = g_object_ref (build_result);

  ide_autotools_build_task_execute_async (build_result,
                                          flags,
                                          cancellable,
                                          ide_autotools_builder_build_cb,
                                          g_object_ref (task));
}

static IdeBuildResult *
ide_autotools_builder_build_finish (IdeBuilder    *builder,
                                    GAsyncResult  *result,
                                    GError       **error)
{
  GTask *task = (GTask *)result;

  g_return_val_if_fail (IDE_IS_AUTOTOOLS_BUILDER (builder), NULL);
  g_return_val_if_fail (G_IS_TASK (task), NULL);

  return g_task_propagate_pointer (task, error);
}

static void
ide_autotools_builder_class_init (IdeAutotoolsBuilderClass *klass)
{
  IdeBuilderClass *builder_class = IDE_BUILDER_CLASS (klass);

  builder_class->build_async = ide_autotools_builder_build_async;
  builder_class->build_finish = ide_autotools_builder_build_finish;
}

static void
ide_autotools_builder_init (IdeAutotoolsBuilder *self)
{
}

gboolean
ide_autotools_builder_get_needs_bootstrap (IdeAutotoolsBuilder *self)
{
  g_autoptr(GFile) configure = NULL;
  GFile *working_directory = NULL;
  IdeConfiguration *configuration;
  IdeContext *context;
  IdeVcs *vcs;

  g_return_val_if_fail (IDE_IS_AUTOTOOLS_BUILDER (self), FALSE);

  context = ide_object_get_context (IDE_OBJECT (self));
  vcs = ide_context_get_vcs (context);
  working_directory = ide_vcs_get_working_directory (vcs);
  configure = g_file_get_child (working_directory, "configure");

  if (!g_file_query_exists (configure, NULL))
    return TRUE;

  configuration = ide_builder_get_configuration (IDE_BUILDER (self));
  if (ide_configuration_get_dirty (configuration))
    return TRUE;

  /*
   * TODO:
   *
   * We might also want to check for dependent files being out of date. For example, if autogen.sh
   * is newer than configure, we should bootstrap. Of course, once we go this far, I'd prefer
   * to make this function asynchronous.
   */

  return FALSE;
}
