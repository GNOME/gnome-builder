/* ide-build-system-discovery.h
 *
 * Copyright © 2017 Christian Hergert <chergert@redhat.com>
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

#pragma once

#include <gio/gio.h>

#include "ide-types.h"

G_BEGIN_DECLS

#define IDE_TYPE_BUILD_SYSTEM_DISCOVERY (ide_build_system_discovery_get_type())

G_DECLARE_INTERFACE (IdeBuildSystemDiscovery, ide_build_system_discovery, IDE, BUILD_SYSTEM_DISCOVERY, GObject)

struct _IdeBuildSystemDiscoveryInterface
{
  GTypeInterface parent_iface;

  gchar *(*discover) (IdeBuildSystemDiscovery  *self,
                      GFile                    *project_file,
                      GCancellable             *cancellable,
                      gint                     *priority,
                      GError                  **error);
};

gchar *ide_build_system_discovery_discover (IdeBuildSystemDiscovery  *self,
                                            GFile                    *project_file,
                                            GCancellable             *cancellable,
                                            gint                     *priority,
                                            GError                  **error);

G_END_DECLS
