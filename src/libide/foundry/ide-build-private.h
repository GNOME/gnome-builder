/* ide-build-private.h
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

#include <vte/vte.h>

#include "ide-foundry-types.h"

G_BEGIN_DECLS

guint8 *_ide_build_utils_filter_color_codes (const guint8    *data,
                                             gsize            len,
                                             gsize           *out_len);
void    _ide_build_manager_start            (IdeBuildManager *self);
void    _ide_pipeline_cancel                (IdePipeline     *self);
void    _ide_pipeline_set_runtime           (IdePipeline     *self,
                                             IdeRuntime      *runtime);
void    _ide_pipeline_set_toolchain         (IdePipeline     *self,
                                             IdeToolchain    *toolchain);
void    _ide_pipeline_set_message           (IdePipeline     *self,
                                             const gchar     *message);
void    _ide_pipeline_mark_broken           (IdePipeline     *self);
void    _ide_pipeline_check_toolchain       (IdePipeline     *self,
                                             IdeDeviceInfo   *info);
void    _ide_pipeline_set_pty_size          (IdePipeline     *self,
                                             guint            rows,
                                             guint            columns);

G_END_DECLS
