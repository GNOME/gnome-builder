/* gbp-jhbuild-runtime.c
 *
 * Copyright 2022 Christian Hergert <chergert@redhat.com>
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

#define G_LOG_DOMAIN "gbp-jhbuild-runtime"

#include "config.h"

#ifdef PLUGIN_MESON
# include "meson/gbp-meson-build-system.h"
#endif

#include "gbp-jhbuild-runtime.h"

struct _GbpJhbuildRuntime
{
  IdeRuntime    parent_instance;
  IdePathCache *path_cache;
  char         *executable_path;
  char         *install_prefix;
};

enum {
  PROP_0,
  PROP_EXECUTABLE_PATH,
  PROP_INSTALL_PREFIX,
  N_PROPS
};

G_DEFINE_FINAL_TYPE (GbpJhbuildRuntime, gbp_jhbuild_runtime, IDE_TYPE_RUNTIME)

static GParamSpec *properties [N_PROPS];

static gboolean
gbp_jhbuild_runtime_run_handler (IdeRunContext       *run_context,
                                 const char * const  *argv,
                                 const char * const  *env,
                                 const char          *cwd,
                                 IdeUnixFDMap        *unix_fd_map,
                                 gpointer             user_data,
                                 GError             **error)
{
  GbpJhbuildRuntime *self = user_data;

  IDE_ENTRY;

  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (GBP_IS_JHBUILD_RUNTIME (self));
  g_assert (IDE_IS_RUN_CONTEXT (run_context));
  g_assert (IDE_IS_UNIX_FD_MAP (unix_fd_map));

  /* First merge our FDs so we can be sure there are no collisions (there
   * shouldn't be because we didn't set anything here).
   */
  if (!ide_run_context_merge_unix_fd_map (run_context, unix_fd_map, error))
    return FALSE;

  /* We always take the CWD of the upper layer */
  ide_run_context_set_cwd (run_context, cwd);

  /* We rewrite the argv to be "jhbuild run ..." */
  ide_run_context_set_argv (run_context, IDE_STRV_INIT (self->executable_path, "run"));

  /* If there is an environment to deliver, then we want that passed to the
   * subprocess but not to affect the parent process (jhbuild). So it will now
   * look something like "jhbuild run env FOO=BAR ..."
   */
  if (env != NULL && env[0] != NULL)
    {
      ide_run_context_append_argv (run_context, "env");
      ide_run_context_append_args (run_context, env);
    }

  /* And now we can add the argv of the upper layers so it might look something
   * like "jhbuild run env FOO=BAR valgrind env BAR=BAZ my-program"
   */
  ide_run_context_append_args (run_context, argv);

  IDE_RETURN (TRUE);
}

static void
gbp_jhbuild_runtime_prepare_run_context (IdeRuntime    *runtime,
                                         IdePipeline   *pipeline,
                                         IdeRunContext *run_context)
{
  GbpJhbuildRuntime *self = (GbpJhbuildRuntime *)runtime;

  IDE_ENTRY;

  g_assert (GBP_IS_JHBUILD_RUNTIME (self));
  g_assert (IDE_IS_PIPELINE (pipeline));
  g_assert (IDE_IS_RUN_CONTEXT (run_context));

  ide_run_context_push_host (run_context);
  ide_run_context_push (run_context,
                        gbp_jhbuild_runtime_run_handler,
                        g_object_ref (self),
                        g_object_unref);

  IDE_EXIT;
}

static gboolean
gbp_jhbuild_runtime_contains_program_in_path (IdeRuntime   *runtime,
                                              const char   *program,
                                              GCancellable *cancellable)
{
  GbpJhbuildRuntime *self = (GbpJhbuildRuntime *)runtime;
  g_autoptr(IdeRunContext) run_context = NULL;
  g_autoptr(IdeSubprocess) subprocess = NULL;
  g_autoptr(GError) error = NULL;
  gboolean found;

  g_assert (GBP_IS_JHBUILD_RUNTIME (self));
  g_assert (program != NULL);
  g_assert (!cancellable || G_IS_CANCELLABLE (cancellable));

  if (ide_path_cache_contains (self->path_cache, program, &found))
    return found;

  run_context = ide_run_context_new ();
  ide_run_context_push_host (run_context);
  ide_run_context_push (run_context,
                        gbp_jhbuild_runtime_run_handler,
                        g_object_ref (self),
                        g_object_unref);

  /* Will use /bin/sh -l -c 'which program' */
  ide_run_context_push_shell (run_context, TRUE);
  ide_run_context_append_argv (run_context, "which");
  ide_run_context_append_argv (run_context, program);

  ide_run_context_take_fd (run_context, -1, STDOUT_FILENO);
  ide_run_context_take_fd (run_context, -1, STDERR_FILENO);

  if (!(subprocess = ide_run_context_spawn (run_context, &error)))
    {
      g_warning ("Failed to spawn subprocess: %s", error->message);
      return FALSE;
    }

  found = ide_subprocess_wait_check (subprocess, cancellable, NULL);
  ide_path_cache_insert (self->path_cache, program, found ? program : NULL);
  return found;
}

static void
gbp_jhbuild_runtime_prepare_configuration (IdeRuntime *runtime,
                                           IdeConfig  *config)
{
  GbpJhbuildRuntime *self = (GbpJhbuildRuntime *)runtime;

  g_assert (GBP_IS_JHBUILD_RUNTIME (self));
  g_assert (IDE_IS_CONFIG (config));

  g_object_set (config,
                "prefix", self->install_prefix,
                "prefix-set", FALSE,
                NULL);

#ifdef PLUGIN_MESON
  {
    IdeContext *context = ide_object_get_context (IDE_OBJECT (runtime));
    IdeBuildSystem *build_system = ide_build_system_from_context (context);

    if (GBP_IS_MESON_BUILD_SYSTEM (build_system))
      ide_config_replace_config_opt (config, "--libdir", "lib");
  }
#endif
}

static void
gbp_jhbuild_runtime_finalize (GObject *object)
{
  GbpJhbuildRuntime *self = (GbpJhbuildRuntime *)object;

  g_clear_pointer (&self->executable_path, g_free);
  g_clear_pointer (&self->install_prefix, g_free);
  g_clear_object (&self->path_cache);

  G_OBJECT_CLASS (gbp_jhbuild_runtime_parent_class)->finalize (object);
}

static void
gbp_jhbuild_runtime_get_property (GObject    *object,
                                  guint       prop_id,
                                  GValue     *value,
                                  GParamSpec *pspec)
{
  GbpJhbuildRuntime *self = GBP_JHBUILD_RUNTIME (object);

  switch (prop_id)
    {
    case PROP_EXECUTABLE_PATH:
      g_value_set_string (value, self->executable_path);
      break;

    case PROP_INSTALL_PREFIX:
      g_value_set_string (value, self->install_prefix);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
gbp_jhbuild_runtime_set_property (GObject      *object,
                                  guint         prop_id,
                                  const GValue *value,
                                  GParamSpec   *pspec)
{
  GbpJhbuildRuntime *self = GBP_JHBUILD_RUNTIME (object);

  switch (prop_id)
    {
    case PROP_EXECUTABLE_PATH:
      self->executable_path = g_value_dup_string (value);
      break;

    case PROP_INSTALL_PREFIX:
      self->install_prefix = g_value_dup_string (value);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
gbp_jhbuild_runtime_class_init (GbpJhbuildRuntimeClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  IdeRuntimeClass *runtime_class = IDE_RUNTIME_CLASS (klass);

  object_class->finalize = gbp_jhbuild_runtime_finalize;
  object_class->get_property = gbp_jhbuild_runtime_get_property;
  object_class->set_property = gbp_jhbuild_runtime_set_property;

  runtime_class->contains_program_in_path = gbp_jhbuild_runtime_contains_program_in_path;
  runtime_class->prepare_configuration = gbp_jhbuild_runtime_prepare_configuration;
  runtime_class->prepare_to_build = gbp_jhbuild_runtime_prepare_run_context;
  runtime_class->prepare_to_run = gbp_jhbuild_runtime_prepare_run_context;

  properties [PROP_EXECUTABLE_PATH] =
    g_param_spec_string ("executable-path", NULL, NULL,
                         NULL,
                         (G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS));

  properties [PROP_INSTALL_PREFIX] =
    g_param_spec_string ("install-prefix", NULL, NULL,
                         NULL,
                         (G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, N_PROPS, properties);
}

static void
gbp_jhbuild_runtime_init (GbpJhbuildRuntime *self)
{
  self->path_cache = ide_path_cache_new ();

  ide_runtime_set_icon_name (IDE_RUNTIME (self), "ui-container-jhbuild-symbolic");
}
