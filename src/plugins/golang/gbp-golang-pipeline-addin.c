/* gbp-golang-pipeline-addin.c
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

#define G_LOG_DOMAIN "gbp-golang-pipeline-addin"

#include "config.h"

#include <glib/gi18n.h>

#include <libide-foundry.h>

#include "gbp-golang-build-system.h"
#include "gbp-golang-pipeline-addin.h"

struct _GbpGolangPipelineAddin
{
  IdeObject parent_instance;
};

G_GNUC_NULL_TERMINATED
static IdeRunCommand *
create_run_command (IdePipeline *pipeline,
                    const char  *project_dir,
                    const char  *goroot,
                    const char  *argv,
                    ...)
{
  IdeRunCommand *ret;
  va_list args;

  g_assert (IDE_IS_PIPELINE (pipeline));
  g_assert (project_dir != NULL);

  ret = ide_run_command_new ();
  if (goroot != NULL)
    ide_run_command_setenv (ret, "GOROOT", goroot);
  ide_run_command_set_cwd (ret, project_dir);

  va_start (args, argv);
  while (argv != NULL)
    {
      ide_run_command_append_argv (ret, argv);
      argv = va_arg (args, const char *);
    }
  va_end (args);

  return g_steal_pointer (&ret);
}

static IdePipelineStage *
attach_run_command (GbpGolangPipelineAddin *self,
                    IdePipeline           *pipeline,
                    IdePipelinePhase       phase,
                    IdeRunCommand         *build_command,
                    IdeRunCommand         *clean_command,
                    const char            *title)
{
  g_autoptr(IdePipelineStage) stage = NULL;
  guint id;

  g_assert (GBP_IS_GOLANG_PIPELINE_ADDIN (self));
  g_assert (IDE_IS_PIPELINE (pipeline));
  g_assert (IDE_IS_RUN_COMMAND (build_command));
  g_assert (!clean_command || IDE_IS_RUN_COMMAND (clean_command));

  stage = ide_pipeline_stage_command_new (build_command, clean_command);
  ide_pipeline_stage_set_name (stage, title);

  id = ide_pipeline_attach (pipeline, phase, 0, stage);
  ide_pipeline_addin_track (IDE_PIPELINE_ADDIN (self), id);

  return g_steal_pointer (&stage);
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

  /* Always defer to go to check if build is needed */
  ide_pipeline_stage_set_completed (stage, FALSE);
}

static void
gbp_golang_pipeline_addin_load (IdePipelineAddin *addin,
                               IdePipeline      *pipeline)
{
  GbpGolangPipelineAddin *self = (GbpGolangPipelineAddin *)addin;
  g_autoptr(IdeRunCommand) fetch_command = NULL;
  g_autoptr(IdePipelineStage) fetch_stage = NULL;
  g_autoptr(IdeRunCommand) build_command = NULL;
  g_autoptr(IdeRunCommand) clean_command = NULL;
  g_autoptr(IdePipelineStage) build_stage = NULL;
  g_autofree char *project_dir = NULL;
  g_autofree char *go = NULL;
  IdeBuildSystem *build_system;
  IdeContext *context;
  IdeConfig *config;
  const char *goroot;

  IDE_ENTRY;

  g_assert (GBP_IS_GOLANG_PIPELINE_ADDIN (self));
  g_assert (IDE_IS_PIPELINE (pipeline));

  context = ide_object_get_context (IDE_OBJECT (addin));
  build_system = ide_build_system_from_context (context);

  if (!GBP_IS_GOLANG_BUILD_SYSTEM (build_system))
    IDE_EXIT;

  project_dir = gbp_golang_build_system_get_project_dir (GBP_GOLANG_BUILD_SYSTEM (build_system));
  config = ide_pipeline_get_config (pipeline);
  go = gbp_golang_build_system_locate_go (GBP_GOLANG_BUILD_SYSTEM (build_system), pipeline, config);

  g_assert (project_dir != NULL);
  g_assert (IDE_IS_CONFIG (config));
  g_assert (go != NULL);

  if (!(goroot = ide_config_getenv (config, "GOROOT")))
    goroot = NULL;

  fetch_command = create_run_command (pipeline, project_dir, goroot, go, "get", "-v", NULL);
  fetch_stage = attach_run_command (self, pipeline, IDE_PIPELINE_PHASE_DOWNLOADS, fetch_command, NULL, _("Fetch dependencies"));

  g_object_set (fetch_stage,
                "ignore-exit-status", TRUE,
                NULL);

  build_command = create_run_command (pipeline, project_dir, goroot, go, "build", "-v", NULL);
  clean_command = create_run_command (pipeline, project_dir, goroot, go, "clean", "-v", NULL);

  build_stage = attach_run_command (self, pipeline, IDE_PIPELINE_PHASE_BUILD, build_command, clean_command, _("Build project"));
  g_signal_connect (build_stage, "query", G_CALLBACK (query_cb), NULL);

  IDE_EXIT;
}

static void
pipeline_addin_iface_init (IdePipelineAddinInterface *iface)
{
  iface->load = gbp_golang_pipeline_addin_load;
}

G_DEFINE_FINAL_TYPE_WITH_CODE (GbpGolangPipelineAddin, gbp_golang_pipeline_addin, IDE_TYPE_OBJECT,
                               G_IMPLEMENT_INTERFACE (IDE_TYPE_PIPELINE_ADDIN, pipeline_addin_iface_init))

static void
gbp_golang_pipeline_addin_class_init (GbpGolangPipelineAddinClass *klass)
{
}

static void
gbp_golang_pipeline_addin_init (GbpGolangPipelineAddin *self)
{
}

