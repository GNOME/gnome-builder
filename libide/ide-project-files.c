/* ide-project-files.c
 *
 * Copyright (C) 2015 Christian Hergert <christian@hergert.me>
 *
 * This file is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as
 * published by the Free Software Foundation; either version 3 of the
 * License, or (at your option) any later version.
 *
 * This file is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "ide-project-file.h"
#include "ide-project-files.h"

G_DEFINE_TYPE (IdeProjectFiles, ide_project_files, IDE_TYPE_PROJECT_ITEM)

static void
ide_project_files_class_init (IdeProjectFilesClass *klass)
{
}

static void
ide_project_files_init (IdeProjectFiles *self)
{
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
      IdeProjectItem *item = g_sequence_get (iter);

      if (IDE_IS_PROJECT_FILE (item))
        {
          IdeProjectFile *file = IDE_PROJECT_FILE (item);
          const gchar *name = ide_project_file_get_name (file);

          if (g_strcmp0 (name, child) == 0)
            return item;
        }
    }

  return NULL;
}

IdeFile *
ide_project_files_get_file_for_path (IdeProjectFiles *self,
                                     const gchar     *path)
{
  IdeProjectItem *item = (IdeProjectItem *)self;
  gchar **parts;
  gsize i;

  g_return_val_if_fail (IDE_IS_PROJECT_FILES (self), NULL);

  parts = g_strsplit (path, G_DIR_SEPARATOR_S, 0);

  for (i = 0; item && parts [i]; i++)
    item = ide_project_files_find_child (item, parts [i]);

  if (item)
    {
      IdeFile *file = NULL;

      g_warning ("TODO: Get/find/create idefile for %s\n", ide_project_file_get_name (IDE_PROJECT_FILE (item)));

      return file;
    }

  return NULL;
}
