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
#include <glib/gi18n.h>
#include <json-glib/json-glib.h>

#include "gbp-flatpak-application-addin.h"
#include "gbp-flatpak-configuration.h"
#include "gbp-flatpak-runner.h"
#include "gbp-flatpak-runtime.h"
#include "gbp-flatpak-subprocess-launcher.h"
#include "gbp-flatpak-util.h"

struct _GbpFlatpakRuntime
{
  IdeRuntime parent_instance;

  gchar *arch;
  gchar *branch;
  gchar *deploy_dir;
  gchar *platform;
  gchar *sdk;
  GFile *deploy_dir_files;
};

G_DEFINE_TYPE (GbpFlatpakRuntime, gbp_flatpak_runtime, IDE_TYPE_RUNTIME)

enum {
  PROP_0,
  PROP_ARCH,
  PROP_BRANCH,
  PROP_DEPLOY_DIR,
  PROP_PLATFORM,
  PROP_SDK,
  N_PROPS
};

static GParamSpec *properties [N_PROPS];

static const gchar *
get_builddir (GbpFlatpakRuntime *self)
{
  IdeContext *context = ide_object_get_context (IDE_OBJECT (self));
  IdeBuildManager *build_manager = ide_context_get_build_manager (context);
  IdeBuildPipeline *pipeline = ide_build_manager_get_pipeline (build_manager);
  const gchar *builddir = ide_build_pipeline_get_builddir (pipeline);

  return builddir;
}


static gchar *
get_staging_directory (GbpFlatpakRuntime *self)
{
  IdeContext *context = ide_object_get_context (IDE_OBJECT (self));
  IdeConfigurationManager *config_manager = ide_context_get_configuration_manager (context);
  IdeConfiguration *config = ide_configuration_manager_get_current (config_manager);

  return gbp_flatpak_get_staging_dir (config);
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
      GFile *manifest;
      GFile *project_file;
      IdeContext *context;
      IdeConfigurationManager *config_manager;
      IdeConfiguration *configuration;

      context = ide_object_get_context (IDE_OBJECT (self));
      config_manager = ide_context_get_configuration_manager (context);
      configuration = ide_configuration_manager_get_current (config_manager);

      build_path = get_staging_directory (self);
      builddir = get_builddir (self);

      /* Attempt to parse the flatpak manifest */
      if (GBP_IS_FLATPAK_CONFIGURATION (configuration) &&
          NULL != (manifest = gbp_flatpak_configuration_get_manifest (GBP_FLATPAK_CONFIGURATION (configuration))) &&
          NULL != (manifest_path = g_file_get_path (manifest)))
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

static gchar *
get_binary_name (GbpFlatpakRuntime *self,
                 IdeBuildTarget    *build_target)
{
  IdeContext *context = ide_object_get_context (IDE_OBJECT (self));
  IdeConfigurationManager *config_manager = ide_context_get_configuration_manager (context);
  IdeConfiguration *config = ide_configuration_manager_get_current (config_manager);

  if (GBP_IS_FLATPAK_CONFIGURATION (config))
    {
      const gchar *command;
      command = gbp_flatpak_configuration_get_command (GBP_FLATPAK_CONFIGURATION (config));
      if (!ide_str_empty0 (command))
        return g_strdup (command);
    }

  /* Use the build target name if there's no command in the manifest */
  {
    g_autofree gchar *build_target_name = NULL;
    build_target_name = ide_build_target_get_name (build_target);
    if (!ide_str_empty0 (build_target_name))
      return g_steal_pointer (&build_target_name);
  }

  /* Use the project name as a last resort */
  {
    IdeProject *project;
    project = ide_context_get_project (context);
    return g_strdup (ide_project_get_name (project));
  }
}

IdeRunner *
gbp_flatpak_runtime_create_runner (IdeRuntime     *runtime,
                                   IdeBuildTarget *build_target)
{
  GbpFlatpakRuntime *self = (GbpFlatpakRuntime *)runtime;
  IdeContext *context;
  GbpFlatpakRunner *runner;
  g_autofree gchar *build_path = NULL;
  g_autofree gchar *binary_name = NULL;

  g_assert (GBP_IS_FLATPAK_RUNTIME (self));
  g_assert (IDE_IS_BUILD_TARGET (build_target));

  context = ide_object_get_context (IDE_OBJECT (self));
  runner = gbp_flatpak_runner_new (context);
  g_assert (GBP_IS_FLATPAK_RUNNER (runner));

  build_path = get_staging_directory (self);
  binary_name = get_binary_name (self, build_target);

  ide_runner_set_run_on_host (IDE_RUNNER (runner), TRUE);
  ide_runner_append_argv (IDE_RUNNER (runner), "flatpak");
  ide_runner_append_argv (IDE_RUNNER (runner), "build");
  ide_runner_append_argv (IDE_RUNNER (runner), "--share=ipc");
  ide_runner_append_argv (IDE_RUNNER (runner), "--socket=x11");
  ide_runner_append_argv (IDE_RUNNER (runner), "--socket=wayland");
  ide_runner_append_argv (IDE_RUNNER (runner), build_path);
  ide_runner_append_argv (IDE_RUNNER (runner), binary_name);

  return IDE_RUNNER (runner);
}

static void
gbp_flatpak_runtime_prepare_configuration (IdeRuntime       *runtime,
                                           IdeConfiguration *configuration)
{
  GbpFlatpakRuntime* self = (GbpFlatpakRuntime *)runtime;

  g_assert (GBP_IS_FLATPAK_RUNTIME (self));
  g_assert (IDE_IS_CONFIGURATION (configuration));

  ide_configuration_set_prefix (configuration, "/app");
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

const gchar *
gbp_flatpak_runtime_get_arch (GbpFlatpakRuntime *self)
{
  g_return_val_if_fail (GBP_IS_FLATPAK_RUNTIME (self), NULL);

  return self->arch;
}

void
gbp_flatpak_runtime_set_arch (GbpFlatpakRuntime *self,
                              const gchar       *arch)
{
  g_return_if_fail (GBP_IS_FLATPAK_RUNTIME (self));

  if (g_strcmp0 (arch, self->arch) != 0)
    {
      g_free (self->arch);
      self->arch = g_strdup (arch);
      g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_ARCH]);
    }
}

const gchar *
gbp_flatpak_runtime_get_branch (GbpFlatpakRuntime *self)
{
  g_return_val_if_fail (GBP_IS_FLATPAK_RUNTIME (self), NULL);

  return self->branch;
}

void
gbp_flatpak_runtime_set_branch (GbpFlatpakRuntime *self,
                                const gchar       *branch)
{
  g_return_if_fail (GBP_IS_FLATPAK_RUNTIME (self));

  if (g_strcmp0 (branch, self->branch) != 0)
    {
      g_free (self->branch);
      self->branch = g_strdup (branch);
      g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_BRANCH]);
    }
}

const gchar *
gbp_flatpak_runtime_get_platform (GbpFlatpakRuntime *self)
{
  g_return_val_if_fail (GBP_IS_FLATPAK_RUNTIME (self), NULL);

  return self->platform;
}

void
gbp_flatpak_runtime_set_platform (GbpFlatpakRuntime *self,
                                  const gchar       *platform)
{
  g_return_if_fail (GBP_IS_FLATPAK_RUNTIME (self));

  g_free (self->platform);
  self->platform = g_strdup (platform);
  g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_PLATFORM]);
}

const gchar *
gbp_flatpak_runtime_get_sdk (GbpFlatpakRuntime *self)
{
  g_return_val_if_fail (GBP_IS_FLATPAK_RUNTIME (self), NULL);

  return self->sdk;
}

gchar *
gbp_flatpak_runtime_get_sdk_name (GbpFlatpakRuntime *self)
{
  const gchar *slash;

  g_return_val_if_fail (GBP_IS_FLATPAK_RUNTIME (self), NULL);

  if (self->sdk == NULL)
    return NULL;

  slash = strchr (self->sdk, '/');

  if (slash == NULL)
    return g_strdup (self->sdk);
  else
    return g_strndup (self->sdk, slash - self->sdk);
}

void
gbp_flatpak_runtime_set_sdk (GbpFlatpakRuntime *self,
                             const gchar       *sdk)
{
  g_return_if_fail (GBP_IS_FLATPAK_RUNTIME (self));

  g_free (self->sdk);
  self->sdk = g_strdup (sdk);
  g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_SDK]);
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
    case PROP_ARCH:
      g_value_set_string (value, gbp_flatpak_runtime_get_arch (self));
      break;

    case PROP_BRANCH:
      g_value_set_string (value, gbp_flatpak_runtime_get_branch (self));
      break;

    case PROP_PLATFORM:
      g_value_set_string (value, gbp_flatpak_runtime_get_platform (self));
      break;

    case PROP_SDK:
      g_value_set_string (value, gbp_flatpak_runtime_get_sdk (self));
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
    case PROP_ARCH:
      gbp_flatpak_runtime_set_arch (self, g_value_get_string (value));
      break;

    case PROP_BRANCH:
      gbp_flatpak_runtime_set_branch (self, g_value_get_string (value));
      break;

    case PROP_PLATFORM:
      gbp_flatpak_runtime_set_platform (self, g_value_get_string (value));
      break;

    case PROP_SDK:
      gbp_flatpak_runtime_set_sdk (self, g_value_get_string (value));
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

  g_clear_pointer (&self->arch, g_free);
  g_clear_pointer (&self->branch, g_free);
  g_clear_pointer (&self->deploy_dir, g_free);
  g_clear_pointer (&self->platform, g_free);
  g_clear_pointer (&self->sdk, g_free);
  g_clear_object (&self->deploy_dir_files);

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

  properties [PROP_ARCH] =
    g_param_spec_string ("arch",
                         "Arch",
                         "Arch",
                         flatpak_get_default_arch (),
                         (G_PARAM_READWRITE | G_PARAM_CONSTRUCT | G_PARAM_STATIC_STRINGS));

  properties [PROP_BRANCH] =
    g_param_spec_string ("branch",
                         "Branch",
                         "Branch",
                         "master",
                         (G_PARAM_READWRITE | G_PARAM_CONSTRUCT | G_PARAM_STATIC_STRINGS));

  properties [PROP_DEPLOY_DIR] =
    g_param_spec_string ("deploy-dir",
                         "Deploy Directory",
                         "The flatpak runtime deploy directory",
                         NULL,
                         (G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS));

  properties [PROP_PLATFORM] =
    g_param_spec_string ("platform",
                         "Platform",
                         "Platform",
                         "org.gnome.Platform",
                         (G_PARAM_READWRITE | G_PARAM_CONSTRUCT | G_PARAM_STATIC_STRINGS));

  properties [PROP_SDK] =
    g_param_spec_string ("sdk",
                         "Sdk",
                         "Sdk",
                         "org.gnome.Sdk",
                         (G_PARAM_READWRITE | G_PARAM_CONSTRUCT | G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, N_PROPS, properties);
}

static void
gbp_flatpak_runtime_init (GbpFlatpakRuntime *self)
{
}

static gchar *
locate_deploy_dir (const gchar *sdk_id)
{
  g_auto(GStrv) parts = g_strsplit (sdk_id, "/", 3);

  if (g_strv_length (parts) == 3)
    return gbp_flatpak_application_addin_get_deploy_dir (gbp_flatpak_application_addin_get_default (),
                                                         parts[0], parts[1], parts[2]);
  return NULL;
}

GbpFlatpakRuntime *
gbp_flatpak_runtime_new (IdeContext           *context,
                         FlatpakInstalledRef  *ref,
                         GCancellable         *cancellable,
                         GError              **error)
{
  g_autofree gchar *sdk_deploy_dir = NULL;
  g_autoptr(GBytes) metadata = NULL;
  g_autoptr(GKeyFile) keyfile = NULL;
  g_autofree gchar *sdk = NULL;
  g_autofree gchar *id = NULL;
  g_autofree gchar *display_name = NULL;
  g_autofree gchar *triplet = NULL;
  g_autoptr(FlatpakRef) sdk_ref = NULL;
  const gchar *name;
  const gchar *arch;
  const gchar *branch;
  const gchar *deploy_dir;

  g_return_val_if_fail (IDE_IS_CONTEXT (context), NULL);
  g_return_val_if_fail (FLATPAK_IS_INSTALLED_REF (ref), NULL);

  name = flatpak_ref_get_name (FLATPAK_REF (ref));
  arch = flatpak_ref_get_arch (FLATPAK_REF (ref));
  branch = flatpak_ref_get_branch (FLATPAK_REF (ref));
  deploy_dir = flatpak_installed_ref_get_deploy_dir (ref);
  triplet = g_strdup_printf ("%s/%s/%s", name, arch, branch);
  id = g_strdup_printf ("flatpak:%s", triplet);

  metadata = flatpak_installed_ref_load_metadata (ref, cancellable, error);
  if (metadata == NULL)
    return NULL;

  keyfile = g_key_file_new ();
  if (!g_key_file_load_from_bytes (keyfile, metadata, 0, error))
    return NULL;

  sdk = g_key_file_get_string (keyfile, "Runtime", "sdk", NULL);

  if (g_str_equal (arch, flatpak_get_default_arch ()))
    display_name = g_strdup_printf (_("%s <b>%s</b>"), name, branch);
  else
    display_name = g_strdup_printf (_("%s <b>%s</b> <span variant='smallcaps'>%s</span>"), name, branch, arch);

  /*
   * If we have an SDK that is different from this runtime, we need to locate
   * the SDK deploy-dir instead (for things like includes, pkg-config, etc).
   */
  if (sdk != NULL && !g_str_equal (sdk, triplet) && NULL != (sdk_deploy_dir = locate_deploy_dir (sdk)))
    deploy_dir = sdk_deploy_dir;

  return g_object_new (GBP_TYPE_FLATPAK_RUNTIME,
                       "context", context,
                       "id", id,
                       "arch", arch,
                       "branch", branch,
                       "deploy-dir", deploy_dir,
                       "display-name", display_name,
                       "platform", name,
                       "sdk", sdk,
                       NULL);
}
