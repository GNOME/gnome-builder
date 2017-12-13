/* gb-beautifier-helper.c
 *
 * Copyright © 2016 sebastien lafargue <slafargue@gnome.org>
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

#define G_LOG_DOMAIN "gb-beautifier-helper"

#include <glib.h>
#include <gtksourceview/gtksource.h>
#include <ide.h>
#include <string.h>

#include "gb-beautifier-helper.h"

typedef struct
{
  GbBeautifierEditorAddin *self;
  GFile                   *file;
  GFileIOStream           *stream;
  gsize                    len;
} SaveTmpState;

static void
save_tmp_state_free (gpointer data)
{
  SaveTmpState *state = (SaveTmpState *)data;

  if (!g_io_stream_is_closed ((GIOStream *)state->stream))
    g_io_stream_close ((GIOStream *)state->stream, NULL, NULL);

  g_clear_object (&state->file);

  g_slice_free (SaveTmpState, state);
}

static void
gb_beautifier_helper_create_tmp_file_cb (GObject      *object,
                                         GAsyncResult *result,
                                         gpointer      user_data)
{
  GOutputStream *output_stream = (GOutputStream *)object;
  g_autoptr(GError) error = NULL;
  g_autoptr(GTask) task = (GTask *)user_data;
  SaveTmpState *state;
  gsize count;

  g_assert (G_IS_OUTPUT_STREAM (output_stream));
  g_assert (G_IS_ASYNC_RESULT (result));
  g_assert (G_IS_TASK (task));

  state = (SaveTmpState *)g_task_get_task_data (task);
  if (!g_output_stream_write_all_finish (output_stream, result, &count, &error))
    g_task_return_error (task, g_steal_pointer (&error));
  else if (g_task_return_error_if_cancelled (task))
    gb_beautifier_helper_remove_tmp_file (state->self, state->file);
  else
    g_task_return_pointer (task, g_steal_pointer (&state->file), g_object_unref);
}

void
gb_beautifier_helper_create_tmp_file_async (GbBeautifierEditorAddin *self,
                                            const gchar             *text,
                                            GAsyncReadyCallback      callback,
                                            GCancellable            *cancellable,
                                            gpointer                 user_data)
{
  SaveTmpState *state;
  GFile *file = NULL;
  GFileIOStream *stream;
  GOutputStream *output_stream;
  g_autoptr(GTask) task = NULL;
  g_autoptr(GError) error = NULL;

  g_assert (GB_IS_BEAUTIFIER_EDITOR_ADDIN (self));
  g_assert (!dzl_str_empty0 (text));
  g_assert (!cancellable || G_IS_CANCELLABLE (cancellable));
  g_assert (callback != NULL);

  task = g_task_new (self, cancellable, callback, user_data);
  g_task_set_source_tag (task, gb_beautifier_helper_create_tmp_file_async);
  state = g_slice_new0 (SaveTmpState);
  state->self = self;
  g_task_set_task_data (task, state, save_tmp_state_free);

  if (NULL == (file = g_file_new_tmp ("gnome-builder-beautifier-XXXXXX.txt", &stream, &error)))
    {
      g_task_return_error (task, g_steal_pointer (&error));
      return;
    }

  state->file = file;
  state->stream = stream;
  state->len = strlen (text);

  output_stream = g_io_stream_get_output_stream ((GIOStream *)stream);
  g_output_stream_write_all_async (output_stream,
                                   text,
                                   state->len,
                                   G_PRIORITY_DEFAULT,
                                   cancellable,
                                   gb_beautifier_helper_create_tmp_file_cb,
                                   g_steal_pointer (&task));
}

GFile *
gb_beautifier_helper_create_tmp_file_finish (GbBeautifierEditorAddin  *self,
                                             GAsyncResult             *result,
                                             GError                  **error)
{
  GTask *task = (GTask *)result;

  g_return_val_if_fail (GB_IS_BEAUTIFIER_EDITOR_ADDIN (self), NULL);
  g_return_val_if_fail (g_task_is_valid (result, self), NULL);

  return g_task_propagate_pointer (task, error);
}

void
gb_beautifier_helper_remove_tmp_file (GbBeautifierEditorAddin *self,
                                      GFile                   *tmp_file)
{
  g_assert (GB_IS_BEAUTIFIER_EDITOR_ADDIN (self));

  g_file_delete (tmp_file, NULL, NULL);
}

const gchar *
gb_beautifier_helper_get_lang_id (GbBeautifierEditorAddin *self,
                                  IdeSourceView           *view)
{
  GtkTextBuffer *buffer;
  GtkSourceLanguage *lang;

  g_assert (GB_IS_BEAUTIFIER_EDITOR_ADDIN (self));
  g_assert (IDE_IS_SOURCE_VIEW (view));

  buffer = GTK_TEXT_BUFFER (gtk_text_view_get_buffer (GTK_TEXT_VIEW (view)));
  if (NULL == (lang = gtk_source_buffer_get_language (GTK_SOURCE_BUFFER (buffer))))
    {
      g_warning ("Beautifier plugin: Can't find a GtkSourceLanguage for the buffer");
      return NULL;
    }

  return gtk_source_language_get_id (lang);
}
