/* ide-golang-pipeline-addin.c
 *
 * Copyright 2018 Loïc BLOT <loic.blot@unix-experience.fr>
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
 */

#define G_LOG_DOMAIN "ide-golang-pipeline-addin"

#include <glib/gi18n.h>

#include "ide-golang-pipeline-addin.h"
#include "ide-golang-build-system.h"
#include "ide-golang-go-stage.h"

static gboolean
register_go_stage (IdeGolangPipelineAddin  *self,
                   IdePipeline              *pipeline,
                   IdePipelinePhase           phase,
                   GError                    **error,
                   const gchar                *label,
                   const gchar                *target,
                   const gchar                *clean_target)
{
  g_autoptr(IdePipelineStage) stage = NULL;
  IdeContext *context;
  guint stage_id;

  g_assert (IDE_IS_GOLANG_PIPELINE_ADDIN (self));
  g_assert (IDE_IS_PIPELINE (pipeline));

  context = ide_object_get_context (IDE_OBJECT (pipeline));

  stage = g_object_new (IDE_TYPE_GOLANG_GO_STAGE,
                        "name", _(label),
                        "clean-target", clean_target,
                        "context", context,
                        "target", target,
                        NULL);

  stage_id = ide_pipeline_attach (pipeline, phase, 0, stage);
  ide_pipeline_addin_track (IDE_PIPELINE_ADDIN (self), stage_id);

  return TRUE;
}

gboolean golang_tree_action_enable_build(gboolean is_dir)
{
  return is_dir;
}

static void
ide_golang_pipeline_addin_load (IdePipelineAddin *addin,
                                   IdePipeline      *pipeline)
{
  IdeGolangPipelineAddin *self = (IdeGolangPipelineAddin *)addin;
  g_autoptr(GError) error = NULL;
  IdeBuildSystem *build_system;
  IdeContext *context;

  g_assert (IDE_IS_GOLANG_PIPELINE_ADDIN (self));
  g_assert (IDE_IS_PIPELINE (pipeline));

  context = ide_object_get_context (IDE_OBJECT (addin));
  build_system = ide_build_system_from_context (context);

  if (!IDE_IS_GOLANG_BUILD_SYSTEM (build_system))
    return;

  if (!register_go_stage (self, pipeline, IDE_PIPELINE_PHASE_BUILD, &error, "Building module", "build", "clean") ||
      !register_go_stage (self, pipeline, IDE_PIPELINE_PHASE_INSTALL, &error, "Installing module", "install", NULL))
    {
      g_assert (error != NULL);
      g_warning ("Failed to create golang launcher: %s", error->message);
      return;
    }
}

static void
addin_iface_init (IdePipelineAddinInterface *iface)
{
  iface->load = ide_golang_pipeline_addin_load;
}

struct _IdeGolangPipelineAddin { IdeObject parent; };

G_DEFINE_TYPE_WITH_CODE (IdeGolangPipelineAddin, ide_golang_pipeline_addin, IDE_TYPE_OBJECT,
                         G_IMPLEMENT_INTERFACE (IDE_TYPE_PIPELINE_ADDIN, addin_iface_init))

static void
ide_golang_pipeline_addin_class_init (IdeGolangPipelineAddinClass *klass)
{
}

static void
ide_golang_pipeline_addin_init (IdeGolangPipelineAddin *self)
{
}
