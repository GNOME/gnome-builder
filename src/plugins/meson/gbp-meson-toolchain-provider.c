/* gbp-meson-toolchain-provider.c
 *
 * Copyright 2018 Collabora Ltd.
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
 * Authors: Corentin Noël <corentin.noel@collabora.com>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#define G_LOG_DOMAIN "gbp-meson-toolchain-provider"

#include "gbp-meson-build-system.h"
#include "gbp-meson-toolchain.h"
#include "gbp-meson-toolchain-provider.h"
#include "gbp-meson-build-system.h"

#define MESON_TOOLCHAIN_FIND_MAX_DEPTH 3

struct _GbpMesonToolchainProvider
{
  IdeObject            parent_instance;
  GPtrArray           *toolchains;
};

typedef struct
{
  GList     *folders;
  GPtrArray *found_files;
} FileSearching;

static FileSearching *
file_searching_new (void)
{
  FileSearching *file_searching;

  file_searching = g_slice_new0 (FileSearching);
  file_searching->found_files = g_ptr_array_new_with_free_func (g_object_unref);

  return file_searching;
}

static void
file_searching_free (FileSearching *fs)
{
  g_clear_pointer (&fs->found_files, g_ptr_array_unref);

  if (fs->folders)
    {
      g_list_free_full (fs->folders, g_object_unref);
      fs->folders = NULL;
    }

  g_slice_free (FileSearching, fs);
}

static void
gbp_meson_toolchain_provider_load_worker (IdeTask      *task,
                                          gpointer      source_object,
                                          gpointer      task_data,
                                          GCancellable *cancellable)
{
  GbpMesonToolchainProvider *self = source_object;
  g_autoptr(GPtrArray) toolchains = NULL;
  GPtrArray *files = task_data;
  IdeContext *context;

  g_assert (IDE_IS_TASK (task));
  g_assert (GBP_IS_MESON_TOOLCHAIN_PROVIDER (self));
  g_assert (files != NULL);
  g_assert (!cancellable || G_IS_CANCELLABLE (cancellable));

  context = ide_object_get_context (IDE_OBJECT (self));
  toolchains = g_ptr_array_new_with_free_func (g_object_unref);

  for (guint i = 0; i < files->len; i++)
    {
      GFile *file = g_ptr_array_index (files, i);
      g_autoptr(GFileInfo) file_info = NULL;
      g_autoptr(GError) file_error = NULL;
      const gchar *content_type;

      g_assert (G_IS_FILE (file));

      file_info = g_file_query_info (file,
                                     G_FILE_ATTRIBUTE_STANDARD_CONTENT_TYPE,
                                     G_FILE_QUERY_INFO_NONE,
                                     cancellable,
                                     &file_error);
      content_type = g_file_info_get_content_type (file_info);
      if (g_content_type_is_mime_type (content_type, "text/plain"))
        {
          g_autoptr(GKeyFile) keyfile = g_key_file_new ();
          g_autofree gchar *path = g_file_get_path (file);
          g_autoptr(GError) keyfile_error = NULL;

          if (g_key_file_load_from_file (keyfile, path, G_KEY_FILE_NONE, &keyfile_error))
            {
              if (g_key_file_has_group (keyfile, "binaries") &&
                  (g_key_file_has_group (keyfile, "host_machine") ||
                   g_key_file_has_group (keyfile, "target_machine")))
                {
                  g_autoptr(GError) toolchain_error = NULL;
                  g_autoptr(GbpMesonToolchain) toolchain = gbp_meson_toolchain_new (context);

                  if (!gbp_meson_toolchain_load (toolchain, file, &toolchain_error))
                    {
                      g_debug ("Error loading %s: %s", path, toolchain_error->message);
                      continue;
                    }

                  g_ptr_array_add (toolchains, g_steal_pointer (&toolchain));
                }
            }
        }
    }

  ide_task_return_pointer (task,
                           g_steal_pointer (&toolchains),
                           g_ptr_array_unref);
}

static void
add_all_files (GFile     *array,
               GPtrArray *dest_array)
{
  g_assert (G_IS_FILE (array));
  g_assert (dest_array != NULL);

  g_ptr_array_add (dest_array, g_object_ref (array));
}

static void
meson_toolchain_provider_search_iterate (GObject      *object,
                                         GAsyncResult *result,
                                         gpointer      user_data)
{
  GFile *file = (GFile *)object;
  g_autoptr(IdeTask) task = user_data;
  g_autoptr(GPtrArray) ret = NULL;
  g_autoptr(GError) error = NULL;
  FileSearching *fs;

  IDE_ENTRY;

  g_assert (G_IS_FILE (file));
  g_assert (G_IS_ASYNC_RESULT (result));
  g_assert (IDE_IS_TASK (task));

  fs = ide_task_get_task_data (task);

  g_assert (fs != NULL);
  g_assert (fs->found_files != NULL);
  g_assert (fs->folders != NULL);

  ret = ide_g_file_find_finish (file, result, &error);
  IDE_PTR_ARRAY_SET_FREE_FUNC (ret, g_object_unref);

  if (ret == NULL)
    {
      ide_task_return_error (task, g_steal_pointer (&error));
      IDE_EXIT;
    }

  g_ptr_array_foreach (ret, (GFunc)add_all_files, fs->found_files);

  /* Remove the item from the list, ensuring we drop our
   * reference from the linked-list too.
   */
  g_assert (fs->folders != NULL);
  g_assert (fs->folders->data == (gpointer)file);
  fs->folders = g_list_delete_link (fs->folders, fs->folders);
  g_object_unref (file);

  if (fs->folders != NULL)
    {
      GCancellable *cancellable = ide_task_get_cancellable (task);

      ide_g_file_find_async (fs->folders->data,
                             "*",
                             cancellable,
                             meson_toolchain_provider_search_iterate,
                             g_steal_pointer (&task));

      IDE_EXIT;
    }

  ide_task_set_task_data (task, g_steal_pointer (&ret), g_ptr_array_unref);
  ide_task_run_in_thread (task, gbp_meson_toolchain_provider_load_worker);

  IDE_EXIT;
}

static void
gbp_meson_toolchain_provider_load_async (IdeToolchainProvider     *provider,
                                         GCancellable             *cancellable,
                                         GAsyncReadyCallback       callback,
                                         gpointer                  user_data)
{
  GbpMesonToolchainProvider *self = (GbpMesonToolchainProvider *)provider;
  g_autoptr(GFile) project_folder = NULL;
  g_autoptr(IdeTask) task = NULL;
  g_autofree gchar *user_folder_path = NULL;
  const gchar * const *system_data_dirs;
  IdeBuildSystem *build_system;
  FileSearching *fs;
  IdeContext *context;
  GList *folders = NULL;

  IDE_ENTRY;

  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (GBP_IS_MESON_TOOLCHAIN_PROVIDER (self));
  g_assert (!cancellable || G_IS_CANCELLABLE (cancellable));

  task = ide_task_new (self, cancellable, callback, user_data);
  ide_task_set_source_tag (task, gbp_meson_toolchain_provider_load_async);
  ide_task_set_priority (task, G_PRIORITY_LOW);

  context = ide_object_get_context (IDE_OBJECT (self));
  build_system = ide_build_system_from_context (context);

  if (!GBP_IS_MESON_BUILD_SYSTEM (build_system))
    {
      ide_task_return_new_error (task,
                                 G_IO_ERROR,
                                 G_IO_ERROR_NOT_SUPPORTED,
                                 "Not using meson, ignoring meson toolchain provider");
      IDE_EXIT;
    }

  system_data_dirs = g_get_system_data_dirs ();

  for (guint i = 0; system_data_dirs[i] != NULL; i++)
    {
      g_autofree gchar *subfolder = NULL;

      subfolder = g_build_filename (system_data_dirs[i], "meson", "cross", NULL);
      folders = g_list_append (folders, g_file_new_for_path (subfolder));
    }

  user_folder_path = g_build_filename (g_get_user_data_dir (), "meson", "cross", NULL);
  folders = g_list_append (folders, g_file_new_for_path (user_folder_path));

  project_folder = ide_context_ref_workdir (context);
  folders = g_list_append (folders, g_steal_pointer (&project_folder));

  fs = file_searching_new ();
  fs->folders = g_steal_pointer (&folders);
  ide_task_set_task_data (task, fs, file_searching_free);

  g_assert (fs != NULL);
  g_assert (fs->folders != NULL);

  /* Unfortunately there is no file extension for this */
  ide_g_file_find_with_depth_async (fs->folders->data,
                                    "*",
                                    MESON_TOOLCHAIN_FIND_MAX_DEPTH,
                                    cancellable,
                                    meson_toolchain_provider_search_iterate,
                                    g_steal_pointer (&task));

  IDE_EXIT;
}

static gboolean
gbp_meson_toolchain_provider_load_finish (IdeToolchainProvider  *provider,
                                          GAsyncResult          *result,
                                          GError               **error)
{
  GbpMesonToolchainProvider *self = (GbpMesonToolchainProvider *)provider;
  g_autoptr(GPtrArray) toolchains = NULL;

  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (GBP_IS_MESON_TOOLCHAIN_PROVIDER (self));
  g_assert (IDE_IS_TASK (result));
  g_assert (ide_task_is_valid (IDE_TASK (result), provider));

  toolchains = ide_task_propagate_pointer (IDE_TASK (result), error);

  if (toolchains == NULL)
    return FALSE;

  g_clear_pointer (&self->toolchains, g_ptr_array_unref);
  self->toolchains = g_ptr_array_ref (toolchains);

  for (guint i = 0; i < toolchains->len; i++)
    {
      IdeToolchain *toolchain = g_ptr_array_index (toolchains, i);

      g_assert (IDE_IS_TOOLCHAIN (toolchain));

      ide_toolchain_provider_emit_added (provider, toolchain);
    }

  return TRUE;
}

static void
gbp_meson_toolchain_provider_unload (IdeToolchainProvider  *provider,
                                     IdeToolchainManager   *manager)
{
  GbpMesonToolchainProvider *self = (GbpMesonToolchainProvider *)provider;

  g_assert (GBP_IS_MESON_TOOLCHAIN_PROVIDER (self));
  g_assert (IDE_IS_TOOLCHAIN_MANAGER (manager));

  g_clear_pointer (&self->toolchains, g_ptr_array_unref);
}

static void
toolchain_provider_iface_init (IdeToolchainProviderInterface *iface)
{
  iface->load_async = gbp_meson_toolchain_provider_load_async;
  iface->load_finish = gbp_meson_toolchain_provider_load_finish;
  iface->unload = gbp_meson_toolchain_provider_unload;
}

G_DEFINE_FINAL_TYPE_WITH_CODE (GbpMesonToolchainProvider,
                         gbp_meson_toolchain_provider,
                         IDE_TYPE_OBJECT,
                         G_IMPLEMENT_INTERFACE (IDE_TYPE_TOOLCHAIN_PROVIDER,
                                                toolchain_provider_iface_init))

static void
gbp_meson_toolchain_provider_class_init (GbpMesonToolchainProviderClass *klass)
{
}

static void
gbp_meson_toolchain_provider_init (GbpMesonToolchainProvider *self)
{
}
