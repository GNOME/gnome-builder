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
#include "gbp-meson-utils.h"

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
  g_autoptr(IdeRunCommand) command = NULL;
  g_autoptr(GPtrArray) replace = NULL;
  const gchar * const *argv;

  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (IDE_IS_PIPELINE_STAGE_COMMAND (stage));
  g_assert (IDE_IS_PIPELINE (pipeline));
  g_assert (!cancellable || G_IS_CANCELLABLE (cancellable));

  /* Defer to ninja to determine completed status */
  ide_pipeline_stage_set_completed (stage, FALSE);

  /* Get the build command, as we might need to rewrite the argv */
  g_object_get (stage, "build-command", &command, NULL);
  if (command == NULL || !(argv = ide_run_command_get_argv (command)))
    return;

  /* Create new argv to start from */
  replace = g_ptr_array_new_with_free_func (g_free);
  for (guint i = 0; argv[i]; i++)
    {
      g_ptr_array_add (replace, g_strdup (argv[i]));
      if (g_strv_contains (ninja_names, argv[i]))
        break;
    }
  g_ptr_array_add (replace, NULL);

  /* Apply truncated argv */
  ide_run_command_set_argv (command, (const gchar * const *)replace->pdata);

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
                ide_run_command_append_argv (command, g_path_skip_root (filename + strlen (builddir)));
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
static IdeRunCommand *
create_run_command (const char *argv, ...)
{
  IdeRunCommand *run_command;
  va_list args;

  run_command = ide_run_command_new ();

  va_start (args, argv);
  while (argv != NULL)
    {
      ide_run_command_append_argv (run_command, argv);
      argv = va_arg (args, const char *);
    }
  va_end (args);

  return g_steal_pointer (&run_command);
}

static IdePipelineStage *
attach_run_command (GbpMesonPipelineAddin *self,
                    IdePipeline           *pipeline,
                    IdeRunCommand         *build_command,
                    IdeRunCommand         *clean_command,
                    const char            *title,
                    IdePipelinePhase       phase)
{
  g_autoptr(IdePipelineStage) stage = NULL;
  guint id;

  g_assert (GBP_IS_MESON_PIPELINE_ADDIN (self));
  g_assert (IDE_IS_PIPELINE (pipeline));
  g_assert (!build_command || IDE_IS_RUN_COMMAND (build_command));
  g_assert (!clean_command || IDE_IS_RUN_COMMAND (clean_command));

  stage = ide_pipeline_stage_command_new (build_command, clean_command);
  ide_pipeline_stage_set_name (stage, title);

  id = ide_pipeline_attach (pipeline, phase, 0, stage);
  ide_pipeline_addin_track (IDE_PIPELINE_ADDIN (self), id);

  /* We return a borrowed instance */
  return stage;
}

static int
is_newer (const char *old,
          const char *new)
{
  g_autoptr(GFile) file_a = g_file_new_for_path (old);
  g_autoptr(GFile) file_b = g_file_new_for_path (new);
  g_autoptr(GFileInfo) info_a = g_file_query_info (file_a, G_FILE_ATTRIBUTE_TIME_MODIFIED, 0, NULL, NULL);
  g_autoptr(GFileInfo) info_b = g_file_query_info (file_b, G_FILE_ATTRIBUTE_TIME_MODIFIED, 0, NULL, NULL);
  guint64 mtime_a;
  guint64 mtime_b;

  if (info_a == NULL || info_b == NULL)
    return FALSE;

  mtime_a = g_file_info_get_attribute_uint64 (info_a, G_FILE_ATTRIBUTE_TIME_MODIFIED);
  mtime_b = g_file_info_get_attribute_uint64 (info_b, G_FILE_ATTRIBUTE_TIME_MODIFIED);

  return mtime_b > mtime_a;
}

static void
devenv_query_cb (IdePipelineStage *stage,
                 IdePipeline      *pipeline,
                 GPtrArray        *targets,
                 GCancellable     *cancellable)
{
  g_autofree char *devenv_file = NULL;
  g_autofree char *build_ninja = NULL;

  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (IDE_IS_PIPELINE_STAGE (stage));
  g_assert (IDE_IS_PIPELINE (pipeline));
  g_assert (!cancellable || G_IS_CANCELLABLE (cancellable));

  devenv_file = ide_pipeline_build_builddir_path (pipeline, ".gnome-builder-devenv", NULL);
  build_ninja = ide_pipeline_build_builddir_path (pipeline, "build.ninja", NULL);

  /* If the build.ninja is newer than our devenv file, it needs to be
   * regenerated to get updated configuration.
   */
  if (!is_newer (build_ninja, devenv_file) ||
      !gbp_meson_devenv_sanity_check (devenv_file))
    ide_pipeline_stage_set_completed (stage, FALSE);
}

static void
gbp_meson_pipeline_addin_load (IdePipelineAddin *addin,
                               IdePipeline      *pipeline)
{
  GbpMesonPipelineAddin *self = (GbpMesonPipelineAddin *)addin;
  g_autoptr(IdeSubprocessLauncher) devenv_launcher = NULL;
  g_autoptr(IdePipelineStage) devenv_stage = NULL;
  g_autoptr(IdeRunCommand) build_command = NULL;
  g_autoptr(IdeRunCommand) clean_command = NULL;
  g_autoptr(IdeRunCommand) config_command = NULL;
  g_autoptr(IdeRunCommand) install_command = NULL;
  g_autoptr(IdeRunCommand) devenv_command = NULL;
  IdePipelineStage *stage;
  g_autofree char *build_dot_ninja = NULL;
  g_autofree char *crossbuild_file = NULL;
  g_autofree char *devenv_file = NULL;
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
  config_command = create_run_command (meson, "setup", ".", srcdir, "--prefix", prefix, NULL);
  if (crossbuild_file != NULL)
    ide_run_command_append_formatted (config_command, "--cross-file=%s", crossbuild_file);
  if (!ide_str_empty0 (config_opts))
    ide_run_command_append_parsed (config_command, config_opts, NULL);
  stage = attach_run_command (self, pipeline, config_command, NULL,
                              _("Configure project"), IDE_PIPELINE_PHASE_CONFIGURE);
  if (g_file_test (build_dot_ninja, G_FILE_TEST_EXISTS))
    ide_pipeline_stage_set_completed (stage, TRUE);

  /* Setup our Build/Clean stage */
  clean_command = create_run_command (ninja, "clean", NULL);
  build_command = create_run_command (ninja, NULL);
  if (parallel > 0)
    ide_run_command_append_formatted (build_command, "-j%u", parallel);
  stage = attach_run_command (self, pipeline, build_command, clean_command,
                              _("Build project"), IDE_PIPELINE_PHASE_BUILD);
  ide_pipeline_stage_set_check_stdout (stage, TRUE);
  g_signal_connect (stage, "query", G_CALLBACK (on_build_stage_query), NULL);

  /* Setup our Install stage */
  install_command = create_run_command (ninja, "install", NULL);
  stage = attach_run_command (self, pipeline, install_command, NULL,
                              _("Install project"), IDE_PIPELINE_PHASE_INSTALL);
  g_signal_connect (stage, "query", G_CALLBACK (on_install_stage_query), NULL);

  /* Setup our introspection stage */
  self->introspection = gbp_meson_introspection_new (pipeline);
  id = ide_pipeline_attach (pipeline,
                            IDE_PIPELINE_PHASE_CONFIGURE | IDE_PIPELINE_PHASE_AFTER,
                            0,
                            IDE_PIPELINE_STAGE (self->introspection));
  ide_pipeline_addin_track (addin, id);

  /* Setup stage to extract "devenv" settings */
  devenv_file = ide_pipeline_build_builddir_path (pipeline, ".gnome-builder-devenv", NULL);
  devenv_command = create_run_command (meson, "devenv", "--dump", NULL);
  stage = attach_run_command (self, pipeline, devenv_command, NULL,
                              _("Cache development environment"),
                              IDE_PIPELINE_PHASE_CONFIGURE | IDE_PIPELINE_PHASE_AFTER);
  ide_pipeline_stage_command_set_stdout_path (IDE_PIPELINE_STAGE_COMMAND (stage), devenv_file);
  ide_pipeline_stage_command_set_ignore_exit_status (IDE_PIPELINE_STAGE_COMMAND (stage), TRUE);
  g_signal_connect (stage, "query", G_CALLBACK (devenv_query_cb), NULL);

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
gbp_meson_pipeline_addin_destroy (IdeObject *object)
{
  GbpMesonPipelineAddin *self = (GbpMesonPipelineAddin *)object;

  g_clear_object (&self->introspection);

  IDE_OBJECT_CLASS (gbp_meson_pipeline_addin_parent_class)->destroy (object);
}

static void
gbp_meson_pipeline_addin_class_init (GbpMesonPipelineAddinClass *klass)
{
  IdeObjectClass *i_object_class = IDE_OBJECT_CLASS (klass);

  i_object_class->destroy = gbp_meson_pipeline_addin_destroy;
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
