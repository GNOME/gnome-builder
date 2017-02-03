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
get_staging_directory (GbpFlatpakRuntime *self)
{
  IdeContext *context;
  IdeProject *project;

  g_assert (GBP_IS_FLATPAK_RUNTIME (self));

  context = ide_object_get_context (IDE_OBJECT (self));
  project = ide_context_get_project (context);

  return g_build_filename (g_get_user_cache_dir (),
                           "gnome-builder",
                           "flatpak",
                           "staging",
                           ide_project_get_id (project),
                           ide_runtime_get_id (IDE_RUNTIME (self)),
                           NULL);
}

static gboolean
gbp_flatpak_runtime_contains_program_in_path (IdeRuntime   *runtime,
                                              const gchar  *program,
                                              GCancellable *cancellable)
{
  GbpFlatpakRuntime *self = (GbpFlatpakRuntime *)runtime;
  g_autoptr(IdeSubprocessLauncher) launcher = NULL;
  g_autoptr(IdeSubprocess) subprocess = NULL;

  g_assert (GBP_IS_FLATPAK_RUNTIME (self));
  g_assert (program != NULL);
  g_assert (!cancellable || G_IS_CANCELLABLE (cancellable));

  /*
   * To check if a program is available, we don't want to use the normal
   * launcher because it will only be available if the build directory
   * has been created and setup. Instead, we will use flatpak to run the
   * runtime which was added in Flatpak 0.6.13.
   */
  launcher = ide_subprocess_launcher_new (G_SUBPROCESS_FLAGS_NONE);

  ide_subprocess_launcher_set_run_on_host (launcher, TRUE);
  ide_subprocess_launcher_set_clear_env (launcher, FALSE);

  ide_subprocess_launcher_push_argv (launcher, "flatpak");
  ide_subprocess_launcher_push_argv (launcher, "run");
  ide_subprocess_launcher_push_argv (launcher, "--command=which");
  ide_subprocess_launcher_push_argv (launcher, self->sdk);
  ide_subprocess_launcher_push_argv (launcher, program);

  subprocess = ide_subprocess_launcher_spawn (launcher, cancellable, NULL);

  if (subprocess != NULL)
    return ide_subprocess_wait_check (subprocess, cancellable, NULL);

  return FALSE;
}

static const gchar *
get_builddir (GbpFlatpakRuntime *self)
{
  IdeContext *context = ide_object_get_context (IDE_OBJECT (self));
  IdeBuildManager *build_manager = ide_context_get_build_manager (context);
  IdeBuildPipeline *pipeline = ide_build_manager_get_pipeline (build_manager);
  const gchar *builddir = ide_build_pipeline_get_builddir (pipeline);

  return builddir;
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
      g_autofree gchar *manifest_path = NULL;
      g_autofree gchar *project_name = NULL;
      g_autofree gchar *project_path = NULL;
      g_autofree gchar *build_path = NULL;
      const gchar *builddir = NULL;
      const gchar *cflags = NULL;
      const gchar *cxxflags = NULL;
      JsonObject *env_vars = NULL;
      JsonParser *parser = NULL;
      g_autoptr(GFileInfo) file_info = NULL;
      IdeContext *context;
      GFile *project_file;

      build_path = get_staging_directory (self);
      builddir = get_builddir (self);

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
          g_autofree gchar *project_file_path = NULL;
          project_file_path = g_file_get_path (project_file);
          if (g_file_test (project_file_path, G_FILE_TEST_IS_DIR))
            {
              project_path = g_file_get_path (project_file);
              project_name = g_file_get_basename (project_file);
            }
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
          bind_mount_option = g_strdup_printf ("--bind-mount=/run/build/%s=%s", project_name, builddir);
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
  g_autofree gchar *manifest_path = NULL;

  g_assert (GBP_IS_FLATPAK_RUNTIME (self));
  g_assert (IDE_IS_CONFIGURATION (configuration));

  if (!ide_configuration_get_app_id (configuration))
    {
      if (!ide_str_empty0 (self->app_id))
        ide_configuration_set_app_id (configuration, self->app_id);
    }

  if (self->manifest != NULL)
    manifest_path = g_file_get_path (self->manifest);

  ide_configuration_set_prefix (configuration, "/app");

  /*
   * TODO: Move this to a GbpFlatpakConfiguration
   *
   * Parse some stuff to use later when building.
   * This really belongs in an IdeConfiguration subclass.
   */

  ide_configuration_set_internal_string (configuration, "flatpak-repo-name", FLATPAK_REPO_NAME);
  ide_configuration_set_internal_string (configuration, "flatpak-sdk", self->sdk);
  ide_configuration_set_internal_string (configuration, "flatpak-runtime", self->platform);
  ide_configuration_set_internal_string (configuration, "flatpak-branch", self->branch);
  ide_configuration_set_internal_string (configuration, "flatpak-module", self->primary_module);
  ide_configuration_set_internal_string (configuration, "flatpak-manifest", manifest_path);

  {
    g_autoptr(JsonParser) parser = NULL;
    g_autoptr(GError) error = NULL;

    parser = json_parser_new ();

    if (json_parser_load_from_file (parser, manifest_path, &error))
      {
        JsonNode *root;
        JsonNode *member;
        JsonObject *root_object;
        JsonArray *ar;

        if (NULL != (root = json_parser_get_root (parser)) &&
            JSON_NODE_HOLDS_OBJECT (root) &&
            NULL != (root_object = json_node_get_object (root)))
          {
            if (json_object_has_member (root_object, "command"))
              ide_configuration_set_internal_string (configuration,
                                                     "flatpak-command",
                                                     json_object_get_string_member (root_object, "command"));

            if (json_object_has_member (root_object, "finish-args") &&
                NULL != (member = json_object_get_member (root_object, "finish-args")) &&
                JSON_NODE_HOLDS_ARRAY (member) &&
                NULL != (ar = json_node_get_array (member)))
              {
                g_autoptr(GPtrArray) finish_args = NULL;
                guint length = json_array_get_length (ar);

                finish_args = g_ptr_array_sized_new (length + 1);

                for (guint i = 0; i < length; i++)
                  {
                    JsonNode *ele = json_array_get_element (ar, i);
                    const gchar *str = json_node_get_string (ele);

                    if (str != NULL)
                      g_ptr_array_add (finish_args, (gchar *)str);
                  }

                g_ptr_array_add (finish_args, NULL);

                ide_configuration_set_internal_strv (configuration,
                                                     "flatpak-finish-args",
                                                     (const gchar * const *)finish_args->pdata);
              }
          }
      }
    else
      g_warning ("Failure to parse Flatpak Manifest: %s", error->message);
  }
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

  build_dir = get_staging_directory (self);
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
