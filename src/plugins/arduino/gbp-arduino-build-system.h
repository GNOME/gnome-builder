/*
 * gbp-arduino-build-system.h
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

#pragma once

#include <libide-foundry.h>

G_BEGIN_DECLS

#define GBP_TYPE_ARDUINO_BUILD_SYSTEM (gbp_arduino_build_system_get_type ())

G_DECLARE_FINAL_TYPE (GbpArduinoBuildSystem, gbp_arduino_build_system, GBP, ARDUINO_BUILD_SYSTEM, IdeObject)

char *gbp_arduino_build_system_get_sketch_yaml_path (GbpArduinoBuildSystem *self);

char *gbp_arduino_build_system_get_project_dir (GbpArduinoBuildSystem *self);

char *gbp_arduino_build_system_locate_arduino (GbpArduinoBuildSystem *self);

G_END_DECLS

