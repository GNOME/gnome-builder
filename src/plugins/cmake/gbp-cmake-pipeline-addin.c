/* gbp-cmake-pipeline-addin.c
 *
 * Copyright 2017-2019 Christian Hergert <chergert@redhat.com>
 * Copyright 2017 Martin Blanchard <tchaik@gmx.com>
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

#define G_LOG_DOMAIN "gbp-cmake-pipeline-addin"

#include <glib/gi18n.h>

#include "gbp-cmake-build-system.h"
#include "gbp-cmake-build-stage-cross-file.h"
#include "gbp-cmake-build-stage-codemodel.h"
#include "gbp-cmake-toolchain.h"
#include "gbp-cmake-pipeline-addin.h"

struct _GbpCMakePipelineAddin
{
  IdeObject parent_instance;
};

static const gchar *ninja_names[] = { "ninja-build", "ninja" };

static void pipeline_addin_iface_init (IdePipelineAddinInterface *iface);

G_DEFINE_FINAL_TYPE_WITH_CODE (GbpCMakePipelineAddin, gbp_cmake_pipeline_addin, IDE_TYPE_OBJECT,
                         G_IMPLEMENT_INTERFACE (IDE_TYPE_PIPELINE_ADDIN, pipeline_addin_iface_init))

static void
gbp_cmake_pipeline_addin_class_init (GbpCMakePipelineAddinClass *klass)
{
}

static void
gbp_cmake_pipeline_addin_init (GbpCMakePipelineAddin *self)
{
}

static void
gbp_cmake_pipeline_addin_stage_query_cb (IdePipelineStage    *stage,
                                         IdePipeline *pipeline,
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
gbp_cmake_pipeline_addin_load (IdePipelineAddin *addin,
                               IdePipeline      *pipeline)
{
  GbpCMakePipelineAddin *self = (GbpCMakePipelineAddin *)addin;
  g_autoptr(GbpCmakeBuildStageCodemodel) codemodel_stage = NULL;
  g_autoptr(IdeRunCommand) configure_command = NULL;
  g_autoptr(IdeRunCommand) build_command = NULL;
  g_autoptr(IdeRunCommand) install_command = NULL;
  g_autoptr(IdeRunCommand) clean_command = NULL;
  g_autoptr(IdePipelineStage) configure_stage = NULL;
  g_autoptr(IdePipelineStage) build_stage = NULL;
  g_autoptr(IdePipelineStage) install_stage = NULL;
  g_autoptr(GError) error = NULL;
  g_autofree gchar *prefix_option = NULL;
  g_autofree gchar *build_ninja = NULL;
  g_autofree gchar *crossbuild_file = NULL;
  g_autoptr(GFile) project_file = NULL;
  g_autofree gchar *project_file_name = NULL;
  g_autofree gchar *srcdir = NULL;
  IdeBuildSystem *build_system;
  IdeConfig *configuration;
  IdeContext *context;
  IdeRuntime *runtime;
  IdeToolchain *toolchain;
  const gchar *ninja = NULL;
  const gchar *config_opts;
  const gchar *prefix;
  const gchar *cmake;
  guint id;
  gint parallelism;

  IDE_ENTRY;

  g_assert (GBP_IS_CMAKE_PIPELINE_ADDIN (self));
  g_assert (IDE_IS_PIPELINE (pipeline));

  context = ide_object_get_context (IDE_OBJECT (self));

  build_system = ide_build_system_from_context (context);
  if (!GBP_IS_CMAKE_BUILD_SYSTEM (build_system))
    IDE_GOTO (failure);

  g_object_get (build_system, "project-file", &project_file, NULL);
  project_file_name = g_file_get_basename (project_file);

  configuration = ide_pipeline_get_config (pipeline);
  runtime = ide_pipeline_get_runtime (pipeline);
  toolchain = ide_pipeline_get_toolchain (pipeline);

  if (g_strcmp0 (project_file_name, "CMakeLists.txt") == 0)
    srcdir = g_path_get_dirname (g_file_peek_path (project_file));
  else
    srcdir = g_strdup (ide_pipeline_get_srcdir (pipeline));

  g_assert (IDE_IS_CONFIG (configuration));
  g_assert (IDE_IS_RUNTIME (runtime));
  g_assert (srcdir != NULL);

  if (!(cmake = ide_config_getenv (configuration, "CMAKE")))
    cmake = "cmake";

  for (guint i = 0; i < G_N_ELEMENTS (ninja_names); i++)
    {
      if (ide_runtime_contains_program_in_path (runtime, ninja_names[i], NULL))
        {
          ninja = ninja_names[i];
          break;
        }
    }

  if (ninja == NULL)
    {
      g_debug ("Failed to locate ninja. CMake building is disabled.");
      IDE_EXIT;
    }

  configure_command = ide_run_command_new ();
  build_command = ide_run_command_new ();
  clean_command = ide_run_command_new ();
  install_command = ide_run_command_new ();

  prefix = ide_config_get_prefix (configuration);
  config_opts = ide_config_get_config_opts (configuration);
  parallelism = ide_config_get_parallelism (configuration);

  /* Create the toolchain file if required */
  if (GBP_IS_CMAKE_TOOLCHAIN (toolchain))
    crossbuild_file = g_strdup (gbp_cmake_toolchain_get_file_path (GBP_CMAKE_TOOLCHAIN (toolchain)));
  else if (g_strcmp0 (ide_toolchain_get_id (toolchain), "default") != 0)
    {
      GbpCMakeBuildStageCrossFile *cross_file_stage;
      cross_file_stage = gbp_cmake_build_stage_cross_file_new (toolchain);
      crossbuild_file = gbp_cmake_build_stage_cross_file_get_path (cross_file_stage, pipeline);

      id = ide_pipeline_attach (pipeline, IDE_PIPELINE_PHASE_PREPARE, 0, IDE_PIPELINE_STAGE (cross_file_stage));
      ide_pipeline_addin_track (addin, id);
    }

  /* Setup ide integration stage for cmake */
  codemodel_stage = gbp_cmake_build_stage_codemodel_new ();
  ide_pipeline_stage_set_name (IDE_PIPELINE_STAGE (codemodel_stage), "Prepare Codemodel");
  id = ide_pipeline_attach (pipeline, IDE_PIPELINE_PHASE_PREPARE, 1, IDE_PIPELINE_STAGE (codemodel_stage));
  ide_pipeline_addin_track (addin, id);

  /* Setup our configure stage. */

  prefix_option = g_strdup_printf ("-DCMAKE_INSTALL_PREFIX=%s", prefix);

  ide_run_command_set_argv (configure_command, IDE_STRV_INIT (cmake, "-G", "Ninja", ".", srcdir));
  ide_run_command_append_argv (configure_command, "-DCMAKE_EXPORT_COMPILE_COMMANDS=1");
  if (config_opts == NULL || strstr (config_opts, "-DCMAKE_BUILD_TYPE=") == NULL)
    ide_run_command_append_argv (configure_command, "-DCMAKE_BUILD_TYPE=RelWithDebInfo");
  ide_run_command_append_argv (configure_command, prefix_option);

  if (crossbuild_file != NULL)
    {
      g_autofree gchar *toolchain_option = g_strdup_printf ("-DCMAKE_TOOLCHAIN_FILE=\"%s\"", crossbuild_file);
      ide_run_command_append_argv (configure_command, toolchain_option);
    }

  if (!ide_str_empty0 (config_opts))
    {
      g_auto(GStrv) argv = NULL;
      gint argc;

      if (!g_shell_parse_argv (config_opts, &argc, &argv, &error))
        IDE_GOTO (failure);

      ide_run_command_append_args (configure_command, (const gchar * const *)argv);
    }

  configure_stage = g_object_new (IDE_TYPE_PIPELINE_STAGE_COMMAND,
                                  "build-command", configure_command,
                                  "name", _("Configure project"),
                                  NULL);

  build_ninja = ide_pipeline_build_builddir_path (pipeline, "build.ninja", NULL);
  if (g_file_test (build_ninja, G_FILE_TEST_IS_REGULAR))
    ide_pipeline_stage_set_completed (configure_stage, TRUE);

  id = ide_pipeline_attach (pipeline, IDE_PIPELINE_PHASE_CONFIGURE, 0, configure_stage);
  ide_pipeline_addin_track (addin, id);

  /* Setup our build stage */

  ide_run_command_append_argv (build_command, ninja);
  ide_run_command_append_argv (clean_command, ninja);

  if (parallelism > 0)
    {
      g_autofree gchar *j = g_strdup_printf ("-j%u", parallelism);

      ide_run_command_append_argv (build_command, j);
      ide_run_command_append_argv (clean_command, j);
    }

  ide_run_command_append_argv (clean_command, "clean");

  build_stage = g_object_new (IDE_TYPE_PIPELINE_STAGE_COMMAND,
                              "build-command", build_command,
                              "clean-command", clean_command,
                              "name", _("Building project"),
                              "check-stdout", TRUE,
                              NULL);
  g_signal_connect (build_stage,
                    "query",
                    G_CALLBACK (gbp_cmake_pipeline_addin_stage_query_cb),
                    NULL);
  id = ide_pipeline_attach (pipeline, IDE_PIPELINE_PHASE_BUILD, 0, build_stage);
  ide_pipeline_addin_track (addin, id);

  /* Setup our install stage */

  ide_run_command_append_args (install_command, IDE_STRV_INIT (ninja, "install"));
  install_stage = g_object_new (IDE_TYPE_PIPELINE_STAGE_COMMAND,
                                "build-command", install_command,
                                "name", _("Installing project"),
                                NULL);
  g_signal_connect (install_stage,
                    "query",
                    G_CALLBACK (gbp_cmake_pipeline_addin_stage_query_cb),
                    NULL);
  id = ide_pipeline_attach (pipeline, IDE_PIPELINE_PHASE_INSTALL, 0, install_stage);
  ide_pipeline_addin_track (addin, id);

  IDE_EXIT;

failure:
  if (error != NULL)
    g_warning ("Failed to setup cmake build pipeline: %s", error->message);
}

static void
pipeline_addin_iface_init (IdePipelineAddinInterface *iface)
{
  iface->load = gbp_cmake_pipeline_addin_load;
}
