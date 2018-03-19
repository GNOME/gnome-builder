/* gbp-cmake-toolchain-provider.c
 *
 * Copyright 2018 Corentin Noël <corentin.noel@collabora.com>
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
 */

#define G_LOG_DOMAIN "gbp-cmake-toolchain-provider"

#include "gbp-cmake-toolchain.h"
#include "gbp-cmake-toolchain-provider.h"
#include "gbp-cmake-build-system.h"

struct _GbpCMakeToolchainProvider
{
  IdeObject            parent_instance;
  IdeToolchainManager *manager;
  GCancellable        *loading_cancellable;
};

void
cmake_toolchain_provider_search_folder (GbpCMakeToolchainProvider  *self,
                                        GFile                      *file);


static void
gbp_cmake_toolchain_verify_async_cb (GObject      *object,
                                     GAsyncResult *result,
                                     gpointer      user_data)
{
  GbpCMakeToolchain *toolchain = (GbpCMakeToolchain *)object;
  GbpCMakeToolchainProvider *provider = user_data;
  g_autoptr(GError) error = NULL;

  IDE_ENTRY;

  g_assert (GBP_IS_CMAKE_TOOLCHAIN (toolchain));
  g_assert (GBP_IS_CMAKE_TOOLCHAIN_PROVIDER (provider));

  if (!gbp_cmake_toolchain_verify_finish (toolchain, result, &error))
    IDE_EXIT;

  ide_toolchain_manager_add (provider->manager, IDE_TOOLCHAIN (toolchain));
  IDE_EXIT;
}

void
cmake_toolchain_provider_add_crossfile (GbpCMakeToolchainProvider  *self,
                                        GFile                      *file)
{
  IdeContext *context;
  g_autoptr(GbpCMakeToolchain) toolchain = NULL;

  g_assert (GBP_IS_CMAKE_TOOLCHAIN_PROVIDER (self));
  g_assert (G_IS_FILE (file));

  context = ide_object_get_context (IDE_OBJECT (self->manager));
  toolchain = gbp_cmake_toolchain_new (context, file);

  gbp_cmake_toolchain_verify_async (g_steal_pointer (&toolchain), NULL, gbp_cmake_toolchain_verify_async_cb, self);
}

void
cmake_toolchain_enumerate_children_cb (GObject      *object,
                                       GAsyncResult *result,
                                       gpointer      user_data)
{
  GbpCMakeToolchainProvider *self = (GbpCMakeToolchainProvider *)user_data;
  GFile *dir = (GFile *)object;
  g_autoptr(GError) error = NULL;
  g_autoptr(GPtrArray) infos = NULL;

  g_assert (G_IS_FILE (dir));
  g_assert (G_IS_ASYNC_RESULT (result));
  g_assert (GBP_IS_CMAKE_TOOLCHAIN_PROVIDER (self));

  infos = ide_g_file_get_children_finish (dir, result, &error);

  if (infos == NULL)
    return;

  for (guint i = 0; i < infos->len; i++)
    {
      GFileInfo *file_info = g_ptr_array_index (infos, i);
      GFileType file_type = g_file_info_get_file_type (file_info);

      if (file_type == G_FILE_TYPE_REGULAR)
        {
          const gchar *name = g_file_info_get_name (file_info);
          /* Cross-compilation files have .cmake extension, we have to blacklist CMakeSystem.cmake
           * in case we are looking into a build folder */
          if (g_str_has_suffix (name, ".cmake") && g_strcmp0(name, "CMakeSystem.cmake") != 0)
            {
              const gchar *content_type = g_file_info_get_content_type (file_info);

              if (g_content_type_is_mime_type (content_type, "text/x-cmake"))
                {
                  g_autoptr(GFile) child = g_file_get_child (dir, name);
                  g_autoptr(GError) file_error = NULL;
                  g_autofree gchar *file_path = g_file_get_path (child);
                  g_autofree gchar *file_contents = NULL;
                  gsize file_contents_len;

                  /* Cross-compilation files should at least define CMAKE_SYSTEM_NAME and CMAKE_SYSTEM_PROCESSOR */
                  if (g_file_get_contents (file_path,
                                           &file_contents, &file_contents_len, &file_error))
                    {
                      const gchar *system_name = g_strstr_len (file_contents,
                                                               file_contents_len,
                                                               "CMAKE_SYSTEM_NAME");
                      if (system_name != NULL)
                        {
                          const gchar *processor_name = g_strstr_len (file_contents,
                                                                      file_contents_len,
                                                                      "CMAKE_SYSTEM_PROCESSOR");
                          if (processor_name != NULL)
                            {
                              cmake_toolchain_provider_add_crossfile (self, child);
                            }
                        }
                    }
                }
          }
        }
      else if (file_type == G_FILE_TYPE_DIRECTORY)
        {
          const gchar *name = g_file_info_get_name (file_info);
          g_autoptr(GFile) child = g_file_get_child (dir, name);

          cmake_toolchain_provider_search_folder (self, child);
        }
    }
}

void
cmake_toolchain_provider_search_folder (GbpCMakeToolchainProvider  *self,
                                        GFile                      *file)
{
  g_assert (GBP_IS_CMAKE_TOOLCHAIN_PROVIDER (self));
  g_assert (G_IS_FILE (file));

  ide_g_file_get_children_async (file,
                                 G_FILE_ATTRIBUTE_STANDARD_NAME","
                                 G_FILE_ATTRIBUTE_STANDARD_TYPE","
                                 G_FILE_ATTRIBUTE_STANDARD_CONTENT_TYPE,
                                 G_FILE_QUERY_INFO_NONE,
                                 G_PRIORITY_LOW,
                                 self->loading_cancellable,
                                 cmake_toolchain_enumerate_children_cb,
                                 self);
}

void
gbp_cmake_toolchain_provider_load (IdeToolchainProvider  *provider,
                                   IdeToolchainManager   *manager)
{
  GbpCMakeToolchainProvider *self = (GbpCMakeToolchainProvider *) provider;
  IdeContext *context;
  g_autoptr(GFile) project_folder = NULL;

  IDE_ENTRY;

  g_assert (GBP_IS_CMAKE_TOOLCHAIN_PROVIDER (self));
  g_assert (IDE_IS_TOOLCHAIN_MANAGER (manager));

  self->manager = g_object_ref (manager);

  context = ide_object_get_context (IDE_OBJECT (manager));
  g_return_if_fail (IDE_IS_CONTEXT (context));
  if (!GBP_IS_CMAKE_BUILD_SYSTEM (ide_context_get_build_system (context)))
    return;

  self->loading_cancellable = g_cancellable_new ();

  project_folder = g_file_get_parent (ide_context_get_project_file (context));
  cmake_toolchain_provider_search_folder (self, project_folder);

  IDE_EXIT;
}

void
gbp_cmake_toolchain_provider_unload (IdeToolchainProvider  *provider,
                                     IdeToolchainManager   *manager)
{
  GbpCMakeToolchainProvider *self = (GbpCMakeToolchainProvider *) provider;

  g_assert (GBP_IS_CMAKE_TOOLCHAIN_PROVIDER (self));
  g_assert (IDE_IS_TOOLCHAIN_MANAGER (manager));

  g_clear_object (&self->manager);
}

static void
toolchain_provider_iface_init (IdeToolchainProviderInterface *iface)
{
  iface->load = gbp_cmake_toolchain_provider_load;
  iface->unload = gbp_cmake_toolchain_provider_unload;
}

G_DEFINE_TYPE_WITH_CODE (GbpCMakeToolchainProvider,
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
