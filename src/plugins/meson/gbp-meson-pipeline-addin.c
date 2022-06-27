/* gbp-meson-pipeline-addin.c
 *
 * Copyright 2017-2019 Christian Hergert <chergert@redhat.com>
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

#define G_LOG_DOMAIN "gbp-meson-pipeline-addin"

#include <glib/gi18n.h>

#include <libide-gui.h>
#include <libide-projects.h>

#include "gbp-meson-toolchain.h"
#include "gbp-meson-build-stage-cross-file.h"
#include "gbp-meson-build-system.h"
#include "gbp-meson-build-target.h"
#include "gbp-meson-introspection.h"
#include "gbp-meson-pipeline-addin.h"

struct _GbpMesonPipelineAddin
{
  IdeObject              parent_instance;
  GbpMesonIntrospection *introspection;
};

static const gchar *ninja_names[] = { "ninja", "ninja-build", NULL };

static void
on_build_stage_query (IdePipelineStage *stage,
                      IdePipeline      *pipeline,
                      GPtrArray        *targets,
                      GCancellable     *cancellable)
{
  IdeSubprocessLauncher *launcher;
  g_autoptr(GPtrArray) replace = NULL;
  const gchar * const *argv;

  g_assert (IDE_IS_PIPELINE_STAGE (stage));
  g_assert (IDE_IS_PIPELINE (pipeline));
  g_assert (!cancellable || G_IS_CANCELLABLE (cancellable));

  /* Defer to ninja to determine completed status */
  ide_pipeline_stage_set_completed (stage, FALSE);

  /* Clear any previous argv items from a possible previous build */
  launcher = ide_pipeline_stage_launcher_get_launcher (IDE_PIPELINE_STAGE_LAUNCHER (stage));
  argv = ide_subprocess_launcher_get_argv (launcher);
  replace = g_ptr_array_new_with_free_func (g_free);
  for (guint i = 0; argv[i]; i++)
    {
      g_ptr_array_add (replace, g_strdup (argv[i]));
      if (g_strv_contains (ninja_names, argv[i]))
        break;
    }
  g_ptr_array_add (replace, NULL);
  ide_subprocess_launcher_set_argv (launcher, (const gchar * const *)replace->pdata);

  /* If we have targets to build, specify them */
  if (targets != NULL)
    {
      for (guint i = 0; i < targets->len; i++)
        {
          IdeBuildTarget *target = g_ptr_array_index (targets, i);

          if (GBP_IS_MESON_BUILD_TARGET (target))
            {
              const char *builddir = ide_pipeline_get_builddir (pipeline);
              const char *filename = gbp_meson_build_target_get_filename (GBP_MESON_BUILD_TARGET (target));

              if (filename != NULL && g_str_has_prefix (filename, builddir))
                ide_subprocess_launcher_push_argv (launcher, g_path_skip_root (filename + strlen (builddir)));
            }
        }
    }
}

static void
on_install_stage_query (IdePipelineStage *stage,
                        IdePipeline      *pipeline,
                        GPtrArray        *targets,
                        GCancellable     *cancellable)
{
  g_assert (IDE_IS_PIPELINE_STAGE (stage));
  g_assert (IDE_IS_PIPELINE (pipeline));
  g_assert (!cancellable || G_IS_CANCELLABLE (cancellable));

  /* Defer to ninja to determine completed status */
  ide_pipeline_stage_set_completed (stage, FALSE);
}

G_GNUC_NULL_TERMINATED
static IdeRunContext *
create_run_context (GbpMesonPipelineAddin *self,
                    IdePipeline           *pipeline,
                    const char            *argv,
                    ...)
{
  IdeRunContext *run_context;
  va_list args;

  g_assert (GBP_IS_MESON_PIPELINE_ADDIN (self));
  g_assert (IDE_IS_PIPELINE (pipeline));

  run_context = ide_run_context_new ();
  ide_pipeline_prepare_run_context (pipeline, run_context);

  va_start (args, argv);
  while (argv != NULL)
    {
      ide_run_context_append_argv (run_context, argv);
      argv = va_arg (args, const char *);
    }
  va_end (args);

  return run_context;
}

static IdePipelineStage *
attach_run_context (GbpMesonPipelineAddin *self,
                    IdePipeline           *pipeline,
                    IdeRunContext         *build_context,
                    IdeRunContext         *clean_context,
                    const char            *title,
                    IdePipelinePhase       phase)
{
  g_autoptr(IdeSubprocessLauncher) build_launcher = NULL;
  g_autoptr(IdeSubprocessLauncher) clean_launcher = NULL;
  g_autoptr(IdePipelineStage) stage = NULL;
  g_autoptr(GError) error = NULL;
  IdeContext *context;
  guint id;

  g_assert (GBP_IS_MESON_PIPELINE_ADDIN (self));
  g_assert (IDE_IS_PIPELINE (pipeline));
  g_assert (!build_context || IDE_IS_RUN_CONTEXT (build_context));
  g_assert (!clean_context || IDE_IS_RUN_CONTEXT (clean_context));

  context = ide_object_get_context (IDE_OBJECT (pipeline));
  stage = ide_pipeline_stage_launcher_new (context, NULL);

  if (build_context != NULL)
    {
      if (!(build_launcher = ide_run_context_end (build_context, &error)))
        {
          g_critical ("Failed to create launcher from run context: %s",
                      error->message);
          return NULL;
        }
    }

  if (clean_context != NULL)
    {
      if (!(clean_launcher = ide_run_context_end (clean_context, &error)))
        {
          g_critical ("Failed to create launcher from run context: %s",
                      error->message);
          return NULL;
        }
    }

  g_object_set (stage,
                "launcher", build_launcher,
                "clean-launcher", clean_launcher,
                "name", title,
                NULL);

  id = ide_pipeline_attach (pipeline, phase, 0, stage);
  ide_pipeline_addin_track (IDE_PIPELINE_ADDIN (self), id);

  /* We return a borrowed instance */
  return stage;
}

static void
gbp_meson_pipeline_addin_load (IdePipelineAddin *addin,
                               IdePipeline      *pipeline)
{
  GbpMesonPipelineAddin *self = (GbpMesonPipelineAddin *)addin;
  g_autoptr(IdeRunContext) build_context = NULL;
  g_autoptr(IdeRunContext) clean_context = NULL;
  g_autoptr(IdeRunContext) config_context = NULL;
  g_autoptr(IdeRunContext) install_context = NULL;
  IdePipelineStage *stage;
  g_autofree char *build_dot_ninja = NULL;
  g_autofree char *crossbuild_file = NULL;
  g_autofree char *meson = NULL;
  g_autofree char *ninja = NULL;
  IdeBuildSystem *build_system;
  IdeToolchain *toolchain;
  IdeContext *context;
  const char *config_opts;
  const char *prefix;
  const char *srcdir;
  IdeConfig *config;
  guint id;
  int parallel;

  IDE_ENTRY;

  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (GBP_IS_MESON_PIPELINE_ADDIN (self));
  g_assert (IDE_IS_PIPELINE (pipeline));

  config = ide_pipeline_get_config (pipeline);
  context = ide_object_get_context (IDE_OBJECT (pipeline));
  build_system = ide_build_system_from_context (context);

  if (!GBP_IS_MESON_BUILD_SYSTEM (build_system))
    IDE_EXIT;

  srcdir = ide_pipeline_get_srcdir (pipeline);
  config_opts = ide_config_get_config_opts (config);
  prefix = ide_config_get_prefix (config);
  build_dot_ninja = ide_pipeline_build_builddir_path (pipeline, "build.ninja", NULL);
  parallel = ide_config_get_parallelism (config);
  toolchain = ide_pipeline_get_toolchain (pipeline);

  /* Discover program locations for meson/ninja */
  meson = gbp_meson_build_system_locate_meson (GBP_MESON_BUILD_SYSTEM (build_system), pipeline);
  ninja = gbp_meson_build_system_locate_ninja (GBP_MESON_BUILD_SYSTEM (build_system), pipeline);

  /* Create the toolchain file if required */
  if (GBP_IS_MESON_TOOLCHAIN (toolchain))
    {
      crossbuild_file = g_strdup (gbp_meson_toolchain_get_file_path (GBP_MESON_TOOLCHAIN (toolchain)));
    }
  else if (g_strcmp0 (ide_toolchain_get_id (toolchain), "default") != 0)
    {
      g_autoptr(GbpMesonBuildStageCrossFile) cross_file_stage = gbp_meson_build_stage_cross_file_new (toolchain);
      id = ide_pipeline_attach (pipeline, IDE_PIPELINE_PHASE_PREPARE, 0, IDE_PIPELINE_STAGE (cross_file_stage));
      crossbuild_file = gbp_meson_build_stage_cross_file_get_path (cross_file_stage, pipeline);
      ide_pipeline_addin_track (addin, id);
    }

  /* Setup our configure stage */
  config_context = create_run_context (self, pipeline, meson, srcdir, ".", "--prefix", prefix, NULL);
  if (crossbuild_file != NULL)
    ide_run_context_append_formatted (config_context, "--cross-file=%s", crossbuild_file);
  if (!ide_str_empty0 (config_opts))
    ide_run_context_append_args_parsed (config_context, config_opts, NULL);
  stage = attach_run_context (self, pipeline, config_context, NULL,
                              _("Configure project"), IDE_PIPELINE_PHASE_CONFIGURE);
  if (g_file_test (build_dot_ninja, G_FILE_TEST_EXISTS))
    ide_pipeline_stage_set_completed (stage, TRUE);

  /* Setup our Build/Clean stage */
  clean_context = create_run_context (self, pipeline, ninja, "clean", NULL);
  build_context = create_run_context (self, pipeline, ninja, NULL);
  if (parallel > 0)
    ide_run_context_append_formatted (build_context, "-j%u", parallel);
  stage = attach_run_context (self, pipeline, build_context, clean_context,
                              _("Build project"), IDE_PIPELINE_PHASE_BUILD);
  ide_pipeline_stage_set_check_stdout (stage, TRUE);
  g_signal_connect (stage, "query", G_CALLBACK (on_build_stage_query), NULL);

  /* Setup our Install stage */
  install_context = create_run_context (self, pipeline, ninja, "install", NULL);
  stage = attach_run_context (self, pipeline, install_context, NULL,
                              _("Install project"), IDE_PIPELINE_PHASE_INSTALL);
  g_signal_connect (stage, "query", G_CALLBACK (on_install_stage_query), NULL);

  /* Setup our introspection stage */
  self->introspection = gbp_meson_introspection_new (pipeline);
  id = ide_pipeline_attach (pipeline,
                            IDE_PIPELINE_PHASE_CONFIGURE | IDE_PIPELINE_PHASE_AFTER,
                            0,
                            IDE_PIPELINE_STAGE (self->introspection));
  ide_pipeline_addin_track (addin, id);

  IDE_EXIT;
}

static void
pipeline_addin_iface_init (IdePipelineAddinInterface *iface)
{
  iface->load = gbp_meson_pipeline_addin_load;
}

G_DEFINE_FINAL_TYPE_WITH_CODE (GbpMesonPipelineAddin, gbp_meson_pipeline_addin, IDE_TYPE_OBJECT,
                               G_IMPLEMENT_INTERFACE (IDE_TYPE_PIPELINE_ADDIN, pipeline_addin_iface_init))

static void
gbp_meson_pipeline_addin_dispose (GObject *object)
{
  GbpMesonPipelineAddin *self = (GbpMesonPipelineAddin *)object;

  g_clear_object (&self->introspection);

  G_OBJECT_CLASS (gbp_meson_pipeline_addin_parent_class)->dispose (object);
}

static void
gbp_meson_pipeline_addin_class_init (GbpMesonPipelineAddinClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->dispose = gbp_meson_pipeline_addin_dispose;
}

static void
gbp_meson_pipeline_addin_init (GbpMesonPipelineAddin *self)
{
}

GbpMesonIntrospection *
gbp_meson_pipeline_addin_get_introspection (GbpMesonPipelineAddin *self)
{
  g_return_val_if_fail (GBP_IS_MESON_PIPELINE_ADDIN (self), NULL);

  return self->introspection;
}
