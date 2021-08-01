/* gbp-cmake-build-stage-cross-file.c
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

#define G_LOG_DOMAIN "gbp-cmake-build-stage-cross-file"

#include "config.h"

#include "gbp-cmake-build-stage-cross-file.h"

struct _GbpCMakeBuildStageCrossFile
{
  IdePipelineStage parent_instance;
  IdeToolchain *toolchain;
};

G_DEFINE_FINAL_TYPE (GbpCMakeBuildStageCrossFile, gbp_cmake_build_stage_cross_file, IDE_TYPE_PIPELINE_STAGE)

static void
_gbp_cmake_file_set (gchar       **content,
                     const gchar  *key,
                     const gchar  *value)
{
  g_autofree gchar* old_content = NULL;

  g_assert (content != NULL);

  old_content = *content;
  if (old_content == NULL)
    *content = g_strdup_printf("SET(%s %s)", key, value);
  else
    *content = g_strdup_printf("%s\nSET(%s %s)", old_content, key, value);
}

static void
_gbp_cmake_file_set_quoted (gchar       **content,
                            const gchar  *key,
                            const gchar  *value)
{
  g_autofree gchar* quoted_value = NULL;

  quoted_value = g_strdup_printf("\"%s\"", value);

  _gbp_cmake_file_set (content, key, quoted_value);
}

static void
gbp_cmake_build_stage_cross_file_query (IdePipelineStage    *stage,
                                        IdePipeline *pipeline,
                                        GPtrArray        *targets,
                                        GCancellable     *cancellable)
{
  GbpCMakeBuildStageCrossFile *self = (GbpCMakeBuildStageCrossFile *)stage;
  g_autofree gchar *crossbuild_file = NULL;

  IDE_ENTRY;

  g_assert (GBP_IS_CMAKE_BUILD_STAGE_CROSS_FILE (self));
  g_assert (IDE_IS_PIPELINE (pipeline));
  g_assert (!cancellable || G_IS_CANCELLABLE (cancellable));

  crossbuild_file = gbp_cmake_build_stage_cross_file_get_path (self, pipeline);
  if (!g_file_test (crossbuild_file, G_FILE_TEST_EXISTS))
    {
      ide_pipeline_stage_set_completed (stage, FALSE);
      IDE_EXIT;
    }

  ide_pipeline_stage_set_completed (stage, TRUE);

  IDE_EXIT;
}

static void
add_lang_executable (const gchar *lang,
                     const gchar *path,
                     gchar **content)
{
  if (g_strcmp0 (lang, IDE_TOOLCHAIN_LANGUAGE_C) == 0)
    _gbp_cmake_file_set_quoted (content, "CMAKE_C_COMPILER", path);
  else if (g_strcmp0 (lang, IDE_TOOLCHAIN_LANGUAGE_CPLUSPLUS) == 0)
    _gbp_cmake_file_set_quoted (content, "CMAKE_CXX_COMPILER", path);
  else if (g_strcmp0 (lang, IDE_TOOLCHAIN_LANGUAGE_VALA) == 0)
    _gbp_cmake_file_set_quoted (content, "VALA_EXECUTABLE", path);
  else if (g_strcmp0 (lang, IDE_TOOLCHAIN_LANGUAGE_FORTRAN) == 0)
    _gbp_cmake_file_set_quoted (content, "CMAKE_Fortran_COMPILER", path);
  else if (g_strcmp0 (lang, IDE_TOOLCHAIN_LANGUAGE_D) == 0)
    _gbp_cmake_file_set_quoted (content, "CMAKE_D_COMPILER", path);
}

static gboolean
gbp_cmake_build_stage_cross_file_build (IdePipelineStage  *stage,
                                        IdePipeline       *pipeline,
                                        GCancellable      *cancellable,
                                        GError           **error)
{
  GbpCMakeBuildStageCrossFile *self = (GbpCMakeBuildStageCrossFile *)stage;
  g_autoptr(IdeTriplet) triplet = NULL;
  g_autofree gchar *crossbuild_path = NULL;
  g_autofree gchar *crossbuild_content = NULL;
  const gchar *binary_path;
  const gchar *key;
  GHashTable *compilers;

  IDE_ENTRY;

  g_assert (GBP_IS_CMAKE_BUILD_STAGE_CROSS_FILE (self));
  g_assert (!cancellable || G_IS_CANCELLABLE (cancellable));
  g_assert (IDE_IS_TOOLCHAIN (self->toolchain));

  ide_pipeline_stage_set_active (stage, TRUE);

  triplet = ide_toolchain_get_host_triplet (self->toolchain);

  key = ide_triplet_get_operating_system (triplet);
  if (key != NULL)
    _gbp_cmake_file_set (&crossbuild_content, "CMAKE_SYSTEM_NAME", key);

  _gbp_cmake_file_set (&crossbuild_content, "CMAKE_SYSTEM_VERSION", "1");

  key = ide_triplet_get_arch (triplet);
  _gbp_cmake_file_set (&crossbuild_content, "CMAKE_SYSTEM_PROCESSOR", key);

  compilers  = ide_toolchain_get_tools_for_id (self->toolchain,
                                               IDE_TOOLCHAIN_TOOL_CC);
  g_hash_table_foreach (compilers, (GHFunc)add_lang_executable, &crossbuild_content);

  binary_path = ide_toolchain_get_tool_for_language (self->toolchain,
                                                     IDE_TOOLCHAIN_LANGUAGE_ANY,
                                                     IDE_TOOLCHAIN_TOOL_AR);
  if (binary_path != NULL)
    _gbp_cmake_file_set_quoted (&crossbuild_content, "CMAKE_LINKER", binary_path);

  binary_path = ide_toolchain_get_tool_for_language (self->toolchain,
                                                     IDE_TOOLCHAIN_LANGUAGE_ANY,
                                                     IDE_TOOLCHAIN_TOOL_PKG_CONFIG);
  if (binary_path != NULL)
    _gbp_cmake_file_set_quoted (&crossbuild_content, "PKG_CONFIG_EXECUTABLE", binary_path);

  binary_path = ide_toolchain_get_tool_for_language (self->toolchain,
                                                     IDE_TOOLCHAIN_LANGUAGE_ANY,
                                                     IDE_TOOLCHAIN_TOOL_EXEC);
  if (binary_path != NULL)
    _gbp_cmake_file_set_quoted (&crossbuild_content, "CMAKE_CROSSCOMPILING_EMULATOR", binary_path);

  crossbuild_path = gbp_cmake_build_stage_cross_file_get_path (self, pipeline);
  if (!g_file_set_contents (crossbuild_path, crossbuild_content, -1, error))
    IDE_RETURN (FALSE);

  ide_pipeline_stage_set_active (stage, FALSE);

  IDE_RETURN (TRUE);
}

static void
ide_pipeline_stage_mkdirs_finalize (GObject *object)
{
  GbpCMakeBuildStageCrossFile *self = (GbpCMakeBuildStageCrossFile *)object;

  g_clear_object (&self->toolchain);

  G_OBJECT_CLASS (gbp_cmake_build_stage_cross_file_parent_class)->finalize (object);
}

static void
gbp_cmake_build_stage_cross_file_class_init (GbpCMakeBuildStageCrossFileClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  IdePipelineStageClass *stage_class = IDE_PIPELINE_STAGE_CLASS (klass);

  object_class->finalize = ide_pipeline_stage_mkdirs_finalize;

  stage_class->build = gbp_cmake_build_stage_cross_file_build;
  stage_class->query = gbp_cmake_build_stage_cross_file_query;
}

static void
gbp_cmake_build_stage_cross_file_init (GbpCMakeBuildStageCrossFile *self)
{
}

GbpCMakeBuildStageCrossFile *
gbp_cmake_build_stage_cross_file_new (IdeToolchain *toolchain)
{
  GbpCMakeBuildStageCrossFile *build_stage;

  g_return_val_if_fail (IDE_IS_TOOLCHAIN (toolchain), NULL);

  build_stage = g_object_new (GBP_TYPE_CMAKE_BUILD_STAGE_CROSS_FILE, NULL);
  build_stage->toolchain = g_object_ref (toolchain);

  return g_steal_pointer (&build_stage);
}

gchar *
gbp_cmake_build_stage_cross_file_get_path (GbpCMakeBuildStageCrossFile *stage,
                                           IdePipeline                 *pipeline)
{
  g_return_val_if_fail (GBP_IS_CMAKE_BUILD_STAGE_CROSS_FILE (stage), NULL);
  g_return_val_if_fail (IDE_IS_PIPELINE (pipeline), NULL);

  return ide_pipeline_build_builddir_path (pipeline, "gnome-builder-crossfile.cmake", NULL);
}
