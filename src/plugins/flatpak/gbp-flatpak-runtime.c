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

#include "config.h"

#include <unistd.h>

#include <glib/gi18n.h>

#include <json-glib/json-glib.h>

#include <libide-vcs.h>

#include "gbp-flatpak-aux.h"
#include "gbp-flatpak-manifest.h"
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

G_DEFINE_FINAL_TYPE (GbpFlatpakRuntime, gbp_flatpak_runtime, IDE_TYPE_RUNTIME)

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

  /* If we have an SDK, we might need to check that runtime */
  if (ret == FALSE &&
      self->sdk != NULL &&
      !ide_str_equal0 (self->platform, self->sdk))
    {
      IdeContext* context = ide_object_get_context (IDE_OBJECT (self));
      if (context)
        {
          g_autoptr(IdeRuntimeManager) manager = ide_object_ensure_child_typed (IDE_OBJECT (context), IDE_TYPE_RUNTIME_MANAGER);
          g_autofree char *arch = ide_runtime_get_arch (runtime);
          g_autofree char *sdk_id = g_strdup_printf ("flatpak:%s/%s/%s", self->sdk, arch, self->branch);
          IdeRuntime *sdk = ide_runtime_manager_get_runtime (manager, sdk_id);

          if (sdk != NULL && sdk != runtime)
            ret = ide_runtime_contains_program_in_path (sdk, program, cancellable);
        }
    }

  /* Cache both positive and negative lookups */
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
      const gchar *config_dir = gbp_flatpak_get_config_dir ();
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

      /* Get access to override installations */
      ide_subprocess_launcher_setenv (ret, "FLATPAK_CONFIG_DIR", config_dir, TRUE);

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

static gboolean
can_pass_through_finish_arg (const char *arg)
{
  if (arg == NULL)
    return FALSE;

  return g_str_has_prefix (arg, "--allow") ||
         g_str_has_prefix (arg, "--share") ||
         g_str_has_prefix (arg, "--socket") ||
         g_str_has_prefix (arg, "--filesystem") ||
         g_str_has_prefix (arg, "--device") ||
         g_str_has_prefix (arg, "--env") ||
         g_str_has_prefix (arg, "--system-talk") ||
         g_str_has_prefix (arg, "--own-name") ||
         g_str_has_prefix (arg, "--talk-name") ||
         g_str_has_prefix (arg, "--add-policy");
}

static gboolean
gbp_flatpak_runtime_handle_run_context_cb (IdeRunContext       *run_context,
                                           const char * const  *argv,
                                           const char * const  *env,
                                           const char          *cwd,
                                           IdeUnixFDMap        *unix_fd_map,
                                           gpointer             user_data,
                                           GError             **error)
{
  IdePipeline *pipeline = user_data;
  GbpFlatpakRuntime *self;
  g_autofree char *project_build_dir_arg = NULL;
  g_autofree char *project_build_dir = NULL;
  g_autofree char *staging_dir = NULL;
  const char *wayland_display;
  const char *app_id;
  IdeContext *context;
  IdeConfig *config;

  IDE_ENTRY;

  g_assert (IDE_IS_PIPELINE (pipeline));
  g_assert (IDE_IS_RUN_CONTEXT (run_context));
  g_assert (IDE_IS_UNIX_FD_MAP (unix_fd_map));

  self = GBP_FLATPAK_RUNTIME (ide_pipeline_get_runtime (pipeline));
  context = ide_object_get_context (IDE_OBJECT (self));

  g_assert (GBP_IS_FLATPAK_RUNTIME (self));
  g_assert (IDE_IS_CONTEXT (context));

  /* Pass through the FD mappings */
  if (!ide_run_context_merge_unix_fd_map (run_context, unix_fd_map, error))
    return FALSE;

  staging_dir = gbp_flatpak_get_staging_dir (pipeline);
  config = ide_pipeline_get_config (pipeline);
  app_id = ide_config_get_app_id (config);

  /* Make sure our worker has access to our Builder-specific Flatpak repository */
  ide_run_context_setenv (run_context, "FLATPAK_CONFIG_DIR", gbp_flatpak_get_config_dir ());

  /* We need access to a few things for "flatpak build" to work and give us
   * access to the display/etc.
   */
  ide_run_context_add_minimal_environment (run_context);

  /* We can pass the CWD directory down just fine */
  ide_run_context_set_cwd (run_context, cwd);

  /* Now setup our basic arguments for the application */
  ide_run_context_append_argv (run_context, "flatpak");
  ide_run_context_append_argv (run_context, "build");
  ide_run_context_append_argv (run_context, "--with-appdir");
  ide_run_context_append_argv (run_context, "--allow=devel");
  ide_run_context_append_argv (run_context, "--die-with-parent");

  /* Make sure we have access to the document portal */
  ide_run_context_append_formatted (run_context,
                                    "--bind-mount=/run/user/%u/doc=/run/user/%u/doc/by-app/%s",
                                    getuid (), getuid (), app_id);

  /* Make sure wayland socket is available. */
  if ((wayland_display = g_getenv ("WAYLAND_DISPLAY")))
    ide_run_context_append_formatted (run_context,
                                      "--bind-mount=/run/user/%u/%s=/run/user/%u/%s",
                                      getuid (), wayland_display, getuid (), wayland_display);

  /* Make sure we have access to fonts and such */
  gbp_flatpak_aux_append_to_run_context (run_context);

  /* Make sure we have access to the build directory */
  project_build_dir = ide_context_cache_filename (context, NULL, NULL);
  project_build_dir_arg = g_strdup_printf ("--filesystem=%s", project_build_dir);
  ide_run_context_append_argv (run_context, project_build_dir_arg);

  /* Convert environment from upper level into --env=FOO=BAR */
  if (env != NULL)
    {
      for (guint i = 0; env[i]; i++)
        {
          g_autofree char *arg = g_strconcat ("--env=", env[i], NULL);
          ide_run_context_append_argv (run_context, arg);
        }
    }

  /* Make sure all of our finish arguments for the manifest are included */
  if (GBP_IS_FLATPAK_MANIFEST (config))
    {
      const gchar * const *finish_args = gbp_flatpak_manifest_get_finish_args (GBP_FLATPAK_MANIFEST (config));

      if (finish_args != NULL)
        {
          for (guint i = 0; finish_args[i]; i++)
            {
              if (can_pass_through_finish_arg (finish_args[i]))
                ide_run_context_append_argv (run_context, finish_args[i]);
            }
        }
    }
  else
    {
      ide_run_context_append_argv (run_context, "--share=ipc");
      ide_run_context_append_argv (run_context, "--share=network");
      ide_run_context_append_argv (run_context, "--socket=x11");
      ide_run_context_append_argv (run_context, "--socket=wayland");
    }

  ide_run_context_append_argv (run_context, "--talk-name=org.freedesktop.portal.*");
  ide_run_context_append_argv (run_context, "--talk-name=org.a11y.Bus");

  /* And last, before our child command, is the staging directory */
  ide_run_context_append_argv (run_context, staging_dir);

  /* And now the upper layer's command arguments */
  ide_run_context_append_args (run_context, argv);

  IDE_RETURN (TRUE);
}

static void
gbp_flatpak_runtime_prepare_to_run (IdeRuntime    *runtime,
                                    IdePipeline   *pipeline,
                                    IdeRunContext *run_context)
{
  IDE_ENTRY;

  g_assert (GBP_IS_FLATPAK_RUNTIME (runtime));
  g_assert (IDE_IS_PIPELINE (pipeline));
  g_assert (IDE_IS_RUN_CONTEXT (run_context));

  /* We have to run "flatpak build" from the host */
  ide_run_context_push_host (run_context);

  /* Handle the upper layer to rewrite the command using "flatpak build" */
  ide_run_context_push (run_context,
                        gbp_flatpak_runtime_handle_run_context_cb,
                        g_object_ref (pipeline),
                        g_object_unref);

  IDE_EXIT;
}

static gboolean
gbp_flatpak_runtime_handle_build_context_cb (IdeRunContext       *run_context,
                                             const char * const  *argv,
                                             const char * const  *env,
                                             const char          *cwd,
                                             IdeUnixFDMap        *unix_fd_map,
                                             gpointer             user_data,
                                             GError             **error)
{
  IdePipeline *pipeline = user_data;
  GbpFlatpakRuntime *self;
  g_autofree char *staging_dir = NULL;
  g_autofree char *ccache_dir = NULL;
  const char *srcdir;
  const char *builddir;
  IdeContext *context;
  IdeConfig *config;

  IDE_ENTRY;

  g_assert (IDE_IS_PIPELINE (pipeline));
  g_assert (IDE_IS_RUN_CONTEXT (run_context));
  g_assert (IDE_IS_UNIX_FD_MAP (unix_fd_map));

  self = GBP_FLATPAK_RUNTIME (ide_pipeline_get_runtime (pipeline));
  context = ide_object_get_context (IDE_OBJECT (self));

  g_assert (GBP_IS_FLATPAK_RUNTIME (self));
  g_assert (IDE_IS_CONTEXT (context));

  /* Pass through the FD mappings */
  if (!ide_run_context_merge_unix_fd_map (run_context, unix_fd_map, error))
    return FALSE;

  staging_dir = gbp_flatpak_get_staging_dir (pipeline);
  srcdir = ide_pipeline_get_srcdir (pipeline);
  builddir = ide_pipeline_get_builddir (pipeline);
  config = ide_pipeline_get_config (pipeline);

  /* Make sure our worker has access to our Builder-specific Flatpak repository */
  ide_run_context_setenv (run_context, "FLATPAK_CONFIG_DIR", gbp_flatpak_get_config_dir ());

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
  ide_run_context_setenv (run_context, "CCACHE_DIR", ccache_dir);

  /* We can pass the CWD directory down just fine */
  ide_run_context_set_cwd (run_context, cwd);

  /* Now setup our basic arguments for the application */
  ide_run_context_append_argv (run_context, "flatpak");
  ide_run_context_append_argv (run_context, "build");
  ide_run_context_append_argv (run_context, "--with-appdir");
  ide_run_context_append_argv (run_context, "--allow=devel");
  ide_run_context_append_argv (run_context, "--die-with-parent");

  /* Make sure we have access to the build directory */
  ide_run_context_append_formatted (run_context, "--filesystem=%s", srcdir);
  ide_run_context_append_formatted (run_context, "--filesystem=%s", builddir);
  ide_run_context_append_formatted (run_context, "--filesystem=%s/gnome-builder", g_get_user_cache_dir ());
  ide_run_context_append_argv (run_context, "--nofilesystem=host");

  /* Ensure build-args are passed down */
  if (GBP_IS_FLATPAK_MANIFEST (config))
    {
      const char * const *build_args = gbp_flatpak_manifest_get_build_args (GBP_FLATPAK_MANIFEST (config));
      if (build_args != NULL)
        ide_run_context_append_args (run_context, build_args);
    }
  else
    {
      /* Somehow got here w/o a manifest, give network access to be nice so
       * things like meson subprojects work and git submodules work.
       */
      ide_run_context_append_argv (run_context, "--share=network");
    }

  /* Convert environment from upper level into --env=FOO=BAR */
  if (env != NULL)
    {
      for (guint i = 0; env[i]; i++)
        {
          g_autofree char *arg = g_strconcat ("--env=", env[i], NULL);
          ide_run_context_append_argv (run_context, arg);
        }
    }

  /* And last, before our child command, is the staging directory */
  ide_run_context_append_argv (run_context, staging_dir);

  /* And now the upper layer's command arguments */
  ide_run_context_append_args (run_context, argv);

  IDE_RETURN (TRUE);
}

static void
gbp_flatpak_runtime_prepare_to_build (IdeRuntime    *runtime,
                                      IdePipeline   *pipeline,
                                      IdeRunContext *run_context)
{
  IDE_ENTRY;

  g_assert (GBP_IS_FLATPAK_RUNTIME (runtime));
  g_assert (IDE_IS_PIPELINE (pipeline));
  g_assert (IDE_IS_RUN_CONTEXT (run_context));

  /* We have to run "flatpak build" from the host */
  ide_run_context_push_host (run_context);

  /* Handle the upper layer to rewrite the command using "flatpak build" */
  ide_run_context_push (run_context,
                        gbp_flatpak_runtime_handle_build_context_cb,
                        g_object_ref (pipeline),
                        g_object_unref);

  IDE_EXIT;
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
                                         path + strlen ("/run/build-runtime/"),
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
                                         path + strlen ("/usr/lib/debug/"),
                                         NULL);
          return g_file_new_for_path (translated);
        }
    }

  if (g_str_equal ("/usr", path))
    return g_object_ref (self->deploy_dir_files);

  if (g_str_has_prefix (path, "/usr/"))
    return g_file_get_child (self->deploy_dir_files, path + strlen ("/usr/"));

  build_dir = get_staging_directory (self);
  app_files_path = g_build_filename (build_dir, "files", NULL);

  if (g_str_equal (path, "/app") || g_str_equal (path, "/app/"))
    return g_file_new_for_path (app_files_path);

  if (g_str_has_prefix (path, "/app/"))
    {
      g_autofree gchar *translated = NULL;

      translated = g_build_filename (app_files_path,
                                     path + strlen ("/app/"),
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
  runtime_class->contains_program_in_path = gbp_flatpak_runtime_contains_program_in_path;
  runtime_class->prepare_configuration = gbp_flatpak_runtime_prepare_configuration;
  runtime_class->prepare_to_build = gbp_flatpak_runtime_prepare_to_build;
  runtime_class->prepare_to_run = gbp_flatpak_runtime_prepare_to_run;
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

GbpFlatpakRuntime *
gbp_flatpak_runtime_new (const char *name,
                         const char *arch,
                         const char *branch,
                         const char *sdk_name,
                         const char *sdk_branch,
                         const char *deploy_dir,
                         const char *metadata,
                         gboolean    is_extension)
{
  g_autofree gchar *id = NULL;
  g_autofree gchar *short_id = NULL;
  g_autofree gchar *display_name = NULL;
  g_autofree gchar *triplet = NULL;
  g_autofree gchar *runtime_name = NULL;
  g_autoptr(IdeTriplet) triplet_object = NULL;
  g_autoptr(GString) category = NULL;

  g_return_val_if_fail (name != NULL, NULL);
  g_return_val_if_fail (arch != NULL, NULL);
  g_return_val_if_fail (branch != NULL, NULL);
  g_return_val_if_fail (deploy_dir != NULL, NULL);

  if (sdk_name == NULL)
    sdk_name = name;

  if (sdk_branch == NULL)
    sdk_branch = branch;

  triplet_object = ide_triplet_new (arch);
  triplet = g_strdup_printf ("%s/%s/%s", name, arch, branch);
  id = g_strdup_printf ("flatpak:%s", triplet);
  short_id = g_strdup_printf ("flatpak:%s-%s", name, branch);

  category = g_string_new ("Flatpak/");

  if (g_str_has_prefix (name, "org.gnome."))
    g_string_append (category, "GNOME/");
  else if (g_str_has_prefix (name, "org.freedesktop."))
    g_string_append (category, "FreeDesktop.org/");
  else if (g_str_has_prefix (name, "org.kde."))
    g_string_append (category, "KDE/");

  if (ide_str_equal0 (ide_get_system_arch (), arch))
    g_string_append (category, name);
  else
    g_string_append_printf (category, "%s (%s)", name, arch);

  if (g_str_equal (arch, ide_get_system_arch ()))
    display_name = g_strdup_printf (_("%s <b>%s</b>"), name, branch);
  else
    display_name = g_strdup_printf (_("%s <b>%s</b> <span fgalpha='36044'>%s</span>"), name, branch, arch);

  runtime_name = g_strdup_printf ("%s %s", _("Flatpak"), triplet);

  return g_object_new (GBP_TYPE_FLATPAK_RUNTIME,
                       "id", id,
                       "short-id", short_id,
                       "triplet", triplet_object,
                       "branch", branch,
                       "category", category->str,
                       "name", runtime_name,
                       "deploy-dir", deploy_dir,
                       "display-name", display_name,
                       "platform", name,
                       "sdk", sdk_name,
                       NULL);
}

char **
gbp_flatpak_runtime_get_refs (GbpFlatpakRuntime *self)
{
  GPtrArray *ar;
  g_autofree char *sdk = NULL;
  g_autofree char *platform = NULL;
  const char *arch;

  g_return_val_if_fail (GBP_IS_FLATPAK_RUNTIME (self), NULL);

  arch = ide_triplet_get_arch (self->triplet);
  platform = g_strdup_printf ("runtime/%s/%s/%s", self->platform, arch, self->branch);
  sdk = g_strdup_printf ("runtime/%s/%s/%s", self->sdk, arch, self->branch);

  ar = g_ptr_array_new ();
  g_ptr_array_add (ar, g_steal_pointer (&sdk));
  if (g_strcmp0 (sdk, platform) != 0)
    g_ptr_array_add (ar, g_steal_pointer (&platform));
  g_ptr_array_add (ar, NULL);

  return (char **)g_ptr_array_free (ar, FALSE);
}
