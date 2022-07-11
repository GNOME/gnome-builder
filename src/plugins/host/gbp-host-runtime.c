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

#include "gbp-host-runtime.h"

struct _GbpHostRuntime
{
  IdeRuntime parent_instance;
};

G_DEFINE_FINAL_TYPE (GbpHostRuntime, gbp_host_runtime, IDE_TYPE_RUNTIME)

static gboolean
gbp_host_runtime_native_contains_program_in_path (IdeRuntime   *runtime,
                                                  const char   *program,
                                                  GCancellable *cancellable)
{
  return g_find_program_in_path (program) != NULL;
}

static gboolean
gbp_host_runtime_flatpak_contains_program_in_path (IdeRuntime   *runtime,
                                                   const char   *program,
                                                   GCancellable *cancellable)
{
  g_autoptr(IdeSubprocess) subprocess = NULL;
  g_autoptr(IdeRunContext) run_context = NULL;
  g_autoptr(GError) error = NULL;
  gboolean ret = FALSE;

  IDE_ENTRY;

  g_assert (GBP_IS_HOST_RUNTIME (runtime));
  g_assert (program != NULL);
  g_assert (!cancellable || G_IS_CANCELLABLE (cancellable));

  run_context = ide_run_context_new ();
  ide_run_context_push_host (run_context);
  ide_run_context_push_shell (run_context, TRUE);
  ide_run_context_append_argv (run_context, "which");
  ide_run_context_append_argv (run_context, program);

  if ((subprocess = ide_run_context_spawn (run_context, &error)))
    ret = ide_subprocess_wait_check (subprocess, cancellable, NULL);

  if (error != NULL)
    g_warning ("Failed to spawn subprocess: %s", error->message);

  IDE_RETURN (ret);
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

  IDE_EXIT;
}

static void
gbp_host_runtime_prepare_to_run (IdeRuntime    *runtime,
                                 IdePipeline   *pipeline,
                                 IdeRunContext *run_context)
{
  g_autofree char *libdir = NULL;
  const char *prefix;
  IdeConfig *config;

  IDE_ENTRY;

  g_assert (GBP_IS_HOST_RUNTIME (runtime));
  g_assert (IDE_IS_PIPELINE (pipeline));
  g_assert (IDE_IS_RUN_CONTEXT (run_context));

  ide_run_context_push_host (run_context);

  config = ide_pipeline_get_config (pipeline);
  prefix = ide_config_get_prefix (config);

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

static void
gbp_host_runtime_class_init (GbpHostRuntimeClass *klass)
{
  IdeRuntimeClass *runtime_class = IDE_RUNTIME_CLASS (klass);

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
}
