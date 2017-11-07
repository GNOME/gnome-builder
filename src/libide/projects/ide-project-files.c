/* ide-project-files.c
 *
 * Copyright © 2015 Christian Hergert <christian@hergert.me>
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

#define G_LOG_DOMAIN "ide-project-files"

#include "ide-context.h"

#include "projects/ide-project-file.h"
#include "projects/ide-project-files.h"
#include "vcs/ide-vcs.h"

typedef struct
{
  GHashTable *files_by_path;
} IdeProjectFilesPrivate;

G_DEFINE_TYPE_WITH_PRIVATE (IdeProjectFiles, ide_project_files,
                            IDE_TYPE_PROJECT_ITEM)

static void
ide_project_files_dispose (GObject *object)
{
  IdeProjectFiles *self = (IdeProjectFiles *)object;
  IdeProjectFilesPrivate *priv = ide_project_files_get_instance_private (self);

  g_clear_pointer (&priv->files_by_path, g_hash_table_unref);

  G_OBJECT_CLASS (ide_project_files_parent_class)->dispose (object);
}

static void
ide_project_files_class_init (IdeProjectFilesClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->dispose = ide_project_files_dispose;
}

static void
ide_project_files_init (IdeProjectFiles *self)
{
  IdeProjectFilesPrivate *priv = ide_project_files_get_instance_private (self);

  priv->files_by_path = g_hash_table_new_full (g_str_hash, g_str_equal,
                                               g_free, g_object_unref);
}

static IdeProjectItem *
ide_project_files_find_child (IdeProjectItem *item,
                              const gchar    *child)
{
  GSequence *children;
  GSequenceIter *iter;

  g_assert (IDE_IS_PROJECT_ITEM (item));
  g_assert (child);

  children = ide_project_item_get_children (item);
  if (!children)
    return NULL;

  for (iter = g_sequence_get_begin_iter (children);
       !g_sequence_iter_is_end (iter);
       iter = g_sequence_iter_next (iter))
    {
      IdeProjectItem *current_item = g_sequence_get (iter);

      if (IDE_IS_PROJECT_FILE (current_item))
        {
          IdeProjectFile *file;
          const gchar *name;

          file = IDE_PROJECT_FILE (current_item);
          name = ide_project_file_get_name (file);

          if (g_strcmp0 (name, child) == 0)
            return current_item;
        }
    }

  return NULL;
}

/**
 * ide_project_files_find_file:
 * @self: (in): an #IdeProjectFiles.
 * @file: a #GFile.
 *
 * Tries to locate an #IdeProjectFile matching the given file.
 * If @file is the working directory, @self is returned.
 *
 * Returns: (transfer none) (nullable): An #IdeProjectItem or %NULL.
 */
IdeProjectItem *
ide_project_files_find_file (IdeProjectFiles *self,
                             GFile           *file)
{
  IdeProjectItem *item;
  IdeContext *context;
  IdeVcs *vcs;
  GFile *workdir;
  gchar **parts;
  gchar *path;
  gsize i;

  g_return_val_if_fail (IDE_IS_PROJECT_FILES (self), NULL);
  g_return_val_if_fail (G_IS_FILE (file), NULL);

  item = IDE_PROJECT_ITEM (self);
  context = ide_object_get_context (IDE_OBJECT (self));
  vcs = ide_context_get_vcs (context);
  workdir = ide_vcs_get_working_directory (vcs);

  if (g_file_equal (workdir, file))
    return IDE_PROJECT_ITEM (self);

  path = g_file_get_relative_path (workdir, file);
  if (path == NULL)
    return NULL;

  parts = g_strsplit (path, G_DIR_SEPARATOR_S, 0);

  for (i = 0; parts [i]; i++)
    {
      if (!(item = ide_project_files_find_child (item, parts [i])))
        break;
    }

  g_strfreev (parts);
  g_free (path);

  return item;
}

/**
 * ide_project_files_get_file_for_path:
 *
 * Retrieves an #IdeFile for the path. If no such path exists within the
 * project, %NULL is returned.
 *
 * Returns: (transfer full) (nullable): An #IdeFile or %NULL.
 */
IdeFile *
ide_project_files_get_file_for_path (IdeProjectFiles *self,
                                     const gchar     *path)
{
  IdeProjectFilesPrivate *priv = ide_project_files_get_instance_private (self);
  IdeProjectItem *item = (IdeProjectItem *)self;
  IdeFile *file = NULL;
  gchar **parts;
  gsize i;

  g_return_val_if_fail (IDE_IS_PROJECT_FILES (self), NULL);

  if ((file = g_hash_table_lookup (priv->files_by_path, path)))
    return g_object_ref (file);

  parts = g_strsplit (path, G_DIR_SEPARATOR_S, 0);

  for (i = 0; item && parts [i]; i++)
    item = ide_project_files_find_child (item, parts [i]);

  if (item)
    {
      IdeContext *context;
      const gchar *file_path;
      GFile *gfile;

      context = ide_object_get_context (IDE_OBJECT (self));
      gfile = ide_project_file_get_file (IDE_PROJECT_FILE (item));
      file_path = ide_project_file_get_path (IDE_PROJECT_FILE (item));
      file = g_object_new (IDE_TYPE_FILE,
                           "context", context,
                           "file", gfile,
                           "path", file_path,
                           NULL);
      if (file)
        g_hash_table_insert (priv->files_by_path, g_strdup (file_path), g_object_ref (file));
    }

  return file;
}

void
ide_project_files_add_file (IdeProjectFiles *self,
                            IdeProjectFile  *file)
{
  IdeProjectItem *item = (IdeProjectItem *)self;
  g_autoptr(GFile) parent = NULL;
  g_autofree gchar *path = NULL;
  IdeContext *context;
  IdeVcs *vcs;
  GFile *workdir;
  GFile *gfile;
  gchar **parts;
  gsize i;

  g_return_if_fail (IDE_IS_PROJECT_FILES (self));
  g_return_if_fail (IDE_IS_PROJECT_FILE (file));

  context = ide_object_get_context (IDE_OBJECT (self));
  vcs = ide_context_get_vcs (context);
  workdir = ide_vcs_get_working_directory (vcs);

  gfile = ide_project_file_get_file (file);
  parent = g_file_get_parent (gfile);
  path = g_file_get_relative_path (workdir, parent);

  if (path == NULL)
  {
    ide_project_item_append (IDE_PROJECT_ITEM (self), IDE_PROJECT_ITEM (file));
    return;
  }

  parts = g_strsplit (path, G_DIR_SEPARATOR_S, 0);

  for (i = 0; parts [i]; i++)
    {
      IdeProjectItem *found;

      found = ide_project_files_find_child (item, parts [i]);

      if (found == NULL)
        {
          g_autoptr(GFileInfo) file_info = NULL;
          g_autofree gchar *child_path = NULL;
          IdeProjectItem *child;
          const gchar *item_path;
          g_autoptr(GFile) item_file = NULL;

          file_info = g_file_info_new ();
          g_file_info_set_file_type (file_info, G_FILE_TYPE_DIRECTORY);
          g_file_info_set_display_name (file_info, parts [i]);
          g_file_info_set_name (file_info, parts [i]);

          item_path = ide_project_file_get_path (IDE_PROJECT_FILE (item));
          child_path = g_strjoin (G_DIR_SEPARATOR_S, item_path, parts [i], NULL);
          item_file = g_file_get_child (workdir, child_path);

          child = g_object_new (IDE_TYPE_PROJECT_FILE,
                                "context", context,
                                "parent", item,
                                "path", path,
                                "file", item_file,
                                "file-info", file_info,
                                NULL);
          ide_project_item_append (item, child);

          item = child;
        }
      else
        {
          item = found;
        }
    }

  ide_project_item_append (item, IDE_PROJECT_ITEM (file));

  g_strfreev (parts);

}
