/* ide-simple-build-system-discovery.h
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

#pragma once

#if !defined (IDE_FOUNDRY_INSIDE) && !defined (IDE_FOUNDRY_COMPILATION)
# error "Only <libide-foundry.h> can be included directly."
#endif

#include <libide-code.h>

#include "ide-build-system-discovery.h"

G_BEGIN_DECLS

#define IDE_TYPE_SIMPLE_BUILD_SYSTEM_DISCOVERY (ide_simple_build_system_discovery_get_type())

IDE_AVAILABLE_IN_ALL
G_DECLARE_DERIVABLE_TYPE (IdeSimpleBuildSystemDiscovery, ide_simple_build_system_discovery, IDE, SIMPLE_BUILD_SYSTEM_DISCOVERY, IdeObject)

struct _IdeSimpleBuildSystemDiscoveryClass
{
  IdeObjectClass parent_class;

  /*< private >*/
  gpointer _reserved[8];
};

IDE_AVAILABLE_IN_ALL
const gchar *ide_simple_build_system_discovery_get_glob     (IdeSimpleBuildSystemDiscovery *self);
IDE_AVAILABLE_IN_ALL
void         ide_simple_build_system_discovery_set_glob     (IdeSimpleBuildSystemDiscovery *self,
                                                             const gchar                   *glob);
IDE_AVAILABLE_IN_ALL
const gchar *ide_simple_build_system_discovery_get_hint     (IdeSimpleBuildSystemDiscovery *self);
IDE_AVAILABLE_IN_ALL
void         ide_simple_build_system_discovery_set_hint     (IdeSimpleBuildSystemDiscovery *self,
                                                             const gchar                   *hint);
IDE_AVAILABLE_IN_ALL
gint         ide_simple_build_system_discovery_get_priority (IdeSimpleBuildSystemDiscovery *self);
IDE_AVAILABLE_IN_ALL
void         ide_simple_build_system_discovery_set_priority (IdeSimpleBuildSystemDiscovery *self,
                                                             gint                           priority);

G_END_DECLS
