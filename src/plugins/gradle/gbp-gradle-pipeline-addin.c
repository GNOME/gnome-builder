/* gbp-gradle-pipeline-addin.c
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

#define G_LOG_DOMAIN "gbp-gradle-pipeline-addin"

#include "config.h"

#include <glib/gi18n.h>

#include <libide-foundry.h>

#include "gbp-gradle-build-system.h"
#include "gbp-gradle-pipeline-addin.h"

struct _GbpGradlePipelineAddin
{
  IdeObject parent_instance;
};

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

  /* Always defer to gradle to check if build is needed */
  ide_pipeline_stage_set_completed (stage, FALSE);
}

static void
gradlew_query_cb (IdePipelineStage *stage,
                  IdePipeline      *pipeline,
                  GPtrArray        *targets,
                  GCancellable     *cancellable,
                  const char       *gradlew_path)
{
  if (g_file_test (gradlew_path, G_FILE_TEST_IS_EXECUTABLE))
    ide_pipeline_stage_set_completed (stage, TRUE);
}

static void
gbp_gradle_pipeline_addin_load (IdePipelineAddin *addin,
                                IdePipeline      *pipeline)
{
  g_autoptr(IdePipelineStage) wrapper_stage = NULL;
  g_autoptr(IdePipelineStage) build_stage = NULL;
  IdeBuildSystem *build_system;
  const char *srcdir;
  IdeContext *context;
  guint id;

  g_autoptr(IdeRunCommand) build_command = NULL;
  g_autoptr(IdeRunCommand) clean_command = NULL;
  g_autoptr(IdeRunCommand) wrapper_command = NULL;

  IDE_ENTRY;

  g_assert (GBP_IS_GRADLE_PIPELINE_ADDIN (addin));
  g_assert (IDE_IS_PIPELINE (pipeline));

  context = ide_object_get_context (IDE_OBJECT (addin));
  build_system = ide_build_system_from_context (context);
  srcdir = ide_pipeline_get_srcdir (pipeline);

  if (!GBP_IS_GRADLE_BUILD_SYSTEM (build_system))
    IDE_EXIT;

  wrapper_command = ide_run_command_new ();
  ide_run_command_set_argv (wrapper_command, IDE_STRV_INIT ("gradle", "wrapper"));
  ide_run_command_set_cwd (wrapper_command, srcdir);
  wrapper_stage = ide_pipeline_stage_command_new (wrapper_command, NULL);
  ide_pipeline_stage_set_name (wrapper_stage, _("Bootstrapping project"));
  g_signal_connect_data (wrapper_stage,
                         "query",
                         G_CALLBACK (gradlew_query_cb),
                         g_build_filename (srcdir, "gradlew", NULL),
                         (GClosureNotify)g_free,
                         0);
  id = ide_pipeline_attach (pipeline, IDE_PIPELINE_PHASE_AUTOGEN, 0, wrapper_stage);
  ide_pipeline_addin_track (addin, id);

  build_command = ide_run_command_new ();
  ide_run_command_set_argv (build_command, IDE_STRV_INIT ("./gradlew", "build"));
  ide_run_command_set_cwd (build_command, srcdir);
  clean_command = ide_run_command_new ();
  ide_run_command_set_argv (clean_command, IDE_STRV_INIT ("./gradlew", "clean"));
  ide_run_command_set_cwd (clean_command, srcdir);
  build_stage = ide_pipeline_stage_command_new (build_command, clean_command);
  ide_pipeline_stage_set_name (build_stage, _("Building project"));
  g_signal_connect (build_stage, "query", G_CALLBACK (query_cb), NULL);
  id = ide_pipeline_attach (pipeline, IDE_PIPELINE_PHASE_BUILD, 0, build_stage);
  ide_pipeline_addin_track (addin, id);

  IDE_EXIT;
}

static void
pipeline_addin_iface_init (IdePipelineAddinInterface *iface)
{
  iface->load = gbp_gradle_pipeline_addin_load;
}

G_DEFINE_FINAL_TYPE_WITH_CODE (GbpGradlePipelineAddin, gbp_gradle_pipeline_addin, IDE_TYPE_OBJECT,
                               G_IMPLEMENT_INTERFACE (IDE_TYPE_PIPELINE_ADDIN, pipeline_addin_iface_init))

static void
gbp_gradle_pipeline_addin_class_init (GbpGradlePipelineAddinClass *klass)
{
}

static void
gbp_gradle_pipeline_addin_init (GbpGradlePipelineAddin *self)
{
}
