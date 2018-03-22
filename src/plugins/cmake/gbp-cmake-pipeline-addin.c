/* gbp-cmake-pipeline-addin.c
 *
 * Copyright 2017 Christian Hergert <chergert@redhat.com>
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
 */

#define G_LOG_DOMAIN "gbp-cmake-pipeline-addin"

#include <glib/gi18n.h>

#include "gbp-cmake-build-system.h"
#include "gbp-cmake-pipeline-addin.h"

struct _GbpCMakePipelineAddin
{
  IdeObject parent_instance;
};

static const gchar *ninja_names[] = { "ninja-build", "ninja" };

static void build_pipeline_addin_iface_init (IdeBuildPipelineAddinInterface *iface);

G_DEFINE_TYPE_WITH_CODE (GbpCMakePipelineAddin, gbp_cmake_pipeline_addin, IDE_TYPE_OBJECT,
                         G_IMPLEMENT_INTERFACE (IDE_TYPE_BUILD_PIPELINE_ADDIN, build_pipeline_addin_iface_init))

static void
gbp_cmake_pipeline_addin_class_init (GbpCMakePipelineAddinClass *klass)
{
}

static void
gbp_cmake_pipeline_addin_init (GbpCMakePipelineAddin *self)
{
}

static void
gbp_cmake_pipeline_addin_stage_query_cb (IdeBuildStage    *stage,
                                         IdeBuildPipeline *pipeline,
                                         GCancellable     *cancellable)
{
  g_assert (IDE_IS_BUILD_STAGE (stage));
  g_assert (IDE_IS_BUILD_PIPELINE (pipeline));
  g_assert (!cancellable || G_IS_CANCELLABLE (cancellable));

  /* Defer to ninja to determine completed status */
  ide_build_stage_set_completed (stage, FALSE);
}

static void
gbp_cmake_pipeline_addin_load (IdeBuildPipelineAddin *addin,
                               IdeBuildPipeline      *pipeline)
{
  GbpCMakePipelineAddin *self = (GbpCMakePipelineAddin *)addin;
  g_autoptr(IdeSubprocessLauncher) configure_launcher = NULL;
  g_autoptr(IdeSubprocessLauncher) build_launcher = NULL;
  g_autoptr(IdeSubprocessLauncher) install_launcher = NULL;
  g_autoptr(IdeSubprocessLauncher) clean_launcher = NULL;
  g_autoptr(IdeBuildStage) configure_stage = NULL;
  g_autoptr(IdeBuildStage) build_stage = NULL;
  g_autoptr(IdeBuildStage) install_stage = NULL;
  g_autoptr(GError) error = NULL;
  g_autofree gchar *prefix_option = NULL;
  g_autofree gchar *build_ninja = NULL;
  IdeBuildSystem *build_system;
  IdeConfiguration *configuration;
  IdeContext *context;
  IdeRuntime *runtime;
  const gchar *ninja = NULL;
  const gchar *config_opts;
  const gchar *prefix;
  const gchar *srcdir;
  const gchar *cmake;
  guint id;
  gint parallelism;

  IDE_ENTRY;

  g_assert (GBP_IS_CMAKE_PIPELINE_ADDIN (self));
  g_assert (IDE_IS_BUILD_PIPELINE (pipeline));

  context = ide_object_get_context (IDE_OBJECT (self));

  build_system = ide_context_get_build_system (context);
  if (!GBP_IS_CMAKE_BUILD_SYSTEM (build_system))
    IDE_GOTO (failure);

  configuration = ide_build_pipeline_get_configuration (pipeline);
  runtime = ide_build_pipeline_get_runtime (pipeline);
  srcdir = ide_build_pipeline_get_srcdir (pipeline);

  g_assert (IDE_IS_CONFIGURATION (configuration));
  g_assert (IDE_IS_RUNTIME (runtime));
  g_assert (srcdir != NULL);

  if (!(cmake = ide_configuration_getenv (configuration, "CMAKE")))
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

  if (NULL == (configure_launcher = ide_build_pipeline_create_launcher (pipeline, &error)) ||
      NULL == (build_launcher = ide_build_pipeline_create_launcher (pipeline, &error)) ||
      NULL == (clean_launcher = ide_build_pipeline_create_launcher (pipeline, &error)) ||
      NULL == (install_launcher = ide_build_pipeline_create_launcher (pipeline, &error)))
    IDE_GOTO (failure);

  prefix = ide_configuration_get_prefix (configuration);
  config_opts = ide_configuration_get_config_opts (configuration);
  parallelism = ide_configuration_get_parallelism (configuration);

  /* Setup our configure stage. */

  prefix_option = g_strdup_printf ("-DCMAKE_INSTALL_PREFIX=%s", prefix);

  ide_subprocess_launcher_push_argv (configure_launcher, cmake);
  ide_subprocess_launcher_push_argv (configure_launcher, "-G");
  ide_subprocess_launcher_push_argv (configure_launcher, "Ninja");
  ide_subprocess_launcher_push_argv (configure_launcher, ".");
  ide_subprocess_launcher_push_argv (configure_launcher, srcdir);
  ide_subprocess_launcher_push_argv (configure_launcher, "-DCMAKE_EXPORT_COMPILE_COMMANDS=1");
  ide_subprocess_launcher_push_argv (configure_launcher, "-DCMAKE_BUILD_TYPE=RelWithDebInfo");
  ide_subprocess_launcher_push_argv (configure_launcher, prefix_option);

  if (!dzl_str_empty0 (config_opts))
    {
      g_auto(GStrv) argv = NULL;
      gint argc;

      if (!g_shell_parse_argv (config_opts, &argc, &argv, &error))
        IDE_GOTO (failure);

      ide_subprocess_launcher_push_args (configure_launcher, (const gchar * const *)argv);
    }

  configure_stage = ide_build_stage_launcher_new (context, configure_launcher);
  ide_build_stage_set_name (configure_stage, _("Configure project"));

  build_ninja = ide_build_pipeline_build_builddir_path (pipeline, "build.ninja", NULL);
  if (g_file_test (build_ninja, G_FILE_TEST_IS_REGULAR))
    ide_build_stage_set_completed (configure_stage, TRUE);

  id = ide_build_pipeline_connect (pipeline, IDE_BUILD_PHASE_CONFIGURE, 0, configure_stage);
  ide_build_pipeline_addin_track (addin, id);

  /* Setup our build stage */

  ide_subprocess_launcher_push_argv (build_launcher, ninja);
  ide_subprocess_launcher_push_argv (clean_launcher, ninja);

  if (parallelism > 0)
    {
      g_autofree gchar *j = g_strdup_printf ("-j%u", parallelism);

      ide_subprocess_launcher_push_argv (build_launcher, j);
      ide_subprocess_launcher_push_argv (clean_launcher, j);
    }

  ide_subprocess_launcher_push_argv (clean_launcher, "clean");

  build_stage = ide_build_stage_launcher_new (context, build_launcher);
  ide_build_stage_set_name (build_stage, _("Building project"));

  ide_build_stage_launcher_set_clean_launcher (IDE_BUILD_STAGE_LAUNCHER (build_stage), clean_launcher);
  ide_build_stage_set_check_stdout (build_stage, TRUE);

  g_signal_connect (build_stage,
                    "query",
                    G_CALLBACK (gbp_cmake_pipeline_addin_stage_query_cb),
                    NULL);

  id = ide_build_pipeline_connect (pipeline, IDE_BUILD_PHASE_BUILD, 0, build_stage);
  ide_build_pipeline_addin_track (addin, id);

  /* Setup our install stage */

  ide_subprocess_launcher_push_argv (install_launcher, ninja);
  ide_subprocess_launcher_push_argv (install_launcher, "install");

  install_stage = ide_build_stage_launcher_new (context, install_launcher);
  ide_build_stage_set_name (install_stage, _("Installing project"));

  g_signal_connect (install_stage,
                    "query",
                    G_CALLBACK (gbp_cmake_pipeline_addin_stage_query_cb),
                    NULL);

  id = ide_build_pipeline_connect (pipeline, IDE_BUILD_PHASE_INSTALL, 0, install_stage);
  ide_build_pipeline_addin_track (addin, id);

  IDE_EXIT;

failure:
  if (error != NULL)
    g_warning ("Failed to setup cmake build pipeline: %s", error->message);
}

static void
build_pipeline_addin_iface_init (IdeBuildPipelineAddinInterface *iface)
{
  iface->load = gbp_cmake_pipeline_addin_load;
}

