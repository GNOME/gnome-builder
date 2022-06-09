/* gbp-npm-pipeline-addin.c
 *
 * Copyright 2018 danigm <danigm@wadobo.com>
 * Copyright 2018 Alberto Fanjul <albfan@gnome.org>
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

#define G_LOG_DOMAIN "gbp-npm-pipeline-addin"

#include "config.h"

#include <glib/gi18n.h>

#include <libide-foundry.h>

#include "gbp-npm-build-system.h"
#include "gbp-npm-pipeline-addin.h"

struct _GbpNpmPipelineAddin
{
  IdeObject parent_instance;
};

static void
gbp_npm_pipeline_addin_load (IdePipelineAddin *addin,
                             IdePipeline      *pipeline)
{
  g_autoptr(IdeSubprocessLauncher) fetch_launcher = NULL;
  g_autoptr(IdePipelineStage) fetch_stage = NULL;
  g_autofree char *project_dir = NULL;
  IdeBuildSystem *build_system;
  const char *npm;
  IdeContext *context;
  IdeConfig *config;
  guint id;

  IDE_ENTRY;

  g_assert (GBP_IS_NPM_PIPELINE_ADDIN (addin));
  g_assert (IDE_IS_PIPELINE (pipeline));

  context = ide_object_get_context (IDE_OBJECT (addin));
  build_system = ide_build_system_from_context (context);
  config = ide_pipeline_get_config (pipeline);

  if (!GBP_IS_NPM_BUILD_SYSTEM (build_system))
    IDE_EXIT;

  npm = ide_config_getenv (config, "NPM");
  if (ide_str_empty0 (npm))
    npm = "npm";

  project_dir = gbp_npm_build_system_get_project_dir (GBP_NPM_BUILD_SYSTEM (build_system));

  /* Fetch dependencies so that we no longer need network access */
  fetch_launcher = ide_pipeline_create_launcher (pipeline, NULL);
  ide_subprocess_launcher_set_cwd (fetch_launcher, project_dir);
  ide_subprocess_launcher_push_argv (fetch_launcher, npm);
  if (!ide_pipeline_is_native (pipeline))
    {
      IdeTriplet *triplet = ide_pipeline_get_host_triplet (pipeline);
      const char *arch = ide_triplet_get_arch (triplet);
      ide_subprocess_launcher_push_args (fetch_launcher, IDE_STRV_INIT ("--arch", arch));
    }
  ide_subprocess_launcher_push_argv (fetch_launcher, "install");
  fetch_stage = ide_pipeline_stage_launcher_new (context, fetch_launcher);
  ide_pipeline_stage_set_name (fetch_stage, _("Downloading npm dependencies"));
  id = ide_pipeline_attach (pipeline, IDE_PIPELINE_PHASE_DOWNLOADS, 0, fetch_stage);
  ide_pipeline_addin_track (addin, id);

  IDE_EXIT;
}

static void
pipeline_addin_iface_init (IdePipelineAddinInterface *iface)
{
  iface->load = gbp_npm_pipeline_addin_load;
}

G_DEFINE_FINAL_TYPE_WITH_CODE (GbpNpmPipelineAddin, gbp_npm_pipeline_addin, IDE_TYPE_OBJECT,
                               G_IMPLEMENT_INTERFACE (IDE_TYPE_PIPELINE_ADDIN, pipeline_addin_iface_init))

static void
gbp_npm_pipeline_addin_class_init (GbpNpmPipelineAddinClass *klass)
{
}

static void
gbp_npm_pipeline_addin_init (GbpNpmPipelineAddin *self)
{
}
