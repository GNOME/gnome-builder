/* ide-build-system-discovery.c
 *
 * Copyright 2017-2019 Christian Hergert <chergert@redhat.com>
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

#define G_LOG_DOMAIN "ide-build-system-discovery"

#include "config.h"

#include "ide-build-system-discovery.h"

G_DEFINE_INTERFACE (IdeBuildSystemDiscovery, ide_build_system_discovery, G_TYPE_OBJECT)

static void
ide_build_system_discovery_default_init (IdeBuildSystemDiscoveryInterface *iface)
{
}

/**
 * ide_build_system_discovery_discover:
 * @self: An #IdeBuildSystemDiscovery
 * @project_file: a #GFile containing the project file (a directory)
 * @cancellable: (nullable): a #GCancellable or %NULL
 * @priority: (out): A location for the priority
 * @error: a location for a #GError or %NULL
 *
 * This virtual method can be used to try to discover the build system to use for
 * a particular project. This might be used in cases like Flatpak where the build
 * system can be determined from the .json manifest rather than auto-discovery
 * by locating project files.
 *
 * Returns: (transfer full): The hint for the build system, which should match what
 *   the build system returns from ide_build_system_get_id().
 */
gchar *
ide_build_system_discovery_discover (IdeBuildSystemDiscovery  *self,
                                     GFile                    *project_file,
                                     GCancellable             *cancellable,
                                     gint                     *priority,
                                     GError                  **error)
{
  g_return_val_if_fail (IDE_IS_BUILD_SYSTEM_DISCOVERY (self), NULL);
  g_return_val_if_fail (G_IS_FILE (project_file), NULL);
  g_return_val_if_fail (!cancellable || G_IS_CANCELLABLE (cancellable), NULL);

  if (priority != NULL)
    *priority = G_MAXINT;

  if (IDE_BUILD_SYSTEM_DISCOVERY_GET_IFACE (self)->discover)
    return IDE_BUILD_SYSTEM_DISCOVERY_GET_IFACE (self)->discover (self, project_file, cancellable, priority, error);

  g_set_error (error,
               G_IO_ERROR,
               G_IO_ERROR_NOT_SUPPORTED,
               "Discovery is not supported");

  return NULL;
}
