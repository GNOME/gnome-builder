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
  g_autoptr(IdeSubprocessLauncher) build_launcher = NULL;
  const char *make;
  IdeConfig *config;
  int j;

  g_assert (IDE_IS_PIPELINE_STAGE_LAUNCHER (stage));
  g_assert (IDE_IS_PIPELINE (pipeline));
  g_assert (!cancellable || G_IS_CANCELLABLE (cancellable));

  config = ide_pipeline_get_config (pipeline);
  if (!(make = ide_config_getenv (config, "MAKE")))
    make = "make";

  build_launcher = ide_pipeline_create_launcher (pipeline, NULL);
  ide_subprocess_launcher_push_argv (build_launcher, make);

  if ((j = ide_config_get_parallelism (config)) > 0)
    ide_subprocess_launcher_push_argv_format (build_launcher, "-j%d", j);

  if (targets != NULL && targets->len > 0)
    {
      for (guint i = 0; i < targets->len; i++)
        {
          IdeBuildTarget *build_target = g_ptr_array_index (targets, i);
          const char *make_target;

          if (!GBP_IS_MAKE_BUILD_TARGET (build_target))
            continue;

          if ((make_target = gbp_make_build_target_get_make_target (GBP_MAKE_BUILD_TARGET (build_target))))
            ide_subprocess_launcher_push_argv (build_launcher, make_target);
        }
    }

  ide_subprocess_launcher_push_args (build_launcher,
                                     ide_config_get_args_for_phase (config, IDE_PIPELINE_PHASE_BUILD));

  /* Always defer to make to check if build is needed */
  ide_pipeline_stage_launcher_set_launcher (IDE_PIPELINE_STAGE_LAUNCHER (stage), build_launcher);
  ide_pipeline_stage_set_completed (stage, FALSE);
}

static void
gbp_make_pipeline_addin_load (IdePipelineAddin *addin,
                              IdePipeline      *pipeline)
{
  g_autoptr(IdeSubprocessLauncher) clean_launcher = NULL;
  g_autoptr(IdeSubprocessLauncher) install_launcher = NULL;
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

  clean_launcher = ide_pipeline_create_launcher (pipeline, NULL);
  ide_subprocess_launcher_push_args (clean_launcher, IDE_STRV_INIT (make, "clean"));

  build_stage = ide_pipeline_stage_launcher_new (context, NULL);
  ide_pipeline_stage_set_name (build_stage, _("Building project"));
  ide_pipeline_stage_launcher_set_clean_launcher (IDE_PIPELINE_STAGE_LAUNCHER (build_stage), clean_launcher);
  g_signal_connect (build_stage, "query", G_CALLBACK (query_cb), NULL);
  id = ide_pipeline_attach (pipeline, IDE_PIPELINE_PHASE_BUILD, 0, build_stage);
  ide_pipeline_addin_track (addin, id);

  install_launcher = ide_pipeline_create_launcher (pipeline, NULL);
  ide_subprocess_launcher_push_args (install_launcher, IDE_STRV_INIT (make, "install"));
  ide_subprocess_launcher_push_args (install_launcher,
                                     ide_config_get_args_for_phase (config, IDE_PIPELINE_PHASE_INSTALL));

  install_stage = ide_pipeline_stage_launcher_new (context, install_launcher);
  ide_pipeline_stage_set_name (install_stage, _("Installing project"));
  g_signal_connect (install_stage, "query", G_CALLBACK (query_cb), NULL);
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
