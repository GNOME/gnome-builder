/* ide-text-iter.c
 *
 * Copyright 2015-2019 Christian Hergert <christian@hergert.me>
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

#define G_LOG_DOMAIN "ide-text-iter"

#include "config.h"

#include <gtk/gtk.h>
#include <gtksourceview/gtksource.h>
#include <string.h>

#include "ide-text-iter.h"

typedef enum
{
  SENTENCE_OK,
  SENTENCE_PARA,
  SENTENCE_FAILED,
} SentenceStatus;

enum
{
  CLASS_0,
  CLASS_NEWLINE,
  CLASS_SPACE,
  CLASS_SPECIAL,
  CLASS_WORD,
};

static int
ide_text_word_classify (gunichar ch)
{
  switch (ch)
    {
    case ' ':
    case '\t':
    case '\n':
      return CLASS_SPACE;

    case '"': case '\'':
    case '(': case ')':
    case '{': case '}':
    case '[': case ']':
    case '<': case '>':
    case '-': case '+': case '*': case '/':
    case '!': case '@': case '#': case '$': case '%':
    case '^': case '&': case ':': case ';': case '?':
    case '|': case '=': case '\\': case '.': case ',':
      return CLASS_SPECIAL;

    case '_':
    default:
      return CLASS_WORD;
    }
}

static int
ide_text_word_classify_newline_stop (gunichar ch)
{
  switch (ch)
    {
    case ' ':
    case '\t':
      return CLASS_SPACE;

    case '\n':
      return CLASS_NEWLINE;

    case '"': case '\'':
    case '(': case ')':
    case '{': case '}':
    case '[': case ']':
    case '<': case '>':
    case '-': case '+': case '*': case '/':
    case '!': case '@': case '#': case '$': case '%':
    case '^': case '&': case ':': case ';': case '?':
    case '|': case '=': case '\\': case '.': case ',':
      return CLASS_SPECIAL;

    case '_':
    default:
      return CLASS_WORD;
    }
}

static int
ide_text_WORD_classify (gunichar ch)
{
  if (g_unichar_isspace (ch))
    return CLASS_SPACE;
  return CLASS_WORD;
}

static int
ide_text_WORD_classify_newline_stop (gunichar ch)
{
  if (ch == '\n')
    return CLASS_NEWLINE;

  if (g_unichar_isspace (ch))
    return CLASS_SPACE;
  return CLASS_WORD;
}

static gboolean
ide_text_iter_line_is_empty (GtkTextIter *iter)
{
  return gtk_text_iter_starts_line (iter) && gtk_text_iter_ends_line (iter);
}

/**
 * ide_text_iter_backward_paragraph_start:
 * @iter: a #GtkTextIter
 *
 * Searches backwards until we find the beginning of a paragraph.
 *
 * Returns: %TRUE if we are not at the beginning of the buffer; otherwise %FALSE.
 */
gboolean
ide_text_iter_backward_paragraph_start (GtkTextIter *iter)
{
  g_return_val_if_fail (iter, FALSE);

  /* Work our way past the current empty lines */
  if (ide_text_iter_line_is_empty (iter))
    while (ide_text_iter_line_is_empty (iter))
      if (!gtk_text_iter_backward_line (iter))
        return FALSE;

  /* Now find first line that is empty */
  while (!ide_text_iter_line_is_empty (iter))
    if (!gtk_text_iter_backward_line (iter))
      return FALSE;

  return TRUE;
}

/**
 * ide_text_iter_forward_paragraph_end:
 * @iter: a #GtkTextIter
 *
 * Searches forward until the end of a paragraph has been hit.
 *
 * Returns: %TRUE if we are not at the end of the buffer; otherwise %FALSE.
 */
gboolean
ide_text_iter_forward_paragraph_end (GtkTextIter *iter)
{
  g_return_val_if_fail (iter, FALSE);

  /* Work our way past the current empty lines */
  if (ide_text_iter_line_is_empty (iter))
    while (ide_text_iter_line_is_empty (iter))
      if (!gtk_text_iter_forward_line (iter))
        return FALSE;

  /* Now find first line that is empty */
  while (!ide_text_iter_line_is_empty (iter))
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
ide_text_iter_backward_sentence_end (GtkTextIter *iter)
{
  GtkTextIter end_bounds;
  GtkTextIter start_bounds;
  gboolean found_para;

  g_return_val_if_fail (iter, FALSE);

  end_bounds = *iter;
  start_bounds = *iter;
  found_para = ide_text_iter_backward_paragraph_start (&start_bounds);

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
ide_text_iter_forward_sentence_end (GtkTextIter *iter)
{
  GtkTextIter end_bounds;
  gboolean found_para;

  g_return_val_if_fail (iter, FALSE);

  end_bounds = *iter;
  found_para = ide_text_iter_forward_paragraph_end (&end_bounds);

  if (!found_para)
    gtk_text_buffer_get_end_iter (gtk_text_iter_get_buffer (iter), &end_bounds);

  while ((gtk_text_iter_compare (iter, &end_bounds) < 0) && gtk_text_iter_forward_char (iter))
    {
      if (gtk_text_iter_forward_find_char (iter, sentence_end_chars, NULL, &end_bounds))
        {
          GtkTextIter copy = *iter;

          while (gtk_text_iter_forward_char (&copy) && (gtk_text_iter_compare (&copy, &end_bounds) < 0))
            {
              gunichar ch;
              gboolean invalid = FALSE;

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
                  invalid = TRUE;
                  break;
                }

              if (invalid)
                break;
            }
        }
    }

  *iter = end_bounds;

  if (found_para)
    return SENTENCE_PARA;

  return SENTENCE_FAILED;
}

gboolean
ide_text_iter_backward_sentence_start (GtkTextIter *iter)
{
  GtkTextIter tmp;
  SentenceStatus status;

  g_return_val_if_fail (iter, FALSE);

  tmp = *iter;
  status = ide_text_iter_backward_sentence_end (&tmp);

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

static gboolean
ide_text_iter_forward_classified_start (GtkTextIter  *iter,
                                         gint        (*classify) (gunichar))
{
  gint begin_class;
  gint cur_class;
  gunichar ch;

  g_assert (iter);

  ch = gtk_text_iter_get_char (iter);
  begin_class = classify (ch);

  /* Move to the first non-whitespace character if necessary. */
  if (begin_class == CLASS_SPACE)
    {
      for (;;)
        {
          if (!gtk_text_iter_forward_char (iter))
            return FALSE;

          ch = gtk_text_iter_get_char (iter);
          cur_class = classify (ch);
          if (cur_class != CLASS_SPACE)
            return TRUE;
        }
    }

  /* move to first character not at same class level. */
  while (gtk_text_iter_forward_char (iter))
    {
      ch = gtk_text_iter_get_char (iter);
      cur_class = classify (ch);

      if (cur_class == CLASS_SPACE)
        {
          begin_class = CLASS_0;
          continue;
        }

      if (cur_class != begin_class || cur_class == CLASS_NEWLINE)
        return TRUE;
    }

  return FALSE;
}

gboolean
ide_text_iter_forward_word_start (GtkTextIter *iter,
                                   gboolean     newline_stop)
{
  if (newline_stop)
    return ide_text_iter_forward_classified_start (iter, ide_text_word_classify_newline_stop);
  else
    return ide_text_iter_forward_classified_start (iter, ide_text_word_classify);
}

gboolean
ide_text_iter_forward_WORD_start (GtkTextIter *iter,
                                   gboolean     newline_stop)
{
  if (newline_stop)
    return ide_text_iter_forward_classified_start (iter, ide_text_WORD_classify_newline_stop);
  else
    return ide_text_iter_forward_classified_start (iter, ide_text_WORD_classify);
}

static gboolean
ide_text_iter_forward_classified_end (GtkTextIter  *iter,
                                       gint        (*classify) (gunichar))
{
  gunichar ch;
  gint begin_class;
  gint cur_class;

  g_assert (iter);

  if (!gtk_text_iter_forward_char (iter))
    return FALSE;

  /* If we are on space, walk to the start of the next word. */
  ch = gtk_text_iter_get_char (iter);
  if (classify (ch) == CLASS_SPACE)
    if (!ide_text_iter_forward_classified_start (iter, classify))
      return FALSE;

  ch = gtk_text_iter_get_char (iter);
  begin_class = classify (ch);

  if (begin_class == CLASS_NEWLINE)
    {
      gtk_text_iter_backward_char (iter);
      return TRUE;
    }

  for (;;)
    {
      if (!gtk_text_iter_forward_char (iter))
        return FALSE;

      ch = gtk_text_iter_get_char (iter);
      cur_class = classify (ch);

      if (cur_class != begin_class || cur_class == CLASS_NEWLINE)
        {
          gtk_text_iter_backward_char (iter);
          return TRUE;
        }
    }

  return FALSE;
}

gboolean
ide_text_iter_forward_word_end (GtkTextIter *iter,
                                 gboolean     newline_stop)
{
  if (newline_stop)
    return ide_text_iter_forward_classified_end (iter, ide_text_word_classify_newline_stop);
  else
    return ide_text_iter_forward_classified_end (iter, ide_text_word_classify);
}

gboolean
ide_text_iter_forward_WORD_end (GtkTextIter *iter,
                                 gboolean     newline_stop)
{
  if (newline_stop)
    return ide_text_iter_forward_classified_end (iter, ide_text_WORD_classify_newline_stop);
  else
    return ide_text_iter_forward_classified_end (iter, ide_text_WORD_classify);
}

static gboolean
ide_text_iter_backward_classified_end (GtkTextIter  *iter,
                                        gint        (*classify) (gunichar))
{
  gunichar ch;
  gint begin_class;
  gint cur_class;

  g_assert (iter);

  ch = gtk_text_iter_get_char (iter);
  begin_class = classify (ch);

  if (begin_class == CLASS_NEWLINE)
  {
    gtk_text_iter_forward_char (iter);
    return TRUE;
  }

  for (;;)
    {
      if (!gtk_text_iter_backward_char (iter))
        return FALSE;

      ch = gtk_text_iter_get_char (iter);
      cur_class = classify (ch);

      if (cur_class == CLASS_NEWLINE)
      {
        gtk_text_iter_forward_char (iter);
        return TRUE;
      }

      /* reset begin_class if we hit space, we can take anything after that */
      if (cur_class == CLASS_SPACE)
        begin_class = CLASS_SPACE;

      if (cur_class != begin_class && cur_class != CLASS_SPACE)
        return TRUE;
    }

  return FALSE;
}

gboolean
ide_text_iter_backward_word_end (GtkTextIter *iter,
                                  gboolean     newline_stop)
{
  if (newline_stop)
    return ide_text_iter_backward_classified_end (iter, ide_text_word_classify_newline_stop);
  else
    return ide_text_iter_backward_classified_end (iter, ide_text_word_classify);
}

gboolean
ide_text_iter_backward_WORD_end (GtkTextIter *iter,
                                  gboolean     newline_stop)
{
  if (newline_stop)
    return ide_text_iter_backward_classified_end (iter, ide_text_WORD_classify_newline_stop);
  else
    return ide_text_iter_backward_classified_end (iter, ide_text_WORD_classify);
}

static gboolean
ide_text_iter_backward_classified_start (GtkTextIter  *iter,
                                          gint        (*classify) (gunichar))
{
  gunichar ch;
  gint begin_class;
  gint cur_class;

  g_assert (iter);

  if (!gtk_text_iter_backward_char (iter))
    return FALSE;

  /* If we are on space, walk to the end of the previous word. */
  ch = gtk_text_iter_get_char (iter);
  if (classify (ch) == CLASS_SPACE)
    if (!ide_text_iter_backward_classified_end (iter, classify))
      return FALSE;

  ch = gtk_text_iter_get_char (iter);
  begin_class = classify (ch);
  if (begin_class == CLASS_NEWLINE)
  {
    gtk_text_iter_forward_char (iter);
    return TRUE;
  }

  for (;;)
    {
      if (!gtk_text_iter_backward_char (iter))
        return FALSE;

      ch = gtk_text_iter_get_char (iter);
      cur_class = classify (ch);

      if (cur_class != begin_class || cur_class == CLASS_NEWLINE)
        {
          gtk_text_iter_forward_char (iter);
          return TRUE;
        }
    }

  return FALSE;
}

gboolean
ide_text_iter_backward_word_start (GtkTextIter *iter,
                                    gboolean     newline_stop)
{
  if (newline_stop)
    return ide_text_iter_backward_classified_start (iter, ide_text_word_classify_newline_stop);
  else
    return ide_text_iter_backward_classified_start (iter, ide_text_word_classify);
}

gboolean
ide_text_iter_backward_WORD_start (GtkTextIter *iter,
                                    gboolean     newline_stop)
{
  if (newline_stop)
    return ide_text_iter_backward_classified_start (iter, ide_text_WORD_classify_newline_stop);
  else
    return ide_text_iter_backward_classified_start (iter, ide_text_WORD_classify);
}

static gboolean
matches_pred (GtkTextIter              *iter,
              IdeTextIterCharPredicate  pred,
              gpointer                  user_data)
{
  gint ch;

  ch = gtk_text_iter_get_char (iter);

  return (*pred) (iter, ch, user_data);
}

/**
 * ide_text_iter_forward_find_char:
 * @pred: (scope call): a callback to locate the char.
 *
 * Similar to gtk_text_iter_forward_find_char but
 * lets us acces to the iter in the predicate.
 *
 * Returns: %TRUE if found
 */
gboolean
ide_text_iter_forward_find_char (GtkTextIter              *iter,
                                  IdeTextIterCharPredicate  pred,
                                  gpointer                  user_data,
                                  const GtkTextIter        *limit)
{
  g_return_val_if_fail (iter != NULL, FALSE);
  g_return_val_if_fail (pred != NULL, FALSE);

  if (limit && gtk_text_iter_compare (iter, limit) >= 0)
    return FALSE;

  while ((limit == NULL ||
         !gtk_text_iter_equal (limit, iter)) &&
         gtk_text_iter_forward_char (iter))
    {
      if (matches_pred (iter, pred, user_data))
        return TRUE;
    }

  return FALSE;
}

/* Similar to gtk_text_iter_backward_find_char but
 * lets us acces to the iter in the predicate
 */
/**
 * ide_text_iter_backward_find_char:
 * @pred: (scope call):
 */
gboolean
ide_text_iter_backward_find_char (GtkTextIter              *iter,
                                   IdeTextIterCharPredicate  pred,
                                   gpointer                  user_data,
                                   const GtkTextIter        *limit)
{
  g_return_val_if_fail (iter != NULL, FALSE);
  g_return_val_if_fail (pred != NULL, FALSE);

  if (limit && gtk_text_iter_compare (iter, limit) <= 0)
    return FALSE;

  while ((limit == NULL ||
         !gtk_text_iter_equal (limit, iter)) &&
         gtk_text_iter_backward_char (iter))
    {
      if (matches_pred (iter, pred, user_data))
        return TRUE;
    }

  return FALSE;
}

/**
 * ide_text_iter_in_string:
 * @iter: a #GtkTextIter indicating the position to check for.
 * @str: A C type string.
 * @str_start: (out): a #GtkTextIter returning the str start iter (if found).
 * @str_end: (out): a #GtkTextIter returning the str end iter (if found).
 * @include_str_bounds: %TRUE if we take into account the str limits as possible @iter positions.
 *
 * Check if @iter position in the buffer is part of @str.
 *
 * Returns: %TRUE if case of succes, %FALSE otherwise.
 */
gboolean
ide_text_iter_in_string (GtkTextIter *iter,
                          const gchar *str,
                          GtkTextIter *str_start,
                          GtkTextIter *str_end,
                          gboolean     include_str_bounds)
{
  gint len;
  gint cursor_offset;
  gint slice_left_pos;
  gint slice_right_pos;
  gint slice_len;
  gint cursor_pos;
  gint str_pos;
  gint end_iter_offset;
  gint res_offset;
  guint count;
  g_autofree gchar *slice = NULL;
  const gchar *slice_ptr;
  const gchar *str_ptr;
  GtkTextIter slice_left = *iter;
  GtkTextIter slice_right = *iter;
  GtkTextIter end_iter;
  gboolean ret = FALSE;

  g_return_val_if_fail (!ide_str_empty0 (str), FALSE);

  len = g_utf8_strlen (str, -1);
  cursor_offset = gtk_text_iter_get_offset (iter);
  slice_left_pos = MAX(0, cursor_offset - len);
  gtk_text_iter_set_offset (&slice_left, slice_left_pos);

  cursor_pos = cursor_offset - slice_left_pos;

  gtk_text_buffer_get_end_iter (gtk_text_iter_get_buffer (iter), &end_iter);
  end_iter_offset = gtk_text_iter_get_offset (&end_iter);

  slice_right_pos = MIN(end_iter_offset, cursor_offset + len);
  gtk_text_iter_set_offset (&slice_right, slice_right_pos);

  slice = gtk_text_iter_get_slice (&slice_left, &slice_right);
  slice_len = slice_right_pos - slice_left_pos;

  slice_ptr = slice;
  for (count = 0; count < slice_len - len + 1; count++)
    {
      str_ptr = strstr (slice_ptr, str);
      if (str_ptr == NULL)
        {
          ret = FALSE;
          break;
        }

      str_pos = g_utf8_pointer_to_offset (slice, str_ptr);

      if ((!include_str_bounds && (str_pos < cursor_pos && cursor_pos < str_pos + len)) ||
          (include_str_bounds && (str_pos <= cursor_pos && cursor_pos <= str_pos + len)))
        {
          ret = TRUE;
          break;
        }

      slice_ptr = g_utf8_next_char (slice_ptr);
    }

  if (ret)
    {
      res_offset = slice_left_pos + str_pos + count;

      if (str_start != NULL)
        {
          *str_start = *iter;
          gtk_text_iter_set_offset (str_start, res_offset);
        }

      if (str_end != NULL)
        {
          *str_end = *iter;
          gtk_text_iter_set_offset (str_end, res_offset + len);
        }
    }

  return ret;
}

/**
 * ide_text_iter_find_chars_backward:
 * @iter: a #GtkTextIter indicating the start position to check for.
 * @limit: (nullable): a #GtkTextIter indicating the limit of the search.
 * @end: (out) (nullable): a #GtkTextIter returning the str end iter (if found).
 * @str: A C type string.
 * @only_at_start: %TRUE if the searched @str string should be constrained to start @iter position.
 *
 * Search backward for a @str string, starting at @iter position till @limit if there's one.
 * In case of succes, @iter is updated to @str start position.
 *
 * Notice that for @str to be found, @iter need to be at least on the @str last char
 *
 * Returns: %TRUE if case of succes, %FALSE otherwise.
 */
gboolean
ide_text_iter_find_chars_backward (GtkTextIter *iter,
                                    GtkTextIter *limit,
                                    GtkTextIter *end,
                                    const gchar *str,
                                    gboolean     only_at_start)
{
  const gchar *base_str;
  const gchar *str_limit;
  GtkTextIter base_cursor;

  g_return_val_if_fail (!ide_str_empty0 (str), FALSE);

  if (!gtk_text_iter_backward_char (iter))
    return FALSE;

  str_limit = str;
  base_str = str = str + strlen (str) - 1;
  base_cursor = *iter;
  do
    {
      *iter = base_cursor;
      do
        {
          if (gtk_text_iter_get_char (iter) != g_utf8_get_char (str))
            {
              if (only_at_start)
                return FALSE;
              else
                break;
            }

          str = g_utf8_find_prev_char (str_limit, str);
          if (str == NULL)
            {
              if (end)
                {
                  *end = base_cursor;
                  gtk_text_iter_forward_char (end);
                }

              return TRUE;
            }

        } while ((gtk_text_iter_backward_char (iter)));

      if (gtk_text_iter_is_start (iter))
        return FALSE;
      else
        str = base_str;

    } while (gtk_text_iter_backward_char (&base_cursor));

  return FALSE;
}

/**
 * ide_text_iter_find_chars_forward:
 * @iter: a #GtkTextIter indicating the start position to check for.
 * @limit: (nullable): a #GtkTextIter indicating the limit of the search.
 * @end: (out) (nullable): a #GtkTextIter returning the str end iter (if found).
 * @str: A C type string.
 * @only_at_start: %TRUE if the searched @str string should be constrained to start @iter position.
 *
 * Search forward for a @str string, starting at @iter position till @limit if there's one.
 * In case of succes, @iter is updated to the found @str start position,
 * otherwise, its position is undefined.
 *
 * Returns: %TRUE if case of succes, %FALSE otherwise.
 */
gboolean
ide_text_iter_find_chars_forward (GtkTextIter *iter,
                                   GtkTextIter *limit,
                                   GtkTextIter *end,
                                   const gchar *str,
                                   gboolean     only_at_start)
{
  const gchar *base_str;
  const gchar *str_limit;
  GtkTextIter base_cursor;
  GtkTextIter real_limit;
  gint str_char_len;
  gint real_limit_offset;

  g_return_val_if_fail (!ide_str_empty0 (str), FALSE);

  if (limit == NULL)
    {
      real_limit = *iter;
      gtk_text_iter_forward_to_end (&real_limit);
    }
  else
    real_limit = *limit;

  str_char_len = g_utf8_strlen (str, -1);
  real_limit_offset = gtk_text_iter_get_offset (&real_limit) - str_char_len;
  if (real_limit_offset < 0)
    return FALSE;

  gtk_text_iter_set_offset (&real_limit, real_limit_offset);
  if (gtk_text_iter_compare(iter, &real_limit) > 0)
    return FALSE;

  str_limit = str + strlen (str);
  base_str = str;
  base_cursor = *iter;
  do
    {
      *iter = base_cursor;
      do
        {
          if (gtk_text_iter_get_char (iter) != g_utf8_get_char (str))
            {
              if (only_at_start)
                return FALSE;
              else
                break;
            }

          str = g_utf8_find_next_char (str, str_limit);
          if (str == NULL)
            {
              if (end)
                {
                  *end = *iter;
                  gtk_text_iter_forward_char (end);
                }

              *iter = base_cursor;
              return TRUE;
            }

        } while ((gtk_text_iter_forward_char (iter)));

    } while (gtk_text_iter_compare(&base_cursor, &real_limit) < 0 &&
             (str = base_str) &&
             gtk_text_iter_forward_char (&base_cursor));

  return FALSE;
}

static inline gboolean
is_symbol_char (gunichar ch)
{
  return g_unichar_isalnum (ch) || (ch == '_');
}

gchar *
ide_text_iter_current_symbol (const GtkTextIter *iter,
                               GtkTextIter       *out_begin)
{
  GtkTextBuffer *buffer;
  GtkTextIter end = *iter;
  GtkTextIter begin = *iter;
  gunichar ch = 0;

  do
    {
      if (!gtk_text_iter_backward_char (&begin))
        break;
      ch = gtk_text_iter_get_char (&begin);
    }
  while (is_symbol_char (ch));

  if (ch && !is_symbol_char (ch))
    gtk_text_iter_forward_char (&begin);

  buffer = gtk_text_iter_get_buffer (iter);

  if (GTK_SOURCE_IS_BUFFER (buffer))
    {
      GtkSourceBuffer *gsb = GTK_SOURCE_BUFFER (buffer);

      if (gtk_source_buffer_iter_has_context_class (gsb, &begin, "comment") ||
          gtk_source_buffer_iter_has_context_class (gsb, &begin, "string") ||
          gtk_source_buffer_iter_has_context_class (gsb, &end, "comment") ||
          gtk_source_buffer_iter_has_context_class (gsb, &end, "string"))
        return NULL;
    }

  if (gtk_text_iter_equal (&begin, &end))
    return NULL;

  if (out_begin != NULL)
    *out_begin = begin;

  return gtk_text_iter_get_slice (&begin, &end);
}
