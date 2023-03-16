/* gbp-host-runtime.c
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

#define G_LOG_DOMAIN "gbp-host-runtime"

#include "config.h"

#include <libide-foundry.h>

#include "gbp-host-runtime.h"

struct _GbpHostRuntime
{
  IdeRuntime    parent_instance;
  IdePathCache *path_cache;
};

G_DEFINE_FINAL_TYPE (GbpHostRuntime, gbp_host_runtime, IDE_TYPE_RUNTIME)

static gboolean
gbp_host_runtime_native_contains_program_in_path (IdeRuntime   *runtime,
                                                  const char   *program,
                                                  GCancellable *cancellable)
{
  GbpHostRuntime *self = (GbpHostRuntime *)runtime;
  g_autofree char *path = NULL;
  gboolean found;

  g_assert (GBP_IS_HOST_RUNTIME (self));
  g_assert (program != NULL);

  if (ide_path_cache_contains (self->path_cache, program, &found))
    return found;

  path = g_find_program_in_path (program);
  ide_path_cache_insert (self->path_cache, program, path);
  return path != NULL;
}

static gboolean
gbp_host_runtime_flatpak_contains_program_in_path (IdeRuntime   *runtime,
                                                   const char   *program,
                                                   GCancellable *cancellable)
{
  GbpHostRuntime *self = (GbpHostRuntime *)runtime;
  g_autoptr(IdeSubprocess) subprocess = NULL;
  g_autoptr(IdeRunContext) run_context = NULL;
  g_autoptr(GError) error = NULL;
  gboolean found = FALSE;

  IDE_ENTRY;

  g_assert (GBP_IS_HOST_RUNTIME (runtime));
  g_assert (program != NULL);
  g_assert (!cancellable || G_IS_CANCELLABLE (cancellable));

  if (ide_path_cache_contains (self->path_cache, program, &found))
    return found;

  run_context = ide_run_context_new ();
  ide_run_context_push_host (run_context);
  ide_run_context_add_minimal_environment (run_context);
  ide_run_context_push_user_shell (run_context, IDE_RUN_CONTEXT_SHELL_LOGIN);
  ide_run_context_append_argv (run_context, "which");
  ide_run_context_append_argv (run_context, program);

  ide_run_context_take_fd (run_context, -1, STDOUT_FILENO);
  ide_run_context_take_fd (run_context, -1, STDERR_FILENO);

  if ((subprocess = ide_run_context_spawn (run_context, &error)))
    {
      found = ide_subprocess_wait_check (subprocess, cancellable, NULL);
      ide_path_cache_insert (self->path_cache, program, found ? program : NULL);
      IDE_RETURN (found);
    }

  g_warning ("Failed to spawn subprocess: %s", error->message);

  IDE_RETURN (FALSE);
}

static void
gbp_host_runtime_prepare_to_build (IdeRuntime    *runtime,
                                   IdePipeline   *pipeline,
                                   IdeRunContext *run_context)
{
  IDE_ENTRY;

  g_assert (GBP_IS_HOST_RUNTIME (runtime));
  g_assert (IDE_IS_PIPELINE (pipeline));
  g_assert (IDE_IS_RUN_CONTEXT (run_context));

  ide_run_context_push_host (run_context);
  ide_run_context_add_minimal_environment (run_context);
  ide_run_context_push_user_shell (run_context, IDE_RUN_CONTEXT_SHELL_LOGIN);

  IDE_EXIT;
}

void
_gbp_host_runtime_prepare_to_run (IdePipeline   *pipeline,
                                  IdeRunContext *run_context)
{
  g_autofree char *libdir = NULL;
  const char *prefix;
  IdeConfig *config;

  IDE_ENTRY;

  g_assert (IDE_IS_PIPELINE (pipeline));
  g_assert (IDE_IS_RUN_CONTEXT (run_context));

  ide_run_context_push_host (run_context);
  ide_run_context_add_minimal_environment (run_context);
  ide_run_context_push_user_shell (run_context, IDE_RUN_CONTEXT_SHELL_LOGIN);

  config = ide_pipeline_get_config (pipeline);
  prefix = ide_config_get_prefix (config);

  /* PATH */
  {
    const char *path = ide_get_user_default_path ();
    g_autofree char *bindir = g_build_filename (prefix, "bin", NULL);
    g_autofree char *newpath = g_strdup_printf ("%s" G_SEARCHPATH_SEPARATOR_S "%s", bindir, path);

    ide_run_context_setenv (run_context, "PATH", newpath);
  }

  /* LD_LIBRARY_PATH */
  {
    static const gchar *tries[] = { "lib64", "lib", "lib32", };

    for (guint i = 0; i < G_N_ELEMENTS (tries); i++)
      {
        g_autofree gchar *ld_library_path = g_build_filename (prefix, tries[i], NULL);

        if (g_file_test (ld_library_path, G_FILE_TEST_IS_DIR))
          {
            ide_run_context_setenv (run_context, "LD_LIBRARY_PATH", ld_library_path);
            libdir = g_steal_pointer (&ld_library_path);
            break;
          }
      }
  }

  /* GSETTINGS_SCHEMA_DIR */
  {
    g_autofree gchar *schemadir = NULL;

    schemadir = g_build_filename (prefix, "share", "glib-2.0", "schemas", NULL);
    ide_run_context_setenv (run_context, "GSETTINGS_SCHEMA_DIR", schemadir);
  }

  /* GI_TYPELIB_PATH */
  if (libdir != NULL)
    {
      g_autofree char *typelib_path = g_build_filename (libdir, "girepository-1.0", NULL);
      ide_run_context_setenv (run_context, "GI_TYPELIB_PATH", typelib_path);
    }

  IDE_EXIT;
}

void
gbp_host_runtime_prepare_to_run (IdeRuntime    *runtime,
                                 IdePipeline   *pipeline,
                                 IdeRunContext *run_context)
{
  g_assert (GBP_IS_HOST_RUNTIME (runtime));
  g_assert (IDE_IS_PIPELINE (pipeline));
  g_assert (IDE_IS_RUN_CONTEXT (run_context));

  _gbp_host_runtime_prepare_to_run (pipeline, run_context);
}

static void
gbp_host_runtime_finalize (GObject *object)
{
  GbpHostRuntime *self = (GbpHostRuntime *)object;

  g_clear_object (&self->path_cache);

  G_OBJECT_CLASS (gbp_host_runtime_parent_class)->finalize (object);
}

static void
gbp_host_runtime_class_init (GbpHostRuntimeClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  IdeRuntimeClass *runtime_class = IDE_RUNTIME_CLASS (klass);

  object_class->finalize = gbp_host_runtime_finalize;

  if (ide_is_flatpak ())
    runtime_class->contains_program_in_path = gbp_host_runtime_flatpak_contains_program_in_path;
  else
    runtime_class->contains_program_in_path = gbp_host_runtime_native_contains_program_in_path;

  runtime_class->prepare_to_run = gbp_host_runtime_prepare_to_run;
  runtime_class->prepare_to_build = gbp_host_runtime_prepare_to_build;
}

static void
gbp_host_runtime_init (GbpHostRuntime *self)
{
  self->path_cache = ide_path_cache_new ();

  ide_runtime_set_icon_name (IDE_RUNTIME (self), "ui-container-host-symbolic");
}
