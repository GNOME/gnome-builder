/* ide-golang--build-system-discovery.c
 *
 * Copyright 2019 Loïc BLOT <loic.blot@unix-experience.fr>
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

#define G_LOG_DOMAIN "ide-golang-build-system-discovery"

#include "gbp-golang-build-system-discovery.h"

#define DISCOVERY_MAX_DEPTH 3

struct _GbpGolangBuildSystemDiscovery
{
  GObject parent_instance;
};


static gchar *
gbp_golang_build_system_discovery_discover (IdeBuildSystemDiscovery  *discovery,
                                             GFile                    *project_file,
                                             GCancellable             *cancellable,
                                             gint                     *priority,
                                             GError                  **error)
{
  g_autoptr(GFileEnumerator) enumerator = NULL;
  gpointer infoptr;
  g_autoptr(GPtrArray) manifests = NULL;

  IDE_ENTRY;

  g_assert (GBP_IS_GOLANG_BUILD_SYSTEM_DISCOVERY (discovery));
  g_assert (G_IS_FILE (project_file));
  g_assert (!cancellable || G_IS_CANCELLABLE (cancellable));
  g_assert (priority != NULL);

  manifests = g_ptr_array_new_with_free_func (g_object_unref);

  enumerator = g_file_enumerate_children (project_file,
                                          G_FILE_ATTRIBUTE_STANDARD_IS_SYMLINK","
                                          G_FILE_ATTRIBUTE_STANDARD_NAME","
                                          G_FILE_ATTRIBUTE_STANDARD_TYPE,
                                          G_FILE_QUERY_INFO_NONE,
                                          cancellable,
                                          NULL);

  if (enumerator == NULL)
    IDE_RETURN (NULL);

  while (NULL != (infoptr = g_file_enumerator_next_file (enumerator, cancellable, NULL)))
    {
      g_autoptr(GFileInfo) info = infoptr;
      GFileType file_type;
      const gchar *name;

      if (g_file_info_get_is_symlink (info))
        continue;

      if (NULL == (name = g_file_info_get_name (info)))
        continue;

      file_type = g_file_info_get_file_type (info);

      if (file_type != G_FILE_TYPE_REGULAR)
        continue;

      if (g_strcmp0 (name, "go.sum") != 0)
            continue;

      IDE_TRACE_MSG ("Discovered buildsystem of type \"golang\"");
      IDE_RETURN (g_strdup ("golang"));
    }

  IDE_RETURN (NULL);
}

static void
build_system_discovery_iface_init (IdeBuildSystemDiscoveryInterface *iface)
{
  iface->discover = gbp_golang_build_system_discovery_discover;
}

G_DEFINE_TYPE_WITH_CODE (GbpGolangBuildSystemDiscovery,
                         gbp_golang_build_system_discovery,
                         G_TYPE_OBJECT,
                         G_IMPLEMENT_INTERFACE (IDE_TYPE_BUILD_SYSTEM_DISCOVERY, build_system_discovery_iface_init))

static void
gbp_golang_build_system_discovery_class_init (GbpGolangBuildSystemDiscoveryClass *klass)
{
}

static void
gbp_golang_build_system_discovery_init (GbpGolangBuildSystemDiscovery *self)
{
}
