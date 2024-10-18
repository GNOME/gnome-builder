/* gbp-flatpak-pipeline-addin.c
 *
 * Copyright 2016-2023 Christian Hergert <chergert@redhat.com>
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

#define G_LOG_DOMAIN "gbp-flatpak-pipeline-addin"

#include <glib/gi18n.h>

#include <libide-foundry.h>
#include <libide-gtk.h>

#include "ide-pipeline-private.h"

#include "gbp-flatpak-manifest.h"
#include "gbp-flatpak-download-stage.h"
#include "gbp-flatpak-pipeline-addin.h"
#include "gbp-flatpak-runtime.h"
#include "gbp-flatpak-util.h"

#define VERSION_CHECK(v,a,b,c)                                         \
  (((v)->major > (a)) ||                                               \
   (((v)->major == (a)) && ((v)->minor > (b))) ||                      \
   (((v)->major == (a)) && ((v)->minor == (b)) && ((v)->micro >= (c))))

enum {
  FLAGS_NONE = 0,
  FLAGS_RUN_ON_HOST = 1,
};

struct _GbpFlatpakPipelineAddin
{
  IdeObject parent_instance;

  char *state_dir;

  struct {
    int major;
    int minor;
    int micro;
  } version;
};

G_DEFINE_QUARK (gb-flatpak-pipeline-error-quark, gb_flatpak_pipeline_error)

enum {
  COMMIT_BUILD_FINISH,
  COMMIT_BUILD_EXPORT,
};

enum {
  EXPORT_BUILD_BUNDLE,
};

static void
ensure_documents_portal (void)
{
  g_autoptr(GDBusConnection) bus = g_bus_get_sync (G_BUS_TYPE_SESSION, NULL, NULL);
  g_autoptr(GVariant) reply = NULL;

  g_assert (G_IS_DBUS_CONNECTION (bus));

  reply = g_dbus_connection_call_sync (bus,
                                       "org.freedesktop.portal.Documents",
                                       "/org/freedesktop/portal/documents",
                                       "org.freedesktop.portal.Documents",
                                       "GetMountPoint",
                                       g_variant_new ("()"),
                                       NULL,
                                       G_DBUS_CALL_FLAGS_NONE,
                                       3000,
                                       NULL,
                                       NULL);
}

static char *
get_arch_option (IdePipeline *pipeline)
{
  g_autofree char *arch = NULL;

  g_assert (IDE_IS_PIPELINE (pipeline));

  arch = ide_pipeline_dup_arch (pipeline);

  return g_strdup_printf ("--arch=%s", arch);
}

static void
sniff_flatpak_builder_version (GbpFlatpakPipelineAddin *self)
{
  g_autoptr(IdeSubprocessLauncher) launcher = NULL;
  g_autoptr(IdeSubprocess) subprocess = NULL;
  g_autofree char *stdout_buf = NULL;
  const char *str;
  int major = 0;
  int minor = 0;
  int micro = 0;

  g_assert (GBP_IS_FLATPAK_PIPELINE_ADDIN (self));

  launcher = ide_subprocess_launcher_new (G_SUBPROCESS_FLAGS_STDOUT_PIPE);
  ide_subprocess_launcher_set_clear_env (launcher, FALSE);
  ide_subprocess_launcher_setenv (launcher, "LANG", "C", TRUE);
  ide_subprocess_launcher_push_argv (launcher, "flatpak-builder");
  ide_subprocess_launcher_push_argv (launcher, "--version");

  subprocess = ide_subprocess_launcher_spawn (launcher, NULL, NULL);
  if (subprocess == NULL)
    return;

  if (!ide_subprocess_communicate_utf8 (subprocess, NULL, NULL, &stdout_buf, NULL, NULL))
    return;

  /* flatpak-builder changed to "flatpak-builder-1.2.3" at some point */
  if (g_str_has_prefix (stdout_buf, "flatpak-builder-"))
    str = stdout_buf + strlen ("flatpak-builder-");
  else if (g_str_has_prefix (stdout_buf, "flatpak-builder "))
    str = stdout_buf + strlen ("flatpak-builder ");
  else
    str = stdout_buf; /* unlikely, but lets try */

  if (sscanf (str, "%d.%d.%d", &major, &minor, &micro) != 3)
    return;

  self->version.major = major;
  self->version.minor = minor;
  self->version.micro = micro;
}

static void
always_run_query_handler (IdePipelineStage *stage,
                          IdePipeline      *pipeline,
                          GPtrArray        *targets,
                          GCancellable     *cancellable,
                          gpointer          user_data)
{
  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (IDE_IS_PIPELINE_STAGE (stage));
  g_assert (IDE_IS_PIPELINE (pipeline));
  g_assert (!cancellable || G_IS_CANCELLABLE (cancellable));

  ide_pipeline_stage_set_completed (stage, FALSE);
}

static gboolean
register_mkdirs_stage (GbpFlatpakPipelineAddin  *self,
                       IdePipeline              *pipeline,
                       IdeContext               *context,
                       GError                  **error)
{
  g_autoptr(IdePipelineStage) mkdirs = NULL;
  g_autofree char *repo_dir = NULL;
  g_autofree char *staging_dir = NULL;
  guint stage_id;

  g_assert (GBP_IS_FLATPAK_PIPELINE_ADDIN (self));
  g_assert (IDE_IS_PIPELINE (pipeline));
  g_assert (IDE_IS_CONTEXT (context));

  mkdirs = ide_pipeline_stage_mkdirs_new (context);
  ide_pipeline_stage_set_name (mkdirs, _("Creating flatpak workspace"));

  repo_dir = gbp_flatpak_get_repo_dir (context);
  staging_dir = gbp_flatpak_get_staging_dir (pipeline);

  ide_pipeline_stage_mkdirs_add_path (IDE_PIPELINE_STAGE_MKDIRS (mkdirs), repo_dir, TRUE, 0750, FALSE);
  ide_pipeline_stage_mkdirs_add_path (IDE_PIPELINE_STAGE_MKDIRS (mkdirs), staging_dir, TRUE, 0750, TRUE);

  stage_id = ide_pipeline_attach (pipeline, IDE_PIPELINE_PHASE_PREPARE, 0, mkdirs);

  ide_pipeline_addin_track (IDE_PIPELINE_ADDIN (self), stage_id);

  return TRUE;
}

static void
reap_staging_dir_cb (GObject      *object,
                     GAsyncResult *result,
                     gpointer      user_data)
{
  IdeDirectoryReaper *reaper = (IdeDirectoryReaper *)object;
  g_autoptr(IdePipelineStage) stage = user_data;
  g_autoptr(GError) error = NULL;
  IdePipelineAddin *addin;
  IdePipeline *pipeline;
  const guint *stage_ids;
  guint n_stages;

  IDE_ENTRY;

  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (IDE_IS_DIRECTORY_REAPER (reaper));
  g_assert (G_IS_ASYNC_RESULT (result));
  g_assert (IDE_IS_PIPELINE_STAGE (stage));

  pipeline = g_object_get_data (G_OBJECT (reaper), "IDE_PIPELINE");

  g_assert (IDE_IS_PIPELINE (pipeline));

  if (!ide_directory_reaper_execute_finish (reaper, result, &error))
    ide_object_warning (stage,
                        "Failed to reap staging directory: %s",
                        error->message);

  /* Since we reaped the directory tree, make sure that the
   * dependencies stage is re-run.
   */
  addin = ide_pipeline_addin_find_by_module_name (pipeline, "flatpak");
  stage_ids = _ide_pipeline_addin_get_stages (addin, &n_stages);
  for (guint i = 0; i < n_stages; i++)
    {
      IdePipelineStage *item = ide_pipeline_get_stage_by_id (pipeline, stage_ids[i]);

      if (g_object_get_data (G_OBJECT (item), "FLATPAK_DEPENDENCIES_STAGE"))
        ide_pipeline_stage_set_completed (item, FALSE);
    }

  ide_pipeline_stage_unpause (stage);

  IDE_EXIT;
}

static void
check_for_build_init_files (IdePipelineStage *stage,
                            IdePipeline      *pipeline,
                            GPtrArray        *targets,
                            GCancellable     *cancellable,
                            const char       *staging_dir)
{
  g_autofree char *metadata = NULL;
  g_autofree char *files = NULL;
  g_autofree char *var = NULL;
  gboolean completed = FALSE;
  gboolean parent_exists;

  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (IDE_IS_PIPELINE_STAGE (stage));
  g_assert (!cancellable || G_IS_CANCELLABLE (cancellable));
  g_assert (staging_dir != NULL);

  /* First make sure that we have access to the Portals service
   * so that any of our build operations succeed.
   */
  ensure_documents_portal ();

  metadata = g_build_filename (staging_dir, "metadata", NULL);
  files = g_build_filename (staging_dir, "files", NULL);
  var = g_build_filename (staging_dir, "var", NULL);

  parent_exists = g_file_test (staging_dir, G_FILE_TEST_IS_DIR);

  if (parent_exists &&
      g_file_test (metadata, G_FILE_TEST_IS_REGULAR) &&
      g_file_test (files, G_FILE_TEST_IS_DIR) &&
      g_file_test (var, G_FILE_TEST_IS_DIR))
    completed = TRUE;

  g_debug ("Checking for previous build-init in %s: %s",
           staging_dir, completed ? "yes" : "no");

  ide_pipeline_stage_set_completed (stage, completed);

  if (!completed && parent_exists)
    {
      g_autoptr(IdeDirectoryReaper) reaper = NULL;
      g_autoptr(GFile) staging = g_file_new_for_path (staging_dir);

      ide_object_message (pipeline,
                          _("Removing stale flatpak staging directory: %s"),
                          staging_dir);

      ide_pipeline_stage_pause (stage);

      reaper = ide_directory_reaper_new ();
      ide_directory_reaper_add_directory (reaper, staging, 0);
      g_object_set_data_full (G_OBJECT (reaper),
                              "IDE_PIPELINE",
                              g_object_ref (pipeline),
                              g_object_unref);
      ide_directory_reaper_execute_async (reaper,
                                          cancellable,
                                          reap_staging_dir_cb,
                                          g_object_ref (stage));
    }
}

static void
reap_staging_dir (IdePipelineStage   *stage,
                  IdeDirectoryReaper *reaper,
                  const char         *staging_dir)
{
  g_autoptr(GFile) dir = NULL;

  g_assert (IDE_IS_PIPELINE_STAGE (stage));
  g_assert (IDE_IS_DIRECTORY_REAPER (reaper));
  g_assert (staging_dir != NULL);

  dir = g_file_new_for_path (staging_dir);
  ide_directory_reaper_add_directory (reaper, dir, 0);
}

static IdeRunContext *
create_run_context_cb (IdePipelineStageCommand *stage,
                       IdeRunCommand           *run_command,
                       gpointer                 user_data)
{
  IdeContext *context = ide_object_get_context (IDE_OBJECT (stage));
  IdeRunContext *run_context = ide_run_context_new ();
  guint flags = GPOINTER_TO_UINT (user_data);

  if (flags & FLAGS_RUN_ON_HOST)
    ide_run_context_push_host (run_context);

  gbp_flatpak_set_config_dir (run_context);

  ide_run_command_prepare_to_run (run_command, run_context, context);

  return run_context;
}

static gboolean
register_build_init_stage (GbpFlatpakPipelineAddin  *self,
                           IdePipeline              *pipeline,
                           IdeContext               *context,
                           GError                  **error)
{
  g_autoptr(IdeRunCommand) run_command = NULL;
  g_autoptr(IdePipelineStage) stage = NULL;
  g_autofree char *staging_dir = NULL;
  g_autofree char *sdk = NULL;
  g_autofree char *arch = NULL;
  const char * const *sdk_extensions = NULL;
  IdeConfig *config;
  IdeRuntime *runtime;
  const char *app_id;
  const char *platform;
  const char *branch;
  guint stage_id;

  g_assert (GBP_IS_FLATPAK_PIPELINE_ADDIN (self));
  g_assert (IDE_IS_PIPELINE (pipeline));
  g_assert (IDE_IS_CONTEXT (context));

  run_command = ide_run_command_new ();

  config = ide_pipeline_get_config (pipeline);
  runtime = ide_pipeline_get_runtime (pipeline);

  if (!GBP_IS_FLATPAK_RUNTIME (runtime))
    {
      g_set_error (error,
                   GB_FLATPAK_PIPELINE_ERROR,
                   GB_FLATPAK_PIPELINE_ERROR_WRONG_RUNTIME,
                   "Configuration changed to a non-flatpak runtime during pipeline initialization");
      return FALSE;
    }

  arch = get_arch_option (pipeline);
  staging_dir = gbp_flatpak_get_staging_dir (pipeline);
  app_id = ide_config_get_app_id (config);
  platform = gbp_flatpak_runtime_get_platform (GBP_FLATPAK_RUNTIME (runtime));
  sdk = gbp_flatpak_runtime_get_sdk_name (GBP_FLATPAK_RUNTIME (runtime));
  branch = gbp_flatpak_runtime_get_branch (GBP_FLATPAK_RUNTIME (runtime));

  if (GBP_IS_FLATPAK_MANIFEST (config))
    sdk_extensions = gbp_flatpak_manifest_get_sdk_extensions (GBP_FLATPAK_MANIFEST (config));

  /*
   * If we got here by using a non-flatpak configuration, then there is a
   * chance we don't have a valid app-id.
   */
  if (ide_str_empty0 (app_id))
    app_id = "com.example.App";

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

  ide_run_command_append_argv (run_command, "flatpak");
  ide_run_command_append_argv (run_command, "build-init");
  ide_run_command_append_argv (run_command, "--type=app");
  ide_run_command_append_argv (run_command, arch);
  ide_run_command_append_argv (run_command, staging_dir);
  ide_run_command_append_argv (run_command, app_id);
  ide_run_command_append_argv (run_command, sdk);
  ide_run_command_append_argv (run_command, platform);
  ide_run_command_append_argv (run_command, branch);

  if (sdk_extensions != NULL)
    {
      for (guint i = 0; sdk_extensions[i]; i++)
        {
          g_auto(GStrv) split = g_strsplit (sdk_extensions[i], "/", 2);
          g_autofree char *arg = g_strdup_printf ("--sdk-extension=%s", split[0]);

          ide_run_command_append_argv (run_command, arg);
        }
    }

  stage = g_object_new (IDE_TYPE_PIPELINE_STAGE_COMMAND,
                        "name", _("Preparing build directory"),
                        "build-command", run_command,
                        NULL);

  g_signal_connect (stage,
                    "create-run-context",
                    G_CALLBACK (create_run_context_cb),
                    GUINT_TO_POINTER (FLAGS_RUN_ON_HOST));

  /*
   * We want to avoid calling build-init if it has already been called.
   * To check if this has happened, we just look for the manifest file
   * located in the directory that had build-init called.
   */
  g_signal_connect_data (stage,
                         "query",
                         G_CALLBACK (check_for_build_init_files),
                         g_strdup (staging_dir),
                         (GClosureNotify)g_free,
                         0);

  /*
   * When reaping the build, during a rebuild, make sure we wipe
   * out the stagint directory too.
   */
  g_signal_connect_data (stage,
                         "reap",
                         G_CALLBACK (reap_staging_dir),
                         g_strdup (staging_dir),
                         (GClosureNotify)g_free,
                         0);

  /* Connect at autogen level because both downloads/dependencies
   * will end up deleting build-init anyway from --force-clean. So
   * when those do not need to run, do this as a last chance to ensure
   * that we have something useful before building the apps.
   */
  stage_id = ide_pipeline_attach (pipeline,
                                  IDE_PIPELINE_PHASE_AUTOGEN | IDE_PIPELINE_PHASE_BEFORE,
                                  0,
                                  stage);
  ide_pipeline_addin_track (IDE_PIPELINE_ADDIN (self), stage_id);

  return TRUE;
}

static gboolean
register_downloads_stage (GbpFlatpakPipelineAddin  *self,
                          IdePipeline              *pipeline,
                          IdeContext               *context,
                          GError                  **error)
{
  g_autoptr(IdePipelineStage) stage = NULL;
  guint stage_id;

  g_assert (GBP_IS_FLATPAK_PIPELINE_ADDIN (self));
  g_assert (IDE_IS_PIPELINE (pipeline));
  g_assert (IDE_IS_CONTEXT (context));

  stage = g_object_new (GBP_TYPE_FLATPAK_DOWNLOAD_STAGE,
                        "name", _("Downloading dependencies"),
                        "state-dir", self->state_dir,
                        NULL);
  stage_id = ide_pipeline_attach (pipeline, IDE_PIPELINE_PHASE_DOWNLOADS, 0, stage);
  ide_pipeline_addin_track (IDE_PIPELINE_ADDIN (self), stage_id);

  return TRUE;
}

static gboolean
register_dependencies_stage (GbpFlatpakPipelineAddin  *self,
                             IdePipeline              *pipeline,
                             IdeContext               *context,
                             GError                  **error)
{
  g_autoptr(IdePipelineStage) stage = NULL;
  g_autoptr(IdeRunCommand) run_command = NULL;
  g_autofree char *arch = NULL;
  g_autofree char *manifest_path = NULL;
  g_autofree char *staging_dir = NULL;
  g_autofree char *stop_at_option = NULL;
  IdeConfig *config;
  const char *primary_module;
  const char *src_dir;
  guint stage_id;

  g_assert (GBP_IS_FLATPAK_PIPELINE_ADDIN (self));
  g_assert (IDE_IS_PIPELINE (pipeline));
  g_assert (IDE_IS_CONTEXT (context));

  config = ide_pipeline_get_config (pipeline);

  /* If there is no manifest, then there are no dependencies
   * to build for this configuration.
   */
  if (!GBP_IS_FLATPAK_MANIFEST (config))
    return TRUE;

  arch = get_arch_option (pipeline);
  primary_module = gbp_flatpak_manifest_get_primary_module (GBP_FLATPAK_MANIFEST (config));
  manifest_path = gbp_flatpak_manifest_get_path (GBP_FLATPAK_MANIFEST (config));

  staging_dir = gbp_flatpak_get_staging_dir (pipeline);
  src_dir = ide_pipeline_get_srcdir (pipeline);

  run_command = ide_run_command_new ();
  ide_run_command_set_cwd (run_command, src_dir);

  if (ide_is_flatpak ())
    {
      g_autofree char *user_dir = NULL;

      user_dir = g_build_filename (g_get_home_dir (), ".local", "share", "flatpak", NULL);
      ide_run_command_setenv (run_command, "FLATPAK_USER_DIR", user_dir);
      ide_run_command_setenv (run_command, "XDG_RUNTIME_DIR", g_get_user_runtime_dir ());
    }

  ide_run_command_append_argv (run_command, "flatpak-builder");
  ide_run_command_append_argv (run_command,  arch);
  ide_run_command_append_argv (run_command,  "--ccache");
  ide_run_command_append_argv (run_command,  "--force-clean");
  ide_run_command_append_argv (run_command,  "--disable-updates");
  ide_run_command_append_argv (run_command,  "--disable-download");

  if (self->state_dir != NULL)
    {
      ide_run_command_append_argv (run_command, "--state-dir");
      ide_run_command_append_argv (run_command, self->state_dir);
    }

  stop_at_option = g_strdup_printf ("--stop-at=%s", primary_module);
  ide_run_command_append_argv (run_command, stop_at_option);
  ide_run_command_append_argv (run_command, staging_dir);
  ide_run_command_append_argv (run_command, manifest_path);

  stage = g_object_new (IDE_TYPE_PIPELINE_STAGE_COMMAND,
                        "name", _("Building dependencies"),
                        "build-command", run_command,
                        NULL);

  g_signal_connect (stage,
                    "create-run-context",
                    G_CALLBACK (create_run_context_cb),
                    NULL);

  g_object_set_data (G_OBJECT (stage),
                     "FLATPAK_DEPENDENCIES_STAGE",
                     GINT_TO_POINTER (1));

  stage_id = ide_pipeline_attach (pipeline, IDE_PIPELINE_PHASE_DEPENDENCIES, 0, stage);
  ide_pipeline_addin_track (IDE_PIPELINE_ADDIN (self), stage_id);

  return TRUE;
}

static gboolean
register_build_finish_stage (GbpFlatpakPipelineAddin  *self,
                             IdePipeline              *pipeline,
                             IdeContext               *context,
                             GError                  **error)
{
  g_autoptr(IdeRunCommand) run_command = NULL;
  g_autoptr(IdePipelineStage) stage = NULL;
  g_autofree char *staging_dir = NULL;
  const char * const *finish_args;
  const char *command;
  IdeConfig *config;
  guint stage_id;

  g_assert (GBP_IS_FLATPAK_PIPELINE_ADDIN (self));
  g_assert (IDE_IS_PIPELINE (pipeline));
  g_assert (IDE_IS_CONTEXT (context));

  config = ide_pipeline_get_config (pipeline);
  if (!GBP_IS_FLATPAK_MANIFEST (config))
    return TRUE;

  command = gbp_flatpak_manifest_get_command (GBP_FLATPAK_MANIFEST (config));
  finish_args = gbp_flatpak_manifest_get_finish_args (GBP_FLATPAK_MANIFEST (config));
  staging_dir = gbp_flatpak_get_staging_dir (pipeline);

  run_command = ide_run_command_new ();

  ide_run_command_append_argv (run_command, "flatpak");
  ide_run_command_append_argv (run_command, "build-finish");

  if (command != NULL)
    {
      ide_run_command_append_argv (run_command, "--command");
      ide_run_command_append_argv (run_command, command);
    }

  ide_run_command_append_args (run_command, finish_args);
  ide_run_command_append_argv (run_command, staging_dir);

  stage = g_object_new (IDE_TYPE_PIPELINE_STAGE_COMMAND,
                        "name", _("Finalizing flatpak build"),
                        "build-command", run_command,
                        NULL);

  g_signal_connect (stage,
                    "create-run-context",
                    G_CALLBACK (create_run_context_cb),
                    GUINT_TO_POINTER (FLAGS_RUN_ON_HOST));

  stage_id = ide_pipeline_attach (pipeline, IDE_PIPELINE_PHASE_COMMIT, COMMIT_BUILD_FINISH, stage);
  ide_pipeline_addin_track (IDE_PIPELINE_ADDIN (self), stage_id);

  return TRUE;
}

static gboolean
register_build_export_stage (GbpFlatpakPipelineAddin  *self,
                             IdePipeline              *pipeline,
                             IdeContext               *context,
                             GError                  **error)
{
  g_autoptr(IdeRunCommand) run_command = NULL;
  g_autoptr(IdePipelineStage) stage = NULL;
  g_autofree char *arch = NULL;
  g_autofree char *repo_dir = NULL;
  g_autofree char *staging_dir = NULL;
  IdeConfig *config;
  guint stage_id;

  g_assert (GBP_IS_FLATPAK_PIPELINE_ADDIN (self));
  g_assert (IDE_IS_PIPELINE (pipeline));
  g_assert (IDE_IS_CONTEXT (context));

  config = ide_pipeline_get_config (pipeline);
  if (!GBP_IS_FLATPAK_MANIFEST (config))
    return TRUE;

  staging_dir = gbp_flatpak_get_staging_dir (pipeline);
  repo_dir = gbp_flatpak_get_repo_dir (context);
  arch = get_arch_option (pipeline);

  run_command = ide_run_command_new ();

  ide_run_command_append_argv (run_command, "flatpak");
  ide_run_command_append_argv (run_command, "build-export");
  ide_run_command_append_argv (run_command, arch);
  ide_run_command_append_argv (run_command, repo_dir);
  ide_run_command_append_argv (run_command, staging_dir);

  stage = g_object_new (IDE_TYPE_PIPELINE_STAGE_COMMAND,
                        "name", _("Exporting staging directory"),
                        "build-command", run_command,
                        NULL);

  g_signal_connect (stage,
                    "query",
                    G_CALLBACK (always_run_query_handler),
                    NULL);

  g_signal_connect (stage,
                    "create-run-context",
                    G_CALLBACK (create_run_context_cb),
                    GUINT_TO_POINTER (FLAGS_RUN_ON_HOST));

  stage_id = ide_pipeline_attach (pipeline, IDE_PIPELINE_PHASE_COMMIT, COMMIT_BUILD_EXPORT, stage);
  ide_pipeline_addin_track (IDE_PIPELINE_ADDIN (self), stage_id);

  return TRUE;
}

static void
build_bundle_notify_completed (IdePipelineStage *stage,
                               GParamSpec       *pspec,
                               const char       *dest_path)
{
  g_assert (IDE_IS_PIPELINE_STAGE (stage));
  g_assert (dest_path != NULL);

  /*
   * If we successfully completed the build-bundle, show the file
   * to the user so they can copy/paste/share/etc.
   */

  if (ide_pipeline_stage_get_completed (stage))
    {
      g_autoptr(GFile) file = g_file_new_for_path (dest_path);
      ide_file_manager_show (file, NULL);
    }
}

static gboolean
register_build_bundle_stage (GbpFlatpakPipelineAddin  *self,
                             IdePipeline              *pipeline,
                             IdeContext               *context,
                             GError                  **error)
{
  g_autoptr(IdePipelineStage) stage = NULL;
  g_autoptr(IdeRunCommand) run_command = NULL;
  g_autofree char *staging_dir = NULL;
  g_autofree char *repo_dir = NULL;
  g_autofree char *dest_path = NULL;
  g_autofree char *arch = NULL;
  g_autofree char *name = NULL;
  IdeConfig *config;
  const char *app_id;
  guint stage_id;

  g_assert (GBP_IS_FLATPAK_PIPELINE_ADDIN (self));
  g_assert (IDE_IS_PIPELINE (pipeline));
  g_assert (IDE_IS_CONTEXT (context));

  config = ide_pipeline_get_config (pipeline);
  if (!GBP_IS_FLATPAK_MANIFEST (config))
    return TRUE;

  staging_dir = gbp_flatpak_get_staging_dir (pipeline);
  repo_dir = gbp_flatpak_get_repo_dir (context);

  app_id = ide_config_get_app_id (config);
  name = g_strdup_printf ("%s.flatpak", app_id);
  dest_path = g_build_filename (staging_dir, name, NULL);

  arch = get_arch_option (pipeline);

  run_command = ide_run_command_new ();

  ide_run_command_append_argv (run_command, "flatpak");
  ide_run_command_append_argv (run_command, "build-bundle");
  ide_run_command_append_argv (run_command, arch);
  ide_run_command_append_argv (run_command, repo_dir);
  ide_run_command_append_argv (run_command, dest_path);
  ide_run_command_append_argv (run_command, app_id);
  /* TODO:
   *
   * We probably need to provide UI/config opt to tweak the branch name
   * if (ide_config_get_is_release (config))
   */
  ide_run_command_append_argv (run_command, "master");

  stage = g_object_new (IDE_TYPE_PIPELINE_STAGE_COMMAND,
                        "name", _("Creating flatpak bundle"),
                        "build-command", run_command,
                        NULL);

  g_signal_connect (stage,
                    "query",
                    G_CALLBACK (always_run_query_handler),
                    NULL);

  g_signal_connect (stage,
                    "create-run-context",
                    G_CALLBACK (create_run_context_cb),
                    GUINT_TO_POINTER (FLAGS_RUN_ON_HOST));

  g_signal_connect_data (stage,
                         "notify::completed",
                         G_CALLBACK (build_bundle_notify_completed),
                         g_steal_pointer (&dest_path),
                         (GClosureNotify)g_free,
                         0);

  stage_id = ide_pipeline_attach (pipeline, IDE_PIPELINE_PHASE_EXPORT, EXPORT_BUILD_BUNDLE, stage);
  ide_pipeline_addin_track (IDE_PIPELINE_ADDIN (self), stage_id);

  return TRUE;
}

static void
gbp_flatpak_pipeline_addin_load (IdePipelineAddin *addin,
                                 IdePipeline      *pipeline)
{
  GbpFlatpakPipelineAddin *self = (GbpFlatpakPipelineAddin *)addin;
  g_autoptr(GError) error = NULL;
  const char *runtime_id;
  IdeContext *context;
  IdeConfig *config;

  g_assert (GBP_IS_FLATPAK_PIPELINE_ADDIN (self));
  g_assert (IDE_IS_PIPELINE (pipeline));

  context = ide_object_get_context (IDE_OBJECT (self));
  config = ide_pipeline_get_config (pipeline);
  runtime_id = ide_config_get_runtime_id (config);

  /* We must at least be using a flatpak runtime */
  if (runtime_id == NULL || !g_str_has_prefix (runtime_id, "flatpak:"))
    return;

  sniff_flatpak_builder_version (self);

  if (VERSION_CHECK (&self->version, 0, 10, 5))
    {
      g_autofree char *default_cache_root = ide_dup_default_cache_dir ();

      /* Use a single flatpak-builder state directory that is
       * kept within .cache (or appropriate mapping directory)
       * instead of littering it within the project checkout.
       * It also allows us to share checkouts and clones between
       * projects instead lots of duplicates.
       *
       * It does have the side effect that it is harder to
       * prune existing data and we may want to address that
       * in the future (either upstream or in here).
       */
      self->state_dir = g_build_filename (default_cache_root, "flatpak-builder", NULL);
    }

  if (!register_mkdirs_stage (self, pipeline, context, &error) ||
      !register_build_init_stage (self, pipeline, context, &error))
    {
      ide_object_warning (pipeline,
                          "Failed to configure flatpak pipeline: %s",
                          error->message);
      return;
    }

  /* We can't do anything more unless we have a flatpak manifest */
  if (!GBP_IS_FLATPAK_MANIFEST (config))
    return;

  if (!register_downloads_stage (self, pipeline, context, &error) ||
      !register_dependencies_stage (self, pipeline, context, &error) ||
      !register_build_finish_stage (self, pipeline, context, &error) ||
      !register_build_export_stage (self, pipeline, context, &error) ||
      !register_build_bundle_stage (self, pipeline, context, &error))
    g_warning ("%s", error->message);
}

static void
gbp_flatpak_pipeline_addin_unload (IdePipelineAddin *addin,
                                   IdePipeline      *pipeline)
{
  GbpFlatpakPipelineAddin *self = (GbpFlatpakPipelineAddin *)addin;

  g_assert (GBP_IS_FLATPAK_PIPELINE_ADDIN (self));
  g_assert (IDE_IS_PIPELINE (pipeline));

  g_clear_pointer (&self->state_dir, g_free);
}

static void
pipeline_addin_iface_init (IdePipelineAddinInterface *iface)
{
  iface->load = gbp_flatpak_pipeline_addin_load;
  iface->unload = gbp_flatpak_pipeline_addin_unload;
}

G_DEFINE_FINAL_TYPE_WITH_CODE (GbpFlatpakPipelineAddin, gbp_flatpak_pipeline_addin, IDE_TYPE_OBJECT,
                               G_IMPLEMENT_INTERFACE (IDE_TYPE_PIPELINE_ADDIN, pipeline_addin_iface_init))

static void
gbp_flatpak_pipeline_addin_class_init (GbpFlatpakPipelineAddinClass *klass)
{
}

static void
gbp_flatpak_pipeline_addin_init (GbpFlatpakPipelineAddin *self)
{
}
