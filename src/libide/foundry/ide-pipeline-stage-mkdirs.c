/* ide-pipeline-stage-mkdirs.c
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

#define G_LOG_DOMAIN "ide-pipeline-stage-mkdirs"

#include "config.h"

#include <glib.h>
#include <glib/gprintf.h>
#include <glib/gstdio.h>
#include <errno.h>
#include <sys/stat.h>
#include <sys/types.h>

#include "ide-pipeline.h"
#include "ide-pipeline-stage-mkdirs.h"

typedef struct
{
  gchar    *path;
  gboolean  with_parents;
  gint      mode;
  guint     remove_on_rebuild : 1;
} Path;

typedef struct
{
  GArray *paths;
} IdePipelineStageMkdirsPrivate;

G_DEFINE_TYPE_WITH_PRIVATE (IdePipelineStageMkdirs, ide_pipeline_stage_mkdirs, IDE_TYPE_PIPELINE_STAGE)

static void
clear_path (gpointer data)
{
  Path *p = data;

  g_clear_pointer (&p->path, g_free);
}

static void
ide_pipeline_stage_mkdirs_query (IdePipelineStage    *stage,
                              IdePipeline *pipeline,
                              GPtrArray        *targets,
                              GCancellable     *cancellable)
{
  IdePipelineStageMkdirs *self = (IdePipelineStageMkdirs *)stage;
  IdePipelineStageMkdirsPrivate *priv = ide_pipeline_stage_mkdirs_get_instance_private (self);

  IDE_ENTRY;

  g_assert (IDE_IS_PIPELINE_STAGE_MKDIRS (self));
  g_assert (IDE_IS_PIPELINE (pipeline));
  g_assert (!cancellable || G_IS_CANCELLABLE (cancellable));

  for (guint i = 0; i < priv->paths->len; i++)
    {
      const Path *path = &g_array_index (priv->paths, Path, i);

      if (!g_file_test (path->path, G_FILE_TEST_EXISTS))
        {
          ide_pipeline_stage_set_completed (stage, FALSE);
          IDE_EXIT;
        }
    }

  ide_pipeline_stage_set_completed (stage, TRUE);

  IDE_EXIT;
}

static gboolean
ide_pipeline_stage_mkdirs_build (IdePipelineStage     *stage,
                                IdePipeline  *pipeline,
                                GCancellable      *cancellable,
                                GError           **error)
{
  IdePipelineStageMkdirs *self = (IdePipelineStageMkdirs *)stage;
  IdePipelineStageMkdirsPrivate *priv = ide_pipeline_stage_mkdirs_get_instance_private (self);

  IDE_ENTRY;

  g_assert (IDE_IS_PIPELINE_STAGE_MKDIRS (self));
  g_assert (!cancellable || G_IS_CANCELLABLE (cancellable));

  ide_pipeline_stage_set_active (stage, TRUE);

  for (guint i = 0; i < priv->paths->len; i++)
    {
      const Path *path = &g_array_index (priv->paths, Path, i);
      g_autofree gchar *message = NULL;
      gboolean r;

      if (g_file_test (path->path, G_FILE_TEST_IS_DIR))
        continue;

      message = g_strdup_printf ("Creating directory “%s”", path->path);
      ide_pipeline_stage_log (IDE_PIPELINE_STAGE (stage), IDE_BUILD_LOG_STDOUT, message, -1);

      if (path->with_parents)
        r = g_mkdir_with_parents (path->path, path->mode);
      else
        r = g_mkdir (path->path, path->mode);

      if (r != 0)
        {
          g_set_error_literal (error,
                               G_FILE_ERROR,
                               g_file_error_from_errno (errno),
                               g_strerror (errno));
          IDE_RETURN (FALSE);
        }
    }

  ide_pipeline_stage_set_active (stage, FALSE);

  IDE_RETURN (TRUE);
}

static void
ide_pipeline_stage_mkdirs_reap (IdePipelineStage   *stage,
                                IdeDirectoryReaper *reaper)
{
  IdePipelineStageMkdirs *self = (IdePipelineStageMkdirs *)stage;
  IdePipelineStageMkdirsPrivate *priv = ide_pipeline_stage_mkdirs_get_instance_private (self);

  g_assert (IDE_IS_PIPELINE_STAGE_MKDIRS (self));
  g_assert (IDE_IS_DIRECTORY_REAPER (reaper));

  ide_pipeline_stage_set_active (stage, TRUE);

  for (guint i = 0; i < priv->paths->len; i++)
    {
      const Path *path = &g_array_index (priv->paths, Path, i);

      if (path->remove_on_rebuild)
        {
          g_autoptr(GFile) file = g_file_new_for_path (path->path);
          ide_directory_reaper_add_directory (reaper, file, 0);
        }
    }

  ide_pipeline_stage_set_active (stage, FALSE);
}

static void
ide_pipeline_stage_mkdirs_finalize (GObject *object)
{
  IdePipelineStageMkdirs *self = (IdePipelineStageMkdirs *)object;
  IdePipelineStageMkdirsPrivate *priv = ide_pipeline_stage_mkdirs_get_instance_private (self);

  g_clear_pointer (&priv->paths, g_array_unref);

  G_OBJECT_CLASS (ide_pipeline_stage_mkdirs_parent_class)->finalize (object);
}

static void
ide_pipeline_stage_mkdirs_class_init (IdePipelineStageMkdirsClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  IdePipelineStageClass *stage_class = IDE_PIPELINE_STAGE_CLASS (klass);

  object_class->finalize = ide_pipeline_stage_mkdirs_finalize;

  stage_class->build = ide_pipeline_stage_mkdirs_build;
  stage_class->query = ide_pipeline_stage_mkdirs_query;
  stage_class->reap = ide_pipeline_stage_mkdirs_reap;
}

static void
ide_pipeline_stage_mkdirs_init (IdePipelineStageMkdirs *self)
{
  IdePipelineStageMkdirsPrivate *priv = ide_pipeline_stage_mkdirs_get_instance_private (self);

  priv->paths = g_array_new (FALSE, FALSE, sizeof (Path));
  g_array_set_clear_func (priv->paths, clear_path);
}

IdePipelineStage *
ide_pipeline_stage_mkdirs_new (IdeContext *context)
{
  return g_object_new (IDE_TYPE_PIPELINE_STAGE_MKDIRS,
                       NULL);
}

void
ide_pipeline_stage_mkdirs_add_path (IdePipelineStageMkdirs *self,
                                 const gchar         *path,
                                 gboolean             with_parents,
                                 gint                 mode,
                                 gboolean             remove_on_rebuild)
{
  IdePipelineStageMkdirsPrivate *priv = ide_pipeline_stage_mkdirs_get_instance_private (self);
  Path ele = { 0 };

  g_return_if_fail (IDE_IS_PIPELINE_STAGE_MKDIRS (self));
  g_return_if_fail (path != NULL);

  ele.path = g_strdup (path);
  ele.with_parents = with_parents;
  ele.mode = mode;
  ele.remove_on_rebuild = !!remove_on_rebuild;

  g_array_append_val (priv->paths, ele);
}
