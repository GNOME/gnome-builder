/* ide-pipeline-stage-launcher.h
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
#include <libide-threading.h>

#include "ide-pipeline-stage.h"

G_BEGIN_DECLS

#define IDE_TYPE_PIPELINE_STAGE_LAUNCHER (ide_pipeline_stage_launcher_get_type())

IDE_AVAILABLE_IN_ALL
G_DECLARE_DERIVABLE_TYPE (IdePipelineStageLauncher, ide_pipeline_stage_launcher, IDE, PIPELINE_STAGE_LAUNCHER, IdePipelineStage)

struct _IdePipelineStageLauncherClass
{
  IdePipelineStageClass parent_class;

  /*< private >*/
  gpointer _reserved[8];
};

IDE_AVAILABLE_IN_ALL
IdePipelineStage         *ide_pipeline_stage_launcher_new                    (IdeContext            *context,
                                                                        IdeSubprocessLauncher *launcher);
IDE_AVAILABLE_IN_ALL
IdeSubprocessLauncher *ide_pipeline_stage_launcher_get_launcher           (IdePipelineStageLauncher *self);
IDE_AVAILABLE_IN_ALL
void                   ide_pipeline_stage_launcher_set_launcher           (IdePipelineStageLauncher *self,
                                                                        IdeSubprocessLauncher *launcher);
IDE_AVAILABLE_IN_ALL
IdeSubprocessLauncher *ide_pipeline_stage_launcher_get_clean_launcher     (IdePipelineStageLauncher *self);
IDE_AVAILABLE_IN_ALL
void                   ide_pipeline_stage_launcher_set_clean_launcher     (IdePipelineStageLauncher *self,
                                                                        IdeSubprocessLauncher *clean_launcher);
IDE_AVAILABLE_IN_ALL
gboolean               ide_pipeline_stage_launcher_get_ignore_exit_status (IdePipelineStageLauncher *self);
IDE_AVAILABLE_IN_ALL
void                   ide_pipeline_stage_launcher_set_ignore_exit_status (IdePipelineStageLauncher *self,
                                                                        gboolean               ignore_exit_status);
IDE_AVAILABLE_IN_ALL
gboolean               ide_pipeline_stage_launcher_get_use_pty            (IdePipelineStageLauncher *self);
IDE_AVAILABLE_IN_ALL
void                   ide_pipeline_stage_launcher_set_use_pty            (IdePipelineStageLauncher *self,
                                                                        gboolean               use_pty);

G_END_DECLS
