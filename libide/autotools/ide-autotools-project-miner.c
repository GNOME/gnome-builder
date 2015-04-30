/* ide-autotools-project-miner.c
 *
 * Copyright (C) 2015 Christian Hergert <christian@hergert.me>
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

#define G_LOG_DOMAIN "ide-autotools-project-miner"

#include <glib/gi18n.h>

#include "ide-autotools-project-miner.h"
#include "ide-debug.h"

#define MAX_MINE_DEPTH 5

struct _IdeAutotoolsProjectMiner
{
  IdeProjectMiner  parent_instance;
  GFile           *root_directory;
};

G_DEFINE_TYPE (IdeAutotoolsProjectMiner, ide_autotools_project_miner, IDE_TYPE_PROJECT_MINER)

enum {
  PROP_0,
  PROP_ROOT_DIRECTORY,
  LAST_PROP
};

static GParamSpec *gParamSpecs [LAST_PROP];

static void
ide_autotools_project_miner_discovered (IdeAutotoolsProjectMiner *self,
                                        GCancellable             *cancellable,
                                        GFile                    *directory,
                                        GFileInfo                *file_info)
{
  g_autofree gchar *uri = NULL;
  g_autofree gchar *name = NULL;
  g_autoptr(GFile) file = NULL;
  g_autoptr(GFile) index_file = NULL;
  g_autoptr(GFileInfo) index_info = NULL;
  g_autoptr(IdeProjectInfo) project_info = NULL;
  g_autoptr(GDateTime) last_modified_at = NULL;
  const gchar *filename;
  guint64 mtime;

  IDE_ENTRY;

  g_assert (IDE_IS_AUTOTOOLS_PROJECT_MINER (self));
  g_assert (G_IS_FILE (directory));
  g_assert (G_IS_FILE_INFO (file_info));

  uri = g_file_get_uri (directory);
  g_debug ("Discovered autotools project at %s", uri);

  mtime = g_file_info_get_attribute_uint64 (file_info, G_FILE_ATTRIBUTE_TIME_MODIFIED);

  /*
   * If there is a git repo, trust the .git/index file for time info,
   * it is more reliable than our directory mtime.
   */
  index_file = g_file_get_child (directory, ".git/index");
  index_info = g_file_query_info (index_file,
                                  G_FILE_ATTRIBUTE_TIME_MODIFIED,
                                  G_FILE_QUERY_INFO_NONE,
                                  cancellable,
                                  NULL);
  if (index_info != NULL)
    mtime = g_file_info_get_attribute_uint64 (index_info, G_FILE_ATTRIBUTE_TIME_MODIFIED);

  last_modified_at = g_date_time_new_from_unix_local (mtime);

  filename = g_file_info_get_attribute_byte_string (file_info, G_FILE_ATTRIBUTE_STANDARD_NAME);
  file = g_file_get_child (directory, filename);
  name = g_file_get_basename (directory);

  project_info = g_object_new (IDE_TYPE_PROJECT_INFO,
                               "directory", directory,
                               "file", file,
                               "last-modified-at", last_modified_at,
                               "name", name,
                               "priority", IDE_AUTOTOOLS_PROJECT_MINER_PRIORITY,
                               NULL);

  ide_project_miner_emit_discovered (IDE_PROJECT_MINER (self), project_info);

  IDE_EXIT;
}

static void
ide_autotools_project_miner_mine_directory (IdeAutotoolsProjectMiner *self,
                                            GFile                    *directory,
                                            guint                     depth,
                                            GCancellable             *cancellable)
{
  g_autoptr(GFileEnumerator) file_enum = NULL;
  GFileInfo *file_info;

  g_assert (IDE_IS_AUTOTOOLS_PROJECT_MINER (self));
  g_assert (G_IS_FILE (directory));
  g_assert (!cancellable || G_IS_CANCELLABLE (cancellable));

  if (depth == MAX_MINE_DEPTH)
    return;

#ifdef IDE_ENABLE_TRACE
  {
    g_autofree gchar *uri = NULL;

    uri = g_file_get_uri (directory);
    IDE_TRACE_MSG ("Mining directory %s", uri);
  }
#endif

  file_enum = g_file_enumerate_children (directory,
                                         G_FILE_ATTRIBUTE_STANDARD_NAME","
                                         G_FILE_ATTRIBUTE_STANDARD_TYPE","
                                         G_FILE_ATTRIBUTE_TIME_MODIFIED,
                                         G_FILE_QUERY_INFO_NONE,
                                         cancellable,
                                         NULL);

  if (file_enum == NULL)
    return;

  while ((file_info = g_file_enumerator_next_file (file_enum, cancellable, NULL)))
    {
      const gchar *filename;
      GFileType file_type;
      GFile *child;

      file_type = g_file_info_get_attribute_uint32 (file_info, G_FILE_ATTRIBUTE_STANDARD_TYPE);
      filename = g_file_info_get_attribute_byte_string (file_info, G_FILE_ATTRIBUTE_STANDARD_NAME);

      if (filename && filename [0] == '.')
        goto cleanup;

      switch (file_type)
        {
        case G_FILE_TYPE_DIRECTORY:
          child = g_file_get_child (directory, filename);
          ide_autotools_project_miner_mine_directory (self, child, depth + 1, cancellable);
          g_clear_object (&child);
          break;

        case G_FILE_TYPE_REGULAR:
          if ((0 == g_strcmp0 (filename, "configure.ac")) ||
              (0 == g_strcmp0 (filename, "configure.in")))
            {
              ide_autotools_project_miner_discovered (self, cancellable, directory, file_info);
              g_clear_object (&file_info);
              return;
            }
          break;

        case G_FILE_TYPE_UNKNOWN:
        case G_FILE_TYPE_SYMBOLIC_LINK:
        case G_FILE_TYPE_SPECIAL:
        case G_FILE_TYPE_SHORTCUT:
        case G_FILE_TYPE_MOUNTABLE:
        default:
          break;
        }

    cleanup:
      g_object_unref (file_info);
    }
}

static void
ide_autotools_project_miner_worker (GTask        *task,
                                    gpointer      source_object,
                                    gpointer      task_data,
                                    GCancellable *cancellable)
{
  IdeAutotoolsProjectMiner *self = source_object;
  GFile *directory = task_data;

  IDE_ENTRY;

  g_assert (G_IS_TASK (task));
  g_assert (IDE_IS_AUTOTOOLS_PROJECT_MINER (self));
  g_assert (G_IS_FILE (directory));
  g_assert (!cancellable || G_IS_CANCELLABLE (cancellable));

  ide_autotools_project_miner_mine_directory (self, directory, 0, cancellable);

  g_task_return_boolean (task, TRUE);

  IDE_EXIT;
}

static void
ide_autotools_project_miner_mine_async (IdeProjectMiner     *miner,
                                        GCancellable        *cancellable,
                                        GAsyncReadyCallback  callback,
                                        gpointer             user_data)
{
  IdeAutotoolsProjectMiner *self = (IdeAutotoolsProjectMiner *)miner;
  g_autoptr(GTask) task = NULL;
  g_autoptr(GFile) directory = NULL;

  g_assert (IDE_IS_AUTOTOOLS_PROJECT_MINER (self));
  g_assert (!cancellable || G_IS_CANCELLABLE (cancellable));

  task = g_task_new (miner, cancellable, callback, user_data);

  directory = g_file_new_for_path (g_get_home_dir ());

  if (self->root_directory)
    g_task_set_task_data (task, g_object_ref (self->root_directory), g_object_unref);
  else
    g_task_set_task_data (task, g_object_ref (directory), g_object_unref);

  g_task_run_in_thread (task, ide_autotools_project_miner_worker);
}

static gboolean
ide_autotools_project_miner_mine_finish (IdeProjectMiner  *miner,
                                         GAsyncResult     *result,
                                         GError          **error)
{
  GTask *task = (GTask *)result;

  g_assert (IDE_IS_AUTOTOOLS_PROJECT_MINER (miner));
  g_assert (G_IS_TASK (task));

  return g_task_propagate_boolean (task, error);
}

static void
ide_autotools_project_miner_finalize (GObject *object)
{
  IdeAutotoolsProjectMiner *self = (IdeAutotoolsProjectMiner *)object;

  g_clear_object (&self->root_directory);

  G_OBJECT_CLASS (ide_autotools_project_miner_parent_class)->finalize (object);
}

static void
ide_autotools_project_miner_get_property (GObject    *object,
                                          guint       prop_id,
                                          GValue     *value,
                                          GParamSpec *pspec)
{
  IdeAutotoolsProjectMiner *self = IDE_AUTOTOOLS_PROJECT_MINER (object);

  switch (prop_id)
    {
    case PROP_ROOT_DIRECTORY:
      g_value_set_object (value, ide_autotools_project_miner_get_root_directory (self));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
ide_autotools_project_miner_set_property (GObject      *object,
                                          guint         prop_id,
                                          const GValue *value,
                                          GParamSpec   *pspec)
{
  IdeAutotoolsProjectMiner *self = IDE_AUTOTOOLS_PROJECT_MINER (object);

  switch (prop_id)
    {
    case PROP_ROOT_DIRECTORY:
      ide_autotools_project_miner_set_root_directory (self, g_value_get_object (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
ide_autotools_project_miner_class_init (IdeAutotoolsProjectMinerClass *klass)
{
  IdeProjectMinerClass *miner_class = IDE_PROJECT_MINER_CLASS (klass);
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = ide_autotools_project_miner_finalize;
  object_class->get_property = ide_autotools_project_miner_get_property;
  object_class->set_property = ide_autotools_project_miner_set_property;

  miner_class->mine_async = ide_autotools_project_miner_mine_async;
  miner_class->mine_finish = ide_autotools_project_miner_mine_finish;

  gParamSpecs [PROP_ROOT_DIRECTORY] =
    g_param_spec_object ("root-directory",
                         _("Root Directory"),
                         _("The root directory to scan from."),
                         G_TYPE_FILE,
                         (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (object_class, PROP_ROOT_DIRECTORY,
                                   gParamSpecs [PROP_ROOT_DIRECTORY]);
}

static void
ide_autotools_project_miner_init (IdeAutotoolsProjectMiner *self)
{
}

/**
 * ide_autotools_project_miner_get_root_directory:
 *
 * Gets the IdeAutotoolsProjectMiner:root-directory property.
 * Scans will start from this directory.
 *
 * Returns: (transfer none) (nullable): A #GFile or %NULL.
 */
GFile *
ide_autotools_project_miner_get_root_directory (IdeAutotoolsProjectMiner *self)
{
  g_return_val_if_fail (IDE_IS_AUTOTOOLS_PROJECT_MINER (self), NULL);

  return self->root_directory;
}

void
ide_autotools_project_miner_set_root_directory (IdeAutotoolsProjectMiner *self,
                                                GFile                    *root_directory)
{
  g_return_if_fail (IDE_IS_AUTOTOOLS_PROJECT_MINER (self));
  g_return_if_fail (!root_directory || G_IS_FILE (root_directory));

  if (g_set_object (&self->root_directory, root_directory))
    g_object_notify_by_pspec (G_OBJECT (self), gParamSpecs [PROP_ROOT_DIRECTORY]);
}
