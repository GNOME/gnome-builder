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

#include <libide-threading.h>
#include <libide-vcs.h>

#include "ide-config-private.h"

#include "gbp-flatpak-aux.h"
#include "gbp-flatpak-manifest.h"
#include "gbp-flatpak-runtime.h"
#include "gbp-flatpak-util.h"

struct _GbpFlatpakRuntime
{
  IdeRuntime parent_instance;

  IdePathCache *path_cache;

  IdeTriplet *triplet;
  char *branch;
  char *deploy_dir;
  char *platform;
  char *sdk;
  char *runtime_dir;
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

static gboolean
gbp_flatpak_runtime_contains_program_in_path (IdeRuntime   *runtime,
                                              const char   *program,
                                              GCancellable *cancellable)
{
  static const char *known_path_dirs[] = { "/bin" };
  GbpFlatpakRuntime *self = (GbpFlatpakRuntime *)runtime;
  gboolean ret = FALSE;
  gboolean found;

  g_assert (GBP_IS_FLATPAK_RUNTIME (self));
  g_assert (program != NULL);
  g_assert (!cancellable || G_IS_CANCELLABLE (cancellable));

  if (ide_path_cache_contains (self->path_cache, program, &found))
    return found;

  for (guint i = 0; i < G_N_ELEMENTS (known_path_dirs); i++)
    {
      g_autofree char *path = NULL;

      path = g_build_filename (self->deploy_dir,
                               "files",
                               known_path_dirs[i],
                               program,
                               NULL);

      /* Check that the file exists instead of things like IS_EXECUTABLE.  The
       * reason we MUST check for either EXISTS or _IS_SYMLINK separately is
       * that EXISTS will check that the destination file exists too. That may
       * not be possible until the mount namespaces are correctly setup.
       *
       * See https://gitlab.gnome.org/GNOME/gnome-builder/-/issues/2050#note_1841120
       */
      if (g_file_test (path, G_FILE_TEST_IS_SYMLINK) ||
          g_file_test (path, G_FILE_TEST_EXISTS))
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
      IdeContext *context = ide_object_get_context (IDE_OBJECT (self));
      IdeRuntimeManager *manager = ide_runtime_manager_from_context (context);
      g_autofree char *arch = ide_runtime_get_arch (runtime);
      g_autofree char *sdk_id = g_strdup_printf ("flatpak:%s/%s/%s", self->sdk, arch, self->branch);
      IdeRuntime *sdk = ide_runtime_manager_get_runtime (manager, sdk_id);

      if (sdk != NULL && sdk != runtime)
        ret = ide_runtime_contains_program_in_path (sdk, program, cancellable);
    }

  /* Cache both positive and negative lookups */
  ide_path_cache_insert (self->path_cache, program, ret ? program : NULL);

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
maybe_profiling (const char * const *argv)
{
  if (argv == NULL)
    return FALSE;

  for (guint i = 0; argv[i]; i++)
    {
      if (strstr (argv[i], "sysprof-agent"))
        return TRUE;
    }

   return FALSE;
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
  gbp_flatpak_set_config_dir (run_context);

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

#if 0
  {
    const char *wayland_display;

    /* Make sure wayland socket is available. */
    if ((wayland_display = g_getenv ("WAYLAND_DISPLAY")))
      ide_run_context_append_formatted (run_context,
                                        "--bind-mount=/run/user/%u/%s=/run/user/%u/%s",
                                        getuid (), wayland_display, getuid (), wayland_display);
  }
#endif

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
        ide_run_context_append_formatted (run_context, "--env=%s", env[i]);
    }

  /* Make sure all of our finish arguments for the manifest are included */
  if (GBP_IS_FLATPAK_MANIFEST (config))
    {
      const char * const *finish_args = gbp_flatpak_manifest_get_finish_args (GBP_FLATPAK_MANIFEST (config));

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

  /* Give access to portals */
  ide_run_context_append_argv (run_context, "--talk-name=org.freedesktop.portal.*");

  /* Layering violation, but always give access to profiler */
  if (maybe_profiling (argv))
    {
      ide_run_context_append_argv (run_context, "--system-talk-name=org.gnome.Sysprof3");
      ide_run_context_append_argv (run_context, "--system-talk-name=org.freedesktop.PolicyKit1");
      ide_run_context_append_argv (run_context, "--filesystem=~/.local/share/flatpak:ro");
      ide_run_context_append_argv (run_context, "--filesystem=host");
    }

  /* Make A11y bus available to the application */
  {
    const char *a11y_bus_unix_path = NULL;
    const char *a11y_bus_address_suffix = NULL;
    const char *a11y_bus = gbp_flatpak_get_a11y_bus (&a11y_bus_unix_path, &a11y_bus_address_suffix);

    ide_run_context_append_argv (run_context, "--talk-name=org.a11y.Bus");

    if (a11y_bus != NULL && a11y_bus_unix_path != NULL)
      {
        ide_run_context_append_formatted (run_context,
                                          "--bind-mount=/run/flatpak/at-spi-bus=%s",
                                          a11y_bus_unix_path);
        ide_run_context_append_formatted (run_context,
                                          "--env=AT_SPI_BUS_ADDRESS=unix:path=/run/flatpak/at-spi-bus%s",
                                          a11y_bus_address_suffix ? a11y_bus_address_suffix : "");
      }
  }

  /* Make sure we have access to user installed fonts for gbp-flatpak-aux.c */
  ide_run_context_append_argv (run_context, "--filesystem=~/.local/share/fonts:ro");

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

static char *
join_paths (const char *prepend,
            const char *path,
            const char *append)
{
  g_autofree char *tmp = ide_search_path_prepend (path, prepend);
  return ide_search_path_append (tmp, append);
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
  g_autofree char *new_path = NULL;
  g_autofree char *default_cache_root = NULL;
  const char *path;
  const char *prepend_path;
  const char *append_path;
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
  gbp_flatpak_set_config_dir (run_context);

  /* We might need access to ccache configs inside the build environ.
   * Usually, this is set by flatpak-builder, but since we are running
   * the incremental build, we need to take care of that too.
   *
   * See https://bugzilla.gnome.org/show_bug.cgi?id=780153
   */
  default_cache_root = ide_dup_default_cache_dir ();
  ccache_dir = g_build_filename (default_cache_root,
                                 "flatpak-builder",
                                 "ccache",
                                 NULL);
  ide_run_context_setenv (run_context, "CCACHE_DIR", ccache_dir);

  /* We can pass the CWD directory down just fine */
  ide_run_context_set_cwd (run_context, cwd);

  /* We want some environment available to the `flatpak build` environment
   * so that we can have working termcolor support.
   */
  ide_run_context_setenv (run_context, "TERM", "xterm-256color");
  ide_run_context_setenv (run_context, "COLORTERM", "truecolor");

  /* Now setup our basic arguments for the application */
  ide_run_context_append_argv (run_context, "flatpak");
  ide_run_context_append_argv (run_context, "build");
  ide_run_context_append_argv (run_context, "--with-appdir");
  ide_run_context_append_argv (run_context, "--allow=devel");
  ide_run_context_append_argv (run_context, "--die-with-parent");

  /* Make sure we have access to the build directory */
  ide_run_context_append_formatted (run_context, "--filesystem=%s", srcdir);
  ide_run_context_append_formatted (run_context, "--filesystem=%s", builddir);
  ide_run_context_append_formatted (run_context, "--filesystem=%s", default_cache_root);
  ide_run_context_append_argv (run_context, "--nofilesystem=host");

  /* Ensure build-args are passed down */
  if (GBP_IS_FLATPAK_MANIFEST (config))
    {
      const char * const *build_args = gbp_flatpak_manifest_get_build_args (GBP_FLATPAK_MANIFEST (config));
      if (build_args != NULL)
        ide_run_context_append_args (run_context, build_args);
    }

  /* Always include `--share=network` because incremental building tends
   * to be different than one-shot building for a Flatpak build as developers
   * are likely to not have all the deps fetched via submodules they just
   * changed or even additional sources within the app's manifest module.
   *
   * See https://gitlab.gnome.org/GNOME/gnome-builder/-/issues/1775 for
   * more information. Having flatpak-builder as a library could allow us
   * to not require these sorts of workarounds.
   */
  if (!g_strv_contains (ide_run_context_get_argv (run_context), "--share=network"))
    ide_run_context_append_argv (run_context, "--share=network");

  /* Prepare an alternate PATH */
  if (!(path = g_environ_getenv ((char **)env, "PATH")))
    path = "/app/bin:/usr/bin";
  prepend_path = ide_config_get_prepend_path (config);
  append_path = ide_config_get_append_path (config);
  new_path = join_paths (prepend_path, path, append_path);

  /* Convert environment from upper level into --env=FOO=BAR */
  if (env != NULL)
    {
      for (guint i = 0; env[i]; i++)
        {
          if (new_path == NULL || !ide_str_equal0 (env[i], "PATH"))
            ide_run_context_append_formatted (run_context, "--env=%s", env[i]);
        }
    }

  if (new_path != NULL)
    ide_run_context_append_formatted (run_context, "--env=PATH=%s", new_path);

  /* If an "env" is set in the build-options for the module, set that */
  if (GBP_IS_FLATPAK_MANIFEST (config))
    gbp_flatpak_manifest_apply_primary_env (GBP_FLATPAK_MANIFEST (config), run_context);

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

  if (GBP_IS_FLATPAK_MANIFEST (config))
    {
      GbpFlatpakManifest *manifest = GBP_FLATPAK_MANIFEST (config);
      const char *build_system = gbp_flatpak_manifest_get_primary_build_system (manifest);

      if (build_system == NULL)
        build_system = "autotools";

      if (g_str_equal (build_system, "autotools"))
        ide_config_replace_config_opt (config, "--libdir", "/app/lib");
      else if (g_str_equal (build_system, "cmake") ||
               g_str_equal (build_system, "cmake-ninja"))
        {
          /* Only override if not manually set by project */
          if (!_ide_config_has_config_opt (config, "-DCMAKE_INSTALL_LIBDIR:PATH"))
            ide_config_replace_config_opt (config, "-DCMAKE_INSTALL_LIBDIR:PATH", "lib");
        }
      else if (g_str_equal (build_system, "meson"))
        ide_config_replace_config_opt (config, "--libdir", "lib");
    }
}

static void
gbp_flatpak_runtime_set_deploy_dir (GbpFlatpakRuntime *self,
                                    const char        *deploy_dir)
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

const char *
gbp_flatpak_runtime_get_branch (GbpFlatpakRuntime *self)
{
  g_return_val_if_fail (GBP_IS_FLATPAK_RUNTIME (self), NULL);

  return self->branch;
}

static void
gbp_flatpak_runtime_set_branch (GbpFlatpakRuntime *self,
                                const char        *branch)
{
  g_return_if_fail (GBP_IS_FLATPAK_RUNTIME (self));

  if (g_set_str (&self->branch, branch))
    g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_BRANCH]);
}

const char *
gbp_flatpak_runtime_get_platform (GbpFlatpakRuntime *self)
{
  g_return_val_if_fail (GBP_IS_FLATPAK_RUNTIME (self), NULL);

  return self->platform;
}

static void
gbp_flatpak_runtime_set_platform (GbpFlatpakRuntime *self,
                                  const char        *platform)
{
  g_return_if_fail (GBP_IS_FLATPAK_RUNTIME (self));

  if (g_set_str (&self->platform, platform))
    g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_PLATFORM]);
}

const char *
gbp_flatpak_runtime_get_sdk (GbpFlatpakRuntime *self)
{
  g_return_val_if_fail (GBP_IS_FLATPAK_RUNTIME (self), NULL);

  return self->sdk;
}

char *
gbp_flatpak_runtime_get_sdk_name (GbpFlatpakRuntime *self)
{
  const char *slash;

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
                             const char        *sdk)
{
  g_return_if_fail (GBP_IS_FLATPAK_RUNTIME (self));

  if (g_set_str (&self->sdk, sdk))
    {
      g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_SDK]);
    }
}

static char **
gbp_flatpak_runtime_get_system_include_dirs (IdeRuntime *runtime)
{
  static const char *include_dirs[] = { "/app/include", "/usr/include", NULL };
  return g_strdupv ((char **)include_dirs);
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
  g_clear_object (&self->deploy_dir_files);
  g_clear_object (&self->path_cache);

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

  runtime_class->contains_program_in_path = gbp_flatpak_runtime_contains_program_in_path;
  runtime_class->prepare_configuration = gbp_flatpak_runtime_prepare_configuration;
  runtime_class->prepare_to_build = gbp_flatpak_runtime_prepare_to_build;
  runtime_class->prepare_to_run = gbp_flatpak_runtime_prepare_to_run;
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
  self->path_cache = ide_path_cache_new ();

  ide_runtime_set_icon_name (IDE_RUNTIME (self), "ui-container-flatpak-symbolic");
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
  g_autofree char *id = NULL;
  g_autofree char *short_id = NULL;
  g_autofree char *triplet = NULL;
  g_autofree char *system_arch = NULL;
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

  system_arch = ide_get_system_arch ();

  if (ide_str_equal0 (system_arch, arch))
    g_string_append (category, name);
  else
    g_string_append_printf (category, "%s (%s)", name, arch);

  return g_object_new (GBP_TYPE_FLATPAK_RUNTIME,
                       "id", id,
                       "short-id", short_id,
                       "triplet", triplet_object,
                       "branch", branch,
                       "category", category->str,
                       "name", triplet,
                       "deploy-dir", deploy_dir,
                       "display-name", triplet,
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

const char *
gbp_flatpak_runtime_get_deploy_dir (GbpFlatpakRuntime *self)
{
  g_return_val_if_fail (GBP_IS_FLATPAK_RUNTIME (self), NULL);

  return self->deploy_dir;
}
