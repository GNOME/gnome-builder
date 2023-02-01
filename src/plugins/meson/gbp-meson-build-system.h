/* gbp-meson-build-system.h
 *
 * Copyright 2017-2019 Christian Hergert <chergert@redhat.com>
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

#include <libide-foundry.h>

G_BEGIN_DECLS

#define GBP_TYPE_MESON_BUILD_SYSTEM (gbp_meson_build_system_get_type())

G_DECLARE_FINAL_TYPE (GbpMesonBuildSystem, gbp_meson_build_system, GBP, MESON_BUILD_SYSTEM, IdeObject)

const gchar * const *gbp_meson_build_system_get_languages   (GbpMesonBuildSystem *self);
char                *gbp_meson_build_system_locate_meson    (GbpMesonBuildSystem *self,
                                                             IdePipeline         *pipeline);
char                *gbp_meson_build_system_locate_ninja    (GbpMesonBuildSystem *self,
                                                             IdePipeline         *pipeline);

G_END_DECLS
