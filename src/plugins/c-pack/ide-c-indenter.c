/* ide-c-indenter.c
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

#define G_LOG_DOMAIN "cindent"

#include <glib/gi18n.h>
#include <libpeas.h>
#include <libide-sourceview.h>

#include "c-parse-helper.h"
#include "ide-c-indenter.h"

#define ITER_INIT_LINE_START(iter, other) \
  gtk_text_buffer_get_iter_at_line( \
    gtk_text_iter_get_buffer(other), \
    (iter), \
    gtk_text_iter_get_line(other))

typedef enum
{
  IDE_C_INDENT_ACTION_ALIGN_PARAMETERS = 1,
  IDE_C_INDENT_ACTION_CLOSE_COMMENT,
  IDE_C_INDENT_ACTION_INDENT_LINE,
  IDE_C_INDENT_ACTION_UNINDENT_CASE_OR_LABEL,
  IDE_C_INDENT_ACTION_UNINDENT_CLOSING_BRACE,
  IDE_C_INDENT_ACTION_UNINDENT_HASH,
  IDE_C_INDENT_ACTION_UNINDENT_OPENING_BRACE,
} IdeCIndentAction;

struct _IdeCIndenter
{
  IdeObject      parent_instance;

  /* no reference */
  IdeSourceView *view;

  gint           pre_scope_indent;
  gint           post_scope_indent;
  gint           condition_indent;
  gint           directive_indent;
  gint           extra_label_indent;
  gint           case_indent;

  IdeCIndentAction indent_action;

  guint          tab_width;
  guint          indent_width;
  guint          use_tabs : 1;
};

static void indenter_iface_init (GtkSourceIndenterInterface *iface);

G_DEFINE_FINAL_TYPE_WITH_CODE (IdeCIndenter, ide_c_indenter, IDE_TYPE_OBJECT,
                         G_IMPLEMENT_INTERFACE (GTK_SOURCE_TYPE_INDENTER, indenter_iface_init))

enum {
  COMMENT_NONE,
  COMMENT_C89,
  COMMENT_C99
};

static inline gboolean
text_iter_isspace (const GtkTextIter *iter)
{
  gunichar ch = gtk_text_iter_get_char (iter);
  return g_unichar_isspace (ch);
}

static inline guint
get_post_scope_indent (IdeCIndenter *c)
{
  if (c->post_scope_indent == -1)
    return c->indent_width;
  return c->post_scope_indent;
}

static inline guint
get_pre_scope_indent (IdeCIndenter *c)
{
  if (c->pre_scope_indent == -1)
    return c->indent_width;
  return c->pre_scope_indent;
}

static inline guint
get_condition_indent (IdeCIndenter *c)
{
  if (c->condition_indent == -1)
    return c->indent_width;
  return c->condition_indent;
}

static inline void
build_indent (IdeCIndenter *c,
              guint         line_offset,
              GtkTextIter  *matching_line,
              GString      *str)
{
  GtkSourceView *view = (GtkSourceView *)c->view;
  guint tab_width = gtk_source_view_get_tab_width (view);
  GtkTextIter iter;
  gunichar ch;

  g_assert (tab_width > 0);
  g_assert (str->len == 0);

  if (!line_offset)
    return;

  gtk_text_buffer_get_iter_at_line (gtk_text_iter_get_buffer (matching_line),
                                    &iter,
                                    gtk_text_iter_get_line (matching_line));

  do {
    ch = gtk_text_iter_get_char (&iter);

    switch (ch)
      {
      case '\t':
        for (guint i = 0; i < tab_width; i++)
          g_string_append_c (str, ' ');
        break;

      case ' ':
      default:
        g_string_append_c (str, ' ');
        break;
      }
  } while (gtk_text_iter_forward_char (&iter) &&
           (gtk_text_iter_compare (&iter, matching_line) <= 0) &&
           (str->len < line_offset));

  while (str->len < line_offset)
    g_string_append_c (str, ' ');

  if (c->use_tabs)
    {
      guint n_tabs;
      guint n_spaces;

      n_tabs = str->len / tab_width;
      n_spaces = str->len % tab_width;

      g_string_truncate (str, 0);

      for (guint i = 0; i < n_tabs; i++)
        g_string_append_c (str, '\t');

      for (guint i = 0; i < n_spaces; i++)
        g_string_append_c (str, ' ');
    }
}

static gboolean
iter_ends_c89_comment (const GtkTextIter *iter)
{
  if (gtk_text_iter_get_char (iter) == '/')
    {
      GtkTextIter tmp;

      tmp = *iter;

      if (gtk_text_iter_backward_char (&tmp) && ('*' == gtk_text_iter_get_char (&tmp)))
        return TRUE;
    }

  return FALSE;
}

static gboolean
non_space_predicate (gunichar ch,
                     gpointer user_data)
{
  return !g_unichar_isspace (ch);
}

static gboolean
line_is_whitespace_until (GtkTextIter *iter)
{
  GtkTextIter cur;

  gtk_text_buffer_get_iter_at_line (gtk_text_iter_get_buffer (iter),
                                    &cur,
                                    gtk_text_iter_get_line (iter));

  for (;
       gtk_text_iter_compare (&cur, iter) < 0;
       gtk_text_iter_forward_char (&cur))
    {
      if (!g_unichar_isspace (gtk_text_iter_get_char (&cur)))
        return FALSE;
    }

  return TRUE;
}

static gboolean is_special (const GtkTextIter *iter)
{
  GtkSourceBuffer *buffer;

  buffer = GTK_SOURCE_BUFFER (gtk_text_iter_get_buffer (iter));
  return (gtk_source_buffer_iter_has_context_class (buffer, iter, "string") ||
          gtk_source_buffer_iter_has_context_class (buffer, iter, "comment"));
}

static gboolean
backward_find_keyword (GtkTextIter       *iter,
                       const gchar       *keyword,
                       const GtkTextIter *limit)
{
  GtkTextIter copy = *iter;

  while (gtk_text_iter_compare (&copy, limit) > 0)
    {
      GtkTextIter begin;
      GtkTextIter end;

      if (!gtk_text_iter_backward_search (&copy, keyword, GTK_TEXT_SEARCH_TEXT_ONLY, &begin, &end, limit))
        return FALSE;

      if (!is_special (&begin) &&
          gtk_text_iter_starts_word (&begin) &&
          gtk_text_iter_ends_word (&end))
        {
          GtkTextIter prev = begin;
          gunichar ch;

          if (gtk_text_iter_backward_char (&prev) &&
              (ch = gtk_text_iter_get_char (&prev)) &&
              !g_unichar_isspace (ch))
            {
              goto again;
            }

          *iter = begin;
          return TRUE;
        }

    again:
      copy = begin;
    }

  return FALSE;
}

static gboolean
backward_find_condition_keyword (GtkTextIter *iter)
{
  GtkTextIter line_start;

  ITER_INIT_LINE_START (&line_start, iter);

  if (backward_find_keyword (iter, "else if", &line_start) ||
      backward_find_keyword (iter, "else", &line_start) ||
      backward_find_keyword (iter, "if", &line_start) ||
      backward_find_keyword (iter, "do", &line_start) ||
      backward_find_keyword (iter, "while", &line_start) ||
      backward_find_keyword (iter, "switch", &line_start) ||
      backward_find_keyword (iter, "for", &line_start))
    return TRUE;

  return FALSE;
}

static gchar *
backward_last_word (GtkTextIter *iter,
                    GtkTextIter *begin)
{
  gtk_text_iter_assign (begin, iter);

  if (gtk_text_iter_backward_word_start (begin))
    {
      GtkTextIter end;

      gtk_text_iter_assign (&end, begin);

      if (gtk_text_iter_ends_word (&end) ||
          gtk_text_iter_forward_word_end (&end))
        return gtk_text_iter_get_slice (begin, &end);
    }

  return NULL;
}

static gboolean
backward_before_c89_comment (GtkTextIter *iter)
{
  GtkTextIter copy;
  GtkTextIter match_start;
  GtkTextIter match_end;
  gunichar ch;

  gtk_text_iter_assign (&copy, iter);

  while (g_unichar_isspace (gtk_text_iter_get_char (iter)))
    if (!gtk_text_iter_backward_char (iter))
      IDE_GOTO (cleanup);

  if (!(ch = gtk_text_iter_get_char (iter)) ||
      (ch != '/') ||
      !gtk_text_iter_backward_char (iter) ||
      !(ch = gtk_text_iter_get_char (iter)) ||
      (ch != '*') ||
      !gtk_text_iter_backward_search (iter, "/*",
                                      GTK_TEXT_SEARCH_TEXT_ONLY,
                                      &match_start, &match_end, NULL) ||
      !gtk_text_iter_backward_find_char (&match_start, non_space_predicate,
                                         NULL, NULL))
    IDE_GOTO (cleanup);

  gtk_text_iter_assign (iter, &match_start);

  return TRUE;

cleanup:
  gtk_text_iter_assign (iter, &copy);

  return FALSE;
}

static gboolean
backward_find_matching_char (GtkTextIter *iter,
                             gunichar     ch)
{
  GtkTextIter copy;
  gunichar match = 0;
  gunichar cur;
  guint count = 1;

  switch (ch) {
  case ')':
    match = '(';
    break;
  case '}':
    match = '{';
    break;
  case '[':
    match = ']';
    break;
  default:
    g_assert_not_reached ();
    break;
  }

  gtk_text_iter_assign (&copy, iter);

  while (gtk_text_iter_backward_char (iter))
    {
      cur = gtk_text_iter_get_char (iter);

      if ((cur == '\'') || (cur == '"'))
        {
          gunichar strcur = 0;

          while (gtk_text_iter_backward_char (iter))
            {
              strcur = gtk_text_iter_get_char (iter);
              if (strcur == cur)
                {
                  GtkTextIter copy2 = *iter;

                  /* check if the character before this is an escape char */
                  if (gtk_text_iter_backward_char (&copy2) &&
                      ('\\' == gtk_text_iter_get_char (&copy2)))
                    continue;

                  break;
                }
            }

          if (strcur != cur)
            break;
        }
      else if ((cur == '/') && iter_ends_c89_comment (iter))
        {
          GtkTextIter tmp = *iter;

          if (backward_before_c89_comment (&tmp))
            {
              *iter = tmp;
              cur = gtk_text_iter_get_char (iter);
            }
        }

      if (cur == match)
        {
          if (--count == 0)
            return TRUE;
        }
      else if (cur == ch)
        count++;
    }

  gtk_text_iter_assign (iter, &copy);

  return FALSE;
}

static gboolean
line_is_space (GtkTextIter *iter)
{
  GtkTextIter begin;

  gtk_text_buffer_get_iter_at_line (gtk_text_iter_get_buffer (iter),
                                    &begin,
                                    gtk_text_iter_get_line (iter));

  for (;
       gtk_text_iter_compare (&begin, iter) < 0;
       gtk_text_iter_forward_char (&begin))
    {
      if (!g_unichar_isspace (gtk_text_iter_get_char (&begin)))
        return FALSE;
    }

  return TRUE;
}

static gboolean
starts_line_space_ok (GtkTextIter *iter)
{
  GtkTextIter tmp;

  gtk_text_buffer_get_iter_at_line (gtk_text_iter_get_buffer (iter),
                                    &tmp,
                                    gtk_text_iter_get_line (iter));

  for (;
       gtk_text_iter_compare (&tmp, iter) < 0;
       gtk_text_iter_forward_char (&tmp))
    {
      if (!g_unichar_isspace (gtk_text_iter_get_char (&tmp)))
        return FALSE;
    }

  return TRUE;
}

static gboolean
backward_find_stmt_expr (GtkTextIter *iter)
{
  return FALSE;
}

static gboolean
backward_to_line_first_char (GtkTextIter *iter)
{
  GtkTextIter tmp;

  gtk_text_buffer_get_iter_at_line (gtk_text_iter_get_buffer (iter),
                                    &tmp,
                                    gtk_text_iter_get_line (iter));

  while (gtk_text_iter_compare (&tmp, iter) <= 0)
    {
      gunichar ch = gtk_text_iter_get_char (&tmp);

      if (!g_unichar_isspace (ch))
        {
          gtk_text_iter_assign (iter, &tmp);
          return TRUE;
        }

      if (!gtk_text_iter_forward_char (&tmp))
        break;
    }

  return FALSE;
}

static gboolean
in_comment (const GtkTextIter *location,
            GtkTextIter       *match_begin,
            gint              *comment_type)
{
  GtkSourceBuffer *buffer = GTK_SOURCE_BUFFER (gtk_text_iter_get_buffer (location));
  GtkTextIter iter = *location;
  GtkTextIter copy;
  gint type = COMMENT_NONE;

  IDE_ENTRY;

  if (comment_type)
    *comment_type = COMMENT_NONE;

  /*
   * A rather esoteric set of heuristics to be able to determine if we are
   * actually in a GtkSourceView comment context.
   */
  if (gtk_text_iter_ends_line (&iter))
    {
      if (gtk_text_iter_is_end (&iter))
        gtk_text_iter_backward_char (&iter);
      gtk_text_iter_backward_char (&iter);
    }

  /*
   * But make sure that our new position isn't the end of a C89 block.
   */
  if (gtk_text_iter_get_char (&iter) == '/')
    {
      GtkTextIter star = iter;

      if (gtk_text_iter_backward_char (&star) && (gtk_text_iter_get_char (&star) == '*'))
        IDE_RETURN (FALSE);
    }

  if (!gtk_source_buffer_iter_has_context_class (buffer, &iter, "comment"))
    IDE_RETURN (FALSE);

  copy = iter;

  while (gtk_source_buffer_iter_has_context_class (buffer, &iter, "comment"))
    {
      copy = iter;

      if (!gtk_text_iter_backward_char (&iter))
        break;
    }

  *match_begin = copy;

  if ((gtk_text_iter_get_char (&copy) == '/') &&
      !gtk_text_iter_is_end (&copy) &&
      gtk_text_iter_forward_char (&copy))
    {
      if (gtk_text_iter_get_char (&copy) == '/')
        type = COMMENT_C99;
      else if (gtk_text_iter_get_char (&copy) == '*')
        type = COMMENT_C89;
    }

  if ((type == COMMENT_C99) && (gtk_text_iter_get_line (&copy) != gtk_text_iter_get_line (location)))
    IDE_RETURN (FALSE);

  if (comment_type)
    *comment_type = type;

  IDE_RETURN (TRUE);
}

#define GET_LINE_OFFSET(_iter) \
  gtk_source_view_get_visual_column(GTK_SOURCE_VIEW(view), _iter)

static void
c_indenter_indent_line (IdeCIndenter  *c,
                        GtkSourceView *view,
                        GtkTextBuffer *buffer,
                        GtkTextIter   *iter)
{
  g_autoptr(GString) str = NULL;
  GtkTextIter original_iter;
  GtkTextIter cur;
  GtkTextIter match_begin;
  GtkTextIter copy;
  gunichar ch;
  gchar *last_word = NULL;
  gint cursor_offset = 0;
  gint comment_type;

  IDE_ENTRY;

  g_return_if_fail (IDE_IS_C_INDENTER (c));

  /*
   * Save our current iter position to restore it later.
   */
  original_iter = cur = *iter;

  /*
   * Move to before the character just inserted.
   */
  gtk_text_iter_backward_char (iter);

  /*
   * Create the buffer for our indentation string.
   */
  str = g_string_new (NULL);

  /*
   * If we are in a c89 multi-line comment, try to match the previous comment
   * line. Function will leave iter at original position unless it matched.
   * If so, it will be at the beginning of the comment.
   */
  if (in_comment (iter, &match_begin, &comment_type))
    {
      guint offset;

      gtk_text_iter_assign (iter, &match_begin);
      offset = GET_LINE_OFFSET (iter);
      if (comment_type == COMMENT_C89)
        {
          build_indent (c, offset + 1, iter, str);
          g_string_append (str, "* ");
        }
      else if (comment_type == COMMENT_C99)
        {
          build_indent (c, offset, iter, str);
          g_string_append (str, "// ");
        }
      IDE_GOTO (cleanup);
    }

  /*
   * If the next thing looking backwards is a complete c89 comment, let's
   * move the iter to before the comment so that we can work with the syntax
   * that is before it.
   */
  if (backward_before_c89_comment (iter))
    gtk_text_iter_assign (&cur, iter);

  /*
   * Move backwards to the last non-space character inserted. This helps us
   * more accurately locate the type of syntax block we are in.
   */
  if (g_unichar_isspace (gtk_text_iter_get_char (iter)))
    if (!gtk_text_iter_backward_find_char (iter, non_space_predicate, NULL, NULL))
      IDE_GOTO (cleanup);

  /*
   * Get our new character as we possibely moved.
   */
  ch = gtk_text_iter_get_char (iter);

  /*
   * We could be:
   *   - In a parameter list for a function declaration.
   *   - In an argument list for a function call.
   *   - Defining enum fields.
   *   - XXX: bunch more.
   */
  if (ch == ',')
    {
      guint offset;

      if (!backward_find_matching_char (iter, ')') &&
          !backward_find_matching_char (iter, '}'))
        IDE_GOTO (cleanup);

      offset = GET_LINE_OFFSET (iter);

      if (gtk_text_iter_get_char (iter) == '(')
        offset++;
      else if (gtk_text_iter_get_char (iter) == '{')
        {
          /*
           * Handle the case where { is not the first character,
           * like "enum {".
           */
          if (backward_to_line_first_char (iter))
            offset = GET_LINE_OFFSET (iter);
          offset += get_post_scope_indent (c);
        }

      build_indent (c, offset, iter, str);
      IDE_GOTO (cleanup);
    }

  /*
   * Looks like the last line was a statement or expression. Let's try to
   * find the beginning of it.
   */
  if (ch == ';')
    {
      guint offset;

      if (backward_find_stmt_expr (iter))
        {
          offset = GET_LINE_OFFSET (iter);
          build_indent (c, offset, iter, str);
          IDE_GOTO (cleanup);
        }
    }

  /*
   * Maybe we are in a conditional if there's an unmatched (.
   */
  copy = *iter;
  if (backward_find_matching_char (&copy, ')') &&
     ((ch != ')') || ((ch == ')') && backward_find_matching_char (&copy, ')'))))
    {
      guint offset;

      offset = GET_LINE_OFFSET (&copy);
      build_indent (c, offset + 1, &copy, str);
      IDE_GOTO (cleanup);
    }

  /*
   * If we just ended a scope, we need to look for the matching scope
   * before it.
   */
  if (ch == '}')
    {
      gtk_text_iter_assign (&copy, iter);

      if (gtk_text_iter_forward_char (iter))
        {
          guint offset = GET_LINE_OFFSET (iter) - 1;

          if (backward_find_matching_char (iter, '}'))
            {
              offset = GET_LINE_OFFSET (iter);
              offset += get_post_scope_indent (c);
            }

          build_indent (c, offset, iter, str);
          IDE_GOTO (cleanup);
        }

      gtk_text_iter_assign (iter, &copy);
    }

  /*
   * Check to see if we just finished a conditional.
   */
  if (ch == ')')
    {
      gtk_text_iter_assign (&copy, iter);

      if (backward_find_matching_char (iter, ')') &&
          backward_find_condition_keyword (iter))
        {
          guint offset = GET_LINE_OFFSET (iter);
          build_indent (c, offset + get_condition_indent (c), iter, str);
          IDE_GOTO (cleanup);
        }

      gtk_text_iter_assign (iter, &copy);
    }

  /*
   * Check to see if we are after else or do. Skip if we see '{'
   * so that we can fallback to regular scoping rules.
   */
  last_word = backward_last_word (iter, &match_begin);
  if ((ch != '{') &&
      ((g_strcmp0 (last_word, "else") == 0) ||
       (g_strcmp0 (last_word, "do") == 0)))
    {
      guint offset;

      if (!line_is_whitespace_until (&match_begin))
        backward_to_line_first_char (&match_begin);

      offset = GET_LINE_OFFSET (&match_begin);
      build_indent (c, offset + get_pre_scope_indent (c), iter, str);
      IDE_GOTO (cleanup);
    }

  /*
   * Work our way back to the most recent scope. Then apply our scope
   * indentation to that.
   */
  if (ch == '{' || backward_find_matching_char (iter, '}'))
    {
      if (line_is_space (iter))
        {
          guint offset;

          offset = GET_LINE_OFFSET (iter);
          build_indent (c, offset + get_post_scope_indent (c), iter, str);
          IDE_GOTO (cleanup);
        }
      else
        {
          if (backward_to_line_first_char (iter))
            {
              guint offset;

              offset = GET_LINE_OFFSET (iter);
              build_indent (c, offset + get_post_scope_indent (c), iter, str);
              IDE_GOTO (cleanup);
            }
        }
    }

cleanup:

  if (str->len > 0)
    {
      /*
       * If we have additional space after where our new indentation
       * will occur, we should chomp it up so that the text starts
       * immediately after our new indentation.
       *
       * GNOME/gnome-builder#545
       */
      while (text_iter_isspace (iter) && !gtk_text_iter_ends_line (iter))
        gtk_text_iter_forward_char (iter);
    }

  /*
   * If we are inserting a newline right before a closing brace (for example
   * after {<cursor>}, we need to indent and then maybe unindent the }.
   */
  if (gtk_text_iter_get_char (&original_iter) == '}')
    {
      GtkTextIter iter2;
      guint offset = 0;

      gtk_text_iter_assign (&iter2, &original_iter);
      if (backward_find_matching_char (&iter2, '}'))
        {
          g_autoptr(GString) str2 = g_string_new (NULL);

          if (line_is_whitespace_until (&iter2))
            offset = GET_LINE_OFFSET (&iter2);
          else if (backward_to_line_first_char (&iter2))
            offset = GET_LINE_OFFSET (&iter2);
          build_indent (c, offset, &iter2, str2);
          g_string_prepend (str2, "\n");
          g_string_prepend (str2, str->str);

          cursor_offset = -(str2->len - str->len);

          g_string_free (str, TRUE);
          str = g_steal_pointer (&str2);
        }
    }

  if (str->len > 0)
    {
      *iter = original_iter;
      gtk_text_buffer_insert (buffer, iter, str->str, str->len);
      gtk_text_iter_forward_chars (iter, cursor_offset);
      gtk_text_buffer_place_cursor (buffer, iter);
    }

  IDE_EXIT;
}

static void
maybe_close_comment (IdeCIndenter  *c,
                     GtkTextBuffer *buffer,
                     GtkTextIter   *iter)
{
  GtkTextIter copy;
  GtkTextIter begin_comment;
  gint comment_type;

  g_assert (IDE_IS_C_INDENTER (c));
  g_assert (GTK_IS_TEXT_BUFFER (buffer));
  g_assert (iter);

  copy = *iter;

  if (!gtk_text_iter_backward_char (&copy))
    return;

  /*
   * Walk backwards ensuring we just inserted a '/' and that it was after
   * a '* ' sequence.
   */
  if (in_comment (&copy, &begin_comment, &comment_type) &&
      (comment_type == COMMENT_C89) &&
      gtk_text_iter_backward_char (&copy) &&
      ('/' == gtk_text_iter_get_char (&copy)) &&
      gtk_text_iter_backward_char (&copy) &&
      (' ' == gtk_text_iter_get_char (&copy)) &&
      gtk_text_iter_backward_char (&copy) &&
      ('*' == gtk_text_iter_get_char (&copy)))
    {
      gtk_text_buffer_delete (buffer, &copy, iter);
      gtk_text_buffer_insert (buffer, iter, "*/", 2);
      gtk_text_buffer_place_cursor (buffer, iter);
    }
}

static void
maybe_unindent_opening_brace (IdeCIndenter    *c,
                              GtkTextView     *view,
                              GtkTextBuffer   *buffer,
                              IdeFileSettings *file_settings,
                              GtkTextIter     *iter)
{
  GtkTextIter copy;

  g_assert (IDE_IS_C_INDENTER (c));
  g_assert (GTK_IS_TEXT_VIEW (view));
  g_assert (GTK_IS_TEXT_BUFFER (buffer));
  g_assert (IDE_IS_FILE_SETTINGS (file_settings));
  g_assert (iter);

  copy = *iter;

  /*
   * Make sure we just inserted a { and then move before it.
   * Ensure that we only have whitespace before the {.
   */
  if (!gtk_text_iter_backward_char (&copy) ||
      ('{' != gtk_text_iter_get_char (&copy)) ||
      !gtk_text_iter_backward_char (&copy))
    return;

  /*
   * Find the opening of the parent scope.
   * We should be at that + post_scope_indent, which is where
   * our conditional would have started.
   */
  if (line_is_whitespace_until (&copy) && backward_find_matching_char (&copy, '}'))
    {
      g_autoptr(GString) str = NULL;
      GtkTextIter line_start;
      guint offset;

      backward_to_line_first_char (&copy);

      offset = GET_LINE_OFFSET (&copy);
      str = g_string_new (NULL);
      build_indent (c, offset + get_post_scope_indent(c) + get_pre_scope_indent (c), &copy, str);
      g_string_append_c (str, '{');

      line_start = *iter;
      gtk_text_iter_set_line_offset (&line_start, 0);
      gtk_text_buffer_delete (buffer, &line_start, iter);
      gtk_text_buffer_insert (buffer, iter, str->str, str->len);

      gtk_text_buffer_place_cursor (buffer, iter);
    }
}

static void
maybe_unindent_closing_brace (IdeCIndenter  *c,
                              GtkTextView   *view,
                              GtkTextBuffer *buffer,
                              GtkTextIter   *iter)
{
  GtkTextIter start;
  GtkTextIter end;

  IDE_ENTRY;

  g_assert (IDE_IS_C_INDENTER (c));
  g_assert (GTK_IS_TEXT_VIEW (view));
  g_assert (GTK_IS_TEXT_BUFFER (buffer));
  g_assert (iter);

  start = end = *iter;

  if (gtk_text_iter_backward_char (&start) &&
      gtk_text_iter_backward_char (&end) &&
      backward_find_matching_char (&start, '}') &&
      line_is_whitespace_until (&end) &&
      (gtk_text_iter_get_offset (&start) + 1) != gtk_text_iter_get_offset (&end))
    {
      g_autoptr(GString) str = NULL;
      guint offset;

      /*
       * Handle the case where { is not the first non-whitespace
       * character on the line.
       */
      if (!starts_line_space_ok (&start))
        backward_to_line_first_char (&start);

      offset = GET_LINE_OFFSET (&start);
      str = g_string_new (NULL);
      build_indent (c, offset, &start, str);
      g_string_append_c (str, '}');

      start = *iter;
      while (!gtk_text_iter_starts_line (&start))
        gtk_text_iter_backward_char (&start);

      end = *iter;

      gtk_text_buffer_delete (buffer, &start, &end);
      gtk_text_buffer_insert (buffer, &start, str->str, str->len);

      *iter = start;
    }

  IDE_EXIT;
}

static void
maybe_unindent_hash (IdeCIndenter  *c,
                     GtkTextBuffer *buffer,
                     GtkTextIter   *iter)
{
  GtkTextIter start;

  g_assert (IDE_IS_C_INDENTER (c));
  g_assert (GTK_IS_TEXT_BUFFER (buffer));
  g_assert (iter);

  start = *iter;

  if (gtk_text_iter_backward_char (&start) &&
      ('#' == gtk_text_iter_get_char (&start)) &&
      line_is_whitespace_until (&start))
    {
      if (c->directive_indent == G_MININT)
        {
          while (!gtk_text_iter_starts_line (&start))
            gtk_text_iter_backward_char (&start);

          /* Delete whitespace before the hash character */
          gtk_text_iter_backward_char (iter);
          gtk_text_buffer_delete (buffer, &start, iter);

          /* Return cursor to after the hash character */
          gtk_text_iter_forward_char (iter);
          gtk_text_buffer_place_cursor (buffer, iter);
        }
      else
        {
          /* TODO: Handle indent when not fully unindenting. */
        }
    }
}

static gchar *
format_parameter (const Parameter *param,
                  guint            max_type,
                  guint            max_star)
{
  GString *str;
  guint i;

  if (param->ellipsis)
    return g_strdup ("...");

  str = g_string_new (param->type);

  for (i = str->len; i < max_type; i++)
    g_string_append_c (str, ' ');

  g_string_append_c (str, ' ');

  for (i = max_star; i > 0; i--)
    {
      if (i <= param->n_star)
        g_string_append_c (str, '*');
      else
        g_string_append_c (str, ' ');
    }

  g_string_append (str, param->name);

  return g_string_free (str, FALSE);
}

static void
format_parameters (GSList        *params,
                   GtkTextBuffer *buffer,
                   GtkTextIter   *begin,
                   GtkTextIter   *end)
{
  g_autoptr(GString) str = NULL;
  GtkTextIter line_start;
  GtkTextIter first_char;
  GSList *iter;
  gchar *slice;
  gchar *join_str;
  guint max_star = 0;
  guint max_type = 0;

  for (iter = params; iter; iter = iter->next)
    {
      Parameter *p = iter->data;

      if (p->n_star)
        max_star = MAX (max_star, p->n_star);
      if (p->type)
        max_type = MAX (max_type, strlen (p->type));
    }

  ITER_INIT_LINE_START (&line_start, begin);

  gtk_text_iter_assign (&first_char, begin);
  backward_to_line_first_char (&first_char);

  slice = gtk_text_iter_get_slice (&line_start, &first_char);
  str = g_string_new (",\n");
  g_string_append (str, slice);
  g_free (slice);

  while (gtk_text_iter_compare (&first_char, begin) < 0)
    {
      g_string_append (str, " ");
      if (!gtk_text_iter_forward_char (&first_char))
        break;
    }

  join_str = g_string_free (str, FALSE);
  str = g_string_new (NULL);

  for (iter = params; iter; iter = iter->next)
    {
      g_autofree gchar *param_str = NULL;

      if (iter != params)
        g_string_append (str, join_str);

      param_str = format_parameter (iter->data, max_type, max_star);
      g_string_append (str, param_str);
    }

  g_free (join_str);

  gtk_text_buffer_delete (buffer, begin, end);
  gtk_text_buffer_insert (buffer, begin, str->str, str->len);

  *end = *begin;
  gtk_text_iter_backward_chars (end, g_utf8_strlen (str->str, str->len));
}

static void
maybe_align_parameters (IdeCIndenter  *c,
                        GtkTextBuffer *buffer,
                        GtkTextIter   *iter)
{
  g_autofree gchar *text = NULL;
  GtkTextIter match_begin;
  GtkTextIter start, end;
  GSList *params = NULL;

  IDE_ENTRY;

  g_assert (IDE_IS_C_INDENTER (c));
  g_assert (GTK_IS_TEXT_BUFFER (buffer));
  g_assert (iter);

  if (in_comment (iter, &match_begin, NULL))
    IDE_RETURN ();

  start = end = *iter;

  if (!gtk_text_iter_backward_char (&start))
    IDE_RETURN ();

  if (gtk_text_iter_backward_char (&start) &&
      backward_find_matching_char (&start, ')') &&
      gtk_text_iter_forward_char (&start) &&
      gtk_text_iter_backward_char (&end) &&
      (gtk_text_iter_compare (&start, &end) < 0) &&
      (text = gtk_text_iter_get_slice (&start, &end)) &&
      (params = parse_parameters (text)) &&
      (params->next != NULL))
    {
      format_parameters (params, buffer, &start, &end);
      *iter = end;
    }

  g_slist_foreach (params, (GFunc)parameter_free, NULL);
  g_slist_free (params);

  IDE_EXIT;
}

static gboolean
line_starts_with_fuzzy (const GtkTextIter *iter,
                        const gchar       *prefix)
{
  GtkTextIter begin;
  GtkTextIter end;
  gboolean ret;
  gchar *line;

  ITER_INIT_LINE_START (&begin, iter);
  ITER_INIT_LINE_START (&end, iter);

  while (!gtk_text_iter_ends_line (&end))
    if (!gtk_text_iter_forward_char (&end))
      return FALSE;

  line = g_strstrip (gtk_text_iter_get_slice (&begin, &end));
  ret = g_str_has_prefix (line, prefix);
  g_free (line);

  return ret;
}

static gboolean
line_is_case (const GtkTextIter *line)
{
  return (line_starts_with_fuzzy (line, "case ") ||
          line_starts_with_fuzzy (line, "default:"));
}

static gboolean
str_maybe_label (const gchar *str)
{
  const gchar *ptr = str;

  if (g_strcmp0 (str, "default:") == 0)
    return FALSE;

  for (; *ptr; ptr = g_utf8_next_char (ptr))
    {
      gunichar ch = g_utf8_get_char (ptr);

      switch (ch)
        {
        case ':':
        case '_':
          break;
        default:
          if (!g_unichar_isalnum (ch))
            return FALSE;
        }
    }

  return (*str != '\0');
}

static gboolean
line_is_label (const GtkTextIter *line)
{
  g_auto(GStrv) parts = NULL;
  g_autofree gchar *text = NULL;
  GtkTextIter begin;
  GtkTextIter end;
  guint i;
  guint count = 0;

  gtk_text_iter_assign (&begin, line);
  while (!gtk_text_iter_starts_line (&begin))
    if (!gtk_text_iter_backward_char (&begin))
      return FALSE;

  gtk_text_iter_assign (&end, line);
  while (!gtk_text_iter_ends_line (&end))
    if (!gtk_text_iter_forward_char (&end))
      return FALSE;

  text = gtk_text_iter_get_slice (&begin, &end);
  g_strdelimit (text, "\t", ' ');
  parts = g_strsplit (text, " ", 0);

  for (i = 0; parts [i]; i++)
    {
      g_strstrip (parts [i]);
      if (*parts [i])
        count++;
    }

  if (count > 1)
    return FALSE;

  count = 0;

  for (i = 0; parts [i]; i++)
    {
      g_strstrip (parts [i]);
      if (str_maybe_label (parts [i]))
        count++;
    }

  return (count == 1);
}

static void
maybe_unindent_case_label (IdeCIndenter  *c,
                           GtkTextView   *view,
                           GtkTextBuffer *buffer,
                           GtkTextIter   *iter)
{
  GtkTextIter match_begin;
  GtkTextIter start, end;
  GtkTextIter aux;

  IDE_ENTRY;

  g_assert (IDE_IS_C_INDENTER (c));
  g_assert (GTK_IS_TEXT_VIEW (view));
  g_assert (GTK_IS_TEXT_BUFFER (buffer));
  g_assert (iter);

  aux = *iter;

  if (in_comment (&aux, &match_begin, NULL))
    IDE_RETURN ();

  if (!gtk_text_iter_backward_char (&aux))
    IDE_RETURN ();

  if (line_is_case (&aux))
    {
      if (backward_find_matching_char (&aux, '}'))
        {
          g_autoptr(GString) str = NULL;
          guint offset;

          if (!line_is_whitespace_until (&aux))
            backward_to_line_first_char (&aux);

          str = g_string_new (NULL);
          offset = GET_LINE_OFFSET (&aux);
          build_indent (c, offset + c->case_indent, &aux, str);

          start = *iter;
          gtk_text_iter_set_line_offset (&start, 0);

          end = start;
          while (g_unichar_isspace (gtk_text_iter_get_char (&end)))
            if (!gtk_text_iter_forward_char (&end))
              IDE_RETURN ();

          gtk_text_buffer_delete (buffer, &start, &end);
          gtk_text_buffer_insert (buffer, &start, str->str, str->len);
        }
    }
  else if (line_is_label (&aux))
    {
      ITER_INIT_LINE_START (&start, iter);
      ITER_INIT_LINE_START (&end, iter);

      while (g_unichar_isspace (gtk_text_iter_get_char (&end)))
        if (!gtk_text_iter_forward_char (&end))
          IDE_RETURN ();

      gtk_text_buffer_delete (buffer, &start, &end);
    }

  IDE_EXIT;
}

static gboolean
ide_c_indenter_is_trigger (GtkSourceIndenter *indenter,
                           GtkSourceView     *view,
                           const GtkTextIter *location,
                           GdkModifierType    state,
                           guint              keyval)
{
  IdeCIndenter *c = (IdeCIndenter *)indenter;
  gboolean maybe_accel = (state & (GDK_CONTROL_MASK|GDK_ALT_MASK)) != 0;

  switch (keyval)
    {
    case GDK_KEY_KP_Enter:
    case GDK_KEY_Return:
      if ((state & GDK_SHIFT_MASK) != 0)
        return FALSE;
      c->indent_action = IDE_C_INDENT_ACTION_INDENT_LINE;
      return TRUE;

    case GDK_KEY_braceleft:
      c->indent_action = IDE_C_INDENT_ACTION_UNINDENT_OPENING_BRACE;
      return !maybe_accel;

    case GDK_KEY_braceright:
      c->indent_action = IDE_C_INDENT_ACTION_UNINDENT_CLOSING_BRACE;
      return !maybe_accel;

    case GDK_KEY_colon:
      c->indent_action = IDE_C_INDENT_ACTION_UNINDENT_CASE_OR_LABEL;
      return !maybe_accel;

    case GDK_KEY_numbersign:
      c->indent_action = IDE_C_INDENT_ACTION_UNINDENT_HASH;
      return !maybe_accel;

    case GDK_KEY_parenright:
      c->indent_action = IDE_C_INDENT_ACTION_ALIGN_PARAMETERS;
      return !maybe_accel;

    case GDK_KEY_slash:
      c->indent_action = IDE_C_INDENT_ACTION_CLOSE_COMMENT;
      return !maybe_accel;

    default:
      c->indent_action = 0;
      return FALSE;
    }
}

static void
ide_c_indenter_indent (GtkSourceIndenter *indenter,
                       GtkSourceView     *view,
                       GtkTextIter       *iter)
{
  IdeCIndenter *c = (IdeCIndenter *)indenter;
  GtkTextView *text_view;
  GtkTextBuffer *buffer;
  IdeFileSettings *file_settings;
  IdeIndentStyle indent_style = IDE_INDENT_STYLE_SPACES;
  guint tab_width = 2;
  gint indent_width = -1;

  g_return_if_fail (IDE_IS_C_INDENTER (c));
  g_return_if_fail (IDE_IS_SOURCE_VIEW (view));

  text_view = GTK_TEXT_VIEW (view);
  buffer = gtk_text_view_get_buffer (text_view);

  c->view = IDE_SOURCE_VIEW (view);

  if (GTK_SOURCE_IS_VIEW (view))
    {
      tab_width = gtk_source_view_get_tab_width (GTK_SOURCE_VIEW (view));
      indent_width = gtk_source_view_get_indent_width (GTK_SOURCE_VIEW (view));
      if (indent_width != -1)
        tab_width = indent_width;
    }

  g_assert (IDE_IS_BUFFER (buffer));

  if ((file_settings = ide_buffer_get_file_settings (IDE_BUFFER (buffer))))
    indent_style = ide_file_settings_get_indent_style (file_settings);

  c->tab_width = tab_width;
  if (indent_width <= 0)
    c->indent_width = tab_width;
  else
    c->indent_width = indent_width;

  c->use_tabs =
    !gtk_source_view_get_insert_spaces_instead_of_tabs (GTK_SOURCE_VIEW (view)) ||
    indent_style == IDE_INDENT_STYLE_TABS;

  switch (c->indent_action) {
  case IDE_C_INDENT_ACTION_ALIGN_PARAMETERS:
    /*
     * If we are closing a function declaration, adjust the spacing of
     * parameters so that *'s are aligned.
     */
    maybe_align_parameters (c, buffer, iter);
    break;

  case IDE_C_INDENT_ACTION_CLOSE_COMMENT:
    maybe_close_comment (c, buffer, iter);
    break;

  case IDE_C_INDENT_ACTION_UNINDENT_CASE_OR_LABEL:
    /*
     * If this is a label or a case, adjust indentation.
     */
    maybe_unindent_case_label (c, text_view, buffer, iter);
    break;

  case IDE_C_INDENT_ACTION_UNINDENT_CLOSING_BRACE:
    /*
     * Possibly need to unindent this line.
     */
    maybe_unindent_closing_brace (c, text_view, buffer, iter);
    break;

  case IDE_C_INDENT_ACTION_UNINDENT_HASH:
    /*
     * If this is a preprocessor directive, adjust indentation.
     */
    maybe_unindent_hash (c, buffer, iter);
    break;

  case IDE_C_INDENT_ACTION_UNINDENT_OPENING_BRACE:
    /*
     * Maybe unindent the opening brace to match the conditional.
     * This could happen if we are doing k&r/linux/etc where the open
     * brace has less indentation than the natural single line conditional
     * child statement.
     */
    maybe_unindent_opening_brace (c, text_view, buffer, file_settings, iter);
    break;

  case IDE_C_INDENT_ACTION_INDENT_LINE:
  default:
    c_indenter_indent_line (c, view, buffer, iter);
    break;
  }
}

static void
indenter_iface_init (GtkSourceIndenterInterface *iface)
{
  iface->is_trigger = ide_c_indenter_is_trigger;
  iface->indent = ide_c_indenter_indent;
}

static void
ide_c_indenter_class_init (IdeCIndenterClass *klass)
{
}

static void
ide_c_indenter_init (IdeCIndenter *self)
{
  self->condition_indent = -1;
  self->pre_scope_indent = -1;
  self->post_scope_indent = -1;
  self->directive_indent = G_MININT;
  self->case_indent = 0;
}
