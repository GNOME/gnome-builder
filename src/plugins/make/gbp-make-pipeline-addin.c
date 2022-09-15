/* gbp-make-pipeline-addin.c
 *
 * Copyright 2017 Matthew Leeds <mleeds@redhat.com>
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

#define G_LOG_DOMAIN "gbp-make-pipeline-addin"

#include "config.h"

#include <glib/gi18n.h>

#include <libide-foundry.h>

#include "gbp-make-build-system.h"
#include "gbp-make-build-target.h"
#include "gbp-make-pipeline-addin.h"

struct _GbpMakePipelineAddin
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
  g_autoptr(IdeRunCommand) build_command = NULL;
  const char *make;
  IdeConfig *config;
  int j;

  g_assert (IDE_IS_PIPELINE_STAGE_COMMAND (stage));
  g_assert (IDE_IS_PIPELINE (pipeline));
  g_assert (!cancellable || G_IS_CANCELLABLE (cancellable));

  config = ide_pipeline_get_config (pipeline);
  if (!(make = ide_config_getenv (config, "MAKE")))
    make = "make";

  build_command = ide_run_command_new ();
  ide_run_command_append_argv (build_command, make);
  ide_run_command_set_cwd (build_command, ide_pipeline_get_builddir (pipeline));

  if ((j = ide_config_get_parallelism (config)) > 0)
    {
      char arg[12];
      g_snprintf (arg, sizeof arg, "-j%d", j);
      ide_run_command_append_argv (build_command, arg);
    }

  if (targets != NULL && targets->len > 0)
    {
      for (guint i = 0; i < targets->len; i++)
        {
          IdeBuildTarget *build_target = g_ptr_array_index (targets, i);
          const char *make_target;

          if (!GBP_IS_MAKE_BUILD_TARGET (build_target))
            continue;

          if ((make_target = gbp_make_build_target_get_make_target (GBP_MAKE_BUILD_TARGET (build_target))))
            ide_run_command_append_argv (build_command, make_target);
        }
    }

  ide_run_command_append_args (build_command,
                               ide_config_get_args_for_phase (config, IDE_PIPELINE_PHASE_BUILD));

  ide_pipeline_stage_command_set_build_command (IDE_PIPELINE_STAGE_COMMAND (stage), build_command);

  /* Always defer to make to check if build is needed */
  ide_pipeline_stage_set_completed (stage, FALSE);
}

static void
always_run_query_cb (IdePipelineStage *stage,
                     IdePipeline      *pipeline,
                     GPtrArray        *targets,
                     GCancellable     *cancellable,
                     gpointer          user_data)
{
  /* Always defer to make to check if install is needed */
  ide_pipeline_stage_set_completed (stage, FALSE);
}

static void
gbp_make_pipeline_addin_load (IdePipelineAddin *addin,
                              IdePipeline      *pipeline)
{
  g_autoptr(IdeRunCommand) clean_command = NULL;
  g_autoptr(IdeRunCommand) install_command = NULL;
  g_autoptr(IdePipelineStage) build_stage = NULL;
  g_autoptr(IdePipelineStage) install_stage = NULL;
  IdeBuildSystem *build_system;
  const char *make;
  IdeContext *context;
  IdeConfig *config;
  guint id;

  IDE_ENTRY;

  g_assert (GBP_IS_MAKE_PIPELINE_ADDIN (addin));
  g_assert (IDE_IS_PIPELINE (pipeline));

  context = ide_object_get_context (IDE_OBJECT (addin));
  build_system = ide_build_system_from_context (context);

  if (!GBP_IS_MAKE_BUILD_SYSTEM (build_system))
    IDE_EXIT;

  config = ide_pipeline_get_config (pipeline);
  if (!(make = ide_config_getenv (config, "MAKE")))
    make = "make";

  g_assert (IDE_IS_CONFIG (config));
  g_assert (make != NULL);

  clean_command = ide_run_command_new ();
  ide_run_command_append_args (clean_command, IDE_STRV_INIT (make, "clean"));

  build_stage = g_object_new (IDE_TYPE_PIPELINE_STAGE_COMMAND,
                              "clean-command", clean_command,
                              "name", _("Building project"),
                              NULL);
  g_signal_connect (build_stage, "query", G_CALLBACK (query_cb), NULL);
  id = ide_pipeline_attach (pipeline, IDE_PIPELINE_PHASE_BUILD, 0, build_stage);
  ide_pipeline_addin_track (addin, id);

  install_command = ide_run_command_new ();
  ide_run_command_append_args (install_command, IDE_STRV_INIT (make, "install"));
  ide_run_command_append_args (install_command,
                               ide_config_get_args_for_phase (config, IDE_PIPELINE_PHASE_INSTALL));

  install_stage = g_object_new (IDE_TYPE_PIPELINE_STAGE_COMMAND,
                                "build-command", install_command,
                                "name", _("Installing project"),
                                NULL);
  g_signal_connect (install_stage, "query", G_CALLBACK (always_run_query_cb), NULL);
  id = ide_pipeline_attach (pipeline, IDE_PIPELINE_PHASE_INSTALL, 0, install_stage);
  ide_pipeline_addin_track (addin, id);

  IDE_EXIT;
}

static void
pipeline_addin_iface_init (IdePipelineAddinInterface *iface)
{
  iface->load = gbp_make_pipeline_addin_load;
}

G_DEFINE_FINAL_TYPE_WITH_CODE (GbpMakePipelineAddin, gbp_make_pipeline_addin, IDE_TYPE_OBJECT,
                               G_IMPLEMENT_INTERFACE (IDE_TYPE_PIPELINE_ADDIN, pipeline_addin_iface_init))

static void
gbp_make_pipeline_addin_class_init (GbpMakePipelineAddinClass *klass)
{
}

static void
gbp_make_pipeline_addin_init (GbpMakePipelineAddin *self)
{
}
