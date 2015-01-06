/* gb-source-auto-indenter-xml.c
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

#define G_LOG_DOMAIN "indent-xml"

#include <string.h>

#include "gb-log.h"
#include "gb-source-auto-indenter-xml.h"

/*
 * TODO:
 *
 * This is very naive. But let's see if it gets the job done enough to not
 * be super annoying. Things like indent_width belong as fields in a private
 * structure.
 *
 * Anywho, if you want to own this module, go for it.
 */

#define INDENT_WIDTH 2

G_DEFINE_TYPE (GbSourceAutoIndenterXml,
               gb_source_auto_indenter_xml,
               GB_TYPE_SOURCE_AUTO_INDENTER)

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

static void
build_indent (GbSourceAutoIndenterXml *xml,
              guint                    line_offset,
              GtkTextIter             *matching_line,
              GString                 *str)
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

GbSourceAutoIndenter *
gb_source_auto_indenter_xml_new (void)
{
  return g_object_new (GB_TYPE_SOURCE_AUTO_INDENTER_XML, NULL);
}

static gboolean
text_iter_in_cdata (const GtkTextIter *location)
{
  GtkTextIter iter = *location;
  gboolean ret = FALSE;

  if (gtk_text_iter_backward_search (&iter, "<![CDATA[",
                                     GTK_TEXT_SEARCH_TEXT_ONLY,
                                     NULL, &iter, NULL))
    {
      if (!gtk_text_iter_forward_search (&iter, "]]>",
                                         GTK_TEXT_SEARCH_TEXT_ONLY,
                                         NULL, NULL, location))
        {
          ret = TRUE;
          GOTO (cleanup);
        }
    }

cleanup:
  return ret;
}

static gboolean
text_iter_backward_to_element_start (const GtkTextIter *iter,
                                     GtkTextIter       *match_begin)
{
  GtkTextIter tmp = *iter;
  gboolean ret = FALSE;
  gint depth = 0;

  g_return_val_if_fail (iter, FALSE);
  g_return_val_if_fail (match_begin, FALSE);

  while (gtk_text_iter_backward_char (&tmp))
    {
      gunichar ch;

      ch = gtk_text_iter_get_char (&tmp);

      if ((ch == '/') && (text_iter_peek_prev_char (&tmp) == '<'))
        {
          gtk_text_iter_backward_char (&tmp);
          depth++;
        }
      else if ((ch == '/') && (text_iter_peek_next_char (&tmp) == '>'))
        {
          depth++;
        }
      else if ((ch == '<') && (text_iter_peek_next_char (&tmp) != '!'))
        {
          depth--;
          if (depth < 0)
            {
              *match_begin = tmp;
              ret = TRUE;
              GOTO (cleanup);
            }
        }
    }

cleanup:
  return ret;
}

static gchar *
gb_source_auto_indenter_xml_indent (GbSourceAutoIndenterXml *xml,
                                    GtkTextIter             *begin,
                                    GtkTextIter             *end,
                                    gint                    *cursor_offset,
                                    guint                    tab_width)
{
  GtkTextIter match_begin;
  GString *str;
  guint offset;

  g_return_val_if_fail (GB_IS_SOURCE_AUTO_INDENTER_XML (xml), NULL);
  g_return_val_if_fail (begin, NULL);
  g_return_val_if_fail (end, NULL);

  str = g_string_new (NULL);

  if (text_iter_backward_to_element_start (begin, &match_begin))
    {
      offset = gtk_text_iter_get_line_offset (&match_begin);
      build_indent (xml, offset + INDENT_WIDTH, &match_begin, str);
      GOTO (cleanup);
    }

  /* do nothing */

cleanup:
  return g_string_free (str, (str->len == 0));
}

static gchar *
gb_source_auto_indenter_xml_maybe_unindent (GbSourceAutoIndenterXml *xml,
                                            GtkTextIter             *begin,
                                            GtkTextIter             *end)
{
  GtkTextIter tmp;
  gunichar ch;

  g_return_val_if_fail (GB_IS_SOURCE_AUTO_INDENTER_XML (xml), NULL);
  g_return_val_if_fail (begin, NULL);
  g_return_val_if_fail (end, NULL);

  tmp = *begin;

  if (!gtk_text_iter_backward_char (&tmp))
    return NULL;

  if (('/' == gtk_text_iter_get_char (&tmp)) &&
      gtk_text_iter_backward_char (&tmp) &&
      ('<' == gtk_text_iter_get_char (&tmp)) &&
      (ch = text_iter_peek_prev_char (&tmp)) &&
      ((ch == ' ') || (ch == '\t')))
    {
      if (ch == '\t')
        {
          gtk_text_iter_backward_char (&tmp);
          *begin = tmp;
          return g_strdup ("</");
        }
      else
        {
          gint count = INDENT_WIDTH;

          while (count)
            {
              if (!gtk_text_iter_backward_char (&tmp) ||
                  !(ch = gtk_text_iter_get_char (&tmp)) ||
                  (ch != ' '))
                return NULL;
              count--;
              if (count == 0)
                GOTO (success);
            }
        }
    }

  return NULL;

success:
  *begin = tmp;
  return g_strdup ("</");
}

static gboolean
find_end (gunichar ch,
          gpointer user_data)
{
  return (ch == '>' || g_unichar_isspace (ch));
}

static gchar *
gb_source_auto_indenter_xml_maybe_add_closing (GbSourceAutoIndenterXml *xml,
                                               GtkTextIter             *begin,
                                               GtkTextIter             *end,
                                               gint                    *cursor_offset)
{
  GtkTextIter match_begin;
  GtkTextIter match_end;
  GtkTextIter copy;

  g_return_val_if_fail (GB_IS_SOURCE_AUTO_INDENTER_XML (xml), NULL);
  g_return_val_if_fail (begin, NULL);
  g_return_val_if_fail (end, NULL);

  copy = *begin;

  gtk_text_iter_backward_char (&copy);
  gtk_text_iter_backward_char (&copy);

  if (gtk_text_iter_get_char (&copy) == '/')
    return NULL;

  copy = *begin;

  if (gtk_text_iter_backward_search (&copy, "<", GTK_TEXT_SEARCH_TEXT_ONLY,
                                     &match_begin, &match_end, NULL))
    {
      gtk_text_iter_forward_char (&match_begin);
      if (gtk_text_iter_get_char (&match_begin) == '/')
        return NULL;

      match_end = match_begin;

      if (gtk_text_iter_forward_find_char (&match_end, find_end, NULL, begin))
        {
          gchar *slice;
          gchar *ret;

          slice = gtk_text_iter_get_slice (&match_begin, &match_end);
          ret = g_strdup_printf ("</%s>", slice);
          *cursor_offset = -strlen (ret);

          g_free (slice);

          return ret;
        }
    }

  return NULL;
}

static gboolean
gb_source_auto_indenter_xml_is_trigger (GbSourceAutoIndenter *indenter,
                                        GdkEventKey          *event)
{
  switch (event->keyval)
    {
    case GDK_KEY_Return:
    case GDK_KEY_KP_Enter:
    case GDK_KEY_slash:
    case GDK_KEY_greater:
      return TRUE;

    default:
      break;
    }

  return FALSE;
}

static gchar *
gb_source_auto_indenter_xml_format (GbSourceAutoIndenter *indenter,
                                    GtkTextView          *view,
                                    GtkTextBuffer        *buffer,
                                    GtkTextIter          *begin,
                                    GtkTextIter          *end,
                                    gint                 *cursor_offset,
                                    GdkEventKey          *trigger)
{
  GbSourceAutoIndenterXml *xml = (GbSourceAutoIndenterXml *)indenter;
  guint tab_width = 2;

  g_return_val_if_fail (GB_IS_SOURCE_AUTO_INDENTER_XML (xml), NULL);

  *cursor_offset = 0;

  if (GTK_SOURCE_IS_VIEW (view))
    tab_width = gtk_source_view_get_tab_width (GTK_SOURCE_VIEW (view));

  /* do nothing if we are in a cdata section */
  if (text_iter_in_cdata (begin))
    return NULL;

  switch (trigger->keyval)
    {
    case GDK_KEY_Return:
    case GDK_KEY_KP_Enter:
      return gb_source_auto_indenter_xml_indent (xml, begin, end, cursor_offset,
                                                 tab_width);

    case GDK_KEY_slash:
      return gb_source_auto_indenter_xml_maybe_unindent (xml, begin, end);

    case GDK_KEY_greater:
      return gb_source_auto_indenter_xml_maybe_add_closing (xml, begin, end,
                                                            cursor_offset);

    default:
      g_return_val_if_reached (NULL);
    }

  return NULL;
}

static void
gb_source_auto_indenter_xml_class_init (GbSourceAutoIndenterXmlClass *klass)
{
  GbSourceAutoIndenterClass *parent = GB_SOURCE_AUTO_INDENTER_CLASS (klass);

  parent->format = gb_source_auto_indenter_xml_format;
  parent->is_trigger = gb_source_auto_indenter_xml_is_trigger;
}

static void
gb_source_auto_indenter_xml_init (GbSourceAutoIndenterXml *self)
{
}
