/* gbp-git-pipeline-addin.c
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

#define G_LOG_DOMAIN "gbp-git-pipeline-addin"

#include "config.h"

#include <libide-foundry.h>
#include <glib/gi18n.h>

#include "gbp-git-pipeline-addin.h"
#include "gbp-git-submodule-stage.h"
#include "gbp-git-vcs.h"

struct _GbpGitPipelineAddin
{
  IdeObject parent_instance;
};

static void
gbp_git_pipeline_addin_load (IdePipelineAddin *addin,
                             IdePipeline      *pipeline)
{
  g_autoptr(GbpGitSubmoduleStage) submodule = NULL;
  IdeContext *context;
  IdeVcs *vcs;
  guint stage_id;

  g_assert (GBP_IS_GIT_PIPELINE_ADDIN (addin));
  g_assert (IDE_IS_PIPELINE (pipeline));

  context = ide_object_get_context (IDE_OBJECT (addin));
  vcs = ide_vcs_from_context (context);

  /* Ignore everything if this isn't a git-based repository */
  if (!GBP_IS_GIT_VCS (vcs))
    return;

  submodule = gbp_git_submodule_stage_new (context);
  stage_id = ide_pipeline_attach (pipeline,
                                  IDE_PIPELINE_PHASE_PREPARE | IDE_PIPELINE_PHASE_AFTER,
                                  100,
                                  IDE_PIPELINE_STAGE (submodule));
  ide_pipeline_addin_track (addin, stage_id);
}

static void
pipeline_addin_iface_init (IdePipelineAddinInterface *iface)
{
  iface->load = gbp_git_pipeline_addin_load;
}

G_DEFINE_FINAL_TYPE_WITH_CODE (GbpGitPipelineAddin, gbp_git_pipeline_addin, IDE_TYPE_OBJECT,
                         G_IMPLEMENT_INTERFACE (IDE_TYPE_PIPELINE_ADDIN,
                                                pipeline_addin_iface_init))

static void
gbp_git_pipeline_addin_class_init (GbpGitPipelineAddinClass *klass)
{
}

static void
gbp_git_pipeline_addin_init (GbpGitPipelineAddin *self)
{
}
