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

#include "ide-back-forward-item.h"
#include "ide-back-forward-list.h"
#include "ide-back-forward-list-private.h"
#include "ide-debug.h"

#define MAX_ITEMS_PER_FILE 5

typedef struct
{
  GHashTable *counter;
  GString    *content;
} IdeBackForwardListSave;

static void
ide_back_forward_list_save_collect (gpointer data,
                                    gpointer user_data)
{
  IdeBackForwardListSave *state = user_data;
  IdeBackForwardItem *item = data;
  g_autofree gchar *hash_key = NULL;
  g_autofree gchar *str = NULL;
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
  g_string_append_printf (state->content, "%s\n", str);
}

static void
ide_back_forward_list_save_cb (GObject      *object,
                               GAsyncResult *result,
                               gpointer      user_data)
{
  g_autoptr(GTask) task = user_data;
  GFile *file = (GFile *)object;
  GError *error = NULL;

  g_assert (G_IS_FILE (file));
  g_assert (G_IS_TASK (task));

  if (!g_file_replace_contents_finish (file, result, NULL, &error))
    g_task_return_error (task, error);
  else
    g_task_return_boolean (task, TRUE);
}

void
_ide_back_forward_list_save_async (IdeBackForwardList  *self,
                                   GFile               *file,
                                   GCancellable        *cancellable,
                                   GAsyncReadyCallback  callback,
                                   gpointer             user_data)
{
  g_autoptr(GTask) task = NULL;
  GString *content;
  GBytes *bytes;

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

  task = g_task_new (self, cancellable, callback, user_data);

  content = g_string_new (NULL);
  _ide_back_forward_list_foreach (self, ide_back_forward_list_save_collect, content);
  bytes = g_bytes_new_take (content->str, content->len + 1);
  g_string_free (content, FALSE);

  g_file_replace_contents_bytes_async (file,
                                       bytes,
                                       NULL,
                                       FALSE,
                                       G_FILE_CREATE_NONE,
                                       cancellable,
                                       ide_back_forward_list_save_cb,
                                       g_object_ref (task));
}

gboolean
_ide_back_forward_list_save_finish (IdeBackForwardList  *self,
                                    GAsyncResult        *result,
                                    GError             **error)
{
  g_return_val_if_fail (IDE_IS_BACK_FORWARD_LIST (self), FALSE);
  g_return_val_if_fail (G_IS_TASK (result), FALSE);

  return g_task_propagate_boolean (G_TASK (result), error);
}
