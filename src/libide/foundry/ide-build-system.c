/* ide-build-system.c
 *
 * Copyright 2015-2019 Christian Hergert <christian@hergert.me>
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

#define G_LOG_DOMAIN "ide-build-system"

#include "config.h"

#include <libide-code.h>
#include <libide-projects.h>
#include <libide-threading.h>
#include <libide-vcs.h>
#include <string.h>

#include "ide-build-manager.h"
#include "ide-build-pipeline.h"
#include "ide-build-system.h"
#include "ide-configuration.h"
#include "ide-device.h"
#include "ide-foundry-compat.h"
#include "ide-toolchain.h"

G_DEFINE_INTERFACE (IdeBuildSystem, ide_build_system, IDE_TYPE_OBJECT)

enum {
  PROP_0,
  PROP_PROJECT_FILE,
  N_PROPS
};

typedef struct
{
  GPtrArray   *files;
  GHashTable  *flags;
  guint        index;
} GetBuildFlagsData;

static GParamSpec *properties [N_PROPS];

static void
get_build_flags_data_free (GetBuildFlagsData *data)
{
  g_clear_pointer (&data->files, g_ptr_array_unref);
  g_clear_pointer (&data->flags, g_hash_table_unref);
  g_slice_free (GetBuildFlagsData, data);
}

gint
ide_build_system_get_priority (IdeBuildSystem *self)
{
  IdeBuildSystemInterface *iface;

  g_return_val_if_fail (IDE_IS_BUILD_SYSTEM (self), 0);

  iface = IDE_BUILD_SYSTEM_GET_IFACE (self);

  if (iface->get_priority != NULL)
    return iface->get_priority (self);

  return 0;
}

static void
ide_build_system_real_get_build_flags_async (IdeBuildSystem      *self,
                                             GFile               *file,
                                             GCancellable        *cancellable,
                                             GAsyncReadyCallback  callback,
                                             gpointer             user_data)
{
  ide_task_report_new_error (self,
                             callback,
                             user_data,
                             ide_build_system_real_get_build_flags_async,
                             G_IO_ERROR,
                             G_IO_ERROR_NOT_SUPPORTED,
                             "Fetching build flags is not supported");
}

static gchar **
ide_build_system_real_get_build_flags_finish (IdeBuildSystem  *self,
                                              GAsyncResult    *result,
                                              GError         **error)
{
  return ide_task_propagate_pointer (IDE_TASK (result), error);
}

static void
ide_build_system_get_build_flags_cb (GObject      *object,
                                     GAsyncResult *result,
                                     gpointer      user_data)
{
  IdeBuildSystem *self = (IdeBuildSystem *)object;
  g_autoptr(IdeTask) task = user_data;
  g_autoptr(GError) error = NULL;
  g_auto(GStrv) flags = NULL;
  GetBuildFlagsData *data;
  GFile *file;

  g_assert (IDE_IS_BUILD_SYSTEM (self));
  g_assert (G_IS_ASYNC_RESULT (result));
  g_assert (IDE_IS_TASK (task));

  data = ide_task_get_task_data (task);
  g_assert (data != NULL);
  g_assert (data->files != NULL);
  g_assert (data->files->len > 0);
  g_assert (data->index < data->files->len);
  g_assert (data->flags != NULL);

  file = g_ptr_array_index (data->files, data->index);
  g_assert (G_IS_FILE (file));

  data->index++;

  flags = ide_build_system_get_build_flags_finish (self, result, &error);

  if (error != NULL)
    g_debug ("Failed to load build flags for \"%s\": %s",
             g_file_peek_path (file),
             error->message);
  else
    g_hash_table_insert (data->flags, g_object_ref (file), g_steal_pointer (&flags));

  if (ide_task_return_error_if_cancelled (task))
    return;

  if (data->index < data->files->len)
    {
      GCancellable *cancellable = ide_task_get_cancellable (task);

      file = g_ptr_array_index (data->files, data->index);
      g_assert (G_IS_FILE (file));

      ide_build_system_get_build_flags_async (self,
                                              file,
                                              cancellable,
                                              ide_build_system_get_build_flags_cb,
                                              g_steal_pointer (&task));
      return;
    }

  ide_task_return_pointer (task,
                           g_steal_pointer (&data->flags),
                           (GDestroyNotify)g_hash_table_unref);
}

static GPtrArray *
copy_files (GPtrArray *in)
{
  GPtrArray *out = g_ptr_array_new_full (in->len, g_object_unref);
  for (guint i = 0; i < in->len; i++)
    g_ptr_array_add (out, g_file_dup (g_ptr_array_index (in, i)));
  return g_steal_pointer (&out);
}

static void
ide_build_system_real_get_build_flags_for_files_async (IdeBuildSystem       *self,
                                                       GPtrArray            *files,
                                                       GCancellable         *cancellable,
                                                       GAsyncReadyCallback   callback,
                                                       gpointer              user_data)
{
  g_autoptr(IdeTask) task = NULL;
  GetBuildFlagsData *data;

  g_return_if_fail (IDE_IS_BUILD_SYSTEM (self));
  g_return_if_fail (files != NULL);
  g_return_if_fail (!cancellable || G_IS_CANCELLABLE (cancellable));

  task = ide_task_new (self, cancellable, callback, user_data);
  ide_task_set_source_tag (task, ide_build_system_real_get_build_flags_for_files_async);
  ide_task_set_priority (task, G_PRIORITY_LOW);

  if (files->len == 0)
    {
      ide_task_return_new_error (task,
                                 G_IO_ERROR,
                                 G_IO_ERROR_INVAL,
                                 "No files were provided");
      return;
    }

  g_assert (files->len > 0);
  g_assert (G_IS_FILE (g_ptr_array_index (files, 0)));

  if (ide_task_return_error_if_cancelled (task))
    return;

  data = g_slice_new0 (GetBuildFlagsData);
  data->files = copy_files (files);
  data->flags = g_hash_table_new_full ((GHashFunc)g_file_hash,
                                       (GEqualFunc)g_file_equal,
                                       g_object_unref,
                                       (GDestroyNotify)g_strfreev);
  ide_task_set_task_data (task, data, get_build_flags_data_free);

  ide_build_system_get_build_flags_async (self,
                                          g_ptr_array_index (files, 0),
                                          cancellable,
                                          ide_build_system_get_build_flags_cb,
                                          g_steal_pointer (&task));
}

static GHashTable *
ide_build_system_real_get_build_flags_for_files_finish (IdeBuildSystem       *self,
                                                        GAsyncResult         *result,
                                                        GError              **error)
{
  return ide_task_propagate_pointer (IDE_TASK (result), error);
}

static void
ide_build_system_default_init (IdeBuildSystemInterface *iface)
{
  iface->get_build_flags_async = ide_build_system_real_get_build_flags_async;
  iface->get_build_flags_finish = ide_build_system_real_get_build_flags_finish;
  iface->get_build_flags_for_files_async = ide_build_system_real_get_build_flags_for_files_async;
  iface->get_build_flags_for_files_finish = ide_build_system_real_get_build_flags_for_files_finish;

  properties [PROP_PROJECT_FILE] =
    g_param_spec_object ("project-file",
                         "Project File",
                         "The project file.",
                         G_TYPE_FILE,
                         (G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS));

  g_object_interface_install_property (iface, properties [PROP_PROJECT_FILE]);
}

static gchar *
ide_build_system_translate (IdeBuildSystem   *self,
                            IdeBuildPipeline *pipeline,
                            const gchar      *prefix,
                            const gchar      *path)
{
  g_autofree gchar *freeme = NULL;
  g_autofree gchar *translated_path = NULL;
  g_autoptr(GFile) file = NULL;
  g_autoptr(GFile) translated = NULL;
  IdeRuntime *runtime;

  g_assert (IDE_IS_BUILD_SYSTEM (self));
  g_assert (!pipeline || IDE_IS_BUILD_PIPELINE (pipeline));
  g_assert (prefix != NULL);
  g_assert (path != NULL);

  if (NULL == pipeline ||
      NULL == (runtime = ide_build_pipeline_get_runtime (pipeline)))
    return g_strdup_printf ("%s%s", prefix, path);

  if (!g_path_is_absolute (path))
    path = freeme = ide_build_pipeline_build_builddir_path (pipeline, path, NULL);

  file = g_file_new_for_path (path);
  translated = ide_runtime_translate_file (runtime, file);
  translated_path = g_file_get_path (translated);

  return g_strdup_printf ("%s%s", prefix, translated_path);
}

static void
ide_build_system_post_process_build_flags (IdeBuildSystem  *self,
                                           gchar          **flags)
{
  IdeBuildPipeline *pipeline;
  IdeBuildManager *build_manager;
  IdeContext *context;

  g_assert (IDE_IS_BUILD_SYSTEM (self));

  if (flags == NULL || flags[0] == NULL)
    return;

  context = ide_object_get_context (IDE_OBJECT (self));
  build_manager = ide_build_manager_from_context (context);
  pipeline = ide_build_manager_get_pipeline (build_manager);

  for (guint i = 0; flags[i] != NULL; i++)
    {
      gchar *flag = flags[i];
      gchar *translated;

      if (flag[0] != '-')
        continue;

      switch (flag[1])
        {
        case 'I':
          if (flag[2] == '\0')
            {
              if (flags[i+1] != NULL)
                {
                  translated = ide_build_system_translate (self, pipeline, "", flags[++i]);
                  flags[i] = translated;
                  g_free (flag);
                }
            }
          else
            {
              translated = ide_build_system_translate (self, pipeline, "-I", &flag[2]);
              flags[i] = translated;
              g_free (flag);
            }
          break;

        case 'D':
        case 'x':
          if (strlen (flag) == 2)
            i++;
          break;

        case 'f': /* -fPIC */
        case 'W': /* -Werror... */
        case 'm': /* -m64 -mtune=native */
        default:
          break;
        }
    }
}

void
ide_build_system_get_build_flags_async (IdeBuildSystem      *self,
                                        GFile               *file,
                                        GCancellable        *cancellable,
                                        GAsyncReadyCallback  callback,
                                        gpointer             user_data)
{
  IDE_ENTRY;

  g_return_if_fail (IDE_IS_BUILD_SYSTEM (self));
  g_return_if_fail (G_IS_FILE (file));
  g_return_if_fail (!cancellable || G_IS_CANCELLABLE (cancellable));

  IDE_BUILD_SYSTEM_GET_IFACE (self)->get_build_flags_async (self, file, cancellable, callback, user_data);

  IDE_EXIT;
}

/**
 * ide_build_system_get_build_flags_finish:
 *
 * Returns: (transfer full):
 *
 * Since: 3.32
 */
gchar **
ide_build_system_get_build_flags_finish (IdeBuildSystem  *self,
                                         GAsyncResult    *result,
                                         GError         **error)
{
  gchar **ret;

  IDE_ENTRY;

  g_return_val_if_fail (IDE_IS_BUILD_SYSTEM (self), NULL);
  g_return_val_if_fail (G_IS_ASYNC_RESULT (result), NULL);

  ret = IDE_BUILD_SYSTEM_GET_IFACE (self)->get_build_flags_finish (self, result, error);
  if (ret != NULL)
    ide_build_system_post_process_build_flags (self, ret);

  IDE_RETURN (ret);
}

/**
 * ide_build_system_get_build_flags_for_files_async:
 * @self: An #IdeBuildSystem instance.
 * @files: (element-type GFile) (transfer none): array of files whose build flags has to be retrieved.
 * @cancellable: (allow-none): a #GCancellable to cancel getting build flags.
 * @callback: function to be called after getting build flags.
 * @user_data: data to pass to @callback.
 *
 * This function will get build flags for all files and returns
 * map of file and its build flags as #GHashTable.
 *
 * Since: 3.32
 */
void
ide_build_system_get_build_flags_for_files_async (IdeBuildSystem       *self,
                                                  GPtrArray            *files,
                                                  GCancellable         *cancellable,
                                                  GAsyncReadyCallback   callback,
                                                  gpointer              user_data)
{
  IDE_ENTRY;

  g_return_if_fail (IDE_IS_BUILD_SYSTEM (self));
  g_return_if_fail ( files != NULL);
  g_return_if_fail (!cancellable || G_IS_CANCELLABLE (cancellable));

  IDE_BUILD_SYSTEM_GET_IFACE (self)->get_build_flags_for_files_async (self, files, cancellable, callback, user_data);

  IDE_EXIT;
}

/**
 * ide_build_system_get_build_flags_for_files_finish:
 * @self: an #IdeBuildSystem
 * @result: a #GAsyncResult
 * @error: a location for a #GError or %NULL
 *
 * Returns: (element-type Ide.File GStrv) (transfer full): a #GHashTable or #GFile to #GStrv
 *
 * Since: 3.32
 */
GHashTable *
ide_build_system_get_build_flags_for_files_finish (IdeBuildSystem  *self,
                                                   GAsyncResult    *result,
                                                   GError         **error)
{
  GHashTable *ret;

  IDE_ENTRY;

  g_return_val_if_fail (IDE_IS_BUILD_SYSTEM (self), NULL);
  g_return_val_if_fail (G_IS_ASYNC_RESULT (result), NULL);

  ret = IDE_BUILD_SYSTEM_GET_IFACE (self)->get_build_flags_for_files_finish (self, result, error);

  if (ret != NULL)
    {
      GHashTableIter iter;
      gchar **flags;

      g_hash_table_iter_init (&iter, ret);

      while (g_hash_table_iter_next (&iter, NULL, (gpointer *)&flags))
        ide_build_system_post_process_build_flags (self, flags);
    }

  IDE_RETURN (ret);
}

gchar *
ide_build_system_get_builddir (IdeBuildSystem   *self,
                               IdeBuildPipeline *pipeline)
{
  gchar *ret = NULL;

  IDE_ENTRY;

  g_return_val_if_fail (IDE_IS_BUILD_SYSTEM (self), NULL);
  g_return_val_if_fail (IDE_IS_BUILD_PIPELINE (pipeline), NULL);

  if (IDE_BUILD_SYSTEM_GET_IFACE (self)->get_builddir)
    ret = IDE_BUILD_SYSTEM_GET_IFACE (self)->get_builddir (self, pipeline);

  if (ret == NULL)
    {
      g_autofree gchar *name = NULL;
      g_autofree gchar *branch = NULL;
      IdeConfiguration *config;
      const gchar *config_id;
      const gchar *runtime_id;
      IdeRuntime *runtime;
      IdeContext *context;
      IdeVcs *vcs;

      context = ide_object_get_context (IDE_OBJECT (self));
      vcs = ide_vcs_from_context (context);
      config = ide_build_pipeline_get_configuration (pipeline);
      config_id = ide_configuration_get_id (config);
      runtime = ide_build_pipeline_get_runtime (pipeline);
      runtime_id = ide_runtime_get_id (runtime);
      branch = ide_vcs_get_branch_name (vcs);

      if (branch != NULL)
        name = g_strdup_printf ("%s-%s-%s", config_id, runtime_id, branch);
      else
        name = g_strdup_printf ("%s-%s", config_id, runtime_id);

      g_strdelimit (name, "@:/ ", '-');

      ret = ide_context_cache_filename (context, "builds", name, NULL);
    }

  IDE_RETURN (ret);
}

gchar *
ide_build_system_get_id (IdeBuildSystem *self)
{
  g_return_val_if_fail (IDE_IS_BUILD_SYSTEM (self), NULL);

  if (IDE_BUILD_SYSTEM_GET_IFACE (self)->get_id)
    return IDE_BUILD_SYSTEM_GET_IFACE (self)->get_id (self);

  return g_strdup (G_OBJECT_TYPE_NAME (self));
}

gchar *
ide_build_system_get_display_name (IdeBuildSystem *self)
{
  g_return_val_if_fail (IDE_IS_BUILD_SYSTEM (self), NULL);

  if (IDE_BUILD_SYSTEM_GET_IFACE (self)->get_display_name)
    return IDE_BUILD_SYSTEM_GET_IFACE (self)->get_display_name (self);

  return ide_build_system_get_id (self);
}

static void
ide_build_system_get_build_flags_for_dir_cb2 (GObject      *object,
                                              GAsyncResult *result,
                                              gpointer      user_data)
{
  IdeBuildSystem *build_system = (IdeBuildSystem *)object;
  g_autoptr(IdeTask) task = user_data;
  g_autoptr(GError) error = NULL;
  g_autoptr(GHashTable) ret = NULL;

  g_assert (IDE_IS_BUILD_SYSTEM (build_system));
  g_assert (G_IS_ASYNC_RESULT (result));
  g_assert (IDE_IS_TASK (task));

  ret = ide_build_system_get_build_flags_for_files_finish (build_system, result, &error);

  if (ret == NULL)
    ide_task_return_error (task, g_steal_pointer (&error));
  else
    ide_task_return_pointer (task,
                             g_steal_pointer (&ret),
                             (GDestroyNotify)g_hash_table_unref);
}

static void
ide_build_system_get_build_flags_for_dir_cb (GObject      *object,
                                             GAsyncResult *result,
                                             gpointer      user_data)
{
  GFile *dir = (GFile *)object;
  g_autoptr(IdeTask) task = user_data;
  g_autoptr(GError) error = NULL;
  g_autoptr(GPtrArray) infos = NULL;
  g_autoptr(GPtrArray) files = NULL;
  IdeBuildSystem *self;
  GCancellable *cancellable;
  IdeContext *context;
  IdeVcs *vcs;

  g_assert (G_IS_FILE (dir));
  g_assert (G_IS_ASYNC_RESULT (result));
  g_assert (IDE_IS_TASK (task));

  infos = ide_g_file_get_children_finish (dir, result, &error);
  IDE_PTR_ARRAY_SET_FREE_FUNC (infos, g_object_unref);

  if (infos == NULL)
    {
      ide_task_return_error (task, g_steal_pointer (&error));
      return;
    }

  self = ide_task_get_source_object (task);
  context = ide_object_get_context (IDE_OBJECT (self));
  vcs = ide_vcs_from_context (context);
  cancellable = ide_task_get_cancellable (task);
  files = g_ptr_array_new_with_free_func (g_object_unref);

  for (guint i = 0; i < infos->len; i++)
    {
      GFileInfo *file_info = g_ptr_array_index (infos, i);
      GFileType file_type = g_file_info_get_file_type (file_info);

      if (file_type == G_FILE_TYPE_REGULAR)
        {
          const gchar *name = g_file_info_get_name (file_info);
          g_autoptr(GFile) child = g_file_get_child (dir, name);

          if (!ide_vcs_is_ignored (vcs, child, NULL))
            g_ptr_array_add (files, g_steal_pointer (&child));
        }
    }

  ide_build_system_get_build_flags_for_files_async (self,
                                                    files,
                                                    cancellable,
                                                    ide_build_system_get_build_flags_for_dir_cb2,
                                                    g_steal_pointer (&task));
}

void
ide_build_system_get_build_flags_for_dir_async (IdeBuildSystem      *self,
                                                GFile               *directory,
                                                GCancellable        *cancellable,
                                                GAsyncReadyCallback  callback,
                                                gpointer             user_data)
{
  g_autoptr(IdeTask) task = NULL;

  g_return_if_fail (IDE_IS_BUILD_SYSTEM (self));
  g_return_if_fail (G_IS_FILE (directory));
  g_return_if_fail (!cancellable || G_IS_CANCELLABLE (cancellable));

  task = ide_task_new (self, cancellable, callback, user_data);
  ide_task_set_source_tag (task, ide_build_system_get_build_flags_for_dir_async);
  ide_task_set_priority (task, G_PRIORITY_LOW);

  ide_g_file_get_children_async (directory,
                                 G_FILE_ATTRIBUTE_STANDARD_NAME","
                                 G_FILE_ATTRIBUTE_STANDARD_TYPE,
                                 G_FILE_QUERY_INFO_NONE,
                                 G_PRIORITY_LOW,
                                 cancellable,
                                 ide_build_system_get_build_flags_for_dir_cb,
                                 g_steal_pointer (&task));
}

/**
 * ide_build_system_get_build_flags_for_dir_finish:
 * @self: an #IdeBuildSystem
 * @result: a #GAsyncResult
 * @error: a location for a #GError or %NULL
 *
 * Returns: (element-type Ide.File GStrv) (transfer full): a #GHashTable of #GFile to #GStrv
 *
 * Since: 3.32
 */
GHashTable *
ide_build_system_get_build_flags_for_dir_finish (IdeBuildSystem  *self,
                                                 GAsyncResult    *result,
                                                 GError         **error)
{
  g_return_val_if_fail (IDE_IS_BUILD_SYSTEM (self), NULL);
  g_return_val_if_fail (G_IS_ASYNC_RESULT (result), NULL);

  return ide_task_propagate_pointer (IDE_TASK (result), error);
}

/**
 * ide_build_system_supports_toolchain:
 * @self: an #IdeBuildSystem
 * @toolchain: a #IdeToolchain
 *
 * Checks whether the build system supports the given toolchain.
 *
 * Returns: %TRUE if the toolchain is supported by the build system, %FALSE otherwise
 *
 * Since: 3.32
 */
gboolean
ide_build_system_supports_toolchain (IdeBuildSystem *self,
                                     IdeToolchain   *toolchain)
{
  const gchar *toolchain_id;

  g_return_val_if_fail (IDE_IS_BUILD_SYSTEM (self), FALSE);
  g_return_val_if_fail (IDE_IS_TOOLCHAIN (toolchain), FALSE);

  toolchain_id = ide_toolchain_get_id (toolchain);
  if (g_strcmp0 (toolchain_id, "default") == 0)
    return TRUE;

  if (IDE_BUILD_SYSTEM_GET_IFACE (self)->supports_toolchain)
    return IDE_BUILD_SYSTEM_GET_IFACE (self)->supports_toolchain (self, toolchain);

  return FALSE;
}
