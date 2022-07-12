/* gbp-waf-pipeline-addin.c
 *
 * Copyright 2019 Alex Mitchell
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

#define G_LOG_DOMAIN "gbp-waf-pipeline-addin"

#include "config.h"

#include <glib/gi18n.h>

#include <libide-foundry.h>

#include "gbp-waf-build-system.h"
#include "gbp-waf-pipeline-addin.h"

struct _GbpWafPipelineAddin
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

  /* Always defer to waf to check if build is needed */
  ide_pipeline_stage_set_completed (stage, FALSE);

  /* TODO: You can use @targets to limit what targets to build */
}

static void
gbp_waf_pipeline_addin_load (IdePipelineAddin *addin,
                             IdePipeline      *pipeline)
{
  g_autoptr(IdeSubprocessLauncher) config_launcher = NULL;
  g_autoptr(IdeSubprocessLauncher) build_launcher = NULL;
  g_autoptr(IdeSubprocessLauncher) clean_launcher = NULL;
  g_autoptr(IdeSubprocessLauncher) install_launcher = NULL;
  g_autoptr(IdePipelineStage) build_stage = NULL;
  g_autoptr(IdePipelineStage) install_stage = NULL;
  g_autoptr(IdePipelineStage) config_stage = NULL;
  g_autofree char *waf = NULL;
  IdeBuildSystem *build_system;
  g_auto(GStrv) waf_argv = NULL;
  const char *config_opts;
  const char *python;
  const char *prefix;
  const char *srcdir;
  IdeContext *context;
  IdeConfig *config;
  guint id;

  IDE_ENTRY;

  g_assert (GBP_IS_WAF_PIPELINE_ADDIN (addin));
  g_assert (IDE_IS_PIPELINE (pipeline));

  context = ide_object_get_context (IDE_OBJECT (addin));
  build_system = ide_build_system_from_context (context);

  if (!GBP_IS_WAF_BUILD_SYSTEM (build_system))
    IDE_EXIT;

  config = ide_pipeline_get_config (pipeline);
  config_opts = ide_config_get_config_opts (config);
  prefix = ide_config_get_prefix (config);
  waf = gbp_waf_build_system_locate_waf (GBP_WAF_BUILD_SYSTEM (build_system));
  srcdir = ide_pipeline_get_srcdir (pipeline);

  if (gbp_waf_build_system_wants_python2 (GBP_WAF_BUILD_SYSTEM (build_system), NULL))
    python = "python2";
  else
    python = "python3";

  g_assert (prefix != NULL);
  g_assert (python != NULL);
  g_assert (IDE_IS_CONFIG (config));
  g_assert (waf != NULL);

  if (ide_str_equal0 (waf, "waf"))
    waf_argv = g_strdupv ((char **)IDE_STRV_INIT ("waf"));
  else
    waf_argv = g_strdupv ((char **)IDE_STRV_INIT (python, waf));

  config_launcher = ide_pipeline_create_launcher (pipeline, NULL);
  ide_subprocess_launcher_set_cwd (config_launcher, srcdir);
  ide_subprocess_launcher_push_args (config_launcher, (const char * const *)waf_argv);
  ide_subprocess_launcher_push_argv (config_launcher, "configure");
  ide_subprocess_launcher_push_argv_format (config_launcher, "--prefix=%s", prefix);
  ide_subprocess_launcher_push_argv_parsed (config_launcher, config_opts);
  config_stage = ide_pipeline_stage_launcher_new (context, config_launcher);
  ide_pipeline_stage_set_name (config_stage, _("Configuring project"));
  id = ide_pipeline_attach (pipeline, IDE_PIPELINE_PHASE_CONFIGURE, 0, config_stage);
  ide_pipeline_addin_track (addin, id);

  build_launcher = ide_pipeline_create_launcher (pipeline, NULL);
  ide_subprocess_launcher_set_cwd (build_launcher, srcdir);
  ide_subprocess_launcher_push_args (build_launcher, (const char * const *)waf_argv);
  ide_subprocess_launcher_push_argv (build_launcher, "build");

  clean_launcher = ide_pipeline_create_launcher (pipeline, NULL);
  ide_subprocess_launcher_set_cwd (clean_launcher, srcdir);
  ide_subprocess_launcher_push_args (clean_launcher, (const char * const *)waf_argv);
  ide_subprocess_launcher_push_argv (clean_launcher, "clean");

  build_stage = ide_pipeline_stage_launcher_new (context, build_launcher);
  ide_pipeline_stage_set_name (build_stage, _("Building project"));
  ide_pipeline_stage_launcher_set_clean_launcher (IDE_PIPELINE_STAGE_LAUNCHER (build_stage), clean_launcher);
  g_signal_connect (build_stage, "query", G_CALLBACK (query_cb), NULL);
  id = ide_pipeline_attach (pipeline, IDE_PIPELINE_PHASE_BUILD, 0, build_stage);
  ide_pipeline_addin_track (addin, id);

  install_launcher = ide_pipeline_create_launcher (pipeline, NULL);
  ide_subprocess_launcher_set_cwd (install_launcher, srcdir);
  ide_subprocess_launcher_push_args (install_launcher, (const char * const *)waf_argv);
  ide_subprocess_launcher_push_argv (install_launcher, "install");

  install_stage = ide_pipeline_stage_launcher_new (context, install_launcher);
  ide_pipeline_stage_set_name (install_stage, _("Installing project"));
  id = ide_pipeline_attach (pipeline, IDE_PIPELINE_PHASE_INSTALL, 0, install_stage);
  ide_pipeline_addin_track (addin, id);

  IDE_EXIT;
}

static void
pipeline_addin_iface_init (IdePipelineAddinInterface *iface)
{
  iface->load = gbp_waf_pipeline_addin_load;
}

G_DEFINE_FINAL_TYPE_WITH_CODE (GbpWafPipelineAddin, gbp_waf_pipeline_addin, IDE_TYPE_OBJECT,
                               G_IMPLEMENT_INTERFACE (IDE_TYPE_PIPELINE_ADDIN, pipeline_addin_iface_init))

static void
gbp_waf_pipeline_addin_class_init (GbpWafPipelineAddinClass *klass)
{
}

static void
gbp_waf_pipeline_addin_init (GbpWafPipelineAddin *self)
{
}
