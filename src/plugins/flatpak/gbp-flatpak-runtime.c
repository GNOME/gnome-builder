/* gb-flatpak-runtime.c
 *
 * Copyright © 2016 Christian Hergert <chergert@redhat.com>
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
  gchar *runtime_dir;
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
  launcher = ide_subprocess_launcher_new (G_SUBPROCESS_FLAGS_STDOUT_SILENCE |
                                          G_SUBPROCESS_FLAGS_STDERR_SILENCE);

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
      g_autofree gchar *project_name = NULL;
      g_autofree gchar *project_path = NULL;
      g_autofree gchar *build_path = NULL;
      g_autofree gchar *ccache_dir = NULL;
      g_auto(GStrv) new_environ = NULL;
      const gchar *builddir = NULL;
      const gchar * const *build_args = NULL;
      GFile *project_file;
      IdeContext *context;
      IdeConfigurationManager *config_manager;
      IdeConfiguration *configuration;

      context = ide_object_get_context (IDE_OBJECT (self));
      config_manager = ide_context_get_configuration_manager (context);
      configuration = ide_configuration_manager_get_current (config_manager);

      build_path = get_staging_directory (self);
      builddir = get_builddir (self);

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

      if (GBP_IS_FLATPAK_CONFIGURATION (configuration))
        build_args = gbp_flatpak_configuration_get_build_args (GBP_FLATPAK_CONFIGURATION (configuration));

      if (build_args != NULL)
        ide_subprocess_launcher_push_args (ret, build_args);
      else
        ide_subprocess_launcher_push_argv (ret, "--share=network");

      /* We might need access to ccache configs inside the build environ.
       * Usually, this is set by flatpak-builder, but since we are running
       * the incremental build, we need to take care of that too.
       *
       * See https://bugzilla.gnome.org/show_bug.cgi?id=780153
       */
      ccache_dir = g_build_filename (g_get_user_cache_dir (),
                                     ide_get_program_name (),
                                     "flatpak-builder",
                                     "ccache",
                                     NULL);
      ide_subprocess_launcher_setenv (ret, "CCACHE_DIR", ccache_dir, FALSE);

      if (!dzl_str_empty0 (project_path))
        {
          g_autofree gchar *filesystem_option_src = NULL;
          g_autofree gchar *filesystem_option_build = NULL;
          g_autofree gchar *build_dir_option = NULL;

          filesystem_option_src = g_strdup_printf ("--filesystem=%s", project_path);
          filesystem_option_build = g_strdup_printf ("--filesystem=%s", builddir);
          build_dir_option = g_strdup_printf ("--build-dir=%s", builddir);
          ide_subprocess_launcher_push_argv (ret, "--nofilesystem=host");
          ide_subprocess_launcher_push_argv (ret, filesystem_option_src);
          ide_subprocess_launcher_push_argv (ret, filesystem_option_build);
          ide_subprocess_launcher_push_argv (ret, build_dir_option);
        }
      new_environ = ide_configuration_get_environ (IDE_CONFIGURATION (configuration));
      if (g_strv_length (new_environ) > 0)
        {
          for (guint i = 0; new_environ[i]; i++)
            {
              if (g_utf8_strlen (new_environ[i], -1) > 1)
                {
                  g_autofree gchar *env_option = NULL;

                  env_option = g_strdup_printf ("--env=%s", new_environ[i]);
                  ide_subprocess_launcher_push_argv (ret, env_option);
                }
            }
        }

      /* We want the configure step to be separate so IdeAutotoolsBuildTask can pass options to it */
      ide_subprocess_launcher_push_argv (ret, "--env=NOCONFIGURE=1");

      ide_subprocess_launcher_push_argv (ret, build_path);

      ide_subprocess_launcher_set_run_on_host (ret, TRUE);
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
      if (!dzl_str_empty0 (command))
        return g_strdup (command);
    }

  /* Use the build target name if there's no command in the manifest */
  {
    g_autofree gchar *build_target_name = NULL;

    build_target_name = ide_build_target_get_name (build_target);
    if (!dzl_str_empty0 (build_target_name))
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
  g_autofree gchar *build_path = NULL;
  g_autofree gchar *binary_name = NULL;
  IdeContext *context;
  IdeRunner *runner;

  g_assert (GBP_IS_FLATPAK_RUNTIME (self));
  g_assert (!build_target || IDE_IS_BUILD_TARGET (build_target));

  context = ide_object_get_context (IDE_OBJECT (self));
  build_path = get_staging_directory (self);

  if (build_target != NULL)
    binary_name = get_binary_name (self, build_target);

  runner = IDE_RUNNER (gbp_flatpak_runner_new (context, build_path, binary_name));
  if (build_target != NULL)
    ide_runner_set_build_target (runner, build_target);

  return runner;
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

static const gchar *
gbp_flatpak_runtime_get_runtime_dir (GbpFlatpakRuntime *self)
{
  if G_UNLIKELY (self->runtime_dir == NULL)
    {
      g_autofree gchar *sdk_name = NULL;
      const gchar *ids[2];

      sdk_name = gbp_flatpak_runtime_get_sdk_name (self);

      ids[0] = self->platform;
      ids[1] = sdk_name;

      for (guint i = 0; i < G_N_ELEMENTS (ids); i++)
        {
          const gchar *id = ids[i];
          g_autofree gchar *name = g_strdup_printf ("%s.Debug", id);
          g_autofree gchar *deploy_path = NULL;
          g_autofree gchar *path = NULL;

          /*
           * The easiest way to reliably stay within the same installation
           * is to just use relative paths to the checkout of the deploy dir.
           */
          deploy_path = g_file_get_path (self->deploy_dir_files);
          path = g_build_filename (deploy_path, "..", "..", "..", "..", "..",
                                   name, self->arch, self->branch, "active", "files",
                                   NULL);
          if (g_file_test (path, G_FILE_TEST_IS_DIR))
            {
              self->runtime_dir = g_steal_pointer (&path);
              break;
            }
        }
    }

  return self->runtime_dir;
}

static GFile *
gbp_flatpak_runtime_translate_file (IdeRuntime *runtime,
                                    GFile      *file)
{
  GbpFlatpakRuntime *self = (GbpFlatpakRuntime *)runtime;
  const gchar *runtime_dir;
  g_autofree gchar *path = NULL;
  g_autofree gchar *build_dir = NULL;
  g_autofree gchar *app_files_path = NULL;
  g_autofree gchar *debug_dir = NULL;

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

  runtime_dir = gbp_flatpak_runtime_get_runtime_dir (self);

  if (runtime_dir != NULL)
    {
      if (g_str_has_prefix (path, "/run/build-runtime/"))
        {
          g_autofree gchar *translated = NULL;

          translated = g_build_filename (runtime_dir,
                                         "source",
                                         path + DZL_LITERAL_LENGTH ("/run/build-runtime/"),
                                         NULL);
          return g_file_new_for_path (translated);
        }

      debug_dir = g_build_filename (runtime_dir, "usr", "lib", NULL);

      if (g_str_equal (path, "/usr/lib/debug") ||
          g_str_equal (path, "/usr/lib/debug/"))
        return g_file_new_for_path (debug_dir);

      if (g_str_has_prefix (path, "/usr/lib/debug/"))
        {
          g_autofree gchar *translated = NULL;

          translated = g_build_filename (debug_dir,
                                         path + DZL_LITERAL_LENGTH ("/usr/lib/debug/"),
                                         NULL);
          return g_file_new_for_path (translated);
        }
    }

  if (g_str_equal ("/usr", path))
    return g_object_ref (self->deploy_dir_files);

  if (g_str_has_prefix (path, "/usr/"))
    return g_file_get_child (self->deploy_dir_files, path + DZL_LITERAL_LENGTH ("/usr/"));

  build_dir = get_staging_directory (self);
  app_files_path = g_build_filename (build_dir, "files", NULL);

  if (g_str_equal (path, "/app") || g_str_equal (path, "/app/"))
    return g_file_new_for_path (app_files_path);

  if (g_str_has_prefix (path, "/app/"))
    {
      g_autofree gchar *translated = NULL;

      translated = g_build_filename (app_files_path,
                                     path + DZL_LITERAL_LENGTH ("/app/"),
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

  if (g_strcmp0 (sdk, self->sdk) != 0)
    {
      g_free (self->sdk);
      self->sdk = g_strdup (sdk);
      g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_SDK]);
    }
}

static gchar **
gbp_flatpak_runtime_get_system_include_dirs (IdeRuntime *runtime)
{
  static const gchar *include_dirs[] = { "/app/include", "/usr/include", NULL };
  return g_strdupv ((gchar **)include_dirs);
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
  g_clear_pointer (&self->runtime_dir, g_free);
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
  runtime_class->get_system_include_dirs = gbp_flatpak_runtime_get_system_include_dirs;

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
