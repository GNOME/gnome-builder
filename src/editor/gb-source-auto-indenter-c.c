/* gb-source-auto-indenter-c.c
 *
 * Copyright (C) 2014 Christian Hergert <christian@hergert.me>
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

#define G_LOG_DOMAIN "indent"

#include <glib/gi18n.h>

#include "gb-log.h"
#include "gb-source-auto-indenter-c.h"

#include "c-parse-helper.h"

struct _GbSourceAutoIndenterCPrivate
{
  gint  scope_indent;
  gint  condition_indent;
  gint  directive_indent;
  guint space_before_paren : 1;
};

enum
{
  PROP_0,
  PROP_SCOPE_INDENT,
  LAST_PROP
};

G_DEFINE_TYPE_WITH_PRIVATE (GbSourceAutoIndenterC, gb_source_auto_indenter_c,
                            GB_TYPE_SOURCE_AUTO_INDENTER)

static GParamSpec *gParamSpecs [LAST_PROP];

#define ITER_INIT_LINE_START(iter, other) \
  gtk_text_buffer_get_iter_at_line( \
    gtk_text_iter_get_buffer(other), \
    (iter), \
    gtk_text_iter_get_line(other))

GbSourceAutoIndenter *
gb_source_auto_indenter_c_new (void)
{
  return g_object_new (GB_TYPE_SOURCE_AUTO_INDENTER_C, NULL);
}

static inline void
build_indent (GbSourceAutoIndenterC *c,
              guint                  line_offset,
              GtkTextIter           *matching_line,
              GString               *str)
{
  GtkTextIter iter;
  gunichar ch;

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
}

static gboolean
non_space_predicate (gunichar ch,
                     gpointer user_data)
{
  return !g_unichar_isspace (ch);
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
backward_find_matching_char (GtkTextIter *iter,
                             gunichar     ch)
{
  GtkTextIter copy;
  gunichar match;
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

  /*
   * TODO: Make this skip past comment blocks!
   */

  gtk_text_iter_assign (&copy, iter);

  while (gtk_text_iter_backward_char (iter))
    {
      cur = gtk_text_iter_get_char (iter);

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
backward_before_c89_comment (GtkTextIter *iter)
{
  GtkTextIter copy;
  GtkTextIter match_start;
  GtkTextIter match_end;
  gunichar ch;

  gtk_text_iter_assign (&copy, iter);

  while (g_unichar_isspace (gtk_text_iter_get_char (iter)))
    if (!gtk_text_iter_backward_char (iter))
      GOTO (cleanup);

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
    GOTO (cleanup);

  gtk_text_iter_assign (iter, &match_start);

  return TRUE;

cleanup:
  gtk_text_iter_assign (iter, &copy);

  return FALSE;
}

static gboolean
in_c89_comment (GtkTextIter *iter,
                GtkTextIter *match_begin)
{
  GtkTextIter saved;
  GtkTextIter after_cur;
  GtkTextIter match_end;
  gboolean ret;

  gtk_text_iter_assign (&saved, iter);

  gtk_text_iter_assign (&after_cur, iter);
  gtk_text_iter_forward_char (&after_cur);

  /*
   * This works by first looking for the end of a comment. Afterwards,
   * we then walk forward looking for the beginning of a comment. If we
   * find one, then we are still in a comment.
   *
   * Not perfect, since we could be in a string, but it's a good start.
   */

  if (gtk_text_iter_backward_search (&after_cur, "*/",
                                     GTK_TEXT_SEARCH_TEXT_ONLY, match_begin,
                                     &match_end, NULL))
    gtk_text_iter_assign (iter, &match_end);
  else
    gtk_text_buffer_get_start_iter (gtk_text_iter_get_buffer (iter), iter);

  /*
   * Walk forwards until we find begin of a comment.
   */
  ret = gtk_text_iter_forward_search (iter, "/*", GTK_TEXT_SEARCH_TEXT_ONLY,
                                      match_begin, &match_end, &after_cur);

  gtk_text_iter_assign (iter, &saved);

  return ret;
}

static gchar *
gb_source_auto_indenter_c_indent (GbSourceAutoIndenterC *c,
                                  GtkTextView           *view,
                                  GtkTextBuffer         *buffer,
                                  GtkTextIter           *iter)
{
  GbSourceAutoIndenterCPrivate *priv;
  GtkTextIter cur;
  GtkTextIter match_begin;
  gunichar ch;
  GString *str;
  gchar *ret = NULL;
  gchar *last_word = NULL;

  ENTRY;

  g_return_val_if_fail (GB_IS_SOURCE_AUTO_INDENTER_C (c), NULL);

  priv = c->priv;

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
      GOTO (cleanup);

  /*
   * Get our last non \n character entered.
   */
  ch = gtk_text_iter_get_char (iter);

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
      GOTO (cleanup);
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
        GOTO (cleanup);

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
          offset += priv->scope_indent;
        }

      build_indent (c, offset, iter, str);
      GOTO (cleanup);
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
          GOTO (cleanup);
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
      GOTO (cleanup);
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
              offset += priv->scope_indent;
            }

          build_indent (c, offset, iter, str);
          GOTO (cleanup);
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
          build_indent (c, offset + priv->condition_indent, iter, str);
          GOTO (cleanup);
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

      offset = gtk_text_iter_get_line_offset (&match_begin);
      build_indent (c, offset + priv->scope_indent, iter, str);
      GOTO (cleanup);
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
          build_indent (c, offset + priv->scope_indent, iter, str);
          GOTO (cleanup);
        }
      else
        {
          if (backward_to_line_first_char (iter))
            {
              guint offset;

              offset = gtk_text_iter_get_line_offset (iter);
              build_indent (c, offset + priv->scope_indent, iter, str);
              GOTO (cleanup);
            }
        }
    }

cleanup:
  gtk_text_iter_assign (iter, &cur);
  g_free (last_word);

  ret = g_string_free (str, FALSE);

  RETURN (ret);
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

static gchar *
maybe_close_comment (GbSourceAutoIndenterC *c,
                     GtkTextIter           *begin,
                     GtkTextIter           *end)
{
  GtkTextIter copy;
  GtkTextIter begin_comment;
  gchar *ret = NULL;

  g_return_val_if_fail (GB_IS_SOURCE_AUTO_INDENTER_C (c), NULL);
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
maybe_unindent_brace (GbSourceAutoIndenterC *c,
                      GtkTextIter           *begin,
                      GtkTextIter           *end)
{
  GtkTextIter saved;
  gchar *ret = NULL;

  ENTRY;

  g_return_val_if_fail (GB_IS_SOURCE_AUTO_INDENTER_C (c), NULL);
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

  RETURN (ret);
}

static gchar *
maybe_unindent_hash (GbSourceAutoIndenterC *c,
                     GtkTextIter           *begin,
                     GtkTextIter           *end)
{
  GtkTextIter saved;
  gchar *ret = NULL;

  g_return_val_if_fail (GB_IS_SOURCE_AUTO_INDENTER_C (c), NULL);
  g_return_val_if_fail (begin, NULL);
  g_return_val_if_fail (end, NULL);

  gtk_text_iter_assign (&saved, begin);

  if (gtk_text_iter_backward_char (begin) &&
      ('#' == gtk_text_iter_get_char (begin)) &&
      line_is_whitespace_until (begin))
    {
      if (c->priv->directive_indent == G_MININT)
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

static gchar *
maybe_space_before_paren (GbSourceAutoIndenterC *c,
                          GtkTextIter           *begin,
                          GtkTextIter           *end)
{
  GtkTextIter match_begin;
  GtkTextIter copy;
  gunichar ch;

  g_return_val_if_fail (GB_IS_SOURCE_AUTO_INDENTER_C (c), NULL);
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
      max_star = MAX (max_star, p->n_star);
      max_type = MAX (max_type, strlen (p->type));
    }

  str = g_string_new (NULL);

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
maybe_align_parameters (GbSourceAutoIndenterC *c,
                        GtkTextIter           *begin,
                        GtkTextIter           *end)
{
  GtkTextIter match_begin;
  GtkTextIter copy;
  GSList *params = NULL;
  gchar *ret = NULL;
  gchar *text = NULL;

  ENTRY;

  g_return_val_if_fail (GB_IS_SOURCE_AUTO_INDENTER_C (c), NULL);
  g_return_val_if_fail (begin, NULL);
  g_return_val_if_fail (end, NULL);

  if (in_c89_comment (begin, &match_begin))
    RETURN (NULL);

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

  RETURN (ret);
}

static gchar *
maybe_add_brace (GbSourceAutoIndenterC *c,
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
      *cursor_offset = -1;
      return g_strdup ("}");
    }

  return NULL;
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
maybe_unindent_case_label (GbSourceAutoIndenterC *c,
                           GtkTextIter           *begin,
                           GtkTextIter           *end)
{
  GtkTextIter match_begin;
  GtkTextIter iter;

  ENTRY;

  gtk_text_iter_assign (&iter, begin);

  if (in_c89_comment (begin, &match_begin))
    RETURN (NULL);

  if (!gtk_text_iter_backward_char (&iter))
    RETURN (NULL);

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
                  RETURN (NULL);
              return g_string_free (str, FALSE);
            }
          else
            {
              if (backward_to_line_first_char (&iter))
                {
                  TODO ("Deal with nested {");
                }
            }
        }
    }
  else if (line_is_label (&iter))
    {
      TODO ("allow configurable label indent");

      ITER_INIT_LINE_START (begin, &iter);
      ITER_INIT_LINE_START (end, &iter);

      while (g_unichar_isspace (gtk_text_iter_get_char (end)))
        if (!gtk_text_iter_forward_char (end))
          return NULL;

      return g_strdup ("");
    }

  RETURN (NULL);
}

static gboolean
gb_source_auto_indenter_c_is_trigger (GbSourceAutoIndenter *indenter,
                                      GdkEventKey          *event)
{
  switch (event->keyval) {
  case GDK_KEY_KP_Enter:
  case GDK_KEY_Return:
  case GDK_KEY_braceleft:
  case GDK_KEY_braceright:
  case GDK_KEY_colon:
  case GDK_KEY_numbersign:
  case GDK_KEY_parenright:
  case GDK_KEY_parenleft:
  case GDK_KEY_slash:
    return TRUE;
  default:
    return FALSE;
  }
}

static gchar *
gb_source_auto_indenter_c_format (GbSourceAutoIndenter *indenter,
                                  GtkTextView          *view,
                                  GtkTextBuffer        *buffer,
                                  GtkTextIter          *begin,
                                  GtkTextIter          *end,
                                  gint                 *cursor_offset,
                                  GdkEventKey          *event)
{
  GbSourceAutoIndenterC *c = (GbSourceAutoIndenterC *)indenter;
  GtkTextIter begin_copy;
  gchar *ret = NULL;

  g_return_val_if_fail (GB_IS_SOURCE_AUTO_INDENTER_C (c), NULL);

  switch (event->keyval) {
  case GDK_KEY_Return:
  case GDK_KEY_KP_Enter:
    gtk_text_iter_assign (&begin_copy, begin);
    ret = gb_source_auto_indenter_c_indent (c, view, buffer, begin);
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

  case GDK_KEY_braceleft:
    /*
     * If we are starting a new scope, maybe add a match closing brace.
     */
    ret = maybe_add_brace (c, begin, end, cursor_offset);
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

  case GDK_KEY_parenleft:
    /*
     * Possibly add a space before the ( if our config requests so.
     */
    ret = maybe_space_before_paren (c, begin, end);
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
gb_source_auto_indenter_c_get_property (GObject    *object,
                                        guint       prop_id,
                                        GValue     *value,
                                        GParamSpec *pspec)
{
  GbSourceAutoIndenterC *c = GB_SOURCE_AUTO_INDENTER_C (object);

  switch (prop_id) {
  case PROP_SCOPE_INDENT:
    g_value_set_uint (value, c->priv->scope_indent);
    break;
  default:
    G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
  }
}

static void
gb_source_auto_indenter_c_set_property (GObject      *object,
                                        guint         prop_id,
                                        const GValue *value,
                                        GParamSpec   *pspec)
{
  GbSourceAutoIndenterC *c = GB_SOURCE_AUTO_INDENTER_C (object);

  switch (prop_id) {
  case PROP_SCOPE_INDENT:
    c->priv->scope_indent = g_value_get_uint (value);
    break;
  default:
    G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
  }

  g_object_notify_by_pspec (object, pspec);
}

static void
gb_source_auto_indenter_c_class_init (GbSourceAutoIndenterCClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GbSourceAutoIndenterClass *indenter_class = GB_SOURCE_AUTO_INDENTER_CLASS (klass);

  object_class->get_property = gb_source_auto_indenter_c_get_property;
  object_class->set_property = gb_source_auto_indenter_c_set_property;

  indenter_class->is_trigger = gb_source_auto_indenter_c_is_trigger;
  indenter_class->format = gb_source_auto_indenter_c_format;

  gParamSpecs [PROP_SCOPE_INDENT] =
    g_param_spec_int ("scope-indent",
                      _("Name"),
                      _("Name"),
                      -32,
                      32,
                      2,
                      (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (object_class, PROP_SCOPE_INDENT,
                                   gParamSpecs [PROP_SCOPE_INDENT]);
}

static void
gb_source_auto_indenter_c_init (GbSourceAutoIndenterC *c)
{
  c->priv = gb_source_auto_indenter_c_get_instance_private (c);

  c->priv->condition_indent = 2;
  c->priv->scope_indent = 2;
  c->priv->directive_indent = G_MININT;
  c->priv->space_before_paren = TRUE;
}
