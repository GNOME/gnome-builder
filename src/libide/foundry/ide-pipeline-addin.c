/* ide-pipeline-addin.c
 *
 * Copyright 2016-2019 Christian Hergert <chergert@redhat.com>
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

#define G_LOG_DOMAIN "ide-pipeline-addin"

#include "config.h"

#include "ide-pipeline-private.h"
#include "ide-pipeline-addin.h"

G_DEFINE_INTERFACE (IdePipelineAddin, ide_pipeline_addin, IDE_TYPE_OBJECT)

static void
ide_pipeline_addin_default_init (IdePipelineAddinInterface *iface)
{
}

/**
 * ide_pipeline_addin_prepare:
 * @self: a #IdePipelineAddin
 * @pipeline: an #IdePipeline
 *
 * This function is called before prepare so that plugins may setup
 * signals on the pipeline that may allow them to affect how other
 * plugins interact.
 *
 * For example, if you need to connect to pipeline::launcher-created,
 * you might want to do that here.
 */
void
ide_pipeline_addin_prepare (IdePipelineAddin *self,
                            IdePipeline      *pipeline)
{
  g_return_if_fail (IDE_IS_PIPELINE_ADDIN (self));
  g_return_if_fail (IDE_IS_PIPELINE (pipeline));

  if (IDE_PIPELINE_ADDIN_GET_IFACE (self)->prepare)
    IDE_PIPELINE_ADDIN_GET_IFACE (self)->prepare (self, pipeline);
}

void
ide_pipeline_addin_load (IdePipelineAddin *self,
                         IdePipeline      *pipeline)
{
  g_return_if_fail (IDE_IS_PIPELINE_ADDIN (self));
  g_return_if_fail (IDE_IS_PIPELINE (pipeline));

  if (IDE_PIPELINE_ADDIN_GET_IFACE (self)->load)
    IDE_PIPELINE_ADDIN_GET_IFACE (self)->load (self, pipeline);
}

void
ide_pipeline_addin_unload (IdePipelineAddin *self,
                           IdePipeline      *pipeline)
{
  GArray *ar;

  g_return_if_fail (IDE_IS_PIPELINE_ADDIN (self));
  g_return_if_fail (IDE_IS_PIPELINE (pipeline));

  if (IDE_PIPELINE_ADDIN_GET_IFACE (self)->unload)
    IDE_PIPELINE_ADDIN_GET_IFACE (self)->unload (self, pipeline);

  /* Unload any stages that are tracked by the addin */
  ar = g_object_get_data (G_OBJECT (self), "IDE_PIPELINE_ADDIN_STAGES");

  if G_LIKELY (ar != NULL)
    {
      for (guint i = 0; i < ar->len; i++)
        {
          guint stage_id = g_array_index (ar, guint, i);

          ide_pipeline_detach (pipeline, stage_id);
        }
    }
}

/**
 * ide_pipeline_addin_track:
 * @self: An #IdePipelineAddin
 * @stage_id: a stage id returned from ide_pipeline_attach()
 *
 * This function will track the stage_id that was returned from
 * ide_pipeline_attach() or similar functions. Doing so results in
 * the stage being automatically disconnected when the addin is unloaded.
 *
 * This means that many #IdePipelineAddin implementations do not need
 * an unload vfunc if they track all registered stages.
 *
 * You should not mix this function with manual pipeline disconnections.
 * While it should work, that is not yet guaranteed.
 */
void
ide_pipeline_addin_track (IdePipelineAddin *self,
                          guint             stage_id)
{
  GArray *ar;

  g_return_if_fail (IDE_IS_PIPELINE_ADDIN (self));
  g_return_if_fail (stage_id > 0);

  ar = g_object_get_data (G_OBJECT (self), "IDE_PIPELINE_ADDIN_STAGES");

  if (ar == NULL)
    {
      ar = g_array_new (FALSE, FALSE, sizeof (guint));
      g_object_set_data_full (G_OBJECT (self), "IDE_PIPELINE_ADDIN_STAGES",
                              ar, (GDestroyNotify)g_array_unref);
    }

  g_array_append_val (ar, stage_id);
}

const guint *
_ide_pipeline_addin_get_stages (IdePipelineAddin *self,
                                guint            *n_stages)
{
  GArray *ar;

  g_return_val_if_fail (IDE_IS_PIPELINE_ADDIN (self), NULL);
  g_return_val_if_fail (n_stages != NULL, NULL);

  *n_stages = 0;

  if (!(ar = g_object_get_data (G_OBJECT (self), "IDE_PIPELINE_ADDIN_STAGES")))
    return NULL;

  return (const guint *)(gpointer)ar->data;
}
