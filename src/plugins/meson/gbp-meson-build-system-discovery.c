/* gbp-meson-build-system-discovery.c
 *
 * Copyright 2018-2019 Christian Hergert <chergert@redhat.com>
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

#define G_LOG_DOMAIN "gbp-meson-build-system-discovery"

#include "config.h"

#include <libide-foundry.h>

#include "gbp-meson-build-system-discovery.h"

struct _GbpMesonBuildSystemDiscovery
{
  GObject parent_instance;
};

static gchar *
gbp_meson_build_system_discovery_discover (IdeBuildSystemDiscovery  *discovery,
                                           GFile                    *directory,
                                           GCancellable             *cancellable,
                                           gint                     *priority,
                                           GError                  **error)
{
  g_autoptr(GFile) meson_build = NULL;
  g_autoptr(GFileInfo) info = NULL;

  g_assert (!IDE_IS_MAIN_THREAD ());
  g_assert (GBP_IS_MESON_BUILD_SYSTEM_DISCOVERY (discovery));
  g_assert (G_IS_FILE (directory));
  g_assert (!cancellable || G_IS_CANCELLABLE (cancellable));
  g_assert (priority != NULL);

  *priority = 0;

  meson_build = g_file_get_child (directory, "meson.build");
  info = g_file_query_info (meson_build,
                            G_FILE_ATTRIBUTE_STANDARD_TYPE,
                            G_FILE_QUERY_INFO_NONE,
                            cancellable,
                            NULL);

  if (info == NULL || g_file_info_get_file_type (info) != G_FILE_TYPE_REGULAR)
    {
      g_set_error (error,
                   G_IO_ERROR,
                   G_IO_ERROR_NOT_SUPPORTED,
                   "Meson is not supported in this project");
      return NULL;
    }

  *priority = GBP_MESON_BUILD_SYSTEM_DISCOVERY_PRIORITY;

  return g_strdup ("meson");
}

static void
build_system_discovery_iface_init (IdeBuildSystemDiscoveryInterface *iface)
{
  iface->discover = gbp_meson_build_system_discovery_discover;
}

G_DEFINE_FINAL_TYPE_WITH_CODE (GbpMesonBuildSystemDiscovery, gbp_meson_build_system_discovery, G_TYPE_OBJECT,
                         G_IMPLEMENT_INTERFACE (IDE_TYPE_BUILD_SYSTEM_DISCOVERY,
                                                build_system_discovery_iface_init))

static void
gbp_meson_build_system_discovery_class_init (GbpMesonBuildSystemDiscoveryClass *klass)
{
}

static void
gbp_meson_build_system_discovery_init (GbpMesonBuildSystemDiscovery *self)
{
}
