/* ide-pipeline-addin.h
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

#include "ide-pipeline.h"

G_BEGIN_DECLS

#define IDE_TYPE_PIPELINE_ADDIN (ide_pipeline_addin_get_type())

IDE_AVAILABLE_IN_ALL
G_DECLARE_INTERFACE (IdePipelineAddin, ide_pipeline_addin, IDE, PIPELINE_ADDIN, IdeObject)

struct _IdePipelineAddinInterface
{
  GTypeInterface type_interface;

  void (*load)    (IdePipelineAddin *self,
                   IdePipeline      *pipeline);
  void (*unload)  (IdePipelineAddin *self,
                   IdePipeline      *pipeline);
  void (*prepare) (IdePipelineAddin *self,
                   IdePipeline      *pipeline);
};

IDE_AVAILABLE_IN_ALL
IdePipelineAddin *ide_pipeline_addin_find_by_module_name (IdePipeline *pipeline,
                                                          const gchar *module_name);
IDE_AVAILABLE_IN_ALL
void              ide_pipeline_addin_prepare             (IdePipelineAddin *self,
                                                          IdePipeline      *pipeline);
IDE_AVAILABLE_IN_ALL
void              ide_pipeline_addin_load                (IdePipelineAddin *self,
                                                          IdePipeline      *pipeline);
IDE_AVAILABLE_IN_ALL
void              ide_pipeline_addin_unload              (IdePipelineAddin *self,
                                                          IdePipeline      *pipeline);
IDE_AVAILABLE_IN_ALL
void              ide_pipeline_addin_track               (IdePipelineAddin *self,
                                                          guint             stage_id);

G_END_DECLS
