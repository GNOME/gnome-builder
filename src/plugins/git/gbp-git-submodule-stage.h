/* gbp-git-submodule-stage.h
 *
 * Copyright 2018-2019 Christian Hergert <chergert@redhat.com>
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

#include <libide-foundry.h>

G_BEGIN_DECLS

#define GBP_TYPE_GIT_SUBMODULE_STAGE (gbp_git_submodule_stage_get_type())

G_DECLARE_FINAL_TYPE (GbpGitSubmoduleStage, gbp_git_submodule_stage, GBP, GIT_SUBMODULE_STAGE, IdePipelineStageCommand)

GbpGitSubmoduleStage *gbp_git_submodule_stage_new          (IdeContext           *context);
void                  gbp_git_submodule_stage_force_update (GbpGitSubmoduleStage *self);

G_END_DECLS
