/* gbp-meson-pipeline-addin.c
 *
 * Copyright 2017-2019 Christian Hergert <chergert@redhat.com>
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

#define G_LOG_DOMAIN "gbp-meson-pipeline-addin"

#include <glib/gi18n.h>

#include "gbp-meson-toolchain.h"
#include "gbp-meson-build-stage-cross-file.h"
#include "gbp-meson-build-system.h"
#include "gbp-meson-build-target.h"
#include "gbp-meson-pipeline-addin.h"

struct _GbpMesonPipelineAddin
{
  IdeObject parent_instance;
};

static const gchar *ninja_names[] = { "ninja", "ninja-build" };

static void
on_build_stage_query (IdePipelineStage *stage,
                      IdePipeline      *pipeline,
                      GPtrArray        *targets,
                      GCancellable     *cancellable)
{
  IdeSubprocessLauncher *launcher;
  g_autoptr(GPtrArray) replace = NULL;
  const gchar * const *argv;

  g_assert (IDE_IS_PIPELINE_STAGE (stage));
  g_assert (IDE_IS_PIPELINE (pipeline));
  g_assert (!cancellable || G_IS_CANCELLABLE (cancellable));

  /* Defer to ninja to determine completed status */
  ide_pipeline_stage_set_completed (stage, FALSE);

  /* Clear any previous argv items from a possible previous build */
  launcher = ide_pipeline_stage_launcher_get_launcher (IDE_PIPELINE_STAGE_LAUNCHER (stage));
  argv = ide_subprocess_launcher_get_argv (launcher);
  replace = g_ptr_array_new_with_free_func (g_free);
  for (guint i = 0; argv[i]; i++)
    {
      g_ptr_array_add (replace, g_strdup (argv[i]));
      if (g_strv_contains (ninja_names, argv[i]))
        break;
    }
  g_ptr_array_add (replace, NULL);
  ide_subprocess_launcher_set_argv (launcher, (gchar **)replace->pdata);

  /* If we have targets to build, specify them */
  if (targets != NULL)
    {
      for (guint i = 0; i < targets->len; i++)
        {
          IdeBuildTarget *target = g_ptr_array_index (targets, i);

          if (GBP_IS_MESON_BUILD_TARGET (target))
            {
              const gchar *filename;

              filename = gbp_meson_build_target_get_filename (GBP_MESON_BUILD_TARGET (target));

              if (filename != NULL)
                ide_subprocess_launcher_push_argv (launcher, filename);
            }
        }
    }
}

static void
on_install_stage_query (IdePipelineStage *stage,
                        IdePipeline      *pipeline,
                        GPtrArray        *targets,
                        GCancellable     *cancellable)
{
  g_assert (IDE_IS_PIPELINE_STAGE (stage));
  g_assert (IDE_IS_PIPELINE (pipeline));
  g_assert (!cancellable || G_IS_CANCELLABLE (cancellable));

  /* Defer to ninja to determine completed status */
  ide_pipeline_stage_set_completed (stage, FALSE);
}

static void
gbp_meson_pipeline_addin_load (IdePipelineAddin *addin,
                               IdePipeline      *pipeline)
{
  GbpMesonPipelineAddin *self = (GbpMesonPipelineAddin *)addin;
  g_autoptr(IdeSubprocessLauncher) config_launcher = NULL;
  g_autoptr(IdeSubprocessLauncher) build_launcher = NULL;
  g_autoptr(IdeSubprocessLauncher) clean_launcher = NULL;
  g_autoptr(IdeSubprocessLauncher) install_launcher = NULL;
  g_autoptr(IdePipelineStage) build_stage = NULL;
  g_autoptr(IdePipelineStage) config_stage = NULL;
  g_autoptr(IdePipelineStage) install_stage = NULL;
  g_autoptr(GError) error = NULL;
  g_autofree gchar *build_ninja = NULL;
  g_autofree gchar *crossbuild_file = NULL;
  IdeBuildSystem *build_system;
  IdeConfig *config;
  IdeContext *context;
  IdeRuntime *runtime;
  IdeToolchain *toolchain;
  const gchar *config_opts;
  const gchar *ninja = NULL;
  const gchar *prefix;
  const gchar *srcdir;
  const gchar *meson;
  guint id;
  gint parallel;

  IDE_ENTRY;

  g_assert (GBP_IS_MESON_PIPELINE_ADDIN (self));
  g_assert (IDE_IS_PIPELINE (pipeline));

  context = ide_object_get_context (IDE_OBJECT (self));

  build_system = ide_build_system_from_context (context);
  if (!GBP_IS_MESON_BUILD_SYSTEM (build_system))
    IDE_GOTO (failure);

  config = ide_pipeline_get_config (pipeline);
  runtime = ide_pipeline_get_runtime (pipeline);
  toolchain = ide_pipeline_get_toolchain (pipeline);
  srcdir = ide_pipeline_get_srcdir (pipeline);

  g_assert (IDE_IS_CONFIG (config));
  g_assert (IDE_IS_RUNTIME (runtime));
  g_assert (srcdir != NULL);

  for (guint i = 0; i < G_N_ELEMENTS (ninja_names); i++)
    {
      if (ide_runtime_contains_program_in_path (runtime, ninja_names[i], NULL))
        {
          ninja = ninja_names[i];
          break;
        }
    }

  if (ninja == NULL)
    ninja = ide_config_getenv (config, "NINJA");

  if (ninja == NULL)
    {
      ide_context_warning (context,
                           _("A Meson-based project is loaded but Ninja could not be found."));
      IDE_EXIT;
    }

  /* Create all our launchers up front */
  if (NULL == (config_launcher = ide_pipeline_create_launcher (pipeline, &error)) ||
      NULL == (build_launcher = ide_pipeline_create_launcher (pipeline, &error)) ||
      NULL == (clean_launcher = ide_pipeline_create_launcher (pipeline, &error)) ||
      NULL == (install_launcher = ide_pipeline_create_launcher (pipeline, &error)))
    IDE_GOTO (failure);

  prefix = ide_config_get_prefix (config);
  config_opts = ide_config_get_config_opts (config);
  parallel = ide_config_get_parallelism (config);

  if (NULL == (meson = ide_config_getenv (config, "MESON")))
    meson = "meson";

  if (!ide_runtime_contains_program_in_path (runtime, meson, NULL))
    ide_context_warning (context,
                         _("A Meson-based project is loaded but meson could not be found."));

  /* Create the toolchain file if required */
  if (GBP_IS_MESON_TOOLCHAIN (toolchain))
    crossbuild_file = g_strdup (gbp_meson_toolchain_get_file_path (GBP_MESON_TOOLCHAIN (toolchain)));
  else if (g_strcmp0 (ide_toolchain_get_id (toolchain), "default") != 0)
    {
      GbpMesonBuildStageCrossFile *cross_file_stage;
      cross_file_stage = gbp_meson_build_stage_cross_file_new (toolchain);
      crossbuild_file = gbp_meson_build_stage_cross_file_get_path (cross_file_stage, pipeline);

      id = ide_pipeline_attach (pipeline, IDE_PIPELINE_PHASE_PREPARE, 0, IDE_PIPELINE_STAGE (cross_file_stage));
      ide_pipeline_addin_track (addin, id);
    }

  /* Setup our meson configure stage. */

  ide_subprocess_launcher_push_argv (config_launcher, meson);
  ide_subprocess_launcher_push_argv (config_launcher, srcdir);
  ide_subprocess_launcher_push_argv (config_launcher, ".");
  ide_subprocess_launcher_push_argv (config_launcher, "--prefix");
  ide_subprocess_launcher_push_argv (config_launcher, prefix);
  if (crossbuild_file != NULL)
    {
      ide_subprocess_launcher_push_argv (config_launcher, "--cross-file");
      ide_subprocess_launcher_push_argv (config_launcher, crossbuild_file);
    }

  if (!ide_str_empty0 (config_opts))
    {
      g_auto(GStrv) argv = NULL;
      gint argc;

      if (!g_shell_parse_argv (config_opts, &argc, &argv, &error))
        IDE_GOTO (failure);

      ide_subprocess_launcher_push_args (config_launcher, (const gchar * const *)argv);
    }

  config_stage = ide_pipeline_stage_launcher_new (context, config_launcher);
  ide_pipeline_stage_set_name (config_stage, _("Configuring project"));
  build_ninja = ide_pipeline_build_builddir_path (pipeline, "build.ninja", NULL);
  if (g_file_test (build_ninja, G_FILE_TEST_IS_REGULAR))
    ide_pipeline_stage_set_completed (config_stage, TRUE);

  id = ide_pipeline_attach (pipeline, IDE_PIPELINE_PHASE_CONFIGURE, 0, config_stage);
  ide_pipeline_addin_track (addin, id);

  /*
   * Register the build launcher which will perform the incremental
   * build of the project when the IDE_PIPELINE_PHASE_BUILD phase is
   * requested of the pipeline.
   */
  ide_subprocess_launcher_push_argv (build_launcher, ninja);
  ide_subprocess_launcher_push_argv (clean_launcher, ninja);

  if (parallel > 0)
    {
      g_autofree gchar *j = g_strdup_printf ("-j%u", parallel);

      ide_subprocess_launcher_push_argv (build_launcher, j);
      ide_subprocess_launcher_push_argv (clean_launcher, j);
    }

  ide_subprocess_launcher_push_argv (clean_launcher, "clean");

  build_stage = ide_pipeline_stage_launcher_new (context, build_launcher);
  ide_pipeline_stage_launcher_set_clean_launcher (IDE_PIPELINE_STAGE_LAUNCHER (build_stage), clean_launcher);
  ide_pipeline_stage_set_check_stdout (build_stage, TRUE);
  ide_pipeline_stage_set_name (build_stage, _("Building project"));
  g_signal_connect (build_stage, "query", G_CALLBACK (on_build_stage_query), NULL);

  id = ide_pipeline_attach (pipeline, IDE_PIPELINE_PHASE_BUILD, 0, build_stage);
  ide_pipeline_addin_track (addin, id);

  /* Setup our install stage */
  ide_subprocess_launcher_push_argv (install_launcher, ninja);
  ide_subprocess_launcher_push_argv (install_launcher, "install");
  install_stage = ide_pipeline_stage_launcher_new (context, install_launcher);
  ide_pipeline_stage_set_name (install_stage, _("Installing project"));
  g_signal_connect (install_stage, "query", G_CALLBACK (on_install_stage_query), NULL);
  id = ide_pipeline_attach (pipeline, IDE_PIPELINE_PHASE_INSTALL, 0, install_stage);
  ide_pipeline_addin_track (addin, id);

  IDE_EXIT;

failure:
  if (error != NULL)
    g_warning ("Failed to setup meson build pipeline: %s", error->message);
}

static void
pipeline_addin_iface_init (IdePipelineAddinInterface *iface)
{
  iface->load = gbp_meson_pipeline_addin_load;
}

G_DEFINE_TYPE_WITH_CODE (GbpMesonPipelineAddin, gbp_meson_pipeline_addin, IDE_TYPE_OBJECT,
                         G_IMPLEMENT_INTERFACE (IDE_TYPE_PIPELINE_ADDIN,
                                                pipeline_addin_iface_init))

static void
gbp_meson_pipeline_addin_class_init (GbpMesonPipelineAddinClass *klass)
{
}

static void
gbp_meson_pipeline_addin_init (GbpMesonPipelineAddin *self)
{
}
