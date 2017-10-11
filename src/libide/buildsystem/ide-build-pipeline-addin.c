/* ide-build-pipeline-addin.c
 *
 * Copyright © 2016 Christian Hergert <chergert@redhat.com>
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

#define G_LOG_DOMAIN "ide-build-pipeline-addin"

#include "ide-context.h"

#include "buildsystem/ide-build-pipeline-addin.h"

G_DEFINE_INTERFACE (IdeBuildPipelineAddin, ide_build_pipeline_addin, IDE_TYPE_OBJECT)

static void
ide_build_pipeline_addin_default_init (IdeBuildPipelineAddinInterface *iface)
{
}

void
ide_build_pipeline_addin_load (IdeBuildPipelineAddin *self,
                               IdeBuildPipeline      *pipeline)
{
  g_return_if_fail (IDE_IS_BUILD_PIPELINE_ADDIN (self));
  g_return_if_fail (IDE_IS_BUILD_PIPELINE (pipeline));

  if (IDE_BUILD_PIPELINE_ADDIN_GET_IFACE (self)->load)
    IDE_BUILD_PIPELINE_ADDIN_GET_IFACE (self)->load (self, pipeline);
}

void
ide_build_pipeline_addin_unload (IdeBuildPipelineAddin *self,
                                 IdeBuildPipeline      *pipeline)
{
  GArray *ar;

  g_return_if_fail (IDE_IS_BUILD_PIPELINE_ADDIN (self));
  g_return_if_fail (IDE_IS_BUILD_PIPELINE (pipeline));

  if (IDE_BUILD_PIPELINE_ADDIN_GET_IFACE (self)->unload)
    IDE_BUILD_PIPELINE_ADDIN_GET_IFACE (self)->unload (self, pipeline);

  /* Unload any stages that are tracked by the addin */
  ar = g_object_get_data (G_OBJECT (self), "IDE_BUILD_PIPELINE_ADDIN_STAGES");

  if G_LIKELY (ar != NULL)
    {
      for (guint i = 0; i < ar->len; i++)
        {
          guint stage_id = g_array_index (ar, guint, i);

          ide_build_pipeline_disconnect (pipeline, stage_id);
        }
    }
}

/**
 * ide_build_pipeline_addin_track:
 * @self: An #IdeBuildPipelineAddin
 * @stage_id: a stage id returned from ide_build_pipeline_connect()
 *
 * This function will track the stage_id that was returned from
 * ide_build_pipeline_connect() or similar functions. Doing so results in
 * the stage being automatically disconnected when the addin is unloaded.
 *
 * This means that many #IdeBuildPipelineAddin implementations do not need
 * an unload vfunc if they track all registered stages.
 *
 * You should not mix this function with manual pipeline disconnections.
 * While it should work, that is not yet guaranteed.
 */
void
ide_build_pipeline_addin_track (IdeBuildPipelineAddin *self,
                                guint                  stage_id)
{
  GArray *ar;

  g_return_if_fail (IDE_IS_BUILD_PIPELINE_ADDIN (self));
  g_return_if_fail (stage_id > 0);

  ar = g_object_get_data (G_OBJECT (self), "IDE_BUILD_PIPELINE_ADDIN_STAGES");

  if (ar == NULL)
    {
      ar = g_array_new (FALSE, FALSE, sizeof (guint));
      g_object_set_data_full (G_OBJECT (self), "IDE_BUILD_PIPELINE_ADDIN_STAGES",
                              ar, (GDestroyNotify)g_array_unref);
    }

  g_array_append_val (ar, stage_id);
}
