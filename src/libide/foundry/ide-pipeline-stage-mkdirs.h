/* ide-pipeline-stage-mkdirs.h
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

#include "ide-pipeline-stage.h"

G_BEGIN_DECLS

#define IDE_TYPE_PIPELINE_STAGE_MKDIRS (ide_pipeline_stage_mkdirs_get_type())

IDE_AVAILABLE_IN_ALL
G_DECLARE_DERIVABLE_TYPE (IdePipelineStageMkdirs, ide_pipeline_stage_mkdirs, IDE, PIPELINE_STAGE_MKDIRS, IdePipelineStage)

struct _IdePipelineStageMkdirsClass
{
  IdePipelineStageClass parent_class;

  /*< private >*/
  gpointer _reserved[8];
};

IDE_AVAILABLE_IN_ALL
IdePipelineStage *ide_pipeline_stage_mkdirs_new      (IdeContext             *context);
IDE_AVAILABLE_IN_ALL
void              ide_pipeline_stage_mkdirs_add_path (IdePipelineStageMkdirs *self,
                                                      const gchar            *path,
                                                      gboolean                with_parents,
                                                      gint                    mode,
                                                      gboolean                remove_on_rebuild);

G_END_DECLS
