/* gbp-meson-build-system-discovery.h
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

#include <glib-object.h>

G_BEGIN_DECLS

#define GBP_TYPE_MESON_BUILD_SYSTEM_DISCOVERY     (gbp_meson_build_system_discovery_get_type())
#define GBP_MESON_BUILD_SYSTEM_DISCOVERY_PRIORITY (-400)

G_DECLARE_FINAL_TYPE (GbpMesonBuildSystemDiscovery, gbp_meson_build_system_discovery, GBP, MESON_BUILD_SYSTEM_DISCOVERY, GObject)

G_END_DECLS
