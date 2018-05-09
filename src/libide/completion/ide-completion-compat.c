/* ide-completion-compat.c
 *
 * Copyright 2018 Christian Hergert <chergert@redhat.com>
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

#include "config.h"

#include "ide-completion-compat.h"

gboolean
ide_completion_provider_context_in_comment (GtkSourceCompletionContext *context)
{
  GtkTextIter iter;

  g_return_val_if_fail (GTK_SOURCE_IS_COMPLETION_CONTEXT (context), FALSE);

  if (gtk_source_completion_context_get_iter (context, &iter))
    {
      GtkSourceBuffer *buffer = GTK_SOURCE_BUFFER (gtk_text_iter_get_buffer (&iter));

      if (gtk_source_buffer_iter_has_context_class (buffer, &iter, "comment"))
        return TRUE;

      if (!gtk_text_iter_starts_line (&iter))
        {
          gtk_text_iter_backward_char (&iter);
          if (gtk_source_buffer_iter_has_context_class (buffer, &iter, "comment"))
            return TRUE;
        }
    }

  return FALSE;
}

gboolean
ide_completion_provider_context_in_comment_or_string (GtkSourceCompletionContext *context)
{
  GtkTextIter iter;

  g_return_val_if_fail (GTK_SOURCE_IS_COMPLETION_CONTEXT (context), FALSE);

  if (gtk_source_completion_context_get_iter (context, &iter))
    {
      GtkSourceBuffer *buffer = GTK_SOURCE_BUFFER (gtk_text_iter_get_buffer (&iter));

      if (gtk_source_buffer_iter_has_context_class (buffer, &iter, "comment") ||
          gtk_source_buffer_iter_has_context_class (buffer, &iter, "string"))
        return TRUE;

      if (!gtk_text_iter_starts_line (&iter))
        {
          gtk_text_iter_backward_char (&iter);
          if (gtk_source_buffer_iter_has_context_class (buffer, &iter, "comment") ||
              gtk_source_buffer_iter_has_context_class (buffer, &iter, "string"))
            return TRUE;
        }
    }

  return FALSE;
}

gchar *
ide_completion_provider_context_current_word (GtkSourceCompletionContext *context)
{
  GtkTextIter end;
  GtkTextIter begin;
  gunichar ch = 0;

  g_return_val_if_fail (GTK_SOURCE_IS_COMPLETION_CONTEXT (context), NULL);

  if (!gtk_source_completion_context_get_iter (context, &end))
    return NULL;

  begin = end;

  do
    {
      if (!gtk_text_iter_backward_char (&begin))
        break;
      ch = gtk_text_iter_get_char (&begin);
    }
  while (g_unichar_isalnum (ch) || (ch == '_'));

  if (ch && !g_unichar_isalnum (ch) && (ch != '_'))
    gtk_text_iter_forward_char (&begin);

  return gtk_text_iter_get_slice (&begin, &end);
}
