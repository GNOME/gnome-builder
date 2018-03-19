/* gbp-meson-toolchain-provider.c
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

#define G_LOG_DOMAIN "gbp-meson-toolchain-provider"

#include "gbp-meson-toolchain.h"
#include "gbp-meson-toolchain-provider.h"
#include "gbp-meson-build-system.h"

struct _GbpMesonToolchainProvider
{
  IdeObject            parent_instance;
  IdeToolchainManager *manager;
  GCancellable        *loading_cancellable;
};

void
meson_toolchain_provider_search_folder (GbpMesonToolchainProvider  *self,
                                        GFile                      *file);

void
meson_toolchain_provider_add_crossfile (GbpMesonToolchainProvider  *self,
                                        GFile                      *file)
{
  IdeContext *context;
  g_autoptr(GbpMesonToolchain) toolchain = NULL;

  g_assert (GBP_IS_MESON_TOOLCHAIN_PROVIDER (self));
  g_assert (G_IS_FILE (file));

  context = ide_object_get_context (IDE_OBJECT (self->manager));
  toolchain = gbp_meson_toolchain_new (context, file);
  ide_toolchain_manager_add (self->manager, IDE_TOOLCHAIN (toolchain));
}

void
meson_toolchain_enumerate_children_cb (GObject      *object,
                                       GAsyncResult *result,
                                       gpointer      user_data)
{
  GbpMesonToolchainProvider *self = (GbpMesonToolchainProvider *)user_data;
  GFile *dir = (GFile *)object;
  g_autoptr(GError) error = NULL;
  g_autoptr(GPtrArray) infos = NULL;

  g_assert (G_IS_FILE (dir));
  g_assert (G_IS_ASYNC_RESULT (result));
  g_assert (GBP_IS_MESON_TOOLCHAIN_PROVIDER (self));

  infos = ide_g_file_get_children_finish (dir, result, &error);

  if (infos == NULL)
    return;

  for (guint i = 0; i < infos->len; i++)
    {
      GFileInfo *file_info = g_ptr_array_index (infos, i);
      GFileType file_type = g_file_info_get_file_type (file_info);

      if (file_type == G_FILE_TYPE_REGULAR)
        {
          const gchar *content_type = g_file_info_get_content_type (file_info);

          if (g_content_type_is_mime_type (content_type, "text/plain"))
            {
              const gchar *name = g_file_info_get_name (file_info);
              g_autoptr(GFile) child = g_file_get_child (dir, name);
              g_autoptr(GKeyFile) keyfile = g_key_file_new ();
              g_autofree gchar *path = g_file_get_path (child);
              g_autoptr(GError) keyfile_error = NULL;

              if (g_key_file_load_from_file (keyfile, path, G_KEY_FILE_NONE, &keyfile_error))
                {
                  if (g_key_file_has_group (keyfile, "binaries") &&
                      (g_key_file_has_group (keyfile, "host_machine") ||
                       g_key_file_has_group (keyfile, "target_machine")))
                    meson_toolchain_provider_add_crossfile (self, child);
                }
            }
        }
      else if (file_type == G_FILE_TYPE_DIRECTORY)
        {
          const gchar *name = g_file_info_get_name (file_info);
          g_autoptr(GFile) child = g_file_get_child (dir, name);

          meson_toolchain_provider_search_folder (self, child);
        }
    }
}

void
meson_toolchain_provider_search_folder (GbpMesonToolchainProvider  *self,
                                        GFile                      *file)
{
  g_assert (GBP_IS_MESON_TOOLCHAIN_PROVIDER (self));
  g_assert (G_IS_FILE (file));

  ide_g_file_get_children_async (file,
                                 G_FILE_ATTRIBUTE_STANDARD_NAME","
                                 G_FILE_ATTRIBUTE_STANDARD_TYPE","
                                 G_FILE_ATTRIBUTE_STANDARD_CONTENT_TYPE,
                                 G_FILE_QUERY_INFO_NONE,
                                 G_PRIORITY_LOW,
                                 self->loading_cancellable,
                                 meson_toolchain_enumerate_children_cb,
                                 self);
}

void
gbp_meson_toolchain_provider_load (IdeToolchainProvider  *provider,
                                   IdeToolchainManager   *manager)
{
  GbpMesonToolchainProvider *self = (GbpMesonToolchainProvider *) provider;
  IdeContext *context;
  const gchar * const *system_data_dirs;
  g_autoptr(GFile) project_folder = NULL;
  g_autoptr(GFile) user_folder = NULL;
  g_autofree gchar *user_folder_path = NULL;

  IDE_ENTRY;

  g_assert (GBP_IS_MESON_TOOLCHAIN_PROVIDER (self));
  g_assert (IDE_IS_TOOLCHAIN_MANAGER (manager));

  self->manager = g_object_ref (manager);

  context = ide_object_get_context (IDE_OBJECT (manager));
  g_return_if_fail (IDE_IS_CONTEXT (context));
  if (!GBP_IS_MESON_BUILD_SYSTEM (ide_context_get_build_system (context)))
    return;

  self->loading_cancellable = g_cancellable_new ();

  /* Starting with version 0.44.0, Meson supported embedded cross-files */
  system_data_dirs = g_get_system_data_dirs ();
  for (gint i = 0; system_data_dirs[i] != NULL; i++)
    {
      g_autofree gchar *subfolder = g_build_filename (system_data_dirs[i], "meson", "cross", NULL);
      g_autoptr(GFile) file = g_file_new_for_path (subfolder);

      meson_toolchain_provider_search_folder (self, file);
    }

  user_folder_path = g_build_filename (g_get_user_data_dir (), "meson", "cross", NULL);
  user_folder = g_file_new_for_path (user_folder_path);
  meson_toolchain_provider_search_folder (self, user_folder);

  project_folder = g_file_get_parent (ide_context_get_project_file (context));
  meson_toolchain_provider_search_folder (self, project_folder);

  IDE_EXIT;
}

void
gbp_meson_toolchain_provider_unload (IdeToolchainProvider  *provider,
                                     IdeToolchainManager   *manager)
{
  GbpMesonToolchainProvider *self = (GbpMesonToolchainProvider *) provider;

  g_assert (GBP_IS_MESON_TOOLCHAIN_PROVIDER (self));
  g_assert (IDE_IS_TOOLCHAIN_MANAGER (manager));

  g_clear_object (&self->manager);
}

static void
toolchain_provider_iface_init (IdeToolchainProviderInterface *iface)
{
  iface->load = gbp_meson_toolchain_provider_load;
  iface->unload = gbp_meson_toolchain_provider_unload;
}

G_DEFINE_TYPE_WITH_CODE (GbpMesonToolchainProvider,
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
