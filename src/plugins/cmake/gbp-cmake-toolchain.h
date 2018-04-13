/* gbp-cmake-toolchain.h
 *
 * Copyright 2018 Corentin Noël <corentin.noel@collabora.com>
 * Copyright 2018 Collabora Ltd.
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

#define GBP_TYPE_CMAKE_TOOLCHAIN (gbp_cmake_toolchain_get_type())

G_DECLARE_FINAL_TYPE (GbpCMakeToolchain, gbp_cmake_toolchain, GBP, CMAKE_TOOLCHAIN, IdeToolchain)

GbpCMakeToolchain  *gbp_cmake_toolchain_new           (IdeContext           *context,
                                                       GFile                *file);
const gchar        *gbp_cmake_toolchain_get_file_path (GbpCMakeToolchain    *self);
void                gbp_cmake_toolchain_set_file_path (GbpCMakeToolchain    *self,
                                                       const gchar          *file_path);
gboolean            gbp_cmake_toolchain_verify        (GbpCMakeToolchain    *self);

G_END_DECLS
