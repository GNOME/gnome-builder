/* gbp-cargo-pipeline-addin.c
 *
 * Copyright 2016-2022 Christian Hergert <chergert@redhat.com>
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

#define G_LOG_DOMAIN "gbp-cargo-pipeline-addin"

#include "config.h"

#include <glib/gi18n.h>

#include <libide-foundry.h>

#include "gbp-cargo-build-system.h"
#include "gbp-cargo-pipeline-addin.h"

struct _GbpCargoPipelineAddin
{
  IdeObject parent_instance;
};

static IdeSubprocessLauncher *
create_launcher (IdePipeline *pipeline,
                 const char  *project_dir,
                 const char  *cargo)
{
  IdeSubprocessLauncher *ret;
  const char *builddir;

  g_assert (IDE_IS_PIPELINE (pipeline));

  if (!(ret = ide_pipeline_create_launcher (pipeline, NULL)))
    return NULL;

  builddir = ide_pipeline_get_builddir (pipeline);
  ide_subprocess_launcher_setenv (ret, "CARGO_TARGET_DIR", builddir, TRUE);
  ide_subprocess_launcher_set_cwd (ret, project_dir);
  ide_subprocess_launcher_push_argv (ret, cargo);

  return ret;
}

static void
query_cb (IdePipelineStage *stage,
          IdePipeline      *pipeline,
          GPtrArray        *targets,
          GCancellable     *cancellable,
          gpointer          user_data)
{
  g_assert (IDE_IS_PIPELINE_STAGE (stage));
  g_assert (IDE_IS_PIPELINE (pipeline));
  g_assert (!cancellable || G_IS_CANCELLABLE (cancellable));

  /* Always defer to cargo to check if build is needed */
  ide_pipeline_stage_set_completed (stage, FALSE);
}

static void
gbp_cargo_pipeline_addin_load (IdePipelineAddin *addin,
                               IdePipeline      *pipeline)
{
  g_autoptr(IdeSubprocessLauncher) fetch_launcher = NULL;
  g_autoptr(IdeSubprocessLauncher) build_launcher = NULL;
  g_autoptr(IdeSubprocessLauncher) clean_launcher = NULL;
  g_autoptr(IdePipelineStage) stage = NULL;
  g_autofree char *project_dir = NULL;
  g_autofree char *cargo = NULL;
  IdeBuildSystem *build_system;
  const char *config_opts;
  IdeContext *context;
  IdeConfig *config;
  guint id;

  IDE_ENTRY;

  g_assert (GBP_IS_CARGO_PIPELINE_ADDIN (addin));
  g_assert (IDE_IS_PIPELINE (pipeline));

  context = ide_object_get_context (IDE_OBJECT (addin));
  build_system = ide_build_system_from_context (context);

  if (!GBP_IS_CARGO_BUILD_SYSTEM (build_system))
    IDE_EXIT;

  project_dir = gbp_cargo_build_system_get_project_dir (GBP_CARGO_BUILD_SYSTEM (build_system));
  config = ide_pipeline_get_config (pipeline);
  config_opts = ide_config_get_config_opts (config);
  cargo = gbp_cargo_build_system_locate_cargo (GBP_CARGO_BUILD_SYSTEM (build_system), pipeline, config);

  g_assert (project_dir != NULL);
  g_assert (IDE_IS_CONFIG (config));
  g_assert (cargo != NULL);

  fetch_launcher = create_launcher (pipeline, project_dir, cargo);
  ide_subprocess_launcher_push_argv (fetch_launcher, "fetch");
  id = ide_pipeline_attach_launcher (pipeline, IDE_PIPELINE_PHASE_DOWNLOADS, 0, fetch_launcher);
  ide_pipeline_addin_track (addin, id);

  build_launcher = create_launcher (pipeline, project_dir, cargo);
  ide_subprocess_launcher_push_argv (build_launcher, "build");
  ide_subprocess_launcher_push_argv (build_launcher, "--message-format");
  ide_subprocess_launcher_push_argv (build_launcher, "human");

  if (!ide_pipeline_is_native (pipeline))
    {
      IdeTriplet *triplet = ide_pipeline_get_host_triplet (pipeline);

      ide_subprocess_launcher_push_argv (build_launcher, "--target");
      ide_subprocess_launcher_push_argv (build_launcher, ide_triplet_get_full_name (triplet));
    }

  if (ide_config_get_parallelism (config) > 0)
    {
      int j = ide_config_get_parallelism (config);
      ide_subprocess_launcher_push_argv_format (build_launcher, "-j%d", j);
    }

  if (!ide_config_get_debug (config))
    ide_subprocess_launcher_push_argv (build_launcher, "--release");

  /* Configure Options get passed to "cargo build" because there is no
   * equivalent "configure stage" for cargo.
   */
  ide_subprocess_launcher_push_argv_parsed (build_launcher, config_opts);

  clean_launcher = create_launcher (pipeline, project_dir, cargo);
  ide_subprocess_launcher_push_argv (clean_launcher, "clean");

  stage = ide_pipeline_stage_launcher_new (context, build_launcher);
  ide_pipeline_stage_set_name (stage, _("Building project"));
  ide_pipeline_stage_launcher_set_clean_launcher (IDE_PIPELINE_STAGE_LAUNCHER (stage), clean_launcher);
  g_signal_connect (stage, "query", G_CALLBACK (query_cb), NULL);
  id = ide_pipeline_attach (pipeline, IDE_PIPELINE_PHASE_BUILD, 0, stage);
  ide_pipeline_addin_track (addin, id);

  IDE_EXIT;
}

static void
pipeline_addin_iface_init (IdePipelineAddinInterface *iface)
{
  iface->load = gbp_cargo_pipeline_addin_load;
}

G_DEFINE_FINAL_TYPE_WITH_CODE (GbpCargoPipelineAddin, gbp_cargo_pipeline_addin, IDE_TYPE_OBJECT,
                               G_IMPLEMENT_INTERFACE (IDE_TYPE_PIPELINE_ADDIN, pipeline_addin_iface_init))

static void
gbp_cargo_pipeline_addin_class_init (GbpCargoPipelineAddinClass *klass)
{
}

static void
gbp_cargo_pipeline_addin_init (GbpCargoPipelineAddin *self)
{
}
