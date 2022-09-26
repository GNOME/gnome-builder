/* ide-pipeline-stage-private.h
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

gboolean         _ide_pipeline_stage_has_query               (IdePipelineStage     *self);
IdePipelinePhase _ide_pipeline_stage_get_phase               (IdePipelineStage     *self);
void             _ide_pipeline_stage_set_phase               (IdePipelineStage     *self,
                                                              IdePipelinePhase      phase);
void             _ide_pipeline_stage_build_with_query_async  (IdePipelineStage     *self,
                                                              IdePipeline          *pipeline,
                                                              GPtrArray            *targets,
                                                              GCancellable         *cancellable,
                                                              GAsyncReadyCallback   callback,
                                                              gpointer              user_data);
gboolean         _ide_pipeline_stage_build_with_query_finish (IdePipelineStage     *self,
                                                              GAsyncResult         *result,
                                                              GError              **error);

G_END_DECLS
