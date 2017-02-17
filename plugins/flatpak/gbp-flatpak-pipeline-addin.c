/* gbp-flatpak-pipeline-addin.c
 *
 * Copyright (C) 2016 Christian Hergert <chergert@redhat.com>
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

#define G_LOG_DOMAIN "gbp-flatpak-pipeline-addin"

#include "gbp-flatpak-pipeline-addin.h"
#include "gbp-flatpak-runtime.h"
#include "gbp-flatpak-transfer.h"
#include "gbp-flatpak-util.h"
#include "gbp-flatpak-configuration.h"

G_DEFINE_QUARK (gb-flatpak-pipeline-error-quark, gb_flatpak_pipeline_error)

enum {
  PREPARE_MKDIRS,
  PREPARE_BUILD_INIT,
  PREPARE_REMOTES,
};

static IdeSubprocessLauncher *
create_subprocess_launcher (void)
{
  IdeSubprocessLauncher *launcher;

  launcher = ide_subprocess_launcher_new (G_SUBPROCESS_FLAGS_STDOUT_PIPE |
                                          G_SUBPROCESS_FLAGS_STDERR_PIPE);
  ide_subprocess_launcher_set_run_on_host (launcher, TRUE);
  ide_subprocess_launcher_set_clear_env (launcher, FALSE);

  return launcher;
}

static gboolean
register_mkdirs_stage (GbpFlatpakPipelineAddin  *self,
                       IdeBuildPipeline         *pipeline,
                       IdeContext               *context,
                       GError                  **error)
{
  g_autoptr(IdeBuildStage) mkdirs = NULL;
  IdeConfiguration *config;
  g_autofree gchar *repo_dir = NULL;
  g_autofree gchar *staging_dir = NULL;
  guint stage_id;

  g_assert (GBP_IS_FLATPAK_PIPELINE_ADDIN (self));
  g_assert (IDE_IS_BUILD_PIPELINE (pipeline));
  g_assert (IDE_IS_CONTEXT (context));

  config = ide_build_pipeline_get_configuration (pipeline);

  mkdirs = ide_build_stage_mkdirs_new (context);

  repo_dir = gbp_flatpak_get_repo_dir (config);
  staging_dir = gbp_flatpak_get_staging_dir (config);

  ide_build_stage_mkdirs_add_path (IDE_BUILD_STAGE_MKDIRS (mkdirs), repo_dir, TRUE, 0750);
  ide_build_stage_mkdirs_add_path (IDE_BUILD_STAGE_MKDIRS (mkdirs), staging_dir, TRUE, 0750);

  stage_id = ide_build_pipeline_connect (pipeline, IDE_BUILD_PHASE_PREPARE, PREPARE_MKDIRS, mkdirs);

  ide_build_pipeline_addin_track (IDE_BUILD_PIPELINE_ADDIN (self), stage_id);

  return TRUE;
}

static void
check_if_file_exists (IdeBuildStage    *stage,
                      IdeBuildPipeline *pipeline,
                      GCancellable     *cancellable,
                      const gchar      *file_path)
{
  gboolean exists;

  g_assert (IDE_IS_BUILD_STAGE (stage));
  g_assert (!cancellable || G_IS_CANCELLABLE (cancellable));
  g_assert (file_path != NULL);

  exists = g_file_test (file_path, G_FILE_TEST_IS_REGULAR);

  IDE_TRACE_MSG ("%s::query checking for %s: %s",
                 G_OBJECT_TYPE_NAME (stage),
                 file_path,
                 exists ? "yes" : "no");

  ide_build_stage_set_completed (stage, exists);
}

static gboolean
register_build_init_stage (GbpFlatpakPipelineAddin  *self,
                           IdeBuildPipeline         *pipeline,
                           IdeContext               *context,
                           GError                  **error)
{
  g_autoptr(IdeSubprocessLauncher) launcher = NULL;
  g_autoptr(IdeBuildStage) stage = NULL;
  g_autofree gchar *staging_dir = NULL;
  g_autofree gchar *sdk = NULL;
  g_autofree gchar *metadata_path = NULL;
  IdeConfiguration *config;
  IdeRuntime *runtime;
  const gchar *app_id;
  const gchar *platform;
  const gchar *branch;
  guint stage_id;

  g_assert (GBP_IS_FLATPAK_PIPELINE_ADDIN (self));
  g_assert (IDE_IS_BUILD_PIPELINE (pipeline));
  g_assert (IDE_IS_CONTEXT (context));

  launcher = create_subprocess_launcher ();

  config = ide_build_pipeline_get_configuration (pipeline);
  runtime = ide_configuration_get_runtime (config);

  if (!GBP_IS_FLATPAK_RUNTIME (runtime))
    {
      g_set_error (error,
                   GB_FLATPAK_PIPELINE_ERROR,
                   GB_FLATPAK_PIPELINE_ERROR_WRONG_RUNTIME,
                   "Configuration changed to a non-flatpak runtime during pipeline initialization");
      return FALSE;
    }

  staging_dir = gbp_flatpak_get_staging_dir (config);
  app_id = ide_configuration_get_app_id (config);
  platform = gbp_flatpak_runtime_get_platform (GBP_FLATPAK_RUNTIME (runtime));
  sdk = gbp_flatpak_runtime_get_sdk_name (GBP_FLATPAK_RUNTIME (runtime));
  branch = gbp_flatpak_runtime_get_branch (GBP_FLATPAK_RUNTIME (runtime));

  if (platform == NULL && sdk == NULL)
    {
      g_set_error (error,
                   G_IO_ERROR,
                   G_IO_ERROR_FAILED,
                   "Platform and SDK are both NULL");
      return FALSE;
    }

  if (platform == NULL)
    platform = sdk;

  if (sdk == NULL)
    sdk = g_strdup (platform);

  metadata_path = g_build_filename (staging_dir, "metadata", NULL);

  ide_subprocess_launcher_push_argv (launcher, "flatpak");
  ide_subprocess_launcher_push_argv (launcher, "build-init");
  ide_subprocess_launcher_push_argv (launcher, staging_dir);
  ide_subprocess_launcher_push_argv (launcher, app_id);
  ide_subprocess_launcher_push_argv (launcher, sdk);
  ide_subprocess_launcher_push_argv (launcher, platform);
  ide_subprocess_launcher_push_argv (launcher, branch);

  stage = g_object_new (IDE_TYPE_BUILD_STAGE_LAUNCHER,
                        "context", context,
                        "launcher", launcher,
                        NULL);

  /*
   * We want to avoid calling build-init if it has already been called.
   * To check if this has happened, we just look for the manifest file
   * located in the directory that had build-init called.
   */
  g_signal_connect_data (stage,
                         "query",
                         G_CALLBACK (check_if_file_exists),
                         g_steal_pointer (&metadata_path),
                         (GClosureNotify)g_free,
                         0);

  stage_id = ide_build_pipeline_connect (pipeline,
                                         IDE_BUILD_PHASE_PREPARE,
                                         PREPARE_BUILD_INIT,
                                         stage);
  ide_build_pipeline_addin_track (IDE_BUILD_PIPELINE_ADDIN (self), stage_id);

  return TRUE;
}

static gboolean
register_dependencies_stage (GbpFlatpakPipelineAddin  *self,
                             IdeBuildPipeline         *pipeline,
                             IdeContext               *context,
                             GError                  **error)
{
  g_autoptr(IdeBuildStage) stage = NULL;
  g_autoptr(IdeSubprocessLauncher) launcher = NULL;
  g_autofree gchar *staging_dir = NULL;
  g_autofree gchar *stop_at_option = NULL;
  IdeConfiguration *config;
  g_autofree gchar *manifest_path = NULL;
  const gchar *primary_module;
  guint stage_id;

  g_assert (GBP_IS_FLATPAK_PIPELINE_ADDIN (self));
  g_assert (IDE_IS_BUILD_PIPELINE (pipeline));
  g_assert (IDE_IS_CONTEXT (context));

  config = ide_build_pipeline_get_configuration (pipeline);

  /* If there is no manifest, then there are no dependencies
   * to build for this configuration.
   */
  if (!GBP_IS_FLATPAK_CONFIGURATION (config))
    return TRUE;

  primary_module = gbp_flatpak_configuration_get_primary_module (GBP_FLATPAK_CONFIGURATION (config));
  manifest_path = gbp_flatpak_configuration_get_manifest_path (GBP_FLATPAK_CONFIGURATION (config));

  staging_dir = gbp_flatpak_get_staging_dir (config);

  launcher = create_subprocess_launcher ();

  ide_subprocess_launcher_push_argv (launcher, "flatpak-builder");
  ide_subprocess_launcher_push_argv (launcher, "--ccache");
  ide_subprocess_launcher_push_argv (launcher, "--force-clean");
  stop_at_option = g_strdup_printf ("--stop-at=%s", primary_module);
  ide_subprocess_launcher_push_argv (launcher, stop_at_option);
  ide_subprocess_launcher_push_argv (launcher, staging_dir);
  ide_subprocess_launcher_push_argv (launcher, manifest_path);

  stage = g_object_new (IDE_TYPE_BUILD_STAGE_LAUNCHER,
                        "context", context,
                        "launcher", launcher,
                        NULL);

  stage_id = ide_build_pipeline_connect (pipeline, IDE_BUILD_PHASE_DEPENDENCIES, 0, stage);
  ide_build_pipeline_addin_track (IDE_BUILD_PIPELINE_ADDIN (self), stage_id);

  return TRUE;
}

static gboolean
register_build_finish_stage (GbpFlatpakPipelineAddin  *self,
                             IdeBuildPipeline         *pipeline,
                             IdeContext               *context,
                             GError                  **error)
{
  const gchar * const *finish_args;
  g_autoptr(IdeBuildStage) stage = NULL;
  g_autoptr(IdeSubprocessLauncher) launcher = NULL;
  g_autofree gchar *staging_dir = NULL;
  g_autofree gchar *export_path = NULL;
  IdeConfiguration *config;
  g_autofree gchar *manifest_path = NULL;
  const gchar *command;
  guint stage_id;

  g_assert (GBP_IS_FLATPAK_PIPELINE_ADDIN (self));
  g_assert (IDE_IS_BUILD_PIPELINE (pipeline));
  g_assert (IDE_IS_CONTEXT (context));

  config = ide_build_pipeline_get_configuration (pipeline);

  /* If there is no manifest, then there are no dependencies
   * to build for this configuration.
   */
  if (!GBP_IS_FLATPAK_CONFIGURATION (config))
    return TRUE;

  manifest_path = gbp_flatpak_configuration_get_manifest_path (GBP_FLATPAK_CONFIGURATION (config));
  command = gbp_flatpak_configuration_get_command (GBP_FLATPAK_CONFIGURATION (config));
  finish_args = gbp_flatpak_configuration_get_finish_args (GBP_FLATPAK_CONFIGURATION (config));

  staging_dir = gbp_flatpak_get_staging_dir (config);

  launcher = create_subprocess_launcher ();

  ide_subprocess_launcher_push_argv (launcher, "flatpak");
  ide_subprocess_launcher_push_argv (launcher, "build-finish");

  /*
   * The --command argument allows the manifest to specify which binary in the
   * path (/app/bin) should be used as the application binary. By default, the
   * first binary found in /app/bin is used. However, for applications that
   * contain supplimental binaries, they may need to specify which is primary.
   */
  if (!ide_str_empty0 (command))
    {
      g_autofree gchar *command_option = NULL;

      command_option = g_strdup_printf ("--command=%s", command);
      ide_subprocess_launcher_push_argv (launcher, command_option);
    }

  /*
   * The finish args include things like --share=network. These specify which
   * sandboxing features are necessary, what host files may need to be mapped
   * in, which D-Bus services to allow, and more.
   */
  ide_subprocess_launcher_push_args (launcher, finish_args);

  /*
   * The staging directory is the location we did build-init with (or which
   * the flatpak-builder was using for building).
   */
  ide_subprocess_launcher_push_argv (launcher, staging_dir);

  stage = g_object_new (IDE_TYPE_BUILD_STAGE_LAUNCHER,
                        "context", context,
                        "launcher", launcher,
                        NULL);

  /*
   * If the export directory is found, we already performed the build-finish
   * and we do not need to run this operation again. So check if the file
   * exists and update IdeBuildStage:completed.
   */
  export_path = g_build_filename (staging_dir, "export", NULL);
  g_signal_connect_data (stage,
                         "query",
                         G_CALLBACK (check_if_file_exists),
                         g_steal_pointer (&export_path),
                         (GClosureNotify)g_free,
                         0);

  stage_id = ide_build_pipeline_connect (pipeline, IDE_BUILD_PHASE_EXPORT, 0, stage);
  ide_build_pipeline_addin_track (IDE_BUILD_PIPELINE_ADDIN (self), stage_id);

  return TRUE;
}

static void
gbp_flatpak_pipeline_addin_load (IdeBuildPipelineAddin *addin,
                                 IdeBuildPipeline      *pipeline)
{
  GbpFlatpakPipelineAddin *self = (GbpFlatpakPipelineAddin *)addin;
  g_autoptr(GError) error = NULL;
  IdeConfiguration *config;
  IdeContext *context;
  IdeRuntime *runtime;

  g_assert (GBP_IS_FLATPAK_PIPELINE_ADDIN (self));
  g_assert (IDE_IS_BUILD_PIPELINE (pipeline));

  config = ide_build_pipeline_get_configuration (pipeline);

  /* TODO: Once we have GbpFlatpakConfiguration, we can check for
   *       that (and it should only allow for valid flatpak runtimes).
   */

  runtime = ide_configuration_get_runtime (config);

  if (!GBP_IS_FLATPAK_RUNTIME (runtime))
    {
      g_message ("Configuration is not using flatpak, ignoring pipeline");
      return;
    }

  /*
   * TODO: We should add the ability to mark a pipeline as broken, if we
   *       detect something that is alarming. That will prevent builds from
   *       occuring altogether and allow us to present issues within the UI.
   */

  context = ide_object_get_context (IDE_OBJECT (self));

  if (!register_mkdirs_stage (self, pipeline, context, &error) ||
      !register_build_init_stage (self, pipeline, context, &error) ||
      !register_dependencies_stage (self, pipeline, context, &error) ||
      !register_build_finish_stage (self, pipeline, context, &error))
    g_warning ("%s", error->message);
}

/* GObject boilerplate */

static void
build_pipeline_addin_iface_init (IdeBuildPipelineAddinInterface *iface)
{
  iface->load = gbp_flatpak_pipeline_addin_load;
}

struct _GbpFlatpakPipelineAddin { IdeObject parent_instance; };

G_DEFINE_TYPE_WITH_CODE (GbpFlatpakPipelineAddin, gbp_flatpak_pipeline_addin, IDE_TYPE_OBJECT,
                         G_IMPLEMENT_INTERFACE (IDE_TYPE_BUILD_PIPELINE_ADDIN,
                                                build_pipeline_addin_iface_init))

static void
gbp_flatpak_pipeline_addin_class_init (GbpFlatpakPipelineAddinClass *klass)
{
}

static void
gbp_flatpak_pipeline_addin_init (GbpFlatpakPipelineAddin *self)
{
}
