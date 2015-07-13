/* ide-c-indenter.c
 *
 * Copyright (C) 2015 Christian Hergert <christian@hergert.me>
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

#include <glib/gi18n.h>
#include <libpeas/peas.h>

#include "c-parse-helper.h"
#include "ide-c-indenter.h"
#include "ide-debug.h"
#include "ide-source-view.h"

#define ITER_INIT_LINE_START(iter, other) \
  gtk_text_buffer_get_iter_at_line( \
    gtk_text_iter_get_buffer(other), \
    (iter), \
    gtk_text_iter_get_line(other))

struct _IdeCIndenter
{
  IdeObject      parent_instance;

  /* no reference */
  IdeSourceView *view;

  gint           scope_indent;
  gint           condition_indent;
  gint           directive_indent;

  guint          space_before_paren : 1;
};

static void indenter_iface_init (IdeIndenterInterface *iface);

G_DEFINE_DYNAMIC_TYPE_EXTENDED (IdeCIndenter, ide_c_indenter, IDE_TYPE_OBJECT, 0,
                                G_IMPLEMENT_INTERFACE (IDE_TYPE_INDENTER, indenter_iface_init))

static gunichar
text_iter_peek_next_char (const GtkTextIter *location)
{
  GtkTextIter iter = *location;

  if (gtk_text_iter_forward_char (&iter))
    return gtk_text_iter_get_char (&iter);

  return 0;
}

static gunichar
text_iter_peek_prev_char (const GtkTextIter *location)
{
  GtkTextIter iter = *location;

  if (gtk_text_iter_backward_char (&iter))
    return gtk_text_iter_get_char (&iter);

  return 0;
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
  guint i;

  g_assert (tab_width > 0);

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
        for (i = 0; i < tab_width; i++)
          g_string_append (str, " ");
        break;

      case ' ':
        g_string_append_unichar (str, ch);
        break;

      default:
        g_string_append_c (str, ' ');
        break;
      }
  } while (gtk_text_iter_forward_char (&iter) &&
           (gtk_text_iter_compare (&iter, matching_line) <= 0) &&
           (str->len < line_offset));

  while (str->len < line_offset)
    g_string_append_c (str, ' ');

  if (!gtk_source_view_get_insert_spaces_instead_of_tabs (view) && (str->len >= tab_width))
    {
      guint n_tabs = str->len / tab_width;
      guint n_spaces = str->len % tab_width;

      g_string_truncate (str, 0);

      for (i = 0; i < n_tabs; i++)
        g_string_append (str, "\t");

      for (i = 0; i < n_spaces; i++)
        g_string_append (str, " ");
    }
}

static gboolean
iter_ends_c89_comment (const GtkTextIter *iter)
{
  if (gtk_text_iter_get_char (iter) == '/')
    {
      GtkTextIter tmp;

      tmp = *iter;

      if (gtk_text_iter_backward_char (&tmp) &&
          ('*' == gtk_text_iter_get_char (&tmp)))
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

static gboolean
backward_find_keyword (GtkTextIter *iter,
                       const gchar *keyword,
                       GtkTextIter *limit)
{
  GtkTextIter begin;
  GtkTextIter end;

  /*
   * If we find the keyword, check to see that the character before it
   * is either a newline or some other space character. (ie, not part of a
   * function name like foo_do().
   */
  if (gtk_text_iter_backward_search (iter, keyword, GTK_TEXT_SEARCH_TEXT_ONLY,
                                     &begin, &end, limit))
    {
      GtkTextIter copy;
      gunichar ch;

      gtk_text_iter_assign (&copy, &begin);

      if (!gtk_text_iter_backward_char (&copy) ||
          !(ch = gtk_text_iter_get_char (&copy)) ||
          g_unichar_isspace (ch))
        {
          gtk_text_iter_assign (iter, &begin);
          return TRUE;
        }
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

/**
 * text_iter_in_c89_comment:
 * @location: (in): A #GtkTextIter containing the target location.
 *
 * The algorith for this is unfortunately trickier than one would expect.
 * Because we could always still have context if we walk backwards that
 * would let us know if we are in a string, we just start from the beginning
 * of the buffer and try to skip forward until we get to our target
 * position.
 *
 * Returns: %TRUE if we think we are in a c89 comment, otherwise %FALSE.
 */
static gboolean
in_c89_comment (const GtkTextIter *location,
                GtkTextIter       *match_begin)
{
  GtkTextBuffer *buffer;
  GtkTextIter iter;
  GtkTextIter after_location;

  buffer = gtk_text_iter_get_buffer (location);
  gtk_text_buffer_get_start_iter (buffer, &iter);

  after_location = *location;
  gtk_text_iter_forward_char (&after_location);

  do
    {
      gunichar ch;

      if (gtk_text_iter_compare (&iter, location) > 0)
        break;

      ch = gtk_text_iter_get_char (&iter);

      /* skip past the c89 comment */
      if ((ch == '/') && (text_iter_peek_next_char (&iter) == '*'))
        {
          GtkTextIter saved = iter;

          if (!gtk_text_iter_forward_chars (&iter, 2) ||
              !gtk_text_iter_forward_search (&iter, "*/",
                                             GTK_TEXT_SEARCH_TEXT_ONLY,
                                             NULL, &iter, &after_location))
            {
              *match_begin = saved;
              IDE_RETURN (TRUE);
            }
        }

      /* skip past a string or character */
      if ((ch == '\'') || (ch == '"'))
        {
          const gchar *match = (ch == '\'') ? "'" : "\"";

        again:
          if (!gtk_text_iter_forward_search (&iter, match,
                                             GTK_TEXT_SEARCH_TEXT_ONLY,
                                             NULL, NULL, NULL))
            return FALSE;

          /* this one is escaped, keep looking */
          if (text_iter_peek_prev_char (&iter) == '\\')
            {
              if (!gtk_text_iter_forward_char (&iter))
                return FALSE;
              goto again;
            }
        }

      /* skip past escaped character */
      if (ch == '\\')
        {
          if (!gtk_text_iter_forward_char (&iter))
            return FALSE;
        }
    }
  while (gtk_text_iter_forward_char (&iter));

  return FALSE;
}

static gchar *
c_indenter_indent (IdeCIndenter *c,
                                  GtkTextView           *view,
                                  GtkTextBuffer         *buffer,
                                  GtkTextIter           *iter)
{
  GtkTextIter cur;
  GtkTextIter match_begin;
  gunichar ch;
  GString *str;
  gchar *ret = NULL;
  gchar *last_word = NULL;

  IDE_ENTRY;

  g_return_val_if_fail (IDE_IS_C_INDENTER (c), NULL);

  /*
   * Save our current iter position to restore it later.
   */
  gtk_text_iter_assign (&cur, iter);

  /*
   * Move to before the character just inserted.
   */
  gtk_text_iter_backward_char (iter);

  /*
   * Create the buffer for our indentation string.
   */
  str = g_string_new (NULL);

  /*
   * Move backwards to the last non-space character inserted. We need to
   * start by moving back one character to get to the pre-newline insertion
   * point.
   */
  if (g_unichar_isspace (gtk_text_iter_get_char (iter)))
    if (!gtk_text_iter_backward_find_char (iter, non_space_predicate, NULL, NULL))
      IDE_GOTO (cleanup);

  /*
   * If we are in a c89 multi-line comment, try to match the previous comment
   * line. Function will leave iter at original position unless it matched.
   * If so, it will be at the beginning of the comment.
   */
  if (in_c89_comment (iter, &match_begin))
    {
      guint offset;

      gtk_text_iter_assign (iter, &match_begin);
      offset = gtk_text_iter_get_line_offset (iter);
      build_indent (c, offset + 1, iter, str);
      g_string_append (str, "* ");
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

      offset = gtk_text_iter_get_line_offset (iter);

      if (gtk_text_iter_get_char (iter) == '(')
        offset++;
      else if (gtk_text_iter_get_char (iter) == '{')
        {
          /*
           * Handle the case where { is not the first character,
           * like "enum {".
           */
          if (backward_to_line_first_char (iter))
            offset = gtk_text_iter_get_line_offset (iter);
          offset += c->scope_indent;
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
          offset = gtk_text_iter_get_line_offset (iter);
          build_indent (c, offset, iter, str);
          IDE_GOTO (cleanup);
        }
    }

  /*
   * Maybe we are in a conditional.
   *
   * TODO: This technically isn't right since it is perfectly reasonable to
   * end a line on a ) but not be done with the entire conditional.
   */
  if ((ch != ')') && backward_find_matching_char (iter, ')'))
    {
      guint offset;

      offset = gtk_text_iter_get_line_offset (iter);
      build_indent (c, offset + 1, iter, str);
      IDE_GOTO (cleanup);
    }

  /*
   * If we just ended a scope, we need to look for the matching scope
   * before it.
   */
  if (ch == '}')
    {
      GtkTextIter copy;

      gtk_text_iter_assign (&copy, iter);

      if (gtk_text_iter_forward_char (iter))
        {
          guint offset = gtk_text_iter_get_line_offset (iter) - 1;

          if (backward_find_matching_char (iter, '}'))
            {
              offset = gtk_text_iter_get_line_offset (iter);
              offset += c->scope_indent;
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
      GtkTextIter copy;

      gtk_text_iter_assign (&copy, iter);

      if (backward_find_matching_char (iter, ')') &&
          backward_find_condition_keyword (iter))
        {
          guint offset = gtk_text_iter_get_line_offset (iter);
          build_indent (c, offset + c->condition_indent, iter, str);
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

      offset = gtk_text_iter_get_line_offset (&match_begin);
      build_indent (c, offset + c->scope_indent, iter, str);
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

          offset = gtk_text_iter_get_line_offset (iter);
          build_indent (c, offset + c->scope_indent, iter, str);
          IDE_GOTO (cleanup);
        }
      else
        {
          if (backward_to_line_first_char (iter))
            {
              guint offset;

              offset = gtk_text_iter_get_line_offset (iter);
              build_indent (c, offset + c->scope_indent, iter, str);
              IDE_GOTO (cleanup);
            }
        }
    }

cleanup:
  gtk_text_iter_assign (iter, &cur);
  g_free (last_word);

  ret = g_string_free (str, FALSE);

  IDE_RETURN (ret);
}

static gchar *
maybe_close_comment (IdeCIndenter *c,
                     GtkTextIter           *begin,
                     GtkTextIter           *end)
{
  GtkTextIter copy;
  GtkTextIter begin_comment;
  gchar *ret = NULL;

  g_return_val_if_fail (IDE_IS_C_INDENTER (c), NULL);
  g_return_val_if_fail (begin, NULL);
  g_return_val_if_fail (end, NULL);

  gtk_text_iter_assign (&copy, begin);

  /*
   * Walk backwards ensuring we just inserted a '/' and that it was after
   * a '* ' sequence.
   */
  if (in_c89_comment (begin, &begin_comment) &&
      gtk_text_iter_backward_char (begin) &&
      ('/' == gtk_text_iter_get_char (begin)) &&
      gtk_text_iter_backward_char (begin) &&
      (' ' == gtk_text_iter_get_char (begin)) &&
      gtk_text_iter_backward_char (begin) &&
      ('*' == gtk_text_iter_get_char (begin)))
    ret = g_strdup ("*/");
  else
    gtk_text_iter_assign (begin, &copy);

  return ret;
}

static gchar *
maybe_unindent_brace (IdeCIndenter *c,
                      GtkTextIter           *begin,
                      GtkTextIter           *end)
{
  GtkTextIter saved;
  gchar *ret = NULL;

  IDE_ENTRY;

  g_return_val_if_fail (IDE_IS_C_INDENTER (c), NULL);
  g_return_val_if_fail (begin, NULL);
  g_return_val_if_fail (end, NULL);

  gtk_text_iter_assign (&saved, begin);

  if (gtk_text_iter_backward_char (begin) &&
      gtk_text_iter_backward_char (end) &&
      backward_find_matching_char (begin, '}') &&
      line_is_whitespace_until (end) &&
      ((gtk_text_iter_get_offset (begin) + 1) !=
        gtk_text_iter_get_offset (end)))
    {
      GString *str;
      guint offset;

      /*
       * Handle the case where { is not the first non-whitespace
       * character on the line.
       */
      if (!starts_line_space_ok (begin))
        backward_to_line_first_char (begin);

      offset = gtk_text_iter_get_line_offset (begin);
      str = g_string_new (NULL);
      build_indent (c, offset, begin, str);
      g_string_append_c (str, '}');

      gtk_text_iter_assign (begin, &saved);
      while (!gtk_text_iter_starts_line (begin))
        gtk_text_iter_backward_char (begin);

      gtk_text_iter_assign (end, &saved);

      ret = g_string_free (str, FALSE);
    }

  if (!ret)
    {
      gtk_text_iter_assign (begin, &saved);
      gtk_text_iter_assign (end, &saved);
    }

  IDE_RETURN (ret);
}

static gchar *
maybe_unindent_hash (IdeCIndenter *c,
                     GtkTextIter           *begin,
                     GtkTextIter           *end)
{
  GtkTextIter saved;
  gchar *ret = NULL;

  g_return_val_if_fail (IDE_IS_C_INDENTER (c), NULL);
  g_return_val_if_fail (begin, NULL);
  g_return_val_if_fail (end, NULL);

  gtk_text_iter_assign (&saved, begin);

  if (gtk_text_iter_backward_char (begin) &&
      ('#' == gtk_text_iter_get_char (begin)) &&
      line_is_whitespace_until (begin))
    {
      if (c->directive_indent == G_MININT)
        {
          while (!gtk_text_iter_starts_line (begin))
            gtk_text_iter_backward_char (begin);
          ret = g_strdup ("#");
        }
      else
        {
          /* TODO: Handle indent when not fully unindenting. */
        }
    }

  if (!ret)
    gtk_text_iter_assign (begin, &saved);

  return ret;
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

#if 0
static gchar *
maybe_space_before_paren (IdeCIndenter *c,
                          GtkTextIter           *begin,
                          GtkTextIter           *end)
{
  GtkTextIter match_begin;
  GtkTextIter copy;
  gunichar ch;

  g_return_val_if_fail (IDE_IS_C_INDENTER (c), NULL);
  g_return_val_if_fail (begin, NULL);
  g_return_val_if_fail (end, NULL);

  if (!c->priv->space_before_paren)
    return NULL;

  if (in_c89_comment (begin, &match_begin))
    return NULL;

  /* ignore preprocessor #define */
  if (line_starts_with_fuzzy (begin, "#"))
    return NULL;

  gtk_text_iter_assign (&copy, begin);

  /*
   * Move back to the character just inserted.
   */
  if (gtk_text_iter_backward_char (begin) &&
      (ch = gtk_text_iter_get_char (begin)) &&
      (ch == '(') &&
      gtk_text_iter_backward_char (begin) &&
      (ch = gtk_text_iter_get_char (begin)) &&
      !g_unichar_isspace (ch) &&
      g_unichar_isalnum (ch))
    {
      gtk_text_iter_forward_char (begin);
      return g_strdup (" (");
    }

  gtk_text_iter_assign (begin, &copy);

  return NULL;
}
#endif

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

static gchar *
format_parameters (GtkTextIter *begin,
                   GSList      *params)
{
  GtkTextIter line_start;
  GtkTextIter first_char;
  GString *str;
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
      gchar *param_str;

      if (iter != params)
        g_string_append (str, join_str);

      param_str = format_parameter (iter->data, max_type, max_star);
      g_string_append (str, param_str);
    }

  g_free (join_str);

  return g_string_free (str, FALSE);
}

static gchar *
maybe_align_parameters (IdeCIndenter *c,
                        GtkTextIter           *begin,
                        GtkTextIter           *end)
{
  GtkTextIter match_begin;
  GtkTextIter copy;
  GSList *params = NULL;
  gchar *ret = NULL;
  gchar *text = NULL;

  IDE_ENTRY;

  g_return_val_if_fail (IDE_IS_C_INDENTER (c), NULL);
  g_return_val_if_fail (begin, NULL);
  g_return_val_if_fail (end, NULL);

  if (in_c89_comment (begin, &match_begin))
    IDE_RETURN (NULL);

  gtk_text_iter_assign (&copy, begin);

  if (gtk_text_iter_backward_char (begin) &&
      backward_find_matching_char (begin, ')') &&
      gtk_text_iter_forward_char (begin) &&
      gtk_text_iter_backward_char (end) &&
      (gtk_text_iter_compare (begin, end) < 0) &&
      (text = gtk_text_iter_get_slice (begin, end)) &&
      (params = parse_parameters (text)) &&
      (params->next != NULL))
    ret = format_parameters (begin, params);

  g_slist_foreach (params, (GFunc)parameter_free, NULL);
  g_slist_free (params);

  if (!ret)
    {
      gtk_text_iter_assign (begin, &copy);
      gtk_text_iter_assign (end, &copy);
    }

  g_free (text);

  IDE_RETURN (ret);
}

#if 0
static gchar *
maybe_add_brace (IdeCIndenter *c,
                 GtkTextIter           *begin,
                 GtkTextIter           *end,
                 gint                  *cursor_offset)
{
  GtkTextIter iter;

  gtk_text_iter_assign (&iter, begin);

  if (gtk_text_iter_backward_char (&iter) &&
      (gtk_text_iter_get_char (&iter) == '{') &&
      (gtk_text_iter_get_char (begin) != '}'))
    {
      GtkTextIter copy = iter;

      gtk_text_iter_assign (&copy, &iter);

      if (gtk_text_iter_backward_word_start (&copy))
        {
          GtkTextIter copy2 = copy;

          if (gtk_text_iter_forward_word_end (&copy2))
            {
              gchar *word;

              word = gtk_text_iter_get_slice (&copy, &copy2);

              if ((g_strcmp0 (word, "enum") == 0) ||
                  (g_strcmp0 (word, "struct") == 0))
                {
                  *cursor_offset = -2;
                  g_free (word);
                  return g_strdup ("};");
                }

              g_free (word);
            }
        }

      *cursor_offset = -1;
      return g_strdup ("}");
    }

  return NULL;
}
#endif

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
  GtkTextIter begin;
  GtkTextIter end;
  gchar *text;
  gchar **parts;
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

  g_free (text);
  g_strfreev (parts);

  return (count == 1);
}

static gchar *
maybe_unindent_case_label (IdeCIndenter *c,
                           GtkTextIter           *begin,
                           GtkTextIter           *end)
{
  GtkTextIter match_begin;
  GtkTextIter iter;

  IDE_ENTRY;

  gtk_text_iter_assign (&iter, begin);

  if (in_c89_comment (begin, &match_begin))
    IDE_RETURN (NULL);

  if (!gtk_text_iter_backward_char (&iter))
    IDE_RETURN (NULL);

  if (line_is_case (&iter))
    {
      if (backward_find_matching_char (&iter, '}'))
        {
          if (line_is_whitespace_until (&iter))
            {
              GString *str;
              guint offset;

              str = g_string_new (NULL);
              offset = gtk_text_iter_get_line_offset (&iter);
              build_indent (c, offset, &iter, str);
              while (!gtk_text_iter_starts_line (begin))
                gtk_text_iter_backward_char (begin);
              gtk_text_iter_assign (end, begin);
              while (g_unichar_isspace (gtk_text_iter_get_char (end)))
                if (!gtk_text_iter_forward_char (end))
                  IDE_RETURN (NULL);
              return g_string_free (str, FALSE);
            }
          else
            {
              if (backward_to_line_first_char (&iter))
                {
#if 0
                  TODO ("Deal with nested {");
#endif
                }
            }
        }
    }
  else if (line_is_label (&iter))
    {
#if 0
      TODO ("allow configurable label indent");
#endif

      ITER_INIT_LINE_START (begin, &iter);
      ITER_INIT_LINE_START (end, &iter);

      while (g_unichar_isspace (gtk_text_iter_get_char (end)))
        if (!gtk_text_iter_forward_char (end))
          return NULL;

      return g_strdup ("");
    }

  IDE_RETURN (NULL);
}

static gboolean
ide_c_indenter_is_trigger (IdeIndenter *indenter,
                           GdkEventKey *event)
{
  switch (event->keyval)
    {
    case GDK_KEY_KP_Enter:
    case GDK_KEY_Return:
      if ((event->state & GDK_SHIFT_MASK) != 0)
        return FALSE;
      /* Fall through */

    case GDK_KEY_braceright:
    case GDK_KEY_colon:
    case GDK_KEY_numbersign:
    case GDK_KEY_parenright:
    case GDK_KEY_slash:
      return TRUE;

    default:
      return FALSE;
    }
}

static gchar *
ide_c_indenter_format (IdeIndenter    *indenter,
                       GtkTextView    *view,
                       GtkTextIter    *begin,
                       GtkTextIter    *end,
                       gint           *cursor_offset,
                       GdkEventKey    *event)
{
  IdeCIndenter *c = (IdeCIndenter *)indenter;
  GtkTextIter begin_copy;
  gchar *ret = NULL;
  GtkTextBuffer *buffer;

  g_return_val_if_fail (IDE_IS_C_INDENTER (c), NULL);
  g_return_val_if_fail (IDE_IS_SOURCE_VIEW (view), NULL);

  buffer = gtk_text_view_get_buffer (view);

  c->view = IDE_SOURCE_VIEW (view);

  switch (event->keyval) {
  case GDK_KEY_Return:
  case GDK_KEY_KP_Enter:
    gtk_text_iter_assign (&begin_copy, begin);
    ret = c_indenter_indent (c, view, buffer, begin);
    gtk_text_iter_assign (begin, &begin_copy);

    /*
     * If we are inserting a newline right before a closing brace (for example
     * after {<cursor>}, we need to indent and then maybe unindent the }.
     */
    if (gtk_text_iter_get_char (begin) == '}')
      {
        GtkTextIter iter;
        GString *str;
        gchar *tmp = ret;
        guint offset = 0;

        str = g_string_new (NULL);

        gtk_text_iter_assign (&iter, begin);
        if (backward_find_matching_char (&iter, '}'))
          {
            if (line_is_whitespace_until (&iter))
              offset = gtk_text_iter_get_line_offset (&iter);
            else if (backward_to_line_first_char (&iter))
              offset = gtk_text_iter_get_line_offset (&iter);
            build_indent (c, offset, &iter, str);
            g_string_prepend (str, "\n");
            g_string_prepend (str, ret);

            *cursor_offset = -(str->len - strlen (ret));

            ret = g_string_free (str, FALSE);
            g_free (tmp);
          }
      }

    break;

  case GDK_KEY_braceright:
    /*
     * Probably need to unindent this line.
     * TODO: Maybe overwrite character.
     */
    ret = maybe_unindent_brace (c, begin, end);
    break;

  case GDK_KEY_colon:
    /*
     * If this is a label or a case, adjust indentation.
     */
    ret = maybe_unindent_case_label (c, begin, end);
    break;

  case GDK_KEY_numbersign:
    /*
     * If this is a preprocessor directive, adjust indentation.
     */
    ret = maybe_unindent_hash (c, begin, end);
    break;

  case GDK_KEY_parenright:
    /*
     * If we are closing a function declaration, adjust the spacing of
     * parameters so that *'s are aligned.
     */
    ret = maybe_align_parameters (c, begin, end);
    break;

  case GDK_KEY_slash:
    /*
     * Check to see if we are right after a "* " and typing "/" while inside
     * of a multi-line comment. Probably just want to close the comment.
     */
    ret = maybe_close_comment (c, begin, end);
    break;

  default:
    break;
  }

  return ret;
}

static void
indenter_iface_init (IdeIndenterInterface *iface)
{
  iface->is_trigger = ide_c_indenter_is_trigger;
  iface->format = ide_c_indenter_format;
}

static void
ide_c_indenter_class_init (IdeCIndenterClass *klass)
{
}

static void
ide_c_indenter_class_finalize (IdeCIndenterClass *klass)
{
}

static void
ide_c_indenter_init (IdeCIndenter *self)
{
  self->condition_indent = 2;
  self->scope_indent = 2;
  self->directive_indent = G_MININT;
  self->space_before_paren = TRUE;
}

void
_ide_c_indenter_register_type (GTypeModule *module)
{
  ide_c_indenter_register_type (module);
}
