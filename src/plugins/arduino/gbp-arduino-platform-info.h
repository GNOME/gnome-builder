/*
 * gbp-arduino-platform-info.c
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

#define GBP_TYPE_ARDUINO_PLATFORM_INFO (gbp_arduino_platform_info_get_type ())

G_DECLARE_FINAL_TYPE (GbpArduinoPlatformInfo, gbp_arduino_platform_info, GBP, ARDUINO_PLATFORM_INFO, GObject)

GbpArduinoPlatformInfo *
gbp_arduino_platform_info_new (const char        *name,
                               const char        *version,
                               const char *const *supported_fqbns,
                               const char        *maintainer,
                               const char        *id,
                               const char        *installed_version);

const char *gbp_arduino_platform_info_get_name (GbpArduinoPlatformInfo *self);

const char *gbp_arduino_platform_info_get_version (GbpArduinoPlatformInfo *self);

const char *const *gbp_arduino_platform_info_get_supported_fqbns (GbpArduinoPlatformInfo *self);

const char *gbp_arduino_platform_info_get_maintainer (GbpArduinoPlatformInfo *self);

const char *gbp_arduino_platform_info_get_id (GbpArduinoPlatformInfo *self);

const char *gbp_arduino_platform_info_get_installed_version (GbpArduinoPlatformInfo *self);

G_END_DECLS

