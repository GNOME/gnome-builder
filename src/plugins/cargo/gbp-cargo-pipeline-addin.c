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

G_GNUC_NULL_TERMINATED
static IdeRunContext *
create_run_context (IdePipeline *pipeline,
                    const char  *project_dir,
                    const char  *argv,
                    ...)
{
  IdeRunContext *ret;
  const char *builddir;
  va_list args;

  g_assert (IDE_IS_PIPELINE (pipeline));

  ret = ide_run_context_new ();
  ide_pipeline_prepare_run_context (pipeline, ret);

  builddir = ide_pipeline_get_builddir (pipeline);
  ide_run_context_setenv (ret, "CARGO_TARGET_DIR", builddir);
  ide_run_context_set_cwd (ret, project_dir);

  va_start (args, argv);
  while (argv != NULL)
    {
      ide_run_context_append_argv (ret, argv);
      argv = va_arg (args, const char *);
    }
  va_end (args);

  return ret;
}

static IdePipelineStage *
attach_run_context (GbpCargoPipelineAddin *self,
                    IdePipeline           *pipeline,
                    IdePipelinePhase       phase,
                    IdeRunContext         *build_context,
                    IdeRunContext         *clean_context,
                    const char            *title)
{
  g_autoptr(IdeSubprocessLauncher) launcher = NULL;
  g_autoptr(IdeSubprocessLauncher) clean_launcher = NULL;
  g_autoptr(IdePipelineStage) stage = NULL;
  g_autoptr(GError) error = NULL;
  IdeContext *context;
  guint id;

  g_assert (GBP_IS_CARGO_PIPELINE_ADDIN (self));
  g_assert (IDE_IS_PIPELINE (pipeline));
  g_assert (IDE_IS_RUN_CONTEXT (build_context));
  g_assert (!clean_context || IDE_IS_RUN_CONTEXT (clean_context));

  if (!(launcher = ide_run_context_end (build_context, &error)))
    {
      g_critical ("Failed to create launcher from run context: %s",
                  error->message);
      return NULL;
    }

  if (clean_context != NULL &&
      !(clean_launcher = ide_run_context_end (clean_context, &error)))
    {
      g_critical ("Failed to create launcher from run context: %s",
                  error->message);
      return NULL;
    }

  context = ide_object_get_context (IDE_OBJECT (pipeline));
  stage = ide_pipeline_stage_launcher_new (context, NULL);

  g_object_set (stage,
                "launcher", launcher,
                "clean-launcher", clean_launcher,
                "name", title,
                NULL);

  id = ide_pipeline_attach (pipeline, phase, 0, stage);
  ide_pipeline_addin_track (IDE_PIPELINE_ADDIN (self), id);

  /* return borrowed reference */
  return stage;
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
  GbpCargoPipelineAddin *self = (GbpCargoPipelineAddin *)addin;
  g_autoptr(IdeRunContext) fetch_context = NULL;
  g_autoptr(IdeRunContext) build_context = NULL;
  g_autoptr(IdeRunContext) clean_context = NULL;
  g_autoptr(IdePipelineStage) stage = NULL;
  g_autofree char *project_dir = NULL;
  g_autofree char *cargo = NULL;
  IdeBuildSystem *build_system;
  const char *config_opts;
  IdeContext *context;
  IdeConfig *config;

  IDE_ENTRY;

  g_assert (GBP_IS_CARGO_PIPELINE_ADDIN (self));
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

  fetch_context = create_run_context (pipeline, project_dir, cargo, "fetch", NULL);
  attach_run_context (self, pipeline, IDE_PIPELINE_PHASE_DOWNLOADS, fetch_context, NULL, _("Fetch dependencies"));

  build_context = create_run_context (pipeline, project_dir, cargo, "build", "--message-format", "human", NULL);
  clean_context = create_run_context (pipeline, project_dir, cargo, "clean", NULL);

  if (!ide_pipeline_is_native (pipeline))
    {
      IdeTriplet *triplet = ide_pipeline_get_host_triplet (pipeline);

      ide_run_context_append_argv (build_context, "--target");
      ide_run_context_append_argv (build_context, ide_triplet_get_full_name (triplet));
    }

  if (ide_config_get_parallelism (config) > 0)
    {
      int j = ide_config_get_parallelism (config);
      ide_run_context_append_formatted (build_context, "-j%d", j);
    }

  if (!ide_config_get_debug (config))
    ide_run_context_append_argv (build_context, "--release");

  /* Configure Options get passed to "cargo build" because there is no
   * equivalent "configure stage" for cargo.
   */
  if (!ide_str_empty0 (config_opts))
    ide_run_context_append_args_parsed (build_context, config_opts, NULL);

  stage = attach_run_context (self, pipeline, IDE_PIPELINE_PHASE_BUILD,
                              build_context, clean_context,
                              _("Build project"));
  g_signal_connect (stage, "query", G_CALLBACK (query_cb), NULL);

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
