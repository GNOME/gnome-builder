/* gb-editor-commands.c
 *
 * Copyright (C) 2014 Christian Hergert <christian@hergert.me>
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

#include "gb-editor-commands.h"
#include "gb-editor-tab.h"
#include "gb-editor-tab-private.h"
#include "gb-log.h"
#include "gb-source-formatter.h"

/**
 * gb_editor_commands_reformat:
 * @tab: A #GbEditorTab.
 *
 * Begin a source reformatting operation.
 *
 * TODO:
 *    - Use source reformatting rules based on the document language.
 *    - Perform operation asynchronously, while locking the editor.
 *    - Track editor state (loading/saving/operation/etc)
 *    - Maybe add GbSourceOperation? These could do lots of
 *      transforms, useful for FixIt's too?
 */
void
gb_editor_commands_reformat (GbEditorTab *tab)
{
  GbEditorTabPrivate *priv;
  GbSourceFormatter *formatter;
  GtkSourceLanguage *language;
  GtkTextBuffer *buffer;
  GtkTextIter begin;
  GtkTextIter end;
  GtkTextIter iter;
  GtkTextMark *insert;
  gboolean fragment = TRUE;
  GError *error = NULL;
  gchar *input = NULL;
  gchar *output = NULL;
  guint line_number;
  guint char_offset;

  ENTRY;

  /*
   * TODO: Do this asynchronously, add tab state, propagate errors.
   */

  g_return_if_fail (GB_IS_EDITOR_TAB (tab));

  priv = tab->priv;

  buffer = gtk_text_view_get_buffer (GTK_TEXT_VIEW (priv->source_view));

  gtk_text_buffer_get_selection_bounds (buffer, &begin, &end);

  if (gtk_text_iter_compare (&begin, &end) == 0)
    {
      gtk_text_buffer_get_bounds (buffer, &begin, &end);
      fragment = FALSE;
    }

  input = gtk_text_buffer_get_text (buffer, &begin, &end, TRUE);

  insert = gtk_text_buffer_get_insert (buffer);
  gtk_text_buffer_get_iter_at_mark (buffer, &iter, insert);
  char_offset = gtk_text_iter_get_line_offset (&iter);
  line_number = gtk_text_iter_get_line (&iter);

  language = gtk_source_buffer_get_language (GTK_SOURCE_BUFFER (buffer));
  formatter = gb_source_formatter_new_from_language (language);

  if (!gb_source_formatter_format (formatter, input, fragment, NULL, &output,
                                   &error))
    {
      g_warning ("%s", error->message);
      g_clear_error (&error);
      GOTO (cleanup);
    }

  gtk_text_buffer_begin_user_action (buffer);

  gb_source_view_clear_snippets (priv->source_view);

  /* TODO: Keep the cursor on same CXCursor from Clang instead of the
   *       same character offset within the buffer. We probably want
   *       to defer this to the formatter API since it will be language
   *       specific.
   */

  gtk_text_buffer_delete (buffer, &begin, &end);
  gtk_text_buffer_insert (buffer, &begin, output, -1);

  if (line_number >= gtk_text_buffer_get_line_count (buffer))
    {
      gtk_text_buffer_get_bounds (buffer, &begin, &iter);
      goto select_range;
    }

  gtk_text_buffer_get_iter_at_line (buffer, &iter, line_number);
  gtk_text_iter_forward_to_line_end (&iter);

  if (gtk_text_iter_get_line (&iter) != line_number)
    gtk_text_iter_backward_char (&iter);
  else if (gtk_text_iter_get_line_offset (&iter) > char_offset)
    gtk_text_buffer_get_iter_at_line_offset (buffer, &iter, line_number, char_offset);

select_range:
  gtk_text_buffer_select_range (buffer, &iter, &iter);
  gtk_text_buffer_end_user_action (buffer);

  gtk_text_view_scroll_to_iter (GTK_TEXT_VIEW (priv->source_view), &iter,
                                0.25, TRUE, 0.5, 0.5);

cleanup:
  g_free (input);
  g_free (output);
  g_clear_object (&formatter);

  EXIT;
}
