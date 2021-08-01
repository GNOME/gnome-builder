/* gbp-cmake-toolchain-provider.c
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

#define G_LOG_DOMAIN "gbp-cmake-toolchain-provider"

#include "gbp-cmake-toolchain.h"
#include "gbp-cmake-toolchain-provider.h"
#include "gbp-cmake-build-system.h"

#define CMAKE_TOOLCHAIN_MAX_FIND_DEPTH 3

struct _GbpCMakeToolchainProvider
{
  IdeObject            parent_instance;
  GPtrArray           *toolchains;
};

static void
gbp_cmake_toolchain_provider_load_worker (IdeTask      *task,
                                          gpointer      source_object,
                                          gpointer      task_data,
                                          GCancellable *cancellable)
{
  GbpCMakeToolchainProvider *self = source_object;
  g_autoptr(GPtrArray) toolchains = NULL;
  IdeContext *context;
  GPtrArray *files = task_data;

  g_assert (IDE_IS_TASK (task));
  g_assert (GBP_IS_CMAKE_TOOLCHAIN_PROVIDER (self));
  g_assert (files != NULL);
  g_assert (!cancellable || G_IS_CANCELLABLE (cancellable));

  context = ide_object_get_context (IDE_OBJECT (self));
  toolchains = g_ptr_array_new_with_free_func (g_object_unref);

  for (guint i = 0; i < files->len; i++)
    {
      GFile *file = g_ptr_array_index (files, i);
      g_autofree gchar *name = NULL;
      g_autoptr(GError) file_error = NULL;
      g_autofree gchar *file_path = NULL;
      g_autofree gchar *file_contents = NULL;
      gsize file_contents_len;

      g_assert (G_IS_FILE (file));

      name = g_file_get_basename (file);
      file_path = g_file_get_path (file);
      /* Cross-compilation files have .cmake extension, we have to blacklist CMakeSystem.cmake
       * in case we are looking into a build folder */
      if (g_strcmp0 (name, "CMakeSystem.cmake") == 0)
        continue;

      /* Cross-compilation files should at least define CMAKE_SYSTEM_NAME and CMAKE_SYSTEM_PROCESSOR */
      if (g_file_get_contents (file_path, &file_contents, &file_contents_len, &file_error))
        {
          g_autoptr(GbpCMakeToolchain) toolchain = NULL;
          g_autoptr(GError) load_error = NULL;
          const gchar *processor_name;
          const gchar *system_name;

          system_name = g_strstr_len (file_contents,
                                      file_contents_len,
                                      "CMAKE_SYSTEM_NAME");
          if (system_name == NULL)
            continue;

          processor_name = g_strstr_len (file_contents,
                                         file_contents_len,
                                         "CMAKE_SYSTEM_PROCESSOR");
          if (processor_name == NULL)
            continue;

          toolchain = gbp_cmake_toolchain_new (context);
          ide_object_append (IDE_OBJECT (self), IDE_OBJECT (toolchain));

          if (!gbp_cmake_toolchain_load (toolchain, file, cancellable, &load_error))
            {
              g_debug ("Error loading %s : %s", file_path, load_error->message);
              ide_clear_and_destroy_object (&toolchain);
              continue;
            }

          g_ptr_array_add (toolchains, g_steal_pointer (&toolchain));
        }
    }

  ide_task_return_pointer (task, g_steal_pointer (&toolchains), g_ptr_array_unref);
}

static void
load_find_files_cb (GObject      *object,
                    GAsyncResult *result,
                    gpointer      user_data)
{
  GFile *file = (GFile *)object;
  g_autoptr(IdeTask) task = user_data;
  g_autoptr(GError) error = NULL;
  g_autoptr(GPtrArray) ret = NULL;

  g_assert (G_IS_FILE (file));
  g_assert (G_IS_ASYNC_RESULT (result));
  g_assert (IDE_IS_TASK (task));

  ret = ide_g_file_find_finish (file, result, &error);
  IDE_PTR_ARRAY_SET_FREE_FUNC (ret, g_object_unref);

  if (ret == NULL)
    {
      ide_task_return_error (task, g_steal_pointer (&error));
      return;
    }

  ide_task_set_task_data (task, g_steal_pointer (&ret), g_ptr_array_unref);
  ide_task_run_in_thread (task, gbp_cmake_toolchain_provider_load_worker);
}

static void
gbp_cmake_toolchain_provider_load_async (IdeToolchainProvider     *provider,
                                         GCancellable             *cancellable,
                                         GAsyncReadyCallback       callback,
                                         gpointer                  user_data)
{
  GbpCMakeToolchainProvider *self = (GbpCMakeToolchainProvider *)provider;
  g_autoptr(IdeTask) task = NULL;
  g_autoptr(GFile) workdir = NULL;
  IdeContext *context;

  IDE_ENTRY;

  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (GBP_IS_CMAKE_TOOLCHAIN_PROVIDER (self));
  g_assert (!cancellable || G_IS_CANCELLABLE (cancellable));

  context = ide_object_get_context (IDE_OBJECT (self));
  workdir = ide_context_ref_workdir (context);

  task = ide_task_new (provider, cancellable, callback, user_data);
  ide_task_set_source_tag (task, gbp_cmake_toolchain_provider_load_async);
  ide_task_set_priority (task, G_PRIORITY_LOW);

  ide_g_file_find_with_depth_async (workdir,
                                    "*.cmake",
                                    CMAKE_TOOLCHAIN_MAX_FIND_DEPTH,
                                    cancellable,
                                    load_find_files_cb,
                                    g_steal_pointer (&task));

  IDE_EXIT;
}

static gboolean
gbp_cmake_toolchain_provider_load_finish (IdeToolchainProvider  *provider,
                                          GAsyncResult          *result,
                                          GError               **error)
{
  GbpCMakeToolchainProvider *self = (GbpCMakeToolchainProvider *)provider;
  g_autoptr(GPtrArray) toolchains = NULL;

  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (GBP_IS_CMAKE_TOOLCHAIN_PROVIDER (self));
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
gbp_cmake_toolchain_provider_unload (IdeToolchainProvider  *provider,
                                     IdeToolchainManager   *manager)
{
  GbpCMakeToolchainProvider *self = (GbpCMakeToolchainProvider *) provider;

  g_assert (GBP_IS_CMAKE_TOOLCHAIN_PROVIDER (self));
  g_assert (IDE_IS_TOOLCHAIN_MANAGER (manager));

  g_clear_pointer (&self->toolchains, g_ptr_array_unref);
}

static void
toolchain_provider_iface_init (IdeToolchainProviderInterface *iface)
{
  iface->load_async = gbp_cmake_toolchain_provider_load_async;
  iface->load_finish = gbp_cmake_toolchain_provider_load_finish;
  iface->unload = gbp_cmake_toolchain_provider_unload;
}

G_DEFINE_FINAL_TYPE_WITH_CODE (GbpCMakeToolchainProvider,
                         gbp_cmake_toolchain_provider,
                         IDE_TYPE_OBJECT,
                         G_IMPLEMENT_INTERFACE (IDE_TYPE_TOOLCHAIN_PROVIDER,
                                                toolchain_provider_iface_init))

static void
gbp_cmake_toolchain_provider_class_init (GbpCMakeToolchainProviderClass *klass)
{
}

static void
gbp_cmake_toolchain_provider_init (GbpCMakeToolchainProvider *self)
{

}
