/* gb-source-auto-indenter-python.c
 *
 * Copyright (C) 2014 Christian Hergert <christian@hergert.me>
 *
 * This file is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as
 * published by the Free Software Foundation; either version 3 of the
 * License, or (at your option) any later version.
 *
 * This file is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#define G_LOG_DOMAIN "python-indent"

#include "gb-log.h"
#include "gb-source-auto-indenter-python.h"

/*
 * TODO:
 *
 * This module is very naive. It only checks for a line ending in : to
 * indent. If you'd like to own this, go for it!
 *
 * I suggest adding something like auto indentation to a matching (
 * for the case when () is used to extend to the next line.
 */

G_DEFINE_TYPE (GbSourceAutoIndenterPython,
               gb_source_auto_indenter_python,
               GB_TYPE_SOURCE_AUTO_INDENTER)

GbSourceAutoIndenter *
gb_source_auto_indenter_python_new (void)
{
  return g_object_new (GB_TYPE_SOURCE_AUTO_INDENTER_PYTHON, NULL);
}

static gboolean
in_pydoc (const GtkTextIter *iter)
{
  /* TODO: implement this */
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

static gchar *
copy_indent (GbSourceAutoIndenterPython *python,
             GtkTextIter                *begin,
             GtkTextIter                *end,
             GtkTextIter                *copy)
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

static gchar *
copy_indent_minus_tab (GbSourceAutoIndenterPython *python,
                       GtkTextView                *view,
                       GtkTextIter                *begin,
                       GtkTextIter                *end,
                       GtkTextIter                *copy)
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
indent_colon (GbSourceAutoIndenterPython *python,
              GtkTextView                *view,
              GtkTextIter                *begin,
              GtkTextIter                *end,
              GtkTextIter                *iter)
{
  GString *str;
  guint tab_width = 4;
  guint offset;
  guint i;

  /*
   * TODO: Assign tab width from source view.
   */

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

  str = g_string_new (NULL);
  for (i = 0; i < (offset + tab_width); i++)
    g_string_append (str, " ");
  return g_string_free (str, FALSE);
}

static gchar *
indent_parens (GbSourceAutoIndenterPython *python,
               GtkTextView                *view,
               GtkTextIter                *begin,
               GtkTextIter                *end,
               GtkTextIter                *iter)
{
  gint count = 1;

  if (gtk_text_iter_backward_find_char (iter, find_paren, &count, NULL))
    {
      GString *str;
      guint offset;
      gint i;

      offset = gtk_text_iter_get_line_offset (iter);

      str = g_string_new (NULL);
      for (i = 0; i <= offset; i++)
        g_string_append (str, " ");
      return g_string_free (str, FALSE);
    }

  return NULL;
}

static gchar *
indent_previous_stmt (GbSourceAutoIndenterPython *python,
                      GtkTextView                *text_view,
                      GtkTextIter                *begin,
                      GtkTextIter                *end,
                      GtkTextIter                *iter)
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
gb_source_auto_indenter_python_format (GbSourceAutoIndenter *indenter,
                                       GtkTextView          *text_view,
                                       GtkTextBuffer        *buffer,
                                       GtkTextIter          *begin,
                                       GtkTextIter          *end,
                                       gint                 *cursor_offset,
                                       GdkEventKey          *event)
{
  GbSourceAutoIndenterPython *python = (GbSourceAutoIndenterPython *)indenter;
  GtkTextIter iter = *begin;
  gunichar ch;

  /* move to the last character of the last line */
  if (!gtk_text_iter_backward_char (&iter) ||
      !gtk_text_iter_backward_char (&iter))
    return NULL;

  /* get the last character */
  ch = gtk_text_iter_get_char (&iter);

  switch (ch)
    {
    case ':':
    case '(':
      return indent_colon (python, text_view, begin, end, &iter);

    case ')':
      return indent_previous_stmt (python, text_view, begin, end, &iter);

    case ',':
      return indent_parens (python, text_view, begin, end, &iter);

    default:
      if (in_pydoc (&iter))
        return copy_indent (python, begin, end, &iter);

      if (line_starts_with (&iter, "return") ||
          line_starts_with (&iter, "break") ||
          line_starts_with (&iter, "continue") ||
          line_starts_with (&iter, "pass"))
        return copy_indent_minus_tab (python, text_view, begin, end, &iter);

      return copy_indent (python, begin, end, &iter);
    }

  return NULL;
}

static gboolean
gb_source_auto_indenter_python_is_trigger (GbSourceAutoIndenter *indenter,
                                           GdkEventKey          *event)
{
  switch (event->keyval)
    {
    case GDK_KEY_KP_Enter:
    case GDK_KEY_Return:
      return TRUE;

    default:
      return FALSE;
    }
}

static void
gb_source_auto_indenter_python_class_init (GbSourceAutoIndenterPythonClass *klass)
{
  GbSourceAutoIndenterClass *indent_class = GB_SOURCE_AUTO_INDENTER_CLASS (klass);

  indent_class->is_trigger = gb_source_auto_indenter_python_is_trigger;
  indent_class->format = gb_source_auto_indenter_python_format;
}

static void
gb_source_auto_indenter_python_init (GbSourceAutoIndenterPython *self)
{
}
