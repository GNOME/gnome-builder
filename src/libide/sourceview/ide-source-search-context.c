/* ide-source-search-context.c
 *
 * Copyright 2018-2019 Christian Hergert <chergert@redhat.com>
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

#define G_LOG_DOMAIN "ide-source-search-context"

#include "config.h"

#include <libide-threading.h>

#include "ide-source-search-context.h"

typedef struct
{
  GtkTextBuffer *buffer;
  GtkTextMark *begin;
  GtkTextMark *end;
  guint wrapped : 1;
} SearchData;

static void
search_data_free (SearchData *sd)
{
  if (sd->begin != NULL)
    {
      gtk_text_buffer_delete_mark (GTK_TEXT_BUFFER (sd->buffer), sd->begin);
      g_clear_object (&sd->begin);
    }

  if (sd->end != NULL)
    {
      gtk_text_buffer_delete_mark (GTK_TEXT_BUFFER (sd->buffer), sd->end);
      g_clear_object (&sd->end);
    }

  g_clear_object (&sd->buffer);
  g_slice_free (SearchData, sd);
}

/**
 * ide_source_search_context_backward_async:
 *
 * This function is an alternate implementation of async backward search
 * that works around an issue in upstream GtkSourceView until it is fixed
 * and we can remove this.
 *
 * https://gitlab.gnome.org/GNOME/gtksourceview/issues/8
 *
 * Since: 3.32
 */
void
ide_source_search_context_backward_async (GtkSourceSearchContext *search,
                                          const GtkTextIter      *iter,
                                          GCancellable           *cancellable,
                                          GAsyncReadyCallback     callback,
                                          gpointer                user_data)
{
  g_autoptr(IdeTask) task = NULL;
  SearchData *data;
  GtkTextIter begin, end;
  gboolean wrapped = FALSE;

  g_return_if_fail (GTK_SOURCE_IS_SEARCH_CONTEXT (search));
  g_return_if_fail (iter != NULL);
  g_return_if_fail (!cancellable || G_IS_CANCELLABLE (cancellable));

  task = ide_task_new (search, cancellable, callback, user_data);
  ide_task_set_priority (task, G_PRIORITY_LOW);
  ide_task_set_source_tag (task, ide_source_search_context_backward_async);

  data = g_slice_new0 (SearchData);

  /*
   * To work around a GtkSourceView bug, we just do this synchronously and
   * then return to get another result on the next go round.
   *
   * We should remove the code once issue 8 is fixed.
   */

  if (gtk_source_search_context_backward (search, iter, &begin, &end, &wrapped))
    {
      data->wrapped = wrapped;
      data->buffer = g_object_ref (GTK_TEXT_BUFFER (gtk_source_search_context_get_buffer (search)));
      data->begin = gtk_text_buffer_create_mark (data->buffer, NULL, &begin, TRUE);
      data->end = gtk_text_buffer_create_mark (data->buffer, NULL, &end, TRUE);

      g_object_ref (data->begin);
      g_object_ref (data->end);
    }

  ide_task_return_pointer (task, data, search_data_free);
}

/**
 * ide_source_search_context_backward_finish2:
 * @search: a #GtkSourceSearchContext
 * @result: a #GAsyncResult
 * @match_begin: (out): a #GtkTextIter
 * @match_end: (out): a #GtkTextIter
 * @has_wrapped_around: (out): a location to a boolean
 * @error: a location for a #GError
 *
 * Since: 3.32
 */
gboolean
ide_source_search_context_backward_finish2 (GtkSourceSearchContext  *search,
                                            GAsyncResult            *result,
                                            GtkTextIter             *match_begin,
                                            GtkTextIter             *match_end,
                                            gboolean                *has_wrapped_around,
                                            GError                 **error)
{
  SearchData *sd;
  gboolean ret;

  g_return_val_if_fail (GTK_SOURCE_IS_SEARCH_CONTEXT (search), FALSE);
  g_return_val_if_fail (IDE_IS_TASK (result), FALSE);

  if (has_wrapped_around)
    *has_wrapped_around = FALSE;
  if (match_begin)
    memset (match_begin, 0, sizeof *match_begin);
  if (match_end)
    memset (match_end, 0, sizeof *match_end);

  if (!(sd = ide_task_propagate_pointer (IDE_TASK (result), error)))
    return FALSE;

  if (match_begin && sd->begin)
    gtk_text_buffer_get_iter_at_mark (sd->buffer, match_begin, sd->begin);

  if (match_end && sd->end)
    gtk_text_buffer_get_iter_at_mark (sd->buffer, match_end, sd->end);

  ret = sd->begin != NULL;

  if (has_wrapped_around)
    *has_wrapped_around = sd->wrapped;

  search_data_free (sd);

  return ret;
}
