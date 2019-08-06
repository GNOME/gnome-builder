/* gb-flatpak-runtime.c
 *
 * Copyright 2016-2019 Christian Hergert <chergert@redhat.com>
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

#define G_LOG_DOMAIN "gbp-flatpak-runtime"

#include <flatpak.h>
#include <glib/gi18n.h>
#include <json-glib/json-glib.h>

#include "gbp-flatpak-application-addin.h"
#include "gbp-flatpak-manifest.h"
#include "gbp-flatpak-runner.h"
#include "gbp-flatpak-runtime.h"
#include "gbp-flatpak-subprocess-launcher.h"
#include "gbp-flatpak-util.h"

struct _GbpFlatpakRuntime
{
  IdeRuntime parent_instance;

  GHashTable *program_paths_cache;

  IdeTriplet *triplet;
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
  PROP_TRIPLET,
  PROP_BRANCH,
  PROP_DEPLOY_DIR,
  PROP_PLATFORM,
  PROP_SDK,
  N_PROPS
};

static GParamSpec *properties [N_PROPS];

static inline gboolean
strv_empty (gchar **strv)
{
  return strv == NULL || strv[0] == NULL;
}

static const gchar *
get_builddir (GbpFlatpakRuntime *self)
{
  g_autoptr(IdeContext) context = ide_object_ref_context (IDE_OBJECT (self));
  g_autoptr(IdeBuildManager) build_manager = ide_build_manager_ref_from_context (context);
  g_autoptr(IdePipeline) pipeline = ide_build_manager_ref_pipeline (build_manager);

  return ide_pipeline_get_builddir (pipeline);
}

static gchar *
get_staging_directory (GbpFlatpakRuntime *self)
{
  g_autoptr(IdeContext) context = ide_object_ref_context (IDE_OBJECT (self));
  g_autoptr(IdeBuildManager) build_manager = ide_build_manager_ref_from_context (context);
  g_autoptr(IdePipeline) pipeline = ide_build_manager_ref_pipeline (build_manager);

  return gbp_flatpak_get_staging_dir (pipeline);
}

static gboolean
gbp_flatpak_runtime_contains_program_in_path (IdeRuntime   *runtime,
                                              const gchar  *program,
                                              GCancellable *cancellable)
{
  static const gchar *known_path_dirs[] = { "/bin" };
  GbpFlatpakRuntime *self = (GbpFlatpakRuntime *)runtime;
  gboolean ret = FALSE;
  gpointer val = NULL;

  g_assert (GBP_IS_FLATPAK_RUNTIME (self));
  g_assert (program != NULL);
  g_assert (!cancellable || G_IS_CANCELLABLE (cancellable));

  if (g_hash_table_lookup_extended (self->program_paths_cache, program, NULL, &val))
    return GPOINTER_TO_UINT (val);

  for (guint i = 0; i < G_N_ELEMENTS (known_path_dirs); i++)
    {
      g_autofree gchar *path = NULL;

      path = g_build_filename (self->deploy_dir,
                               "files",
                               known_path_dirs[i],
                               program,
                               NULL);

      if (g_file_test (path, G_FILE_TEST_IS_EXECUTABLE))
        {
          ret = TRUE;
          break;
        }
    }

  g_hash_table_insert (self->program_paths_cache,
                       (gchar *)g_intern_string (program),
                       GUINT_TO_POINTER (ret));

  return ret;
}

static IdeSubprocessLauncher *
gbp_flatpak_runtime_create_launcher (IdeRuntime  *runtime,
                                     GError     **error)
{
  IdeSubprocessLauncher *ret;
  GbpFlatpakRuntime *self = (GbpFlatpakRuntime *)runtime;
  g_autoptr(IdeContext) context = NULL;
  const gchar *runtime_id;

  g_return_val_if_fail (GBP_IS_FLATPAK_RUNTIME (self), NULL);

  context = ide_object_ref_context (IDE_OBJECT (self));
  g_return_val_if_fail (IDE_IS_CONTEXT (context), NULL);

  runtime_id = ide_runtime_get_id (runtime);
  g_return_val_if_fail (g_str_has_prefix (runtime_id, "flatpak:"), NULL);

  ret = gbp_flatpak_subprocess_launcher_new (G_SUBPROCESS_FLAGS_STDOUT_PIPE | G_SUBPROCESS_FLAGS_STDERR_PIPE);

  if (ret != NULL && !ide_context_has_project (context))
    {
      ide_subprocess_launcher_set_cwd (ret, g_get_home_dir ());
      ide_subprocess_launcher_set_run_on_host (ret, TRUE);
      gbp_flatpak_subprocess_launcher_use_run (GBP_FLATPAK_SUBPROCESS_LAUNCHER (ret),
                                               runtime_id + strlen ("flatpak:"));
    }
  else if (ret != NULL)
    {
      g_autofree gchar *build_path = NULL;
      g_autofree gchar *ccache_dir = NULL;
      g_auto(GStrv) new_environ = NULL;
      const gchar *builddir = NULL;
      const gchar *project_path = NULL;
      const gchar * const *build_args = NULL;
      g_autoptr(IdeConfigManager) config_manager = NULL;
      IdeConfig *configuration;
      IdeVcs *vcs;

      config_manager = ide_config_manager_ref_from_context (context);
      configuration = ide_config_manager_ref_current (config_manager);

      build_path = get_staging_directory (self);
      builddir = get_builddir (self);

      /* Find the project directory path */
      vcs = ide_vcs_ref_from_context (context);
      project_path = g_file_peek_path (ide_vcs_get_workdir (vcs));

      /* Add 'flatpak build' and the specified arguments to the launcher */
      ide_subprocess_launcher_push_argv (ret, "flatpak");
      ide_subprocess_launcher_push_argv (ret, "build");

      if (GBP_IS_FLATPAK_MANIFEST (configuration))
        build_args = gbp_flatpak_manifest_get_build_args (GBP_FLATPAK_MANIFEST (configuration));

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

      if (!ide_str_empty0 (project_path))
        {
          g_autofree gchar *filesystem_option_src = NULL;
          g_autofree gchar *filesystem_option_build = NULL;
          g_autofree gchar *filesystem_option_cache = NULL;

          filesystem_option_src = g_strdup_printf ("--filesystem=%s", project_path);
          filesystem_option_build = g_strdup_printf ("--filesystem=%s", builddir);
          filesystem_option_cache = g_strdup_printf ("--filesystem=%s/gnome-builder", g_get_user_cache_dir ());
          ide_subprocess_launcher_push_argv (ret, "--nofilesystem=host");
          ide_subprocess_launcher_push_argv (ret, filesystem_option_cache);
          ide_subprocess_launcher_push_argv (ret, filesystem_option_src);
          ide_subprocess_launcher_push_argv (ret, filesystem_option_build);
        }

      new_environ = ide_config_get_environ (IDE_CONFIG (configuration));

      if (!strv_empty (new_environ))
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
get_manifst_command (GbpFlatpakRuntime *self)
{
  IdeContext *context = ide_object_get_context (IDE_OBJECT (self));
  IdeConfigManager *config_manager = ide_config_manager_from_context (context);
  IdeConfig *config = ide_config_manager_get_current (config_manager);

  if (GBP_IS_FLATPAK_MANIFEST (config))
    {
      const gchar *command;

      command = gbp_flatpak_manifest_get_command (GBP_FLATPAK_MANIFEST (config));
      if (!ide_str_empty0 (command))
        return g_strdup (command);
    }

  /* Use the project id as a last resort */
  return ide_context_dup_project_id (context);
}

static IdeRunner *
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

  binary_name = get_manifst_command (self);

  if ((runner = IDE_RUNNER (gbp_flatpak_runner_new (context, build_path, build_target, binary_name))))
    ide_object_append (IDE_OBJECT (self), IDE_OBJECT (runner));

  return runner;
}

static void
gbp_flatpak_runtime_prepare_configuration (IdeRuntime *runtime,
                                           IdeConfig  *config)
{
  g_assert (GBP_IS_FLATPAK_RUNTIME (runtime));
  g_assert (IDE_IS_CONFIG (config));

  ide_config_set_prefix (config, "/app");
  ide_config_set_prefix_set (config, FALSE);
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
                                   name, ide_triplet_get_arch (self->triplet),
                                   self->branch, "active", "files",
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

IdeTriplet *
gbp_flatpak_runtime_get_triplet (GbpFlatpakRuntime *self)
{
  g_return_val_if_fail (GBP_IS_FLATPAK_RUNTIME (self), NULL);

  return self->triplet;
}

static void
gbp_flatpak_runtime_set_triplet (GbpFlatpakRuntime *self,
                                 IdeTriplet        *triplet)
{
  g_return_if_fail (GBP_IS_FLATPAK_RUNTIME (self));

  if (self->triplet != triplet)
    {
      g_clear_pointer (&self->triplet, ide_triplet_unref);
      self->triplet = ide_triplet_ref (triplet);
      g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_TRIPLET]);
    }
}

const gchar *
gbp_flatpak_runtime_get_branch (GbpFlatpakRuntime *self)
{
  g_return_val_if_fail (GBP_IS_FLATPAK_RUNTIME (self), NULL);

  return self->branch;
}

static void
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

static void
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

static void
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

static IdeTriplet *
gbp_flatpak_runtime_real_get_triplet (IdeRuntime *runtime)
{
  return ide_triplet_ref (GBP_FLATPAK_RUNTIME (runtime)->triplet);
}

static gboolean
gbp_flatpak_runtime_supports_toolchain (IdeRuntime   *self,
                                        IdeToolchain *toolchain)
{
  return FALSE;
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
    case PROP_TRIPLET:
      g_value_set_boxed (value, gbp_flatpak_runtime_get_triplet (self));
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
    case PROP_TRIPLET:
      gbp_flatpak_runtime_set_triplet (self, g_value_get_boxed (value));
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

  g_clear_pointer (&self->triplet, ide_triplet_unref);
  g_clear_pointer (&self->branch, g_free);
  g_clear_pointer (&self->runtime_dir, g_free);
  g_clear_pointer (&self->deploy_dir, g_free);
  g_clear_pointer (&self->platform, g_free);
  g_clear_pointer (&self->sdk, g_free);
  g_clear_pointer (&self->program_paths_cache, g_hash_table_unref);
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
  runtime_class->get_triplet = gbp_flatpak_runtime_real_get_triplet;
  runtime_class->supports_toolchain = gbp_flatpak_runtime_supports_toolchain;

  properties [PROP_TRIPLET] =
    g_param_spec_boxed ("triplet",
                         "Triplet",
                         "Architecture Triplet",
                         IDE_TYPE_TRIPLET,
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
  self->program_paths_cache = g_hash_table_new (g_str_hash, g_str_equal);
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
gbp_flatpak_runtime_new (FlatpakInstalledRef  *ref,
                         gboolean              is_extension,
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
  g_autofree gchar *runtime_name = NULL;
  g_autoptr(IdeTriplet) triplet_object = NULL;
  g_autoptr(GString) category = NULL;
  const gchar *name;
  const gchar *arch;
  const gchar *branch;
  const gchar *deploy_dir;

  g_return_val_if_fail (FLATPAK_IS_INSTALLED_REF (ref), NULL);
  g_return_val_if_fail (!cancellable || G_IS_CANCELLABLE (cancellable), NULL);

  arch = flatpak_ref_get_arch (FLATPAK_REF (ref));

  name = flatpak_ref_get_name (FLATPAK_REF (ref));
  branch = flatpak_ref_get_branch (FLATPAK_REF (ref));
  deploy_dir = flatpak_installed_ref_get_deploy_dir (ref);
  triplet_object = ide_triplet_new (arch);
  triplet = g_strdup_printf ("%s/%s/%s", name, arch, branch);
  id = g_strdup_printf ("flatpak:%s", triplet);

  category = g_string_new ("Flatpak/");

  if (g_str_has_prefix (name, "org.gnome."))
    g_string_append (category, "GNOME/");
  else if (g_str_has_prefix (name, "org.freedesktop."))
    g_string_append (category, "FreeDesktop.org/");
  else if (g_str_has_prefix (name, "org.kde."))
    g_string_append (category, "KDE/");

  if (ide_str_equal0 (flatpak_get_default_arch (), arch))
    g_string_append (category, name);
  else
    g_string_append_printf (category, "%s (%s)", name, arch);

  if (!(metadata = flatpak_installed_ref_load_metadata (ref, cancellable, error)))
    return NULL;

  keyfile = g_key_file_new ();
  if (!g_key_file_load_from_bytes (keyfile, metadata, 0, error))
    return NULL;

  if (g_key_file_has_group (keyfile, "ExtensionOf") && !is_extension)
    {
      g_set_error (error,
                   G_IO_ERROR,
                   G_IO_ERROR_NOT_SUPPORTED,
                   "Runtime is an extension");
      return NULL;
    }

  sdk = g_key_file_get_string (keyfile, "Runtime", "sdk", NULL);

  if (g_str_equal (arch, flatpak_get_default_arch ()))
    display_name = g_strdup_printf (_("%s <b>%s</b>"), name, branch);
  else
    display_name = g_strdup_printf (_("%s <b>%s</b> <span fgalpha='36044'>%s</span>"), name, branch, arch);

  runtime_name = g_strdup_printf ("%s %s", _("Flatpak"), triplet);

  /*
   * If we have an SDK that is different from this runtime, we need to locate
   * the SDK deploy-dir instead (for things like includes, pkg-config, etc).
   */
  if (!is_extension)
    {
      if (sdk != NULL && !g_str_equal (sdk, triplet) && NULL != (sdk_deploy_dir = locate_deploy_dir (sdk)))
        deploy_dir = sdk_deploy_dir;
    }

  return g_object_new (GBP_TYPE_FLATPAK_RUNTIME,
                       "id", id,
                       "triplet", triplet_object,
                       "branch", branch,
                       "category", category->str,
                       "name", runtime_name,
                       "deploy-dir", deploy_dir,
                       "display-name", display_name,
                       "platform", name,
                       "sdk", sdk,
                       NULL);
}
