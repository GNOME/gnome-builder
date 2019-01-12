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

#include "ide-golang-build-system-discovery.h"

#define DISCOVERY_MAX_DEPTH 3

struct _IdeGolangBuildSystemDiscovery
{
  GObject parent_instance;
};


static gchar *
ide_golang_build_system_discovery_discover (IdeBuildSystemDiscovery  *discovery,
                                             GFile                    *project_file,
                                             GCancellable             *cancellable,
                                             gint                     *priority,
                                             GError                  **error)
{
  IDE_ENTRY;
  IDE_RETURN (NULL);
}

static void
build_system_discovery_iface_init (IdeBuildSystemDiscoveryInterface *iface)
{
  iface->discover = ide_golang_build_system_discovery_discover;
}

G_DEFINE_TYPE_WITH_CODE (IdeGolangBuildSystemDiscovery,
                         ide_golang_build_system_discovery,
                         G_TYPE_OBJECT,
                         G_IMPLEMENT_INTERFACE (IDE_TYPE_BUILD_SYSTEM_DISCOVERY, build_system_discovery_iface_init))

static void
ide_golang_build_system_discovery_class_init (IdeGolangBuildSystemDiscoveryClass *klass)
{
}

static void
ide_golang_build_system_discovery_init (IdeGolangBuildSystemDiscovery *self)
{
}
