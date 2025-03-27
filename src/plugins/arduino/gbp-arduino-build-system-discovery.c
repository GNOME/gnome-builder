/*
 * gbp-arduino-build-system-discovery.c
 *
 * Copyright 2025 Nokse <nokse@posteo.com>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this library; if not, see <http://www.gnu.org/licenses/>.
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#define G_LOG_DOMAIN "gbp-arduino-build-system-discovery"

#include "config.h"

#include "gbp-arduino-build-system-discovery.h"

struct _GbpArduinoBuildSystemDiscovery
{
  IdeSimpleBuildSystemDiscovery parent_instance;
};

G_DEFINE_FINAL_TYPE (GbpArduinoBuildSystemDiscovery, gbp_arduino_build_system_discovery, IDE_TYPE_SIMPLE_BUILD_SYSTEM_DISCOVERY)

static void
gbp_arduino_build_system_discovery_class_init (GbpArduinoBuildSystemDiscoveryClass *klass)
{
}

static void
gbp_arduino_build_system_discovery_init (GbpArduinoBuildSystemDiscovery *self)
{
  g_object_set (self,
                "glob", "+(sketch.yaml|sketch.yml)",
                "hint", "arduino",
                "priority", -300,
                NULL);
}

