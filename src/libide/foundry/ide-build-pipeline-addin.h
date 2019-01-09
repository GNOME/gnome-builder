/* ide-build-pipeline-addin.h
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

#if !defined (IDE_FOUNDRY_INSIDE) && !defined (IDE_FOUNDRY_COMPILATION)
# error "Only <libide-foundry.h> can be included directly."
#endif

#include <libide-core.h>

#include "ide-build-pipeline.h"

G_BEGIN_DECLS

#define IDE_TYPE_BUILD_PIPELINE_ADDIN (ide_build_pipeline_addin_get_type())

IDE_AVAILABLE_IN_3_32
G_DECLARE_INTERFACE (IdeBuildPipelineAddin, ide_build_pipeline_addin, IDE, BUILD_PIPELINE_ADDIN, IdeObject)

struct _IdeBuildPipelineAddinInterface
{
  GTypeInterface type_interface;

  void (*load)   (IdeBuildPipelineAddin *self,
                  IdeBuildPipeline      *pipeline);
  void (*unload) (IdeBuildPipelineAddin *self,
                  IdeBuildPipeline      *pipeline);
};

IDE_AVAILABLE_IN_3_32
void ide_build_pipeline_addin_load   (IdeBuildPipelineAddin *self,
                                      IdeBuildPipeline      *pipeline);
IDE_AVAILABLE_IN_3_32
void ide_build_pipeline_addin_unload (IdeBuildPipelineAddin *self,
                                      IdeBuildPipeline      *pipeline);
IDE_AVAILABLE_IN_3_32
void ide_build_pipeline_addin_track  (IdeBuildPipelineAddin *self,
                                      guint                  stage_id);

G_END_DECLS
