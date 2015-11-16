/* ide-back-forward-list-load.c
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

#include <stdio.h>
#include <stdlib.h>

#include "ide-back-forward-item.h"
#include "ide-back-forward-list.h"
#include "ide-back-forward-list-private.h"
#include "ide-context.h"
#include "ide-debug.h"

static void
ide_back_forward_list_load_cb (GObject      *object,
                               GAsyncResult *result,
                               gpointer      user_data)
{
  g_autoptr(GTask) task = user_data;
  g_autofree gchar *contents = NULL;
  g_auto(GStrv) lines = NULL;
  IdeBackForwardList *self;
  IdeContext *context;
  GError *error = NULL;
  GFile *file = (GFile *)object;
  gsize length = 0;
  gsize n_lines;
  gint i;

  g_assert (G_IS_FILE (file));
  g_assert (G_IS_TASK (task));

  self = g_task_get_source_object (task);
  g_assert (IDE_IS_BACK_FORWARD_LIST (self));

  context = ide_object_get_context (IDE_OBJECT (self));
  g_assert (IDE_IS_CONTEXT (context));

  if (!g_file_load_contents_finish (file, result, &contents, &length, NULL, &error))
    {
      g_task_return_error (task, error);
      return;
    }

  if (length > (10 * 1024 * 1024))
    {
      g_task_return_new_error (task,
                               G_IO_ERROR,
                               G_IO_ERROR_INVALID_DATA,
                               "Implausible file size discovered");
      return;
    }

  if (!g_utf8_validate (contents, length, NULL))
    {
      g_task_return_new_error (task,
                               G_IO_ERROR,
                               G_IO_ERROR_INVALID_DATA,
                               "The content was not UTF-8 formatted");
      return;
    }

  lines = g_strsplit (contents, "\n", 0);
  n_lines = g_strv_length (lines);

  for (i = n_lines; i > 0; i--)
    {
      const gchar *line = lines [i - 1];
      g_autoptr(IdeUri) uri = NULL;
      g_autoptr(IdeBackForwardItem) item = NULL;
      g_autofree gchar *new_style_uri = NULL;
      char *old_style_uri = NULL;
      guint lineno = 0;
      guint line_offset = 0;

      if (ide_str_empty0 (line))
        continue;

      /* Convert from old style "LINE OFFSET URI" to new-style "URI". */
      if (3 == sscanf (line, "%u %u %ms", &lineno, &line_offset, &old_style_uri))
        {
          line = new_style_uri = g_strdup_printf ("%s#L%u_%u", old_style_uri, lineno, line_offset);
          free (old_style_uri);
        }

      uri = ide_uri_new (line, 0, &error);

      if (uri == NULL)
        {
          g_task_return_error (task, error);
          return;
        }

      item = ide_back_forward_item_new (context, uri);
      ide_back_forward_list_push (self, item);
    }

  g_task_return_boolean (task, TRUE);
}

void
_ide_back_forward_list_load_async (IdeBackForwardList  *self,
                                   GFile               *file,
                                   GCancellable        *cancellable,
                                   GAsyncReadyCallback  callback,
                                   gpointer             user_data)
{
  g_autoptr(GTask) task = NULL;

  g_assert (IDE_IS_BACK_FORWARD_LIST (self));
  g_assert (G_IS_FILE (file));
  g_assert (!cancellable || G_IS_CANCELLABLE (cancellable));

#ifdef IDE_ENABLE_TRACE
  {
    g_autofree gchar *path = NULL;

    path = g_file_get_path (file);
    IDE_TRACE_MSG ("Loading %s", path);
  }
#endif

  task = g_task_new (self, cancellable, callback, user_data);

  g_file_load_contents_async (file,
                              cancellable,
                              ide_back_forward_list_load_cb,
                              g_object_ref (task));
}

gboolean
_ide_back_forward_list_load_finish (IdeBackForwardList  *self,
                                    GAsyncResult        *result,
                                    GError             **error)
{
  g_return_val_if_fail (IDE_IS_BACK_FORWARD_LIST (self), FALSE);
  g_return_val_if_fail (G_IS_TASK (result), FALSE);

  return g_task_propagate_boolean (G_TASK (result), error);
}
