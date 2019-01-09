/* ide-python-indenter.c
 *
 * Copyright 2014-2019 Christian Hergert <christian@hergert.me>
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

#define G_LOG_DOMAIN "ide-python-indenter"

#include <gtksourceview/gtksource.h>
#include <libide-sourceview.h>
#include <string.h>

#include "ide-python-indenter.h"

struct _IdePythonIndenter
{
  IdeObject parent_instance;
};

/*
 * TODO:
 *
 * This module is very naive. Why don't you come and make it better!
 */

static void indenter_iface_init (IdeIndenterInterface *iface);

G_DEFINE_TYPE_WITH_CODE (IdePythonIndenter, ide_python_indenter, IDE_TYPE_OBJECT,
                         G_IMPLEMENT_INTERFACE (IDE_TYPE_INDENTER, indenter_iface_init))

static gboolean
in_pydoc (const GtkTextIter *iter)
{
  GtkTextIter copy = *iter;
  GtkSourceBuffer *buffer;

  buffer = GTK_SOURCE_BUFFER (gtk_text_iter_get_buffer (iter));

  if (gtk_source_buffer_iter_has_context_class (buffer, &copy, "comment") ||
      gtk_source_buffer_iter_has_context_class (buffer, &copy, "string"))
    return TRUE;

  return FALSE;
}

static gboolean
line_starts_with (GtkTextIter *line,
                  const gchar *prefix)
{
  GtkTextIter begin = *line;
  GtkTextIter end = *line;
  gboolean ret;
  gchar *text;

  while (!gtk_text_iter_starts_line (&begin))
    if (!gtk_text_iter_backward_char (&begin))
      break;

  while (!gtk_text_iter_ends_line (&end))
    if (!gtk_text_iter_forward_char (&end))
      break;

  text = gtk_text_iter_get_slice (&begin, &end);
  g_strstrip (text);
  ret = g_str_has_prefix (text, prefix);
  g_free (text);

  return ret;
}

#if 0
static gboolean
line_ends_with (const GtkTextIter *iter,
                const gchar       *suffix)
{
  GtkTextIter begin = *iter;
  GtkTextIter end;
  gboolean ret = FALSE;

  while (!gtk_text_iter_ends_line (&begin))
    gtk_text_iter_forward_char (&begin);

  end = begin;

  if (gtk_text_iter_backward_chars (&begin, strlen (suffix)))
    {
      gchar *slice;

      slice = gtk_text_iter_get_slice (&begin, &end);
      ret = g_str_equal (slice, suffix);
      g_free (slice);
    }

  return ret;
}
#endif

static gchar *
copy_indent (IdePythonIndenter *python,
             GtkTextIter       *begin,
             GtkTextIter       *end,
             GtkTextIter       *copy)
{
  GString *str;

  str = g_string_new (NULL);

  gtk_text_iter_set_line_offset (copy, 0);

  while (!gtk_text_iter_ends_line (copy))
    {
      gunichar ch;

      ch = gtk_text_iter_get_char (copy);

      if (!g_unichar_isspace (ch))
        break;

      g_string_append_unichar (str, ch);

      if (gtk_text_iter_ends_line (copy) ||
          !gtk_text_iter_forward_char (copy))
        break;
    }

  return g_string_free (str, FALSE);
}

static gboolean
backtrack_to_open_pair (GtkTextIter *iter)
{
  GtkTextIter copy;
  GtkSourceBuffer *buffer;

  buffer = GTK_SOURCE_BUFFER (gtk_text_iter_get_buffer (iter));
  copy = *iter;

  do
    {
      GtkTextIter match_start;
      GtkTextIter match_end;
      gunichar ch;

      if (gtk_source_buffer_iter_has_context_class (buffer, &copy, "comment") ||
          gtk_source_buffer_iter_has_context_class (buffer, &copy, "string"))
        continue;

      ch = gtk_text_iter_get_char (&copy);

      switch (ch)
        {
        case '=':
          return FALSE;

        case '{':
        case '(':
        case '[':
          *iter = copy;
          return TRUE;

        case ')':
          if (!gtk_text_iter_backward_search (&copy, "(",
                                              GTK_TEXT_SEARCH_TEXT_ONLY,
                                              &match_start, &match_end,
                                              NULL))
            return FALSE;
          copy = match_start;
          break;

        case ']':
          if (!gtk_text_iter_backward_search (&copy, "[",
                                              GTK_TEXT_SEARCH_TEXT_ONLY,
                                              &match_start, &match_end,
                                              NULL))
            return FALSE;
          copy = match_start;
          break;

        case '}':
          if (!gtk_text_iter_backward_search (&copy, "{",
                                              GTK_TEXT_SEARCH_TEXT_ONLY,
                                              &match_start, &match_end,
                                              NULL))
            return FALSE;
          copy = match_start;
          break;

        case '\'':
          if (!gtk_text_iter_backward_search (&copy, "'",
                                              GTK_TEXT_SEARCH_TEXT_ONLY,
                                              &match_start, &match_end,
                                              NULL))
            return FALSE;
          copy = match_start;
          break;

        case '"':
          if (!gtk_text_iter_backward_search (&copy, "\"",
                                              GTK_TEXT_SEARCH_TEXT_ONLY,
                                              &match_start, &match_end,
                                              NULL))
            return FALSE;
          copy = match_start;
          break;

        default:
          break;
        }
    }
  while (gtk_text_iter_backward_char (&copy));

  return FALSE;
}

static gchar *
copy_indent_minus_tab (IdePythonIndenter *python,
                       GtkTextView       *view,
                       GtkTextIter       *begin,
                       GtkTextIter       *end,
                       GtkTextIter       *copy)
{
  GString *str;
  gchar *copied;
  guint tab_width = 4;

  copied = copy_indent (python, begin, end, copy);
  str = g_string_new (copied);
  g_free (copied);

  if (GTK_SOURCE_IS_VIEW (view))
    tab_width = gtk_source_view_get_tab_width (GTK_SOURCE_VIEW (view));

  if (tab_width <= str->len)
    g_string_truncate (str, str->len - tab_width);

  return g_string_free (str, FALSE);
}

static gboolean
find_bracket (gunichar ch,
              gpointer state)
{
  gint *count = state;

  switch (ch)
    {
    case '[':
      (*count)--;
      break;

    case ']':
      (*count)++;
      break;

    default:
      break;
    }

  return (*count) == 0;
}

static gboolean
find_paren (gunichar ch,
            gpointer state)
{
  gint *count = state;

  switch (ch)
    {
    case '(':
      (*count)--;
      break;

    case ')':
      (*count)++;
      break;

    default:
      break;
    }

  return (*count) == 0;
}

static gchar *
indent_colon (IdePythonIndenter *python,
              GtkTextView       *view,
              GtkTextIter       *begin,
              GtkTextIter       *end,
              GtkTextIter       *iter)
{
  GString *str;
  gboolean is_colon;
  guint tab_width = 4;
  guint offset;
  guint i;

  /*
   * TODO: Assign tab width from source view.
   */

  is_colon = gtk_text_iter_get_char (iter) == ':';

  /*
   * Work our way back to the first character of the first line. Jumping past
   * strings and parens.
   */
  while (gtk_text_iter_backward_char (iter))
    {
      GtkTextIter match_begin;
      GtkTextIter match_end;
      gunichar ch;
      gint count;

      if (gtk_text_iter_get_line_offset (iter) == 0)
        break;

      ch = gtk_text_iter_get_char (iter);

      switch (ch)
        {
        case ']':
          count = 1;
          if (!gtk_text_iter_backward_find_char (iter, find_bracket, &count,
                                                 NULL))
            return NULL;
          break;

        case ')':
          count = 1;
          if (!gtk_text_iter_backward_find_char (iter, find_paren, &count,
                                                 NULL))
            return NULL;
          break;

        case '\'':
          if (!gtk_text_iter_backward_search (iter, "'",
                                              GTK_TEXT_SEARCH_TEXT_ONLY,
                                              &match_begin, &match_end, NULL))
            return NULL;
          *iter = match_begin;
          break;

        case '"':
          if (!gtk_text_iter_backward_search (iter, "\"",
                                              GTK_TEXT_SEARCH_TEXT_ONLY,
                                              &match_begin, &match_end, NULL))
            return NULL;
          *iter = match_begin;
          break;

        default:
          break;
        }
    }

  /*
   * Now work forward to the first non-whitespace char on this line.
   */
  while (!gtk_text_iter_ends_line (iter) &&
         g_unichar_isspace (gtk_text_iter_get_char (iter)))
    gtk_text_iter_forward_char (iter);

  offset = gtk_text_iter_get_line_offset (iter);

  /*
   * If we are actually still in the parameter list, possibly indent more.
   * I don't like that this code is here, it really belongs somewhere else.
   */
  if (!is_colon) {
    GtkTextIter copy = *iter;

    if (gtk_text_iter_forward_chars (&copy, 4))
      {
        gchar *slice;

        slice = gtk_text_iter_get_slice (iter, &copy);
        if (g_strcmp0 (slice, "def ") == 0)
          offset += tab_width;
        g_free (slice);
      }
  }

  str = g_string_new (NULL);
  for (i = 0; i < (offset + tab_width); i++)
    g_string_append (str, " ");
  return g_string_free (str, FALSE);
}

static gchar *
indent_parens (IdePythonIndenter *python,
               GtkTextView       *view,
               GtkTextIter       *begin,
               GtkTextIter       *end,
               GtkTextIter       *iter)
{
  GtkTextIter copy;
  GString *str;
  gint count = 1;

  copy = *iter;

  /* if we come across an opening paren on this line, we will move 1 space
   * past it. otherwise, just copy the previous line's indentation.
   */
  if (gtk_text_iter_backward_find_char (iter, find_paren, &count, NULL) &&
      (gtk_text_iter_get_line (iter) == gtk_text_iter_get_line (&copy)))
    {
      guint offset;

      offset = gtk_text_iter_get_line_offset (iter);

      str = g_string_new (NULL);
      for (guint i = 0; i <= offset; i++)
        g_string_append (str, " ");
      return g_string_free (str, FALSE);
    }

  str = g_string_new (NULL);

  gtk_text_iter_set_line_offset (&copy, 0);

  while (g_unichar_isspace (gtk_text_iter_get_char (&copy)))
    {
      g_string_append (str, " ");
      if (!gtk_text_iter_forward_char (&copy))
        break;
    }

  return g_string_free (str, FALSE);
}

static gchar *
indent_previous_stmt (IdePythonIndenter *python,
                      GtkTextView       *text_view,
                      GtkTextIter       *begin,
                      GtkTextIter       *end,
                      GtkTextIter       *iter)
{
  gint count = 1;

  if (gtk_text_iter_backward_find_char (iter, find_paren, &count, NULL))
    {
      GString *str;
      guint offset;
      guint i;

      gtk_text_iter_set_line_offset (iter, 0);

      /*
       * TODO:
       *
       * If the previous line ended in backslash (\), then we need to keep
       * walking backwards. We also need to handle statements like:
       */

      while (g_unichar_isspace (gtk_text_iter_get_char (iter)))
        if (!gtk_text_iter_forward_char (iter))
          break;

      offset = gtk_text_iter_get_line_offset (iter);

      str = g_string_new (NULL);
      for (i = 0; i < offset; i++)
        g_string_append (str, " ");
      return g_string_free (str, FALSE);
    }

  return NULL;
}

static gchar *
indent_for_pair (IdePythonIndenter *python,
                 GtkTextView       *text_view,
                 GtkTextIter       *begin,
                 GtkTextIter       *end,
                 GtkTextIter       *iter,
                 gint              *cursor_offset)
{
  GtkTextIter copy = *iter;
  gunichar ch;
  gunichar prev_ch;
  guint tab_width = 4; /* TODO */

  prev_ch = gtk_text_iter_get_char (&copy);
  gtk_text_iter_forward_char (&copy);
  gtk_text_iter_forward_char (&copy);
  ch = gtk_text_iter_get_char (&copy);

  copy = *iter;

  if ((prev_ch == '{' && ch == '}') ||
      (prev_ch == '[' && ch == ']') ||
      (prev_ch == '(' && ch == ')'))
    {
      gchar *copied;
      GString *str;
      guint i;

      copied = copy_indent (python, begin, end, &copy);
      str = g_string_new (copied);

      for (i = 0; i < tab_width; i++)
        g_string_append (str, " ");
      g_string_append (str, "\n");
      g_string_append (str, copied);
      *cursor_offset = -strlen (copied) - 1;
      g_free (copied);

      return g_string_free (str, FALSE);
    }
  else
    return indent_colon (python, text_view, begin, end, iter);
}

static gboolean
move_first_nonspace_char (GtkTextIter *iter)
{
  g_assert (iter != NULL);

  gtk_text_iter_set_line_offset (iter, 0);

  while (TRUE)
    {
      gunichar ch;

      ch = gtk_text_iter_get_char (iter);
      if (!g_unichar_isspace (ch))
        break;

      if (gtk_text_iter_ends_line (iter))
        break;

      if (!gtk_text_iter_forward_char (iter))
        break;
    }

  return g_unichar_isspace (gtk_text_iter_get_char (iter));
}

static gboolean
move_to_visual_column (GtkSourceView *sv,
                       GtkTextIter   *iter,
                       guint          line_offset)
{
  gtk_text_iter_set_line_offset (iter, 0);

  while (line_offset > gtk_source_view_get_visual_column (sv, iter))
    {
      if (gtk_text_iter_ends_line (iter))
        break;
      gtk_text_iter_forward_char (iter);
    }

  return TRUE;
}

static gboolean
move_previous_line (GtkSourceView *sv,
                    GtkTextIter   *iter,
                    guint          line_offset)
{
  guint line;

  line = gtk_text_iter_get_line (iter);
  if (line == 0)
    return FALSE;

  gtk_text_iter_set_line (iter, line - 1);

  return move_to_visual_column (sv, iter, line_offset);
}

static gchar *
maybe_unindent_else_or_elif (IdePythonIndenter *python,
                             GtkTextView       *tv,
                             GtkTextIter       *begin,
                             GtkTextIter       *end)
{
  GtkTextIter copy = *begin;
  GtkSourceView *sv;
  gboolean matches;
  gchar *slice;

  IDE_ENTRY;

  sv = GTK_SOURCE_VIEW (tv);

  gtk_text_iter_backward_chars (&copy, 4);
  slice = gtk_text_iter_get_slice (&copy, begin);
  matches = g_str_equal (slice, "else") || g_str_equal (slice, "elif");

  /* only continue if this is the first word on the line */
  if (matches)
    {
      guint line_offset;

      line_offset = gtk_text_iter_get_line_offset (&copy);
      move_first_nonspace_char (&copy);
      if (line_offset != (guint)gtk_text_iter_get_line_offset (&copy))
        IDE_GOTO (failure);
    }

  if (matches)
    {
      guint line_offset;

      line_offset = gtk_source_view_get_visual_column (sv, &copy);

      while (TRUE)
        {
          if (!move_previous_line (sv, &copy, line_offset))
            IDE_GOTO (failure);

          move_first_nonspace_char (&copy);

          if (gtk_source_view_get_visual_column (sv, &copy) > line_offset)
            continue;

          if (line_starts_with (&copy, "if ") || line_starts_with (&copy, "for "))
            break;
        }

      move_first_nonspace_char (&copy);

      if ((line_starts_with (&copy, "if ") || line_starts_with (&copy, "for ")))
        {
          line_offset = gtk_source_view_get_visual_column (sv, &copy);
          move_to_visual_column (sv, begin, line_offset);
          IDE_RETURN (slice);
        }
    }

failure:
  g_free (slice);

  IDE_RETURN (NULL);
}

static gchar *
ide_python_indenter_format (IdeIndenter *indenter,
                            GtkTextView *text_view,
                            GtkTextIter *begin,
                            GtkTextIter *end,
                            gint        *cursor_offset,
                            GdkEventKey *event)
{
  IdePythonIndenter *python = (IdePythonIndenter *)indenter;
  GtkTextIter iter = *begin;
  gunichar ch;
  gint line;

  /* Figure out the inserted character */
  gtk_text_iter_backward_char (&iter);
  ch = gtk_text_iter_get_char (&iter);

  /*
   * If we triggered from 'e' or 'f', but that is not what was inserted,
   * short-circuit now. (Such as from é compose keys, etc).
   */
  if (event->keyval == GDK_KEY_e || event->keyval == GDK_KEY_f)
    {
      if (ch != 'e' && ch != 'f')
        return NULL;

      /*
       * Possibly trying to adjust "else" or "elif". we always return in this
       * block, since we don't want to process anything else.
       */
      return maybe_unindent_else_or_elif (python, text_view, begin, end);
    }

  iter = *begin;
  line = gtk_text_iter_get_line (&iter);

  /* move to the last character of the last line */
  if (!gtk_text_iter_backward_char (&iter) ||
      !gtk_text_iter_backward_char (&iter))
    return NULL;

  /* if the previous line was empty, don't do any indenting. */
  if ((line - gtk_text_iter_get_line (&iter)) > 1)
    return NULL;

  /* get the last character */
  ch = gtk_text_iter_get_char (&iter);

  if (in_pydoc (&iter))
    return copy_indent (python, begin, end, &iter);

  switch (ch)
    {
    case ':':
      return indent_colon (python, text_view, begin, end, &iter);

    case '(':
    case '[':
    case '{':
      return indent_for_pair (python, text_view, begin, end, &iter,
                              cursor_offset);

    case ')':
      return indent_previous_stmt (python, text_view, begin, end, &iter);

    case ',':
      return indent_parens (python, text_view, begin, end, &iter);

    default:
      if (g_unichar_isspace (gtk_text_iter_get_char (&iter)))
        return copy_indent (python, begin, end, &iter);

      if (line_starts_with (&iter, "return") ||
          line_starts_with (&iter, "break") ||
          line_starts_with (&iter, "continue") ||
          line_starts_with (&iter, "pass"))
        return copy_indent_minus_tab (python, text_view, begin, end, &iter);

      if (backtrack_to_open_pair (&iter))
        {
          GString *str;
          guint offset;
          guint i;

          offset = gtk_text_iter_get_line_offset (&iter);
          str = g_string_new (NULL);

          for (i = 0; i <= offset; i++)
            g_string_append (str, " ");

          return g_string_free (str, FALSE);
        }

      if (ch == ')' || ch == ']' || ch == '}')
        {
          GtkTextIter copy;

          copy = iter;

          gtk_text_iter_backward_char (&copy);

          if (backtrack_to_open_pair (&copy))
            {
              gtk_text_iter_set_line_offset (&copy, 0);
              while (g_unichar_isspace (gtk_text_iter_get_char (&copy)) &&
                     !gtk_text_iter_ends_line (&copy) &&
                     gtk_text_iter_forward_char (&copy))
                {
                  /* Do nothing */
                }

              return copy_indent (python, begin, end, &copy);
            }
        }

      return copy_indent (python, begin, end, &iter);
    }

  return NULL;
}

static gboolean
ide_python_indenter_is_trigger (IdeIndenter *indenter,
                                GdkEventKey *event)
{
  GdkDisplay *display = gdk_window_get_display (event->window);
  GdkKeymap *keymap = gdk_keymap_get_for_display (display);
  guint modifier_state;

  modifier_state = gdk_keymap_get_modifier_state (keymap);
  if (modifier_state & GDK_SHIFT_MASK ||
      modifier_state & GDK_CONTROL_MASK ||
      modifier_state & GDK_MOD1_MASK)
    return FALSE;

  switch (event->keyval)
    {
    case GDK_KEY_e:
    case GDK_KEY_f:
    case GDK_KEY_KP_Enter:
    case GDK_KEY_Return:
      return TRUE;

    default:
      return FALSE;
    }
}

static void
indenter_iface_init (IdeIndenterInterface *iface)
{
  iface->is_trigger = ide_python_indenter_is_trigger;
  iface->format = ide_python_indenter_format;
}

static void
ide_python_indenter_class_init (IdePythonIndenterClass *klass)
{
}

static void
ide_python_indenter_init (IdePythonIndenter *self)
{
}
