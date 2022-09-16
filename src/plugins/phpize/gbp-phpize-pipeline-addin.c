/* gbp-phpize-pipeline-addin.c
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

#define G_LOG_DOMAIN "gbp-phpize-pipeline-addin"

#include "config.h"

#include <glib/gi18n.h>

#include <libide-foundry.h>

#include "gbp-phpize-build-system.h"
#include "gbp-phpize-pipeline-addin.h"

struct _GbpPhpizePipelineAddin
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

  /* Always defer to make to check if build is needed */
  ide_pipeline_stage_set_completed (stage, FALSE);
}

static void
gbp_phpize_pipeline_addin_load (IdePipelineAddin *addin,
                               IdePipeline      *pipeline)
{
  g_autoptr(IdeRunCommand) bootstrap_command = NULL;
  g_autoptr(IdeRunCommand) config_command = NULL;
  g_autoptr(IdeRunCommand) build_command = NULL;
  g_autoptr(IdeRunCommand) clean_command = NULL;
  g_autoptr(IdeRunCommand) install_command = NULL;
  g_autoptr(IdePipelineStage) bootstrap_stage = NULL;
  g_autoptr(IdePipelineStage) config_stage = NULL;
  g_autoptr(IdePipelineStage) build_stage = NULL;
  g_autoptr(IdePipelineStage) install_stage = NULL;
  g_autofree char *configure_path = NULL;
  IdeBuildSystem *build_system;
  const char *builddir;
  const char *srcdir;
  const char *prefix;
  const char *config_opts;
  IdeContext *context;
  IdeConfig *config;
  guint id;
  int j;

  IDE_ENTRY;

  g_assert (GBP_IS_PHPIZE_PIPELINE_ADDIN (addin));
  g_assert (IDE_IS_PIPELINE (pipeline));

  context = ide_object_get_context (IDE_OBJECT (addin));
  build_system = ide_build_system_from_context (context);

  if (!GBP_IS_PHPIZE_BUILD_SYSTEM (build_system))
    IDE_EXIT;

  srcdir = ide_pipeline_get_srcdir (pipeline);
  builddir = ide_pipeline_get_srcdir (pipeline);
  config = ide_pipeline_get_config (pipeline);
  config_opts = ide_config_get_config_opts (config);
  configure_path = g_build_filename (srcdir, "configure", NULL);
  prefix = ide_config_get_prefix (config);

  g_assert (IDE_IS_CONFIG (config));
  g_assert (srcdir != NULL);
  g_assert (builddir != NULL);
  g_assert (prefix != NULL);

  bootstrap_command = ide_run_command_new ();
  ide_run_command_append_argv (bootstrap_command, "phpize");
  ide_run_command_set_cwd (bootstrap_command, srcdir);
  bootstrap_stage = ide_pipeline_stage_command_new (bootstrap_command, NULL);
  ide_pipeline_stage_set_name (bootstrap_stage, _("Bootstrapping project"));
  ide_pipeline_stage_set_completed (bootstrap_stage, g_file_test (configure_path, G_FILE_TEST_EXISTS));
  id = ide_pipeline_attach (pipeline, IDE_PIPELINE_PHASE_AUTOGEN, 0, bootstrap_stage);
  ide_pipeline_addin_track (addin, id);

  config_command = ide_run_command_new ();
  ide_run_command_append_argv (config_command, configure_path);
  ide_run_command_append_formatted (config_command, "--prefix=%s", prefix);
  ide_run_command_append_parsed (config_command, config_opts, NULL);
  config_stage = ide_pipeline_stage_command_new (config_command, NULL);
  ide_pipeline_stage_set_name (config_stage, _("Configuring project"));
  id = ide_pipeline_attach (pipeline, IDE_PIPELINE_PHASE_CONFIGURE, 0, config_stage);
  ide_pipeline_addin_track (addin, id);

  build_command = ide_run_command_new ();
  ide_run_command_append_argv (build_command, "make");
  if ((j = ide_config_get_parallelism (config)) > 0)
    ide_run_command_append_formatted (build_command, "-j=%d", j);
  clean_command = ide_run_command_new ();
  ide_run_command_append_args (clean_command, IDE_STRV_INIT ("make", "clean"));
  build_stage = ide_pipeline_stage_command_new (build_command, clean_command);
  ide_pipeline_stage_set_name (build_stage, _("Building project"));
  g_signal_connect (build_stage, "query", G_CALLBACK (query_cb), NULL);
  id = ide_pipeline_attach (pipeline, IDE_PIPELINE_PHASE_BUILD, 0, build_stage);
  ide_pipeline_addin_track (addin, id);

  install_command = ide_run_command_new ();
  ide_run_command_append_args (install_command, IDE_STRV_INIT ("make", "install"));
  install_stage = ide_pipeline_stage_command_new (install_command, NULL);
  ide_pipeline_stage_set_name (install_stage, _("Installing project"));
  ide_pipeline_stage_set_completed (install_stage, g_file_test (configure_path, G_FILE_TEST_EXISTS));
  id = ide_pipeline_attach (pipeline, IDE_PIPELINE_PHASE_INSTALL, 0, install_stage);
  ide_pipeline_addin_track (addin, id);

  IDE_EXIT;
}

static void
pipeline_addin_iface_init (IdePipelineAddinInterface *iface)
{
  iface->load = gbp_phpize_pipeline_addin_load;
}

G_DEFINE_FINAL_TYPE_WITH_CODE (GbpPhpizePipelineAddin, gbp_phpize_pipeline_addin, IDE_TYPE_OBJECT,
                               G_IMPLEMENT_INTERFACE (IDE_TYPE_PIPELINE_ADDIN, pipeline_addin_iface_init))

static void
gbp_phpize_pipeline_addin_class_init (GbpPhpizePipelineAddinClass *klass)
{
}

static void
gbp_phpize_pipeline_addin_init (GbpPhpizePipelineAddin *self)
{
}
