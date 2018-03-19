/* gbp-meson-toolchain-provider.c
 *
 * Copyright © 2018 Corentin Noël <corentin.noel@collabora.com>
 * Copyright © 2018 Collabora Ltd.
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

#include "gbp-meson-toolchain-provider.h"

struct _GbpMesonToolchainProvider
{
  IdeObject            parent_instance;
  IdeToolchainManager *manager;
};

void
meson_toolchain_provider_search_folder (GbpMesonToolchainProvider  *self,
                                        const gchar                *folder)
{
  g_critical ("Searching in %s…", folder);
  //TODO search for cross-files
}

void
gbp_meson_toolchain_provider_load (IdeToolchainProvider  *provider,
                                   IdeToolchainManager   *manager)
{
  GbpMesonToolchainProvider *self = (GbpMesonToolchainProvider *) provider;
  IdeContext *context;
  GFile *project_file;
  const gchar * const *system_data_dirs;
  g_autoptr(GFile) project_folder = NULL;
  g_autofree gchar *folder = NULL;
  gint i = 0;

  IDE_ENTRY;

  g_assert (GBP_IS_MESON_TOOLCHAIN_PROVIDER (self));
  g_assert (IDE_IS_TOOLCHAIN_MANAGER (manager));

  self->manager = g_object_ref (manager);

  /* Starting with version 0.44.0, Meson supported embedded cross-files */
  system_data_dirs = g_get_system_data_dirs ();
  while (system_data_dirs[i] != NULL) {
      folder = g_build_filename (system_data_dirs[i], "meson", "cross", NULL);
      meson_toolchain_provider_search_folder (self, folder);
      i++;
  }

  folder = g_build_filename (g_get_user_data_dir (), "meson", "cross", NULL);
  meson_toolchain_provider_search_folder (self, folder);

  context = ide_object_get_context (IDE_OBJECT (manager));
  g_return_if_fail (IDE_IS_CONTEXT (context));

  project_file = ide_context_get_project_file (context);
  project_folder = g_file_get_parent (project_file);
  folder = g_file_get_path (project_folder);
  meson_toolchain_provider_search_folder (self, folder);

  IDE_EXIT;
}

void
gbp_meson_toolchain_provider_unload (IdeToolchainProvider  *provider,
                                     IdeToolchainManager   *manager)
{
  GbpMesonToolchainProvider *self = (GbpMesonToolchainProvider *) provider;

  g_assert (GBP_IS_MESON_TOOLCHAIN_PROVIDER (self));
  g_assert (IDE_IS_TOOLCHAIN_MANAGER (manager));

  g_object_unref (self->manager);
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
