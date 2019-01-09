/* ide-build-stage-private.h
 *
 * Copyright 2016-2019 Christian Hergert <chergert@redhat.com>
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

#include "ide-foundry-types.h"

G_BEGIN_DECLS

gboolean      _ide_build_stage_has_query                 (IdeBuildStage        *self);
IdeBuildPhase _ide_build_stage_get_phase                 (IdeBuildStage        *self);
void          _ide_build_stage_set_phase                 (IdeBuildStage        *self,
                                                          IdeBuildPhase         phase);
void          _ide_build_stage_execute_with_query_async  (IdeBuildStage        *self,
                                                          IdeBuildPipeline     *pipeline,
                                                          GPtrArray            *targets,
                                                          GCancellable         *cancellable,
                                                          GAsyncReadyCallback   callback,
                                                          gpointer              user_data);
gboolean      _ide_build_stage_execute_with_query_finish (IdeBuildStage        *self,
                                                          GAsyncResult         *result,
                                                          GError              **error);

G_END_DECLS
