/* ide-golang--build-system-discovery.h
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

#pragma once

#include <ide.h>

G_BEGIN_DECLS

#define TYPE_GOLANG_BUILD_SYSTEM_DISCOVERY (gbp_golang_build_system_discovery_get_type())

G_DECLARE_FINAL_TYPE (GbpGolangBuildSystemDiscovery, gbp_golang_build_system_discovery, GBP, GOLANG_BUILD_SYSTEM_DISCOVERY, GObject)

G_END_DECLS
