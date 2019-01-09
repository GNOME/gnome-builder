/* ide-git-pipeline-addin.c
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

#include "config.h"

#define G_LOG_DOMAIN "ide-git-pipeline-addin"

#include <glib/gi18n.h>

#include "ide-git-pipeline-addin.h"
#include "ide-git-submodule-stage.h"
#include "ide-git-vcs.h"

struct _IdeGitPipelineAddin
{
  IdeObject parent_instance;
};

static void
ide_git_pipeline_addin_load (IdeBuildPipelineAddin *addin,
                             IdeBuildPipeline      *pipeline)
{
  g_autoptr(IdeGitSubmoduleStage) submodule = NULL;
  IdeContext *context;
  IdeVcs *vcs;
  guint stage_id;

  g_assert (IDE_IS_GIT_PIPELINE_ADDIN (addin));
  g_assert (IDE_IS_BUILD_PIPELINE (pipeline));

  context = ide_object_get_context (IDE_OBJECT (addin));
  vcs = ide_context_get_vcs (context);

  /* Ignore everything if this isn't a git-based repository */
  if (!IDE_IS_GIT_VCS (vcs))
    return;

  submodule = ide_git_submodule_stage_new (context);
  stage_id = ide_build_pipeline_connect (pipeline,
                                         IDE_BUILD_PHASE_DOWNLOADS,
                                         100,
                                         IDE_BUILD_STAGE (submodule));
  ide_build_pipeline_addin_track (addin, stage_id);
}

static void
build_pipeline_addin_iface_init (IdeBuildPipelineAddinInterface *iface)
{
  iface->load = ide_git_pipeline_addin_load;
}

G_DEFINE_TYPE_WITH_CODE (IdeGitPipelineAddin, ide_git_pipeline_addin, IDE_TYPE_OBJECT,
                         G_IMPLEMENT_INTERFACE (IDE_TYPE_BUILD_PIPELINE_ADDIN,
                                                build_pipeline_addin_iface_init))

static void
ide_git_pipeline_addin_class_init (IdeGitPipelineAddinClass *klass)
{
}

static void
ide_git_pipeline_addin_init (IdeGitPipelineAddin *self)
{
}
