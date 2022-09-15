/* gbp-buildstream-pipeline-addin.c
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

#define G_LOG_DOMAIN "gbp-buildstream-pipeline-addin"

#include "config.h"

#include <glib/gi18n.h>

#include <libide-foundry.h>

#include "gbp-buildstream-build-system.h"
#include "gbp-buildstream-pipeline-addin.h"

struct _GbpBuildstreamPipelineAddin
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

  /* Always defer to buildstream to check if build is needed */
  ide_pipeline_stage_set_completed (stage, FALSE);
}

static void
gbp_buildstream_pipeline_addin_load (IdePipelineAddin *addin,
                                     IdePipeline      *pipeline)
{
  g_autoptr(IdeRunCommand) build_command = NULL;
  g_autoptr(IdePipelineStage) stage = NULL;
  IdeBuildSystem *build_system;
  IdeContext *context;
  guint id;

  IDE_ENTRY;

  g_assert (GBP_IS_BUILDSTREAM_PIPELINE_ADDIN (addin));
  g_assert (IDE_IS_PIPELINE (pipeline));

  context = ide_object_get_context (IDE_OBJECT (addin));
  build_system = ide_build_system_from_context (context);

  if (!GBP_IS_BUILDSTREAM_BUILD_SYSTEM (build_system))
    IDE_EXIT;

  if (!ide_pipeline_contains_program_in_path (pipeline, "bst", NULL))
    {
      ide_object_message (addin, "%s",
                          _("BuildStream project in use but “bst” executable could not be found."));
      IDE_EXIT;
    }

  build_command = ide_run_command_new ();
  ide_run_command_set_argv (build_command, IDE_STRV_INIT ("bst", "build"));
  ide_run_command_set_cwd (build_command, ide_pipeline_get_srcdir (pipeline));

  stage = g_object_new (IDE_TYPE_PIPELINE_STAGE_COMMAND,
                        "build-command", build_command,
                        "name", _("Building project"),
                        NULL);
  g_signal_connect (stage, "query", G_CALLBACK (query_cb), NULL);
  id = ide_pipeline_attach (pipeline, IDE_PIPELINE_PHASE_BUILD, 0, stage);
  ide_pipeline_addin_track (addin, id);

  IDE_EXIT;
}

static void
pipeline_addin_iface_init (IdePipelineAddinInterface *iface)
{
  iface->load = gbp_buildstream_pipeline_addin_load;
}

G_DEFINE_FINAL_TYPE_WITH_CODE (GbpBuildstreamPipelineAddin, gbp_buildstream_pipeline_addin, IDE_TYPE_OBJECT,
                               G_IMPLEMENT_INTERFACE (IDE_TYPE_PIPELINE_ADDIN, pipeline_addin_iface_init))

static void
gbp_buildstream_pipeline_addin_class_init (GbpBuildstreamPipelineAddinClass *klass)
{
}

static void
gbp_buildstream_pipeline_addin_init (GbpBuildstreamPipelineAddin *self)
{
}
