/* gbp-meson-pipeline-addin.c
 *
 * Copyright 2017 Christian Hergert <chergert@redhat.com>
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

#define G_LOG_DOMAIN "gbp-meson-pipeline-addin"

#include <glib/gi18n.h>

#include "gbp-meson-toolchain.h"
#include "gbp-meson-build-system.h"
#include "gbp-meson-pipeline-addin.h"

struct _GbpMesonPipelineAddin
{
  IdeObject parent_instance;
};

static const gchar *ninja_names[] = { "ninja", "ninja-build" };

static void
on_stage_query (IdeBuildStage    *stage,
                IdeBuildPipeline *pipeline,
                GCancellable     *cancellable)
{
  g_assert (IDE_IS_BUILD_STAGE (stage));
  g_assert (IDE_IS_BUILD_PIPELINE (pipeline));
  g_assert (!cancellable || G_IS_CANCELLABLE (cancellable));

  /* Defer to ninja to determine completed status */
  ide_build_stage_set_completed (stage, FALSE);
}

static gchar *
_gbp_meson_get_quoted_field (const gchar *unquoted_field)
{
    g_return_val_if_fail (unquoted_field != NULL, NULL);

    return g_strconcat ("'", unquoted_field, "'", NULL);
}

static void
add_lang_executable (gchar *lang,
                     gchar *path,
                     GKeyFile *keyfile)
{
    g_autofree gchar *quoted_field = _gbp_meson_get_quoted_field (path);
    g_key_file_set_string (keyfile, "binaries", lang, quoted_field);
}

static void
gbp_meson_pipeline_addin_load (IdeBuildPipelineAddin *addin,
                               IdeBuildPipeline      *pipeline)
{
  GbpMesonPipelineAddin *self = (GbpMesonPipelineAddin *)addin;
  g_autoptr(IdeSubprocessLauncher) config_launcher = NULL;
  g_autoptr(IdeSubprocessLauncher) build_launcher = NULL;
  g_autoptr(IdeSubprocessLauncher) clean_launcher = NULL;
  g_autoptr(IdeSubprocessLauncher) install_launcher = NULL;
  g_autoptr(IdeBuildStage) build_stage = NULL;
  g_autoptr(IdeBuildStage) config_stage = NULL;
  g_autoptr(IdeBuildStage) install_stage = NULL;
  g_autoptr(GError) error = NULL;
  g_autofree gchar *build_ninja = NULL;
  g_autofree gchar *crossbuild_file = NULL;
  IdeBuildSystem *build_system;
  IdeConfiguration *config;
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
  g_assert (IDE_IS_BUILD_PIPELINE (pipeline));

  context = ide_object_get_context (IDE_OBJECT (self));

  build_system = ide_context_get_build_system (context);
  if (!GBP_IS_MESON_BUILD_SYSTEM (build_system))
    IDE_GOTO (failure);

  config = ide_build_pipeline_get_configuration (pipeline);
  runtime = ide_build_pipeline_get_runtime (pipeline);
  toolchain = ide_build_pipeline_get_toolchain (pipeline);
  srcdir = ide_build_pipeline_get_srcdir (pipeline);

  g_assert (IDE_IS_CONFIGURATION (config));
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
    {
      g_debug ("Failed to locate ninja. Meson building is disabled.");
      IDE_EXIT;
    }

  /* Create all our launchers up front */
  if (NULL == (config_launcher = ide_build_pipeline_create_launcher (pipeline, &error)) ||
      NULL == (build_launcher = ide_build_pipeline_create_launcher (pipeline, &error)) ||
      NULL == (clean_launcher = ide_build_pipeline_create_launcher (pipeline, &error)) ||
      NULL == (install_launcher = ide_build_pipeline_create_launcher (pipeline, &error)))
    IDE_GOTO (failure);

  prefix = ide_configuration_get_prefix (config);
  config_opts = ide_configuration_get_config_opts (config);
  parallel = ide_configuration_get_parallelism (config);

  if (NULL == (meson = ide_configuration_getenv (config, "MESON")))
    meson = "meson";

  /* Create the toolchain file is required */
  if (GBP_IS_MESON_TOOLCHAIN (toolchain))
    crossbuild_file = g_strdup (gbp_meson_toolchain_get_file_path (GBP_MESON_TOOLCHAIN (toolchain)));
  else if (g_strcmp0 (ide_toolchain_get_id (toolchain), "default") != 0)
    {
      g_autoptr(GKeyFile) crossbuild_keyfile = NULL;
      g_autoptr(IdeTriplet) triplet = NULL;
      g_autofree gchar *quoted_field = NULL;

      crossbuild_file = g_strconcat ("gnome-builder-", ide_toolchain_get_id (toolchain), ".crossfile", NULL);
      crossbuild_file = ide_build_pipeline_build_builddir_path (pipeline, crossbuild_file, NULL);

      crossbuild_keyfile = g_key_file_new ();
      triplet = ide_toolchain_get_host_triplet (toolchain);

      g_hash_table_foreach (ide_toolchain_get_compilers (toolchain), (GHFunc)add_lang_executable, crossbuild_keyfile);

      quoted_field = _gbp_meson_get_quoted_field (ide_toolchain_get_archiver (toolchain));
      if (quoted_field != NULL)
        g_key_file_set_string (crossbuild_keyfile, "binaries", "ar", quoted_field);

      quoted_field = _gbp_meson_get_quoted_field (ide_toolchain_get_strip (toolchain));
      if (quoted_field != NULL)
        g_key_file_set_string (crossbuild_keyfile, "binaries", "strip", quoted_field);

      quoted_field = _gbp_meson_get_quoted_field (ide_toolchain_get_pkg_config (toolchain));
      if (quoted_field != NULL)
        g_key_file_set_string (crossbuild_keyfile, "binaries", "pkgconfig", quoted_field);

      quoted_field = _gbp_meson_get_quoted_field (ide_toolchain_get_exe_wrapper (toolchain));
      if (quoted_field != NULL)
        g_key_file_set_string (crossbuild_keyfile, "binaries", "exe_wrapper", quoted_field);

      quoted_field = _gbp_meson_get_quoted_field (ide_triplet_get_kernel (triplet));
      if (quoted_field != NULL)
        g_key_file_set_string (crossbuild_keyfile, "host_machine", "system", quoted_field);

      quoted_field = _gbp_meson_get_quoted_field (ide_triplet_get_cpu (triplet));
      if (quoted_field != NULL)
        g_key_file_set_string (crossbuild_keyfile, "host_machine", "cpu_family", quoted_field);

      if (!g_key_file_save_to_file (crossbuild_keyfile, crossbuild_file, &error))
        IDE_GOTO (failure);
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

  if (!dzl_str_empty0 (config_opts))
    {
      g_auto(GStrv) argv = NULL;
      gint argc;

      if (!g_shell_parse_argv (config_opts, &argc, &argv, &error))
        IDE_GOTO (failure);

      ide_subprocess_launcher_push_args (config_launcher, (const gchar * const *)argv);
    }

  config_stage = ide_build_stage_launcher_new (context, config_launcher);
  ide_build_stage_set_name (config_stage, _("Configuring project"));
  build_ninja = ide_build_pipeline_build_builddir_path (pipeline, "build.ninja", NULL);
  if (g_file_test (build_ninja, G_FILE_TEST_IS_REGULAR))
    ide_build_stage_set_completed (config_stage, TRUE);

  id = ide_build_pipeline_connect (pipeline, IDE_BUILD_PHASE_CONFIGURE, 0, config_stage);
  ide_build_pipeline_addin_track (addin, id);

  /*
   * Register the build launcher which will perform the incremental
   * build of the project when the IDE_BUILD_PHASE_BUILD phase is
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

  build_stage = ide_build_stage_launcher_new (context, build_launcher);
  ide_build_stage_launcher_set_clean_launcher (IDE_BUILD_STAGE_LAUNCHER (build_stage), clean_launcher);
  ide_build_stage_set_check_stdout (build_stage, TRUE);
  ide_build_stage_set_name (build_stage, _("Building project"));
  g_signal_connect (build_stage, "query", G_CALLBACK (on_stage_query), NULL);

  id = ide_build_pipeline_connect (pipeline, IDE_BUILD_PHASE_BUILD, 0, build_stage);
  ide_build_pipeline_addin_track (addin, id);

  /* Setup our install stage */
  ide_subprocess_launcher_push_argv (install_launcher, ninja);
  ide_subprocess_launcher_push_argv (install_launcher, "install");
  install_stage = ide_build_stage_launcher_new (context, install_launcher);
  ide_build_stage_set_name (install_stage, _("Installing project"));
  g_signal_connect (install_stage, "query", G_CALLBACK (on_stage_query), NULL);
  id = ide_build_pipeline_connect (pipeline, IDE_BUILD_PHASE_INSTALL, 0, install_stage);
  ide_build_pipeline_addin_track (addin, id);

  IDE_EXIT;

failure:
  if (error != NULL)
    g_warning ("Failed to setup meson build pipeline: %s", error->message);
}

static void
build_pipeline_addin_iface_init (IdeBuildPipelineAddinInterface *iface)
{
  iface->load = gbp_meson_pipeline_addin_load;
}

G_DEFINE_TYPE_WITH_CODE (GbpMesonPipelineAddin, gbp_meson_pipeline_addin, IDE_TYPE_OBJECT,
                         G_IMPLEMENT_INTERFACE (IDE_TYPE_BUILD_PIPELINE_ADDIN,
                                                build_pipeline_addin_iface_init))

static void
gbp_meson_pipeline_addin_class_init (GbpMesonPipelineAddinClass *klass)
{
}

static void
gbp_meson_pipeline_addin_init (GbpMesonPipelineAddin *self)
{
}
