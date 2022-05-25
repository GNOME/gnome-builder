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
  g_autoptr(IdeSubprocessLauncher) bootstrap_launcher = NULL;
  g_autoptr(IdeSubprocessLauncher) config_launcher = NULL;
  g_autoptr(IdeSubprocessLauncher) build_launcher = NULL;
  g_autoptr(IdeSubprocessLauncher) clean_launcher = NULL;
  g_autoptr(IdeSubprocessLauncher) install_launcher = NULL;
  g_autoptr(IdePipelineStage) bootstrap_stage = NULL;
  g_autoptr(IdePipelineStage) config_stage = NULL;
  g_autoptr(IdePipelineStage) build_stage = NULL;
  g_autoptr(IdePipelineStage) install_stage = NULL;
  g_autoptr(GError) error = NULL;
  g_autofree char *configure_path = NULL;
  g_auto(GStrv) config_argv = NULL;
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

  if (!ide_str_empty0 (config_opts))
    {
      int argc;

      if (!g_shell_parse_argv (config_opts, &argc, &config_argv, &error))
        {
          ide_object_message (addin,
                              "%s: %s",
                              _("Cannot parse arguments to configure"),
                              error->message);
          IDE_EXIT;
        }
    }

  g_assert (IDE_IS_CONFIG (config));
  g_assert (srcdir != NULL);
  g_assert (builddir != NULL);
  g_assert (prefix != NULL);

  bootstrap_launcher = ide_pipeline_create_launcher (pipeline, NULL);
  ide_subprocess_launcher_push_argv (bootstrap_launcher, "phpize");
  ide_subprocess_launcher_set_cwd (bootstrap_launcher, srcdir);
  bootstrap_stage = ide_pipeline_stage_launcher_new (context, bootstrap_launcher);
  ide_pipeline_stage_set_name (bootstrap_stage, _("Bootstrapping project"));
  ide_pipeline_stage_set_completed (bootstrap_stage, g_file_test (configure_path, G_FILE_TEST_EXISTS));
  id = ide_pipeline_attach (pipeline, IDE_PIPELINE_PHASE_AUTOGEN, 0, bootstrap_stage);
  ide_pipeline_addin_track (addin, id);

  config_launcher = ide_pipeline_create_launcher (pipeline, NULL);
  ide_subprocess_launcher_push_argv (config_launcher, configure_path);
  ide_subprocess_launcher_push_argv_format (config_launcher, "--prefix=%s", prefix);
  if (config_argv)
    ide_subprocess_launcher_push_args (config_launcher, (const char * const *)config_argv);
  config_stage = ide_pipeline_stage_launcher_new (context, config_launcher);
  ide_pipeline_stage_set_name (config_stage, _("Configuring project"));
  id = ide_pipeline_attach (pipeline, IDE_PIPELINE_PHASE_CONFIGURE, 0, config_stage);
  ide_pipeline_addin_track (addin, id);

  build_launcher = ide_pipeline_create_launcher (pipeline, NULL);
  ide_subprocess_launcher_push_argv (build_launcher, "make");
  if ((j = ide_config_get_parallelism (config)) > 0)
    ide_subprocess_launcher_push_argv_format (build_launcher, "-j=%d", j);
  clean_launcher = ide_pipeline_create_launcher (pipeline, NULL);
  ide_subprocess_launcher_push_args (clean_launcher, IDE_STRV_INIT ("make", "clean"));
  build_stage = ide_pipeline_stage_launcher_new (context, build_launcher);
  ide_pipeline_stage_launcher_set_clean_launcher (IDE_PIPELINE_STAGE_LAUNCHER (build_stage), clean_launcher);
  ide_pipeline_stage_set_name (build_stage, _("Building project"));
  g_signal_connect (build_stage, "query", G_CALLBACK (query_cb), NULL);
  id = ide_pipeline_attach (pipeline, IDE_PIPELINE_PHASE_BUILD, 0, build_stage);
  ide_pipeline_addin_track (addin, id);

  install_launcher = ide_pipeline_create_launcher (pipeline, NULL);
  ide_subprocess_launcher_push_args (install_launcher, IDE_STRV_INIT ("make", "install"));
  install_stage = ide_pipeline_stage_launcher_new (context, install_launcher);
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
