/* gbp-meson-build-stage-cross-file.h
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

#define GBP_TYPE_MESON_BUILD_STAGE_CROSS_FILE (gbp_meson_build_stage_cross_file_get_type())

G_DECLARE_FINAL_TYPE (GbpMesonBuildStageCrossFile, gbp_meson_build_stage_cross_file, GBP, MESON_BUILD_STAGE_CROSS_FILE, IdePipelineStage)

GbpMesonBuildStageCrossFile *gbp_meson_build_stage_cross_file_new      (IdeToolchain                *toolchain);
gchar                       *gbp_meson_build_stage_cross_file_get_path (GbpMesonBuildStageCrossFile *stage,
                                                                        IdePipeline                 *pipeline);

G_END_DECLS
