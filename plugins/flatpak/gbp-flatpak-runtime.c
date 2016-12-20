/* gb-flatpak-runtime.c
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

#define G_LOG_DOMAIN "gbp-flatpak-runtime"

#include <flatpak.h>
#include <json-glib/json-glib.h>

#include "gbp-flatpak-runtime.h"
#include "gbp-flatpak-subprocess-launcher.h"
#include "gbp-flatpak-runner.h"

struct _GbpFlatpakRuntime
{
  IdeRuntime parent_instance;

  gchar *app_id;
  gchar *branch;
  gchar *deploy_dir;
  gchar *platform;
  gchar *primary_module;
  gchar *sdk;
  GFile *deploy_dir_files;
  GFile *manifest;
};

G_DEFINE_TYPE (GbpFlatpakRuntime, gbp_flatpak_runtime, IDE_TYPE_RUNTIME)

enum {
  PROP_0,
  PROP_APP_ID,
  PROP_BRANCH,
  PROP_DEPLOY_DIR,
  PROP_MANIFEST,
  PROP_PLATFORM,
  PROP_PRIMARY_MODULE,
  PROP_SDK,
  N_PROPS
};

static GParamSpec *properties [N_PROPS];

static gchar *
get_build_directory (GbpFlatpakRuntime *self)
{
  IdeContext *context;
  IdeProject *project;

  g_assert (GBP_IS_FLATPAK_RUNTIME (self));

  context = ide_object_get_context (IDE_OBJECT (self));
  project = ide_context_get_project (context);

  return g_build_filename (g_get_user_cache_dir (),
                           "gnome-builder",
                           "builds",
                           ide_project_get_id (project),
                           "flatpak",
                           ide_runtime_get_id (IDE_RUNTIME (self)),
                           NULL);
}

static gboolean
gbp_flatpak_runtime_contains_program_in_path (IdeRuntime   *runtime,
                                              const gchar  *program,
                                              GCancellable *cancellable)
{
  g_autoptr(IdeSubprocessLauncher) launcher = NULL;
  g_autoptr(IdeSubprocess) subprocess = NULL;

  g_assert (IDE_IS_RUNTIME (runtime));
  g_assert (program != NULL);
  g_assert (!cancellable || G_IS_CANCELLABLE (cancellable));

  launcher = ide_runtime_create_launcher (runtime, 0);

  ide_subprocess_launcher_push_argv (launcher, "which");
  ide_subprocess_launcher_push_argv (launcher, program);

  subprocess = ide_subprocess_launcher_spawn (launcher, cancellable, NULL);

  return (subprocess != NULL) && ide_subprocess_wait_check (subprocess, cancellable, NULL);
}

/**
 * manifest_has_multiple_modules:
 *
 * Searches a #JsonObject to see if it has more than one
 * element in a "modules" list.
 */
static gboolean
manifest_has_multiple_modules (JsonObject *object)
{
  JsonArray *modules;
  guint num_modules;

  modules = json_object_get_array_member (object, "modules");
  if (modules == NULL)
    return FALSE;

  num_modules = json_array_get_length (modules);
  if (num_modules > 1)
      return TRUE;
  else if (num_modules == 0)
      return FALSE;
  else
    {
      object = json_array_get_object_element (modules, 0);
      if (json_object_has_member (object, "modules"))
        {
          modules = json_object_get_array_member (object, "modules");
          if (modules == NULL)
            return FALSE;
          return (json_array_get_length (modules) > 0);
        }
      return FALSE;
    }
}

static void
gbp_flatpak_runtime_prebuild_worker (GTask        *task,
                                     gpointer      source_object,
                                     gpointer      task_data,
                                     GCancellable *cancellable)
{
  GbpFlatpakRuntime *self = source_object;
  IdeBuildResult *build_result = (IdeBuildResult *)task_data;
  IdeContext *context;
  IdeConfigurationManager *config_manager;
  IdeConfiguration *configuration;
  IdeRuntimeManager *runtime_manager;
  const gchar *flatpak_repo_name = NULL;
  gboolean already_ran_build_init = FALSE;
  g_autofree gchar *build_path = NULL;
  g_autofree gchar *flatpak_repo_path = NULL;
  g_autofree gchar *metadata_path = NULL;
  g_autoptr(GFile) build_dir = NULL;
  g_autoptr(GFile) flatpak_repo_dir = NULL;
  g_autoptr(GFile) metadata_file = NULL;
  g_autoptr(IdeSubprocessLauncher) launcher = NULL;
  g_autoptr(IdeSubprocess) process = NULL;
  g_autoptr(GError) error = NULL;

  g_assert (G_IS_TASK (task));
  g_assert (GBP_IS_FLATPAK_RUNTIME (self));
  g_assert (!cancellable || G_IS_CANCELLABLE (cancellable));
  g_assert (IDE_IS_BUILD_RESULT (build_result));

  build_path = get_build_directory (self);
  build_dir = g_file_new_for_path (build_path);

  if (!g_file_query_exists (build_dir, cancellable))
    {
      if (!g_file_make_directory_with_parents (build_dir, cancellable, &error))
        {
          g_task_return_error (task, g_steal_pointer (&error));
          return;
        }
    }

  context = ide_object_get_context (IDE_OBJECT (self));
  config_manager = ide_context_get_configuration_manager (context);
  configuration = ide_configuration_manager_get_current (config_manager);
  runtime_manager = ide_context_get_runtime_manager (context);

  g_assert (IDE_IS_CONFIGURATION (configuration));
  g_assert (IDE_IS_RUNTIME_MANAGER (runtime_manager));

  /* Make sure there's a local flatpak repo we can use to export the build */
  flatpak_repo_path = g_build_filename (g_get_user_cache_dir (),
                                        "gnome-builder",
                                        "flatpak-repo",
                                        NULL);
  flatpak_repo_dir = g_file_new_for_path (flatpak_repo_path);
  if (!g_file_query_exists (flatpak_repo_dir, cancellable))
    {
      if (!g_file_make_directory_with_parents (flatpak_repo_dir, cancellable, &error))
        {
          g_task_return_error (task, g_steal_pointer (&error));
          return;
        }
    }

  launcher = IDE_RUNTIME_CLASS (gbp_flatpak_runtime_parent_class)->create_launcher (IDE_RUNTIME (self), &error);
  if (launcher == NULL)
    {
      g_task_return_error (task, g_steal_pointer (&error));
      return;
    }
  ide_subprocess_launcher_push_argv (launcher, "flatpak");
  ide_subprocess_launcher_push_argv (launcher, "remote-add");
  ide_subprocess_launcher_push_argv (launcher, "--user");
  ide_subprocess_launcher_push_argv (launcher, "--no-gpg-verify");
  ide_subprocess_launcher_push_argv (launcher, "--if-not-exists");
  flatpak_repo_name = ide_configuration_get_internal_string (configuration, "flatpak-repo-name");
  g_assert (!ide_str_empty0 (flatpak_repo_name));
  ide_subprocess_launcher_push_argv (launcher, flatpak_repo_name);
  ide_subprocess_launcher_push_argv (launcher, flatpak_repo_path);
  process = ide_subprocess_launcher_spawn (launcher, cancellable, &error);

  if (process == NULL)
    {
      g_task_return_error (task, g_steal_pointer (&error));
      return;
    }
  ide_build_result_log_subprocess (build_result, process);
  if (!ide_subprocess_wait_check (process, cancellable, &error))
    {
      g_task_return_error (task, g_steal_pointer (&error));
      return;
    }

  ide_configuration_set_internal_object (configuration, "flatpak-repo-dir", flatpak_repo_dir);

  /* Check if flatpak build-init has been run by checking for the metadata file */
  metadata_path = g_build_filename (build_path, "metadata", NULL);
  metadata_file = g_file_new_for_path (metadata_path);
  g_assert (metadata_file != NULL);
  if (g_file_query_exists (metadata_file, cancellable))
    already_ran_build_init = TRUE;

  /*
   * Install the runtime and sdk if they're just the standard gnome ones,
   * and run flatpak-builder if necessary.
   */
  if (self->manifest != NULL)
    {
      gchar *manifest_path;
      g_autoptr(JsonParser) parser = NULL;
      JsonNode *root_node = NULL;
      JsonObject *root_object = NULL;
      gboolean has_multiple_modules;

      manifest_path = g_file_get_path (self->manifest);
      g_assert (!ide_str_empty0 (manifest_path));

      parser = json_parser_new ();
      if (!json_parser_load_from_file (parser, manifest_path, &error))
        {
          g_task_return_error (task, g_steal_pointer (&error));
          return;
        }
      root_node = json_parser_get_root (parser);
      g_assert (JSON_NODE_HOLDS_OBJECT (root_node));
      root_object = json_node_get_object (root_node);
      has_multiple_modules = manifest_has_multiple_modules (root_object);

      if (g_strcmp0 (self->platform, "org.gnome.Platform") == 0 ||
          g_strcmp0 (self->sdk, "org.gnome.Sdk") == 0)
        {
          gchar *gnome_repo_name = NULL;
          gchar *gnome_repo_path = NULL;
          const gchar *arch = NULL;
          g_autofree gchar *runtime_id = NULL;
          g_autofree gchar *sdk_id = NULL;
          IdeRuntime *runtime;
          IdeRuntime *sdk;

          arch = flatpak_get_default_arch ();

          runtime_id = g_strdup_printf ("flatpak:%s/%s/%s", self->platform, self->branch, arch);
          sdk_id = g_strdup_printf ("flatpak:%s/%s/%s", self->sdk, self->branch, arch);
          runtime = ide_runtime_manager_get_runtime (runtime_manager, runtime_id);
          sdk = ide_runtime_manager_get_runtime (runtime_manager, sdk_id);

          /* Add the gnome or gnome-nightly remote */
          if (runtime == NULL || sdk == NULL)
            {
              g_autoptr(IdeSubprocessLauncher) launcher2 = NULL;
              g_autoptr(IdeSubprocess) process2 = NULL;

              if (g_strcmp0 (self->branch, "master") == 0)
                {
                  gnome_repo_name = "gnome-nightly";
                  gnome_repo_path = "https://sdk.gnome.org/gnome-nightly.flatpakrepo";
                }
              else
                {
                  gnome_repo_name = "gnome";
                  gnome_repo_path = "https://sdk.gnome.org/gnome.flatpakrepo";
                }

              launcher2 = IDE_RUNTIME_CLASS (gbp_flatpak_runtime_parent_class)->create_launcher (IDE_RUNTIME (self), &error);
              if (launcher2 == NULL)
                {
                  g_task_return_error (task, g_steal_pointer (&error));
                  return;
                }
              ide_subprocess_launcher_push_argv (launcher2, "flatpak");
              ide_subprocess_launcher_push_argv (launcher2, "remote-add");
              ide_subprocess_launcher_push_argv (launcher2, "--user");
              ide_subprocess_launcher_push_argv (launcher2, "--if-not-exists");
              ide_subprocess_launcher_push_argv (launcher2, "--from");
              ide_subprocess_launcher_push_argv (launcher2, gnome_repo_name);
              ide_subprocess_launcher_push_argv (launcher2, gnome_repo_path);
              ide_build_result_log_stderr (build_result,
                                           "Adding missing flatpak repository %s from %s\n",
                                           gnome_repo_name, gnome_repo_path);
              process2 = ide_subprocess_launcher_spawn (launcher2, cancellable, &error);

              if (process2 != NULL)
                ide_build_result_log_subprocess (build_result, process2);

              if (process2 == NULL || !ide_subprocess_wait_check (process2, cancellable, &error))
                {
                  g_task_return_error (task, g_steal_pointer (&error));
                  return;
                }
            }

          /* Install the runtime */
          if (runtime == NULL && g_strcmp0 (self->platform, "org.gnome.Platform") == 0)
            {
              g_autoptr(IdeSubprocessLauncher) launcher3 = NULL;
              g_autoptr(IdeSubprocess) process3 = NULL;

              launcher3 = IDE_RUNTIME_CLASS (gbp_flatpak_runtime_parent_class)->create_launcher (IDE_RUNTIME (self), &error);
              if (launcher3 == NULL)
                {
                  g_task_return_error (task, g_steal_pointer (&error));
                  return;
                }
              ide_subprocess_launcher_push_argv (launcher3, "flatpak");
              ide_subprocess_launcher_push_argv (launcher3, "install");
              ide_subprocess_launcher_push_argv (launcher3, "--user");
              ide_subprocess_launcher_push_argv (launcher3, "--runtime");
              ide_subprocess_launcher_push_argv (launcher3, gnome_repo_name);
              ide_subprocess_launcher_push_argv (launcher3, self->platform);
              ide_subprocess_launcher_push_argv (launcher3, self->branch);
              ide_build_result_log_stderr (build_result,
                                           "Installing missing flatpak runtime %s (%s)\n",
                                           self->platform, self->branch);
              process3 = ide_subprocess_launcher_spawn (launcher3, cancellable, &error);

              if (process3 != NULL)
                ide_build_result_log_subprocess (build_result, process3);

              if (process3 == NULL || !ide_subprocess_wait_check (process3, cancellable, &error))
                {
                  g_task_return_error (task, g_steal_pointer (&error));
                  return;
                }
            }

          /* Install the sdk */
          if (sdk == NULL && g_strcmp0 (self->sdk, "org.gnome.Sdk") == 0)
            {
              g_autoptr(IdeSubprocessLauncher) launcher4 = NULL;
              g_autoptr(IdeSubprocess) process4 = NULL;

              launcher4 = IDE_RUNTIME_CLASS (gbp_flatpak_runtime_parent_class)->create_launcher (IDE_RUNTIME (self), &error);
              if (launcher4 == NULL)
                {
                  g_task_return_error (task, g_steal_pointer (&error));
                  return;
                }
              ide_subprocess_launcher_push_argv (launcher4, "flatpak");
              ide_subprocess_launcher_push_argv (launcher4, "install");
              ide_subprocess_launcher_push_argv (launcher4, "--user");
              ide_subprocess_launcher_push_argv (launcher4, "--runtime");
              ide_subprocess_launcher_push_argv (launcher4, gnome_repo_name);
              ide_subprocess_launcher_push_argv (launcher4, self->sdk);
              ide_subprocess_launcher_push_argv (launcher4, self->branch);
              ide_build_result_log_stderr (build_result,
                                           "Installing missing flatpak SDK %s (%s)\n",
                                           self->sdk, self->branch);
              process4 = ide_subprocess_launcher_spawn (launcher4, cancellable, &error);

              if (process4 != NULL)
                ide_build_result_log_subprocess (build_result, process4);

              if (process4 == NULL || !ide_subprocess_wait_check (process4, cancellable, &error))
                {
                  g_task_return_error (task, g_steal_pointer (&error));
                  return;
                }
            }
        }

      /* No need to run flatpak-builder if there are no dependencies */
      if (has_multiple_modules)
        {
          g_autoptr(IdeSubprocessLauncher) launcher5 = NULL;
          g_autoptr(IdeSubprocess) process5 = NULL;
          g_autoptr(GFile) success_file = NULL;
          g_autofree gchar *stop_at_option = NULL;
          g_autofree gchar *success_filename = NULL;

          success_filename = g_build_filename (build_path, "flatpak-builder-success", NULL);
          success_file = g_file_new_for_path (success_filename);
          if (g_file_query_exists (success_file, cancellable))
            {
              g_task_return_boolean (task, TRUE);
              return;
            }

          /* Run flatpak-builder to build just the dependencies */
          launcher5 = IDE_RUNTIME_CLASS (gbp_flatpak_runtime_parent_class)->create_launcher (IDE_RUNTIME (self), &error);
          if (launcher5 == NULL)
            {
              g_task_return_error (task, g_steal_pointer (&error));
              return;
            }
          ide_subprocess_launcher_push_argv (launcher5, "flatpak-builder");
          ide_subprocess_launcher_push_argv (launcher5, "--ccache");
          ide_subprocess_launcher_push_argv (launcher5, "--force-clean");
          stop_at_option = g_strdup_printf ("--stop-at=%s", self->primary_module);
          ide_subprocess_launcher_push_argv (launcher5, stop_at_option);
          ide_subprocess_launcher_push_argv (launcher5, build_path);
          ide_subprocess_launcher_push_argv (launcher5, manifest_path);
          process5 = ide_subprocess_launcher_spawn (launcher5, cancellable, &error);

          if (process5 == NULL)
            {
              g_task_return_error (task, g_steal_pointer (&error));
              return;
            }
          ide_build_result_log_subprocess (build_result, process5);
          if (!ide_subprocess_wait_check (process5, cancellable, &error))
            {
              g_task_return_error (task, g_steal_pointer (&error));
              return;
            }

          /*
           * Make a file indicating that flatpak-builder finished successfully,
           * so we know whether to run it for the next build.
           */
          g_object_unref (g_file_create (success_file, 0, cancellable, NULL));

          g_task_return_boolean (task, TRUE);
          return;
        }
    }

  /* Run flatpak build-init */
  if (!already_ran_build_init)
    {
      const gchar *app_id = NULL;
      g_autoptr(IdeSubprocessLauncher) launcher6 = NULL;
      g_autoptr(IdeSubprocess) process6 = NULL;

      app_id = self->app_id;
      if (ide_str_empty0 (app_id))
        {
          g_warning ("Could not determine application ID");
          app_id = "org.gnome.FlatpakApp";
        }

      launcher6 = IDE_RUNTIME_CLASS (gbp_flatpak_runtime_parent_class)->create_launcher (IDE_RUNTIME (self), &error);
      if (launcher6 == NULL)
        {
          g_task_return_error (task, g_steal_pointer (&error));
          return;
        }
      ide_subprocess_launcher_push_argv (launcher6, "flatpak");
      ide_subprocess_launcher_push_argv (launcher6, "build-init");
      ide_subprocess_launcher_push_argv (launcher6, build_path);
      ide_subprocess_launcher_push_argv (launcher6, app_id);
      ide_subprocess_launcher_push_argv (launcher6, self->sdk);
      ide_subprocess_launcher_push_argv (launcher6, self->platform);
      ide_subprocess_launcher_push_argv (launcher6, self->branch);
      process6 = ide_subprocess_launcher_spawn (launcher6, cancellable, &error);

      if (process6 == NULL)
        {
          g_task_return_error (task, g_steal_pointer (&error));
          return;
        }
      ide_build_result_log_subprocess (build_result, process6);
      if (!ide_subprocess_wait_check (process6, cancellable, &error))
        {
          g_task_return_error (task, g_steal_pointer (&error));
          return;
        }
    }

  g_task_return_boolean (task, TRUE);
}

static void
gbp_flatpak_runtime_prebuild_async (IdeRuntime          *runtime,
                                    IdeBuildResult      *build_result,
                                    GCancellable        *cancellable,
                                    GAsyncReadyCallback  callback,
                                    gpointer             user_data)
{
  GbpFlatpakRuntime *self = (GbpFlatpakRuntime *)runtime;
  g_autoptr(GTask) task = NULL;

  g_assert (GBP_IS_FLATPAK_RUNTIME (self));
  g_assert (IDE_IS_BUILD_RESULT (build_result));
  g_assert (!cancellable || G_IS_CANCELLABLE (cancellable));

  task = g_task_new (self, cancellable, callback, user_data);
  g_task_set_task_data (task, g_object_ref (build_result), (GDestroyNotify)g_object_unref);
  g_task_run_in_thread (task, gbp_flatpak_runtime_prebuild_worker);
}

static gboolean
gbp_flatpak_runtime_prebuild_finish (IdeRuntime    *runtime,
                                     GAsyncResult  *result,
                                     GError       **error)
{
  GbpFlatpakRuntime *self = (GbpFlatpakRuntime *)runtime;

  g_assert (GBP_IS_FLATPAK_RUNTIME (self));
  g_assert (G_IS_TASK (result));

  return g_task_propagate_boolean (G_TASK (result), error);
}

static void
gbp_flatpak_runtime_postinstall_worker (GTask        *task,
                                        gpointer      source_object,
                                        gpointer      task_data,
                                        GCancellable *cancellable)
{
  GbpFlatpakRuntime *self = source_object;
  IdeBuildResult *build_result = (IdeBuildResult *)task_data;
  IdeContext *context;
  IdeConfigurationManager *config_manager;
  IdeConfiguration *configuration;
  const gchar *repo_name = NULL;
  const gchar *app_id = NULL;
  g_autofree gchar *repo_path = NULL;
  g_autofree gchar *build_path = NULL;
  g_autofree gchar *manifest_path = NULL;
  g_autofree gchar *export_path = NULL;
  g_autoptr(GFile) export_dir = NULL;
  g_autoptr(IdeSubprocessLauncher) launcher2 = NULL;
  g_autoptr(IdeSubprocessLauncher) launcher3 = NULL;
  g_autoptr(IdeSubprocessLauncher) launcher4 = NULL;
  g_autoptr(IdeSubprocess) process2 = NULL;
  g_autoptr(IdeSubprocess) process3 = NULL;
  g_autoptr(IdeSubprocess) process4 = NULL;
  g_autoptr(GError) error = NULL;

  g_assert (G_IS_TASK (task));
  g_assert (GBP_IS_FLATPAK_RUNTIME (self));
  g_assert (IDE_IS_BUILD_RESULT (build_result));
  g_assert (!cancellable || G_IS_CANCELLABLE (cancellable));

  context = ide_object_get_context (IDE_OBJECT (self));
  config_manager = ide_context_get_configuration_manager (context);
  configuration = ide_configuration_manager_get_current (config_manager);

  build_path = get_build_directory (self);
  repo_name = ide_configuration_get_internal_string (configuration, "flatpak-repo-name");
  repo_path = g_file_get_path (ide_configuration_get_internal_object (configuration, "flatpak-repo-dir"));

  g_assert (!ide_str_empty0 (repo_name));
  g_assert (!ide_str_empty0 (repo_path));

  /* Check if flatpak build-finish has already been run by checking for the export directory */
  export_path = g_build_filename (build_path, "export", NULL);
  export_dir = g_file_new_for_path (export_path);
  g_assert (export_dir != NULL);
  if (!g_file_query_exists (export_dir, cancellable))
    {
      g_autoptr(IdeSubprocessLauncher) launcher = NULL;
      g_autoptr(IdeSubprocess) process = NULL;
      JsonArray *finish_args = NULL;
      const gchar *command = NULL;
      JsonParser *parser = NULL;

      /* Attempt to parse the flatpak manifest */
      if (self->manifest != NULL && (manifest_path = g_file_get_path (self->manifest)))
        {
          GError *json_error = NULL;
          JsonObject *root_object;

          parser = json_parser_new ();
          json_parser_load_from_file (parser, manifest_path, &json_error);
          if (json_error)
            g_debug ("Error parsing flatpak manifest %s: %s", manifest_path, json_error->message);
          else
            {
              root_object = json_node_get_object (json_parser_get_root (parser));
              if (root_object != NULL)
                {
                  if (json_object_has_member (root_object, "command"))
                    command = json_object_get_string_member (root_object, "command");
                  if (json_object_has_member (root_object, "finish-args"))
                    finish_args = json_object_get_array_member (root_object, "finish-args");
                }
            }
        }

      /* Finalize the build directory */
      launcher = IDE_RUNTIME_CLASS (gbp_flatpak_runtime_parent_class)->create_launcher (IDE_RUNTIME (self), &error);
      if (launcher == NULL)
        {
          g_task_return_error (task, g_steal_pointer (&error));
          return;
        }
      ide_subprocess_launcher_push_argv (launcher, "flatpak");
      ide_subprocess_launcher_push_argv (launcher, "build-finish");
      if (!ide_str_empty0 (command))
        {
          g_autofree gchar *command_option = NULL;
          command_option = g_strdup_printf ("--command=%s", command);
          ide_subprocess_launcher_push_argv (launcher, command_option);
        }
      if (finish_args != NULL)
        {
          for (guint i = 0; i < json_array_get_length (finish_args); i++)
            {
              const gchar *arg;
              arg = json_array_get_string_element (finish_args, i);
              if (!ide_str_empty0 (arg))
                ide_subprocess_launcher_push_argv (launcher, arg);
            }
        }
      ide_subprocess_launcher_push_argv (launcher, build_path);

      g_clear_object (&parser);

      process = ide_subprocess_launcher_spawn (launcher, cancellable, &error);

      if (process == NULL)
        {
          g_task_return_error (task, g_steal_pointer (&error));
          return;
        }
      ide_build_result_log_subprocess (build_result, process);
      if (!ide_subprocess_wait_check (process, cancellable, &error))
        {
          g_task_return_error (task, g_steal_pointer (&error));
          return;
        }
    }

  /* Export the build to the repo */
  launcher2 = IDE_RUNTIME_CLASS (gbp_flatpak_runtime_parent_class)->create_launcher (IDE_RUNTIME (self), &error);
  if (launcher2 == NULL)
    {
      g_task_return_error (task, g_steal_pointer (&error));
      return;
    }
  ide_subprocess_launcher_push_argv (launcher2, "flatpak");
  ide_subprocess_launcher_push_argv (launcher2, "build-export");
  ide_subprocess_launcher_push_argv (launcher2, "--subject=\"Development build\"");
  ide_subprocess_launcher_push_argv (launcher2, repo_path);
  ide_subprocess_launcher_push_argv (launcher2, build_path);
  process2 = ide_subprocess_launcher_spawn (launcher2, cancellable, &error);

  if (process2 == NULL)
    {
      g_task_return_error (task, g_steal_pointer (&error));
      return;
    }
  ide_build_result_log_subprocess (build_result, process2);
  if (!ide_subprocess_wait_check (process2, cancellable, &error))
    {
      g_task_return_error (task, g_steal_pointer (&error));
      return;
    }

  app_id = self->app_id;
  if (ide_str_empty0 (app_id))
    {
      g_warning ("Could not determine application ID");
      app_id = "org.gnome.FlatpakApp";
    }
  /* Try to uninstall it in case this isn't the first run */
  launcher3 = IDE_RUNTIME_CLASS (gbp_flatpak_runtime_parent_class)->create_launcher (IDE_RUNTIME (self), &error);
  if (launcher3 == NULL)
    {
      g_task_return_error (task, g_steal_pointer (&error));
      return;
    }
  ide_subprocess_launcher_push_argv (launcher3, "flatpak");
  ide_subprocess_launcher_push_argv (launcher3, "uninstall");
  ide_subprocess_launcher_push_argv (launcher3, "--user");
  ide_subprocess_launcher_push_argv (launcher3, app_id);
  process3 = ide_subprocess_launcher_spawn (launcher3, cancellable, &error);

  if (process3 == NULL)
    {
      g_task_return_error (task, g_steal_pointer (&error));
      return;
    }
  ide_subprocess_wait (process3, cancellable, NULL);

  /* Finally install the app */
  launcher4 = IDE_RUNTIME_CLASS (gbp_flatpak_runtime_parent_class)->create_launcher (IDE_RUNTIME (self), &error);
  if (launcher4 == NULL)
    {
      g_task_return_error (task, g_steal_pointer (&error));
      return;
    }
  ide_subprocess_launcher_push_argv (launcher4, "flatpak");
  ide_subprocess_launcher_push_argv (launcher4, "install");
  ide_subprocess_launcher_push_argv (launcher4, "--user");
  ide_subprocess_launcher_push_argv (launcher4, "--app");
  ide_subprocess_launcher_push_argv (launcher4, "--no-deps");
  ide_subprocess_launcher_push_argv (launcher4, repo_name);
  ide_subprocess_launcher_push_argv (launcher4, app_id);

  process4 = ide_subprocess_launcher_spawn (launcher4, cancellable, &error);

  if (process4 == NULL)
    {
      g_task_return_error (task, g_steal_pointer (&error));
      return;
    }
  ide_build_result_log_subprocess (build_result, process4);
  if (!ide_subprocess_wait_check (process4, cancellable, &error))
    {
      g_task_return_error (task, g_steal_pointer (&error));
      return;
    }

  g_task_return_boolean (task, TRUE);
}

static void
gbp_flatpak_runtime_postinstall_async (IdeRuntime          *runtime,
                                       IdeBuildResult      *build_result,
                                       GCancellable        *cancellable,
                                       GAsyncReadyCallback  callback,
                                       gpointer             user_data)
{
  GbpFlatpakRuntime *self = (GbpFlatpakRuntime *)runtime;
  g_autoptr(GTask) task = NULL;

  g_assert (GBP_IS_FLATPAK_RUNTIME (self));
  g_assert (IDE_IS_BUILD_RESULT (build_result));
  g_assert (!cancellable || G_IS_CANCELLABLE (cancellable));

  task = g_task_new (self, cancellable, callback, user_data);
  g_task_set_task_data (task, g_object_ref (build_result), (GDestroyNotify)g_object_unref);
  g_task_run_in_thread (task, gbp_flatpak_runtime_postinstall_worker);
}

static gboolean
gbp_flatpak_runtime_postinstall_finish (IdeRuntime    *runtime,
                                        GAsyncResult  *result,
                                        GError       **error)
{
  GbpFlatpakRuntime *self = (GbpFlatpakRuntime *)runtime;

  g_assert (GBP_IS_FLATPAK_RUNTIME (self));
  g_assert (G_IS_TASK (result));

  return g_task_propagate_boolean (G_TASK (result), error);
}

static IdeSubprocessLauncher *
gbp_flatpak_runtime_create_launcher (IdeRuntime  *runtime,
                                     GError     **error)
{
  IdeSubprocessLauncher *ret;
  GbpFlatpakRuntime *self = (GbpFlatpakRuntime *)runtime;

  g_return_val_if_fail (GBP_IS_FLATPAK_RUNTIME (self), NULL);

  ret = gbp_flatpak_subprocess_launcher_new (G_SUBPROCESS_FLAGS_STDOUT_PIPE | G_SUBPROCESS_FLAGS_STDERR_PIPE);

  if (ret != NULL)
    {
      g_autofree gchar *build_path = NULL;
      g_autofree gchar *manifest_path = NULL;
      gchar *project_path;
      gchar *project_name;
      const gchar *cflags = NULL;
      const gchar *cxxflags = NULL;
      JsonObject *env_vars = NULL;
      JsonParser *parser = NULL;
      g_autoptr(GFileInfo) file_info = NULL;
      IdeContext *context;
      GFile *project_file;

      build_path = get_build_directory (self);

      /* Attempt to parse the flatpak manifest */
      if (self->manifest != NULL && (manifest_path = g_file_get_path (self->manifest)))
        {
          GError *json_error = NULL;
          JsonObject *root_object;

          parser = json_parser_new ();
          json_parser_load_from_file (parser, manifest_path, &json_error);
          if (json_error)
            g_debug ("Error parsing flatpak manifest %s: %s", manifest_path, json_error->message);
          else
            {
              root_object = json_node_get_object (json_parser_get_root (parser));
              if (root_object != NULL && json_object_has_member (root_object, "build-options"))
                {
                  JsonObject *build_options = NULL;
                  build_options = json_object_get_object_member (root_object, "build-options");
                  if (build_options != NULL)
                    {
                      if (json_object_has_member (build_options, "cflags"))
                        cflags = json_object_get_string_member (build_options, "cflags");
                      if (json_object_has_member (build_options, "cxxflags"))
                        cxxflags = json_object_get_string_member (build_options, "cxxflags");
                      if (json_object_has_member (build_options, "env"))
                        env_vars = json_object_get_object_member (build_options, "env");
                    }
                }
            }
        }

      /* Find the project directory path */
      context = ide_object_get_context (IDE_OBJECT (self));
      project_file = ide_context_get_project_file (context);
      if (project_file != NULL)
        {
          if (g_file_test (g_file_get_path (project_file), G_FILE_TEST_IS_DIR))
            project_path = g_file_get_path (project_file);
          else
            {
              g_autoptr(GFile) project_dir = NULL;
              project_dir = g_file_get_parent (project_file);
              project_path = g_file_get_path (project_dir);
              project_name = g_file_get_basename (project_dir);
            }
        }

      /* Add 'flatpak build' and the specified arguments to the launcher */
      ide_subprocess_launcher_push_argv (ret, "flatpak");
      ide_subprocess_launcher_push_argv (ret, "build");
      ide_subprocess_launcher_push_argv (ret, "--share=network");
      if (!ide_str_empty0 (project_path))
        {
          g_autofree gchar *filesystem_option = NULL;
          g_autofree gchar *bind_mount_option = NULL;
          g_autofree gchar *build_dir_option = NULL;
          filesystem_option = g_strdup_printf ("--filesystem=%s", project_path);
          bind_mount_option = g_strdup_printf ("--bind-mount=/run/build/%s=%s", project_name, project_path);
          build_dir_option = g_strdup_printf ("--build-dir=/run/build/%s", project_name);
          ide_subprocess_launcher_push_argv (ret, "--nofilesystem=host");
          ide_subprocess_launcher_push_argv (ret, filesystem_option);
          ide_subprocess_launcher_push_argv (ret, bind_mount_option);
          ide_subprocess_launcher_push_argv (ret, build_dir_option);
        }
      if (env_vars != NULL)
        {
          g_autoptr(GList) env_list = NULL;
          GList *l;
          env_list = json_object_get_members (env_vars);
          for (l = env_list; l != NULL; l = l->next)
            {
              const gchar *env_name = (gchar *)l->data;
              const gchar *env_value = json_object_get_string_member (env_vars, env_name);
              if (!ide_str_empty0 (env_name) && !ide_str_empty0 (env_value))
                {
                  g_autofree gchar *env_option = NULL;
                  env_option = g_strdup_printf ("--env=%s=%s", env_name, env_value);
                  ide_subprocess_launcher_push_argv (ret, env_option);
                }
            }
        }
      if (!ide_str_empty0 (cflags))
        {
          g_autofree gchar *cflags_option = NULL;
          cflags_option = g_strdup_printf ("--env=CFLAGS=%s", cflags);
          ide_subprocess_launcher_push_argv (ret, cflags_option);
        }
      if (!ide_str_empty0 (cxxflags))
        {
          g_autofree gchar *cxxflags_option = NULL;
          cxxflags_option = g_strdup_printf ("--env=CXXFLAGS=%s", cxxflags);
          ide_subprocess_launcher_push_argv (ret, cxxflags_option);
        }

      /* We want the configure step to be separate so IdeAutotoolsBuildTask can pass options to it */
      ide_subprocess_launcher_push_argv (ret, "--env=NOCONFIGURE=1");

      ide_subprocess_launcher_push_argv (ret, build_path);

      ide_subprocess_launcher_set_run_on_host (ret, TRUE);

      g_clear_object (&parser);
    }

  return ret;
}

IdeRunner *
gbp_flatpak_runtime_create_runner (IdeRuntime     *runtime,
                                   IdeBuildTarget *build_target)
{
  GbpFlatpakRuntime *self = (GbpFlatpakRuntime *)runtime;
  IdeContext *context;
  IdeConfigurationManager *config_manager;
  IdeConfiguration *configuration;
  GbpFlatpakRunner *runner;
  const gchar *app_id = NULL;
  const gchar *config_app_id = NULL;
  g_autofree gchar *own_name = NULL;
  g_autofree gchar *app_id_override = NULL;

  g_assert (GBP_IS_FLATPAK_RUNTIME (self));
  g_assert (IDE_IS_BUILD_TARGET (build_target));

  context = ide_object_get_context (IDE_OBJECT (self));
  config_manager = ide_context_get_configuration_manager (context);
  configuration = ide_configuration_manager_get_current (config_manager);

  runner = gbp_flatpak_runner_new (context);
  g_assert (GBP_IS_FLATPAK_RUNNER (runner));

  app_id = self->app_id;
  config_app_id = ide_configuration_get_app_id (configuration);
  if (ide_str_empty0 (app_id))
    {
      g_warning ("Could not determine application ID");
      app_id = "org.gnome.FlatpakApp";
    }

  if (g_strcmp0 (app_id, config_app_id) != 0)
    {
      own_name = g_strdup_printf ("--own-name=%s", config_app_id);
      app_id_override = g_strdup_printf ("--gapplication-app-id=%s", config_app_id);
    }

  ide_runner_set_run_on_host (IDE_RUNNER (runner), TRUE);
  ide_runner_append_argv (IDE_RUNNER (runner), "flatpak");
  ide_runner_append_argv (IDE_RUNNER (runner), "run");
  if (own_name != NULL)
    ide_runner_append_argv (IDE_RUNNER (runner), own_name);
  ide_runner_append_argv (IDE_RUNNER (runner), "--share=ipc");
  ide_runner_append_argv (IDE_RUNNER (runner), "--socket=x11");
  ide_runner_append_argv (IDE_RUNNER (runner), "--socket=wayland");
  ide_runner_append_argv (IDE_RUNNER (runner), app_id);
  if (app_id_override)
    ide_runner_append_argv (IDE_RUNNER (runner), app_id_override);

  return IDE_RUNNER (runner);
}

static void
gbp_flatpak_runtime_prepare_configuration (IdeRuntime       *runtime,
                                           IdeConfiguration *configuration)
{
  GbpFlatpakRuntime* self = (GbpFlatpakRuntime *)runtime;

  g_assert (GBP_IS_FLATPAK_RUNTIME (self));
  g_assert (IDE_IS_CONFIGURATION (configuration));

  if (!ide_configuration_get_app_id (configuration))
    {
      if (!ide_str_empty0 (self->app_id))
        ide_configuration_set_app_id (configuration, self->app_id);
    }

  ide_configuration_set_prefix (configuration, "/app");
  ide_configuration_set_internal_string (configuration, "flatpak-repo-name", FLATPAK_REPO_NAME);
}

static void
gbp_flatpak_runtime_set_deploy_dir (GbpFlatpakRuntime *self,
                                    const gchar       *deploy_dir)
{
  g_autoptr(GFile) file = NULL;

  g_assert (GBP_IS_FLATPAK_RUNTIME (self));
  g_assert (self->deploy_dir == NULL);
  g_assert (self->deploy_dir_files == NULL);

  if (deploy_dir != NULL)
    {
      self->deploy_dir = g_strdup (deploy_dir);
      file = g_file_new_for_path (deploy_dir);
      self->deploy_dir_files = g_file_get_child (file, "files");
    }
}

static GFile *
gbp_flatpak_runtime_translate_file (IdeRuntime *runtime,
                                    GFile      *file)
{
  GbpFlatpakRuntime *self = (GbpFlatpakRuntime *)runtime;
  g_autofree gchar *path = NULL;
  g_autofree gchar *build_dir = NULL;
  g_autofree gchar *app_files_path = NULL;

  g_assert (GBP_IS_FLATPAK_RUNTIME (self));
  g_assert (G_IS_FILE (file));

  /*
   * If we have a manifest and the runtime is not yet installed,
   * then we can't do a whole lot right now. We have to wait for
   * the runtime to be installed and a new runtime instance will
   * be loaded.
   */
  if (self->deploy_dir_files == NULL || self->deploy_dir == NULL)
    return NULL;

  if (!g_file_is_native (file))
    return NULL;

  if (NULL == (path = g_file_get_path (file)))
    return NULL;

  if (g_str_equal ("/usr", path))
    return g_object_ref (self->deploy_dir_files);

  if (g_str_has_prefix (path, "/usr/"))
    return g_file_get_child (self->deploy_dir_files, path + IDE_LITERAL_LENGTH ("/usr/"));

  build_dir = get_build_directory (self);
  app_files_path = g_build_filename (build_dir, "files", NULL);

  if (g_str_equal (path, "/app"))
    return g_file_new_for_path (app_files_path);

  if (g_str_has_prefix (path, "/app/"))
    {
      g_autofree gchar *translated = NULL;

      translated = g_build_filename (app_files_path,
                                     path + IDE_LITERAL_LENGTH ("/app/"),
                                     NULL);
      return g_file_new_for_path (translated);
    }

  return NULL;
}

static void
gbp_flatpak_runtime_get_property (GObject    *object,
                                  guint       prop_id,
                                  GValue     *value,
                                  GParamSpec *pspec)
{
  GbpFlatpakRuntime *self = GBP_FLATPAK_RUNTIME(object);

  switch (prop_id)
    {
    case PROP_BRANCH:
      g_value_set_string (value, self->branch);
      break;

    case PROP_PLATFORM:
      g_value_set_string (value, self->platform);
      break;

    case PROP_SDK:
      g_value_set_string (value, self->sdk);
      break;

    case PROP_PRIMARY_MODULE:
      g_value_set_string (value, self->primary_module);
      break;

    case PROP_APP_ID:
      g_value_set_string (value, self->app_id);
      break;

    case PROP_MANIFEST:
      g_value_set_object (value, self->manifest);
      break;

    case PROP_DEPLOY_DIR:
      g_value_set_string (value, self->deploy_dir);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
    }
}

static void
gbp_flatpak_runtime_set_property (GObject      *object,
                                  guint         prop_id,
                                  const GValue *value,
                                  GParamSpec   *pspec)
{
  GbpFlatpakRuntime *self = GBP_FLATPAK_RUNTIME(object);

  switch (prop_id)
    {
    case PROP_BRANCH:
      self->branch = g_value_dup_string (value);
      break;

    case PROP_PLATFORM:
      self->platform = g_value_dup_string (value);
      break;

    case PROP_SDK:
      self->sdk = g_value_dup_string (value);
      break;

    case PROP_PRIMARY_MODULE:
      self->primary_module = g_value_dup_string (value);
      break;

    case PROP_APP_ID:
      self->app_id = g_value_dup_string (value);
      break;

    case PROP_MANIFEST:
      self->manifest = g_value_dup_object (value);
      break;

    case PROP_DEPLOY_DIR:
      gbp_flatpak_runtime_set_deploy_dir (self, g_value_get_string (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
    }
}

static void
gbp_flatpak_runtime_finalize (GObject *object)
{
  GbpFlatpakRuntime *self = (GbpFlatpakRuntime *)object;

  g_clear_pointer (&self->sdk, g_free);
  g_clear_pointer (&self->platform, g_free);
  g_clear_pointer (&self->branch, g_free);
  g_clear_pointer (&self->primary_module, g_free);
  g_clear_pointer (&self->app_id, g_free);
  g_clear_pointer (&self->deploy_dir, g_free);
  g_clear_object (&self->deploy_dir_files);
  g_clear_object (&self->manifest);

  G_OBJECT_CLASS (gbp_flatpak_runtime_parent_class)->finalize (object);
}

static void
gbp_flatpak_runtime_class_init (GbpFlatpakRuntimeClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  IdeRuntimeClass *runtime_class = IDE_RUNTIME_CLASS (klass);

  object_class->finalize = gbp_flatpak_runtime_finalize;
  object_class->get_property = gbp_flatpak_runtime_get_property;
  object_class->set_property = gbp_flatpak_runtime_set_property;

  runtime_class->prebuild_async = gbp_flatpak_runtime_prebuild_async;
  runtime_class->prebuild_finish = gbp_flatpak_runtime_prebuild_finish;
  runtime_class->postinstall_async = gbp_flatpak_runtime_postinstall_async;
  runtime_class->postinstall_finish = gbp_flatpak_runtime_postinstall_finish;
  runtime_class->create_launcher = gbp_flatpak_runtime_create_launcher;
  runtime_class->create_runner = gbp_flatpak_runtime_create_runner;
  runtime_class->contains_program_in_path = gbp_flatpak_runtime_contains_program_in_path;
  runtime_class->prepare_configuration = gbp_flatpak_runtime_prepare_configuration;
  runtime_class->translate_file = gbp_flatpak_runtime_translate_file;

  properties [PROP_BRANCH] =
    g_param_spec_string ("branch",
                         "Branch",
                         "Branch",
                         "master",
                         (G_PARAM_READWRITE |
                          G_PARAM_CONSTRUCT |
                          G_PARAM_STATIC_STRINGS));

  properties [PROP_DEPLOY_DIR] =
    g_param_spec_string ("deploy-dir",
                         "Deploy Directory",
                         "The flatpak runtime deploy directory",
                         NULL,
                         (G_PARAM_READWRITE |
                          G_PARAM_CONSTRUCT_ONLY |
                          G_PARAM_STATIC_STRINGS));

  properties [PROP_PLATFORM] =
    g_param_spec_string ("platform",
                         "Platform",
                         "Platform",
                         "org.gnome.Platform",
                         (G_PARAM_READWRITE |
                          G_PARAM_CONSTRUCT |
                          G_PARAM_STATIC_STRINGS));

  properties [PROP_SDK] =
    g_param_spec_string ("sdk",
                         "Sdk",
                         "Sdk",
                         "org.gnome.Sdk",
                         (G_PARAM_READWRITE |
                          G_PARAM_CONSTRUCT |
                          G_PARAM_STATIC_STRINGS));

  properties [PROP_PRIMARY_MODULE] =
    g_param_spec_string ("primary-module",
                         "Primary module",
                         "Primary module",
                         NULL,
                         (G_PARAM_READWRITE |
                          G_PARAM_CONSTRUCT |
                          G_PARAM_STATIC_STRINGS));

  properties [PROP_APP_ID] =
    g_param_spec_string ("app-id",
                         "App ID",
                         "App ID",
                         NULL,
                         (G_PARAM_READWRITE |
                          G_PARAM_CONSTRUCT |
                          G_PARAM_STATIC_STRINGS));

  properties [PROP_MANIFEST] =
    g_param_spec_object ("manifest",
                         "Manifest",
                         "Manifest file for use with flatpak-builder",
                         G_TYPE_FILE,
                         (G_PARAM_READWRITE |
                          G_PARAM_CONSTRUCT |
                          G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, N_PROPS, properties);
}

static void
gbp_flatpak_runtime_init (GbpFlatpakRuntime *self)
{
}
