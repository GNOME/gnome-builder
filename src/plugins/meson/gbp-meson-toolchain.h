/* gbp-meson-toolchain.h
 *
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
 *
 * Authors: Corentin Noël <corentin.noel@collabora.com>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include <libide-foundry.h>

G_BEGIN_DECLS

#define GBP_TYPE_MESON_TOOLCHAIN (gbp_meson_toolchain_get_type())

G_DECLARE_FINAL_TYPE (GbpMesonToolchain, gbp_meson_toolchain, GBP, MESON_TOOLCHAIN, IdeSimpleToolchain)

GbpMesonToolchain  *gbp_meson_toolchain_new              (IdeContext             *context);
gboolean            gbp_meson_toolchain_load             (GbpMesonToolchain      *self,
                                                          GFile                  *file,
                                                          GError                **error);
const gchar        *gbp_meson_toolchain_get_file_path    (GbpMesonToolchain      *self);

G_END_DECLS
