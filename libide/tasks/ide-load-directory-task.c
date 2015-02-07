/* ide-load-directory-task.c
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

#include <glib/gi18n.h>

#include "ide-load-directory-task.h"
#include "ide-project-item.h"
#include "ide-project-file.h"

#define NUM_FILES_DEFAULT 1000

typedef struct
{
  gint        ref_count;
  guint       failed : 1;
  GTask      *task;
  GHashTable *project_items;
} TaskState;

static void enumerate_children_cb (GObject      *object,
                                   GAsyncResult *result,
                                   gpointer      user_data);
static void query_info_cb         (GObject      *object,
                                   GAsyncResult *result,
                                   gpointer      user_data);
static void next_files_cb         (GObject      *object,
                                   GAsyncResult *result,
                                   gpointer      user_data);

static void
task_state_unref (TaskState *state)
{
  g_return_if_fail (state->ref_count > 0);

  if (--state->ref_count == 0)
    {
      if (state->failed)
        g_task_return_new_error (state->task,
                                 G_IO_ERROR,
                                 G_IO_ERROR_FAILED,
                                 _("Failed to enumerate files."));
      else
        g_task_return_boolean (state->task, TRUE);
      g_hash_table_unref (state->project_items);
      g_slice_free (TaskState, state);
    }
}

static TaskState *
task_state_ref (TaskState *state)
{
  g_return_if_fail (state->ref_count > 0);

  ++state->ref_count;

  return state;
}

static gboolean
should_ignore_file (const gchar *name)
{
  g_return_val_if_fail (name, FALSE);

  if (g_str_equal (name, ".git"))
    return TRUE;

  name = strrchr (name, '.');

  if (name &&
      (g_str_has_suffix (name, "~") ||
       g_str_equal (name, ".so") ||
       g_str_equal (name, ".o") ||
       g_str_equal (name, ".la")))
    return TRUE;

  return FALSE;
}

static void
next_files_cb (GObject      *object,
               GAsyncResult *result,
               gpointer      user_data)
{
  GFileEnumerator *enumerator = (GFileEnumerator *)object;
  IdeProjectItem *parent;
  IdeContext *context;
  TaskState *state = user_data;
  GError *error = NULL;
  GFile *directory;
  GList *list;
  GList *iter;

  g_return_if_fail (G_IS_FILE_ENUMERATOR (enumerator));
  g_return_if_fail (state);

  directory = g_file_enumerator_get_container (enumerator);
  parent = g_hash_table_lookup (state->project_items, directory);
  context = ide_object_get_context (IDE_OBJECT (parent));

  list = g_file_enumerator_next_files_finish (enumerator, result, &error);

  if (error)
    {
      state->failed = TRUE;
      goto cleanup;
    }

  for (iter = list; iter; iter = iter->next)
    {
      IdeProjectItem *item;
      GFileInfo *file_info = iter->data;
      const gchar *name;
      GFileType file_type;
      GFile *file = NULL;

      g_assert (G_IS_FILE_INFO (file_info));

      file_type = g_file_info_get_file_type (file_info);

      if ((file_type != G_FILE_TYPE_DIRECTORY) &&
          (file_type != G_FILE_TYPE_REGULAR))
        continue;

      name = g_file_info_get_display_name (file_info);

      if (should_ignore_file (name))
        continue;

      item = g_object_new (IDE_TYPE_PROJECT_FILE,
                           "context", context,
                           "file-info", file_info,
                           "parent", parent,
                           NULL);
      ide_project_item_append (parent, item);

      file = g_file_enumerator_get_child (enumerator, file_info);

      if (file_type == G_FILE_TYPE_DIRECTORY)
        {
          g_hash_table_insert (state->project_items,
                               g_object_ref (file),
                               g_object_ref (item));
          g_file_enumerate_children_async (file,
                                           (G_FILE_ATTRIBUTE_STANDARD_TYPE","
                                            G_FILE_ATTRIBUTE_STANDARD_DISPLAY_NAME),
                                           G_FILE_QUERY_INFO_NONE,
                                           G_PRIORITY_DEFAULT,
                                           g_task_get_cancellable (state->task),
                                           enumerate_children_cb,
                                           task_state_ref (state));
        }

      g_clear_object (&item);
      g_clear_object (&file);
    }

cleanup:
  g_list_foreach (list, (GFunc)g_object_unref, NULL);
  g_list_free (list);
  task_state_unref (state);
}

static void
enumerate_children_cb (GObject      *object,
                       GAsyncResult *result,
                       gpointer      user_data)
{
  GFileEnumerator *enumerator;
  TaskState *state = user_data;
  GError *error = NULL;
  GFile *directory = (GFile *)object;

  g_return_if_fail (G_IS_FILE (directory));
  g_return_if_fail (state);

  enumerator = g_file_enumerate_children_finish (directory, result, &error);

  if (!enumerator)
    {
      state->failed = TRUE;
      goto cleanup;
    }

  g_file_enumerator_next_files_async (enumerator,
                                      NUM_FILES_DEFAULT,
                                      G_PRIORITY_DEFAULT,
                                      g_task_get_cancellable (state->task),
                                      next_files_cb,
                                      task_state_ref (state));

cleanup:
  g_clear_object (&enumerator);
  task_state_unref (state);
}

static void
query_info_cb (GObject      *object,
               GAsyncResult *result,
               gpointer      user_data)
{
  GFileInfo *file_info;
  GFileType file_type;
  TaskState *state = user_data;
  GError *error = NULL;
  GFile *directory = (GFile *)object;

  g_return_if_fail (G_IS_FILE (directory));
  g_return_if_fail (state);

  file_info = g_file_query_info_finish (directory, result, &error);

  if (!file_info)
    {
      state->failed = TRUE;
      goto cleanup;
    }

  file_type = g_file_info_get_file_type (file_info);

  if (file_type != G_FILE_TYPE_DIRECTORY)
    {
      state->failed = TRUE;
      goto cleanup;
    }

  g_file_enumerate_children_async (directory,
                                   (G_FILE_ATTRIBUTE_STANDARD_TYPE","
                                    G_FILE_ATTRIBUTE_STANDARD_DISPLAY_NAME),
                                   G_FILE_QUERY_INFO_NONE,
                                   G_PRIORITY_DEFAULT,
                                   g_task_get_cancellable (state->task),
                                   enumerate_children_cb,
                                   task_state_ref (state));

cleanup:
  g_clear_object (&file_info);
  task_state_unref (state);
}

GTask *
ide_load_directory_task_new (gpointer             source_object,
                             GFile               *directory,
                             IdeProjectItem      *parent,
                             int                  io_priority,
                             GCancellable        *cancellable,
                             GAsyncReadyCallback  callback,
                             gpointer             user_data)
{
  TaskState *state;
  GTask *task;

  g_return_val_if_fail (G_IS_FILE (directory), NULL);
  g_return_val_if_fail (IDE_IS_PROJECT_ITEM (parent), NULL);
  g_return_val_if_fail (!cancellable || G_IS_CANCELLABLE (cancellable), NULL);

  state = g_slice_new0 (TaskState);
  state->ref_count = 1;
  state->task = g_task_new (source_object, cancellable, callback, user_data);
  state->project_items = g_hash_table_new_full (g_direct_hash,
                                                g_direct_equal,
                                                g_object_unref,
                                                g_object_unref);

  g_hash_table_insert (state->project_items,
                       g_object_ref (directory),
                       g_object_ref (parent));

  g_file_query_info_async (directory,
                           G_FILE_ATTRIBUTE_STANDARD_TYPE,
                           G_FILE_QUERY_INFO_NONE,
                           io_priority,
                           cancellable,
                           query_info_cb,
                           task_state_ref (state));

  task = g_object_ref (state->task);
  task_state_unref (state);

  return task;
}
