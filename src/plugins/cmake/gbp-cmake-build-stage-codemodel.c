/* gbp-cmake-build-stage-codemodel.c
 *
 * Copyright 2021 Günther Wagner <info@gunibert.de>
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

#define G_LOG_DOMAIN "gbp-cmake-build-stage-codemodel"

#include <json-glib/json-glib.h>

#include "gbp-cmake-build-stage-codemodel.h"

struct _GbpCmakeBuildStageCodemodel
{
  IdePipelineStage parent_instance;
};

G_DEFINE_TYPE (GbpCmakeBuildStageCodemodel, gbp_cmake_build_stage_codemodel, IDE_TYPE_PIPELINE_STAGE)

GbpCmakeBuildStageCodemodel *
gbp_cmake_build_stage_codemodel_new (void)
{
  return g_object_new (GBP_TYPE_CMAKE_BUILD_STAGE_CODEMODEL, NULL);
}

static gchar *
gbp_cmake_build_stage_codemodel_get_query_path (GbpCmakeBuildStageCodemodel *self,
                                                IdePipeline                 *pipeline)
{
  g_return_val_if_fail (GBP_IS_CMAKE_BUILD_STAGE_CODEMODEL (self), NULL);
  g_return_val_if_fail (IDE_IS_PIPELINE (pipeline), NULL);

  return ide_pipeline_build_builddir_path (pipeline, ".cmake", "api", "v1", "query", "client-builder", NULL);
}

static JsonNode *
gbp_cmake_build_stage_codemodel_create_query (GbpCmakeBuildStageCodemodel *self)
{
  g_autoptr(JsonBuilder) builder = NULL;

  g_return_val_if_fail (GBP_IS_CMAKE_BUILD_STAGE_CODEMODEL (self), NULL);

  builder = json_builder_new ();
  json_builder_begin_object (builder);

  json_builder_set_member_name (builder, "requests");
  json_builder_begin_array (builder);

  json_builder_begin_object (builder);

  json_builder_set_member_name (builder, "kind");
  json_builder_add_string_value (builder, "codemodel");

  json_builder_set_member_name (builder, "version");
  json_builder_add_int_value (builder, 2);

  json_builder_end_object (builder);

  json_builder_end_array (builder);

  json_builder_end_object (builder);

  return json_builder_get_root (builder);
}

static gboolean
gbp_cmake_build_stage_codemodel_build (IdePipelineStage  *stage,
                                       IdePipeline       *pipeline,
                                       GCancellable      *cancellable,
                                       GError           **error)
{
  GbpCmakeBuildStageCodemodel *self = GBP_CMAKE_BUILD_STAGE_CODEMODEL (stage);
  g_autoptr(JsonGenerator) generator = NULL;
  g_autoptr(JsonNode) query = NULL;
  g_autofree gchar *path = NULL;
  g_autofree gchar *queryfile = NULL;
  gboolean ret;

  IDE_ENTRY;

  g_assert (GBP_IS_CMAKE_BUILD_STAGE_CODEMODEL (self));
  g_assert (!cancellable || G_IS_CANCELLABLE (cancellable));

  ide_pipeline_stage_set_active (stage, TRUE);

  path = gbp_cmake_build_stage_codemodel_get_query_path (self, pipeline);
  ret = g_mkdir_with_parents (path, 0750);

  if (ret != 0)
    {
        g_set_error_literal (error,
                             G_FILE_ERROR,
                             g_file_error_from_errno (errno),
                             g_strerror (errno));
        IDE_RETURN (FALSE);
    }

  generator = json_generator_new ();
  query = gbp_cmake_build_stage_codemodel_create_query (self);
  json_generator_set_root (generator, query);
  queryfile = g_build_filename (path, "query.json", NULL);
  json_generator_to_file (generator, queryfile, NULL);

  ide_pipeline_stage_set_active (stage, FALSE);

  IDE_RETURN (TRUE);
}

static void
gbp_cmake_build_stage_codemodel_query (IdePipelineStage *stage,
                                       IdePipeline      *pipeline,
                                       GPtrArray        *targets,
                                       GCancellable     *cancellable)
{
  GbpCmakeBuildStageCodemodel *self = GBP_CMAKE_BUILD_STAGE_CODEMODEL (stage);
  g_autofree gchar *path = NULL;

  IDE_ENTRY;

  g_assert (GBP_IS_CMAKE_BUILD_STAGE_CODEMODEL (self));
  g_assert (IDE_IS_PIPELINE (pipeline));
  g_assert (!cancellable || G_IS_CANCELLABLE (cancellable));

  path = gbp_cmake_build_stage_codemodel_get_query_path (self, pipeline);
  if (!g_file_test (path, G_FILE_TEST_EXISTS))
    {
      ide_pipeline_stage_set_completed (stage, FALSE);
      IDE_EXIT;
    }

  ide_pipeline_stage_set_completed (stage, TRUE);

  IDE_EXIT;
}

static void
gbp_cmake_build_stage_codemodel_class_init (GbpCmakeBuildStageCodemodelClass *klass)
{
  IdePipelineStageClass *stage_class = IDE_PIPELINE_STAGE_CLASS (klass);

  stage_class->build = gbp_cmake_build_stage_codemodel_build;
  stage_class->query = gbp_cmake_build_stage_codemodel_query;
}

static void
gbp_cmake_build_stage_codemodel_init (GbpCmakeBuildStageCodemodel *self)
{
}
