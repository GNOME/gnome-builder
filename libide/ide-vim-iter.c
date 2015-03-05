/* ide-vim-iter.c
 *
 * Copyright (C) 2015 Christian Hergert <christian@hergert.me>
 *
 * This file is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This file is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <gtk/gtk.h>

#include "ide-debug.h"
#include "ide-vim-iter.h"

typedef enum
{
  SENTENCE_OK,
  SENTENCE_PARA,
  SENTENCE_FAILED,
} SentenceStatus;

static gboolean
_ide_vim_iter_line_is_empty (GtkTextIter *iter)
{
  return gtk_text_iter_starts_line (iter) && gtk_text_iter_ends_line (iter);
}

/**
 * _ide_vim_iter_backward_paragraph_start:
 * @iter: A #GtkTextIter
 *
 * Searches backwards until we find the beginning of a paragraph.
 *
 * Returns: %TRUE if we are not at the beginning of the buffer; otherwise %FALSE.
 */
gboolean
_ide_vim_iter_backward_paragraph_start (GtkTextIter *iter)
{
  g_return_val_if_fail (iter, FALSE);

  /* Work our way past the current empty lines */
  if (_ide_vim_iter_line_is_empty (iter))
    while (_ide_vim_iter_line_is_empty (iter))
      if (!gtk_text_iter_backward_line (iter))
        return FALSE;

  /* Now find first line that is empty */
  while (!_ide_vim_iter_line_is_empty (iter))
    if (!gtk_text_iter_backward_line (iter))
      return FALSE;

  return TRUE;
}

/**
 * _ide_vim_iter_forward_paragraph_end:
 * @iter: A #GtkTextIter
 *
 * Searches forward until the end of a paragraph has been hit.
 *
 * Returns: %TRUE if we are not at the end of the buffer; otherwise %FALSE.
 */
gboolean
_ide_vim_iter_forward_paragraph_end (GtkTextIter *iter)
{
  g_return_val_if_fail (iter, FALSE);

  /* Work our way past the current empty lines */
  if (_ide_vim_iter_line_is_empty (iter))
    while (_ide_vim_iter_line_is_empty (iter))
      if (!gtk_text_iter_forward_line (iter))
        return FALSE;

  /* Now find first line that is empty */
  while (!_ide_vim_iter_line_is_empty (iter))
    if (!gtk_text_iter_forward_line (iter))
      return FALSE;

  return TRUE;
}

static gboolean
sentence_end_chars (gunichar ch,
                    gpointer user_data)
{
  switch (ch)
    {
    case '!':
    case '.':
    case '?':
      return TRUE;

    default:
      return FALSE;
    }
}

static SentenceStatus
_ide_vim_iter_backward_sentence_end (GtkTextIter *iter)
{
  GtkTextIter end_bounds;
  GtkTextIter start_bounds;
  gboolean found_para;

  g_return_val_if_fail (iter, FALSE);

  end_bounds = *iter;
  start_bounds = *iter;
  found_para = _ide_vim_iter_backward_paragraph_start (&start_bounds);

  if (!found_para)
    gtk_text_buffer_get_start_iter (gtk_text_iter_get_buffer (iter), &start_bounds);

  while ((gtk_text_iter_compare (iter, &start_bounds) > 0) && gtk_text_iter_backward_char (iter))
    {
      if (gtk_text_iter_backward_find_char (iter, sentence_end_chars, NULL, &end_bounds))
        {
          GtkTextIter copy = *iter;

          while (gtk_text_iter_forward_char (&copy) && (gtk_text_iter_compare (&copy, &end_bounds) < 0))
            {
              gunichar ch;

              ch = gtk_text_iter_get_char (&copy);

              switch (ch)
                {
                case ']':
                case ')':
                case '"':
                case '\'':
                  continue;

                case ' ':
                case '\n':
                  *iter = copy;
                  return SENTENCE_OK;

                default:
                  break;
                }
            }
        }
    }

  *iter = start_bounds;

  if (found_para)
    return SENTENCE_PARA;

  return SENTENCE_FAILED;
}

gboolean
_ide_vim_iter_forward_sentence_end (GtkTextIter *iter)
{
  g_return_val_if_fail (iter, FALSE);

  return FALSE;
}

gboolean
_ide_vim_iter_backward_sentence_start (GtkTextIter *iter)
{
  GtkTextIter tmp;
  SentenceStatus status;

  g_return_val_if_fail (iter, FALSE);

  tmp = *iter;
  status = _ide_vim_iter_backward_sentence_end (&tmp);

  switch (status)
    {
    case SENTENCE_PARA:
    case SENTENCE_OK:
      {
        GtkTextIter copy = tmp;

        /*
         * try to work forward to first non-whitespace char.
         * if we land where we started, discard the walk.
         */
        while (g_unichar_isspace (gtk_text_iter_get_char (&copy)))
          if (!gtk_text_iter_forward_char (&copy))
            break;
        if (gtk_text_iter_compare (&copy, iter) < 0)
          tmp = copy;
        *iter = tmp;

        return TRUE;
      }

    case SENTENCE_FAILED:
    default:
      gtk_text_buffer_get_start_iter (gtk_text_iter_get_buffer (iter), iter);
      return FALSE;
    }
}
