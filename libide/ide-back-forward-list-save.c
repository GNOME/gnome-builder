/* ide-back-forward-list-save.c
 *
 * Copyright (C) 2015 Christian Hergert <chergert@redhat.com>
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

#define G_LOG_DOMAIN "ide-back-forward-list"

#include "ide-back-forward-item.h"
#include "ide-back-forward-list.h"
#include "ide-back-forward-list-private.h"
#include "ide-debug.h"

#define MAX_ITEMS_PER_FILE 5

typedef struct
{
  GHashTable *counter;
  GString    *content;
  GFile      *file;
} IdeBackForwardListSave;

static void
ide_back_forward_list_save_free (gpointer data)
{
  IdeBackForwardListSave *state = data;

  if (state != NULL)
    {
      g_clear_object (&state->file);

      g_string_free (state->content, TRUE);
      state->content = NULL;

      g_clear_pointer (&state->counter, g_hash_table_unref);

      g_slice_free (IdeBackForwardListSave, state);
    }
}

static void
ide_back_forward_list_save_collect (gpointer data,
                                    gpointer user_data)
{
  IdeBackForwardListSave *state = user_data;
  IdeBackForwardItem *item = data;
  g_autofree gchar *str = NULL;
  gchar *hash_key = NULL;
  IdeUri *uri;
  gsize count;

  g_assert (IDE_IS_BACK_FORWARD_ITEM (item));
  g_assert (state != NULL);
  g_assert (state->content != NULL);
  g_assert (state->counter != NULL);

  uri = ide_back_forward_item_get_uri (item);

  hash_key = g_strdup_printf ("%s://%s%s",
                              ide_uri_get_scheme (uri) ?: "",
                              ide_uri_get_host (uri) ?: "",
                              ide_uri_get_path (uri) ?: "");

  count = GPOINTER_TO_SIZE (g_hash_table_lookup (state->counter, hash_key));

  if (count == MAX_ITEMS_PER_FILE)
    {
      g_free (hash_key);
      return;
    }

  g_hash_table_insert (state->counter, hash_key, GSIZE_TO_POINTER (count + 1));

  str = ide_uri_to_string (uri, 0);
  if (str != NULL)
    g_string_append_printf (state->content, "%s\n", str);
}

static void
ide_back_forward_list_save_worker (GTask        *task,
                                   gpointer      source_object,
                                   gpointer      task_data,
                                   GCancellable *cancellable)
{
  IdeBackForwardListSave *state = task_data;
  g_autoptr(GFile) parent = NULL;
  GError *error = NULL;
  gboolean ret;

  IDE_ENTRY;

  g_assert (IDE_IS_BACK_FORWARD_LIST (source_object));
  g_assert (G_IS_TASK (task));
  g_assert (state != NULL);
  g_assert (G_IS_FILE (state->file));
  g_assert (state->content != NULL);

  parent = g_file_get_parent (state->file);

  if (!g_file_query_exists (parent, cancellable))
    {
      if (!g_file_make_directory_with_parents (parent, cancellable, &error))
        {
          g_task_return_error (task, error);
          IDE_EXIT;
        }
    }

  ret = g_file_replace_contents (state->file,
                                 state->content->str,
                                 state->content->len,
                                 NULL,
                                 FALSE,
                                 G_FILE_CREATE_NONE,
                                 NULL,
                                 cancellable,
                                 &error);

  if (ret == FALSE)
    g_task_return_error (task, error);
  else
    g_task_return_boolean (task, TRUE);

  IDE_EXIT;
}

void
_ide_back_forward_list_save_async (IdeBackForwardList  *self,
                                   GFile               *file,
                                   GCancellable        *cancellable,
                                   GAsyncReadyCallback  callback,
                                   gpointer             user_data)
{
  IdeBackForwardListSave *state;
  g_autoptr(GTask) task = NULL;

  IDE_ENTRY;

  g_return_if_fail (IDE_IS_BACK_FORWARD_LIST (self));
  g_return_if_fail (G_IS_FILE (file));
  g_return_if_fail (!cancellable || G_IS_CANCELLABLE (cancellable));

#ifdef IDE_ENABLE_TRACE
  {
    g_autofree gchar *path = NULL;

    path = g_file_get_path (file);
    IDE_TRACE_MSG ("Saving %s", path);
  }
#endif

  state = g_slice_new0 (IdeBackForwardListSave);
  state->content = g_string_new (NULL);
  state->counter = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, NULL);
  state->file = g_object_ref (file);
  _ide_back_forward_list_foreach (self, ide_back_forward_list_save_collect, state);

  task = g_task_new (self, cancellable, callback, user_data);
  g_task_set_task_data (task, state, ide_back_forward_list_save_free);

  if (state->content->len == 0)
    g_task_return_boolean (task, TRUE);
  else
    g_task_run_in_thread (task, ide_back_forward_list_save_worker);

  IDE_EXIT;
}

gboolean
_ide_back_forward_list_save_finish (IdeBackForwardList  *self,
                                    GAsyncResult        *result,
                                    GError             **error)
{
  gboolean ret;

  IDE_ENTRY;

  g_return_val_if_fail (IDE_IS_BACK_FORWARD_LIST (self), FALSE);
  g_return_val_if_fail (G_IS_TASK (result), FALSE);

  ret = g_task_propagate_boolean (G_TASK (result), error);

  IDE_RETURN (ret);
}
