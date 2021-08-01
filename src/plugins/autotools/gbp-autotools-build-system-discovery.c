/* gbp-autotools-build-system-discovery.c
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

#define G_LOG_DOMAIN "gbp-autotools-build-system-discovery"

#include "config.h"

#include "gbp-autotools-build-system-discovery.h"

struct _GbpAutotoolsBuildSystemDiscovery
{
  IdeSimpleBuildSystemDiscovery parent_instance;
};

G_DEFINE_FINAL_TYPE (GbpAutotoolsBuildSystemDiscovery,
               gbp_autotools_build_system_discovery,
               IDE_TYPE_SIMPLE_BUILD_SYSTEM_DISCOVERY)

static void
gbp_autotools_build_system_discovery_class_init (GbpAutotoolsBuildSystemDiscoveryClass *klass)
{
}

static void
gbp_autotools_build_system_discovery_init (GbpAutotoolsBuildSystemDiscovery *self)
{
  g_object_set (self,
                "glob", "configure.ac",
                "hint", "autotools",
                "priority", -300,
                NULL);
}
