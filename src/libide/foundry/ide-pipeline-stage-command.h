/* ide-pipeline-stage-command.h
 *
 * Copyright 2022 Christian Hergert <chergert@redhat.com>
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

#include "ide-foundry-types.h"
#include "ide-pipeline-stage.h"

G_BEGIN_DECLS

#define IDE_TYPE_PIPELINE_STAGE_COMMAND (ide_pipeline_stage_command_get_type())

IDE_AVAILABLE_IN_ALL
G_DECLARE_DERIVABLE_TYPE (IdePipelineStageCommand, ide_pipeline_stage_command, IDE, PIPELINE_STAGE_COMMAND, IdePipelineStage)

struct _IdePipelineStageCommandClass
{
  IdePipelineStageClass parent_class;
};

IDE_AVAILABLE_IN_ALL
IdePipelineStage *ide_pipeline_stage_command_new                    (IdeRunCommand *build_command,
                                                                     IdeRunCommand *clean_command);
IDE_AVAILABLE_IN_ALL
void              ide_pipeline_stage_command_set_build_command      (IdePipelineStageCommand *self,
                                                                     IdeRunCommand           *build_command);
IDE_AVAILABLE_IN_ALL
void              ide_pipeline_stage_command_set_clean_command      (IdePipelineStageCommand *self,
                                                                     IdeRunCommand           *clean_command);
IDE_AVAILABLE_IN_45
void              ide_pipeline_stage_command_set_stdout_path        (IdePipelineStageCommand *self,
                                                                     const char              *stdout_path);
IDE_AVAILABLE_IN_45
void              ide_pipeline_stage_command_set_ignore_exit_status (IdePipelineStageCommand *self,
                                                                     gboolean                 ignore_exit_status);

G_END_DECLS
