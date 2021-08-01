/* gbp-cmake-build-system-discovery.c
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

#define G_LOG_DOMAIN "gbp-cmake-build-system-discovery"

#include "config.h"

#include "gbp-cmake-build-system-discovery.h"

struct _GbpCmakeBuildSystemDiscovery
{
  IdeSimpleBuildSystemDiscovery parent;
};

G_DEFINE_FINAL_TYPE (GbpCmakeBuildSystemDiscovery,
               gbp_cmake_build_system_discovery,
               IDE_TYPE_SIMPLE_BUILD_SYSTEM_DISCOVERY)

static void
gbp_cmake_build_system_discovery_class_init (GbpCmakeBuildSystemDiscoveryClass *klass)
{
}

static void
gbp_cmake_build_system_discovery_init (GbpCmakeBuildSystemDiscovery *self)
{
  g_object_set (self,
                "hint", "cmake",
                "glob", "CMakeLists.txt",
                "priority", 100,
                NULL);
}
