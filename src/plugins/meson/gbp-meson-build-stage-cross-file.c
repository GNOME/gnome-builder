/* gbp-meson-build-stage-cross-file.c
 *
 * Copyright 2018 Collabora Ltd.
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
 * Authors: Corentin Noël <corentin.noel@collabora.com>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#define G_LOG_DOMAIN "gbp-meson-build-stage-cross-file"

#include "config.h"

#include "gbp-meson-build-stage-cross-file.h"
#include "gbp-meson-utils.h"

struct _GbpMesonBuildStageCrossFile
{
  IdePipelineStage parent_instance;
  IdeToolchain *toolchain;
};

G_DEFINE_FINAL_TYPE (GbpMesonBuildStageCrossFile, gbp_meson_build_stage_cross_file, IDE_TYPE_PIPELINE_STAGE)

static void
add_lang_executable (const gchar *lang,
                     const gchar *path,
                     GKeyFile *keyfile)
{
  if (g_strcmp0 (lang, IDE_TOOLCHAIN_LANGUAGE_CPLUSPLUS) == 0)
    lang = "cpp";

  gbp_meson_key_file_set_string_quoted (keyfile, "binaries", lang, path);
}

static void
gbp_meson_build_stage_cross_file_query (IdePipelineStage    *stage,
                                        IdePipeline *pipeline,
                                        GPtrArray        *targets,
                                        GCancellable     *cancellable)
{
  GbpMesonBuildStageCrossFile *self = (GbpMesonBuildStageCrossFile *)stage;
  g_autofree gchar *crossbuild_file = NULL;

  IDE_ENTRY;

  g_assert (GBP_IS_MESON_BUILD_STAGE_CROSS_FILE (self));
  g_assert (IDE_IS_PIPELINE (pipeline));
  g_assert (!cancellable || G_IS_CANCELLABLE (cancellable));

  crossbuild_file = gbp_meson_build_stage_cross_file_get_path (self, pipeline);
  if (!g_file_test (crossbuild_file, G_FILE_TEST_EXISTS))
    {
      ide_pipeline_stage_set_completed (stage, FALSE);
      IDE_EXIT;
    }

  ide_pipeline_stage_set_completed (stage, TRUE);

  IDE_EXIT;
}

static gboolean
gbp_meson_build_stage_cross_file_build (IdePipelineStage  *stage,
                                        IdePipeline       *pipeline,
                                        GCancellable      *cancellable,
                                        GError           **error)
{
  GbpMesonBuildStageCrossFile *self = (GbpMesonBuildStageCrossFile *)stage;
  g_autoptr(GKeyFile) crossbuild_keyfile = NULL;
  g_autoptr(IdeTriplet) triplet = NULL;
  g_autoptr(IdeSubprocessLauncher) env_launcher = NULL;
  g_autofree gchar *crossbuild_file = NULL;
  const gchar *binary_path;
  const gchar *flags;
  GHashTable *compilers;

  IDE_ENTRY;

  g_assert (GBP_IS_MESON_BUILD_STAGE_CROSS_FILE (self));
  g_assert (!cancellable || G_IS_CANCELLABLE (cancellable));
  g_assert (IDE_IS_TOOLCHAIN (self->toolchain));

  ide_pipeline_stage_set_active (stage, TRUE);

  crossbuild_keyfile = g_key_file_new ();
  triplet = ide_toolchain_get_host_triplet (self->toolchain);

  compilers  = ide_toolchain_get_tools_for_id (self->toolchain,
                                               IDE_TOOLCHAIN_TOOL_CC);
  g_hash_table_foreach (compilers, (GHFunc)add_lang_executable, crossbuild_keyfile);

  binary_path = ide_toolchain_get_tool_for_language (self->toolchain,
                                                     IDE_TOOLCHAIN_LANGUAGE_ANY,
                                                     IDE_TOOLCHAIN_TOOL_AR);
  if (binary_path != NULL)
    gbp_meson_key_file_set_string_quoted (crossbuild_keyfile, "binaries", "ar", binary_path);

  binary_path = ide_toolchain_get_tool_for_language (self->toolchain,
                                                     IDE_TOOLCHAIN_LANGUAGE_ANY,
                                                     IDE_TOOLCHAIN_TOOL_STRIP);
  if (binary_path != NULL)
    gbp_meson_key_file_set_string_quoted (crossbuild_keyfile, "binaries", "strip", binary_path);

  binary_path = ide_toolchain_get_tool_for_language (self->toolchain,
                                                     IDE_TOOLCHAIN_LANGUAGE_ANY,
                                                     IDE_TOOLCHAIN_TOOL_PKG_CONFIG);
  if (binary_path != NULL)
    gbp_meson_key_file_set_string_quoted (crossbuild_keyfile, "binaries", "pkgconfig", binary_path);

  binary_path = ide_toolchain_get_tool_for_language (self->toolchain,
                                                     IDE_TOOLCHAIN_LANGUAGE_ANY,
                                                     IDE_TOOLCHAIN_TOOL_EXEC);
  if (binary_path != NULL)
    gbp_meson_key_file_set_string_quoted (crossbuild_keyfile, "binaries", "exe_wrapper", binary_path);

  binary_path = ide_triplet_get_kernel (triplet);
  gbp_meson_key_file_set_string_quoted (crossbuild_keyfile, "host_machine", "system", binary_path);

  binary_path = ide_triplet_get_arch (triplet);
  gbp_meson_key_file_set_string_quoted (crossbuild_keyfile, "host_machine", "cpu_family", binary_path);

  gbp_meson_key_file_set_string_quoted (crossbuild_keyfile, "host_machine", "cpu", binary_path);
  gbp_meson_key_file_set_string_quoted (crossbuild_keyfile, "host_machine", "endian", "little");

  env_launcher = ide_pipeline_create_launcher (pipeline, error);
  flags = ide_subprocess_launcher_getenv (env_launcher, "CFLAGS");
  if (flags != NULL)
    gbp_meson_key_file_set_string_array_quoted (crossbuild_keyfile, "properties", "c_args", flags);

  flags = ide_subprocess_launcher_getenv (env_launcher, "LDFLAGS");
  if (flags != NULL)
    gbp_meson_key_file_set_string_array_quoted (crossbuild_keyfile, "properties", "c_link_args", flags);

  crossbuild_file = gbp_meson_build_stage_cross_file_get_path (self, pipeline);
  if (!g_key_file_save_to_file (crossbuild_keyfile, crossbuild_file, error))
    IDE_RETURN (FALSE);

  ide_pipeline_stage_set_active (stage, FALSE);

  IDE_RETURN (TRUE);
}

static void
ide_pipeline_stage_mkdirs_finalize (GObject *object)
{
  GbpMesonBuildStageCrossFile *self = (GbpMesonBuildStageCrossFile *)object;

  g_clear_object (&self->toolchain);

  G_OBJECT_CLASS (gbp_meson_build_stage_cross_file_parent_class)->finalize (object);
}

static void
gbp_meson_build_stage_cross_file_class_init (GbpMesonBuildStageCrossFileClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  IdePipelineStageClass *stage_class = IDE_PIPELINE_STAGE_CLASS (klass);

  object_class->finalize = ide_pipeline_stage_mkdirs_finalize;

  stage_class->build = gbp_meson_build_stage_cross_file_build;
  stage_class->query = gbp_meson_build_stage_cross_file_query;
}

static void
gbp_meson_build_stage_cross_file_init (GbpMesonBuildStageCrossFile *self)
{
}

GbpMesonBuildStageCrossFile *
gbp_meson_build_stage_cross_file_new (IdeToolchain *toolchain)
{
  GbpMesonBuildStageCrossFile *build_stage;

  g_return_val_if_fail (IDE_IS_TOOLCHAIN (toolchain), NULL);

  build_stage = g_object_new (GBP_TYPE_MESON_BUILD_STAGE_CROSS_FILE, NULL);
  build_stage->toolchain = g_object_ref (toolchain);

  return g_steal_pointer (&build_stage);
}

gchar *
gbp_meson_build_stage_cross_file_get_path (GbpMesonBuildStageCrossFile *stage,
                                           IdePipeline                 *pipeline)
{
  g_return_val_if_fail (GBP_IS_MESON_BUILD_STAGE_CROSS_FILE (stage), NULL);
  g_return_val_if_fail (IDE_IS_PIPELINE (pipeline), NULL);

  return ide_pipeline_build_builddir_path (pipeline, "gnome-builder-meson.crossfile", NULL);
}
