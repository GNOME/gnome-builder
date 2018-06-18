/* ide-build-stage-mkdirs.c
 *
 * Copyright 2016 Christian Hergert <chergert@redhat.com>
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

#define G_LOG_DOMAIN "ide-build-stage-mkdirs"

#include "config.h"

#include <glib.h>
#include <glib/gprintf.h>
#include <glib/gstdio.h>
#include <errno.h>
#include <sys/stat.h>
#include <sys/types.h>

#include "ide-debug.h"

#include "buildsystem/ide-build-pipeline.h"
#include "buildsystem/ide-build-stage-mkdirs.h"

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
} IdeBuildStageMkdirsPrivate;

G_DEFINE_TYPE_WITH_PRIVATE (IdeBuildStageMkdirs, ide_build_stage_mkdirs, IDE_TYPE_BUILD_STAGE)

static void
clear_path (gpointer data)
{
  Path *p = data;

  dzl_clear_pointer (&p->path, g_free);
}

static void
ide_build_stage_mkdirs_query (IdeBuildStage    *stage,
                              IdeBuildPipeline *pipeline,
                              GCancellable     *cancellable)
{
  IdeBuildStageMkdirs *self = (IdeBuildStageMkdirs *)stage;
  IdeBuildStageMkdirsPrivate *priv = ide_build_stage_mkdirs_get_instance_private (self);

  IDE_ENTRY;

  g_assert (IDE_IS_BUILD_STAGE_MKDIRS (self));
  g_assert (IDE_IS_BUILD_PIPELINE (pipeline));
  g_assert (!cancellable || G_IS_CANCELLABLE (cancellable));

  for (guint i = 0; i < priv->paths->len; i++)
    {
      const Path *path = &g_array_index (priv->paths, Path, i);

      if (!g_file_test (path->path, G_FILE_TEST_EXISTS))
        {
          ide_build_stage_set_completed (stage, FALSE);
          IDE_EXIT;
        }
    }

  ide_build_stage_set_completed (stage, TRUE);

  IDE_EXIT;
}

static gboolean
ide_build_stage_mkdirs_execute (IdeBuildStage     *stage,
                                IdeBuildPipeline  *pipeline,
                                GCancellable      *cancellable,
                                GError           **error)
{
  IdeBuildStageMkdirs *self = (IdeBuildStageMkdirs *)stage;
  IdeBuildStageMkdirsPrivate *priv = ide_build_stage_mkdirs_get_instance_private (self);

  IDE_ENTRY;

  g_assert (IDE_IS_BUILD_STAGE_MKDIRS (self));
  g_assert (!cancellable || G_IS_CANCELLABLE (cancellable));

  ide_build_stage_set_active (stage, TRUE);

  for (guint i = 0; i < priv->paths->len; i++)
    {
      const Path *path = &g_array_index (priv->paths, Path, i);
      g_autofree gchar *message = NULL;
      gboolean r;

      if (g_file_test (path->path, G_FILE_TEST_IS_DIR))
        continue;

      message = g_strdup_printf ("Creating directory “%s”", path->path);
      ide_build_stage_log (IDE_BUILD_STAGE (stage), IDE_BUILD_LOG_STDOUT, message, -1);

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

  ide_build_stage_set_active (stage, FALSE);

  IDE_RETURN (TRUE);
}

static void
ide_build_stage_mkdirs_reap (IdeBuildStage      *stage,
                             DzlDirectoryReaper *reaper)
{
  IdeBuildStageMkdirs *self = (IdeBuildStageMkdirs *)stage;
  IdeBuildStageMkdirsPrivate *priv = ide_build_stage_mkdirs_get_instance_private (self);

  g_assert (IDE_IS_BUILD_STAGE_MKDIRS (self));
  g_assert (DZL_IS_DIRECTORY_REAPER (reaper));

  ide_build_stage_set_active (stage, TRUE);

  for (guint i = 0; i < priv->paths->len; i++)
    {
      const Path *path = &g_array_index (priv->paths, Path, i);

      if (path->remove_on_rebuild)
        {
          g_autoptr(GFile) file = g_file_new_for_path (path->path);
          dzl_directory_reaper_add_directory (reaper, file, 0);
        }
    }

  ide_build_stage_set_active (stage, FALSE);
}

static void
ide_build_stage_mkdirs_finalize (GObject *object)
{
  IdeBuildStageMkdirs *self = (IdeBuildStageMkdirs *)object;
  IdeBuildStageMkdirsPrivate *priv = ide_build_stage_mkdirs_get_instance_private (self);

  dzl_clear_pointer (&priv->paths, g_array_unref);

  G_OBJECT_CLASS (ide_build_stage_mkdirs_parent_class)->finalize (object);
}

static void
ide_build_stage_mkdirs_class_init (IdeBuildStageMkdirsClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  IdeBuildStageClass *stage_class = IDE_BUILD_STAGE_CLASS (klass);

  object_class->finalize = ide_build_stage_mkdirs_finalize;

  stage_class->execute = ide_build_stage_mkdirs_execute;
  stage_class->query = ide_build_stage_mkdirs_query;
  stage_class->reap = ide_build_stage_mkdirs_reap;
}

static void
ide_build_stage_mkdirs_init (IdeBuildStageMkdirs *self)
{
  IdeBuildStageMkdirsPrivate *priv = ide_build_stage_mkdirs_get_instance_private (self);

  priv->paths = g_array_new (FALSE, FALSE, sizeof (Path));
  g_array_set_clear_func (priv->paths, clear_path);
}

IdeBuildStage *
ide_build_stage_mkdirs_new (IdeContext *context)
{
  return g_object_new (IDE_TYPE_BUILD_STAGE_MKDIRS,
                       "context", context,
                       NULL);
}

void
ide_build_stage_mkdirs_add_path (IdeBuildStageMkdirs *self,
                                 const gchar         *path,
                                 gboolean             with_parents,
                                 gint                 mode,
                                 gboolean             remove_on_rebuild)
{
  IdeBuildStageMkdirsPrivate *priv = ide_build_stage_mkdirs_get_instance_private (self);
  Path ele = { 0 };

  g_return_if_fail (IDE_IS_BUILD_STAGE_MKDIRS (self));
  g_return_if_fail (path != NULL);

  ele.path = g_strdup (path);
  ele.with_parents = with_parents;
  ele.mode = mode;
  ele.remove_on_rebuild = !!remove_on_rebuild;

  g_array_append_val (priv->paths, ele);
}
