/* ide-xml-highlighter.c
 *
 * Copyright 2015 Dimitris Zenios <dimitris.zenios@gmail.com>
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

#define G_LOG_DOMAIN "ide-xml-highlighter"

#include "config.h"

#include <glib/gi18n.h>

#include "ide-xml.h"
#include "ide-xml-highlighter.h"

#define HIGHLIGH_TIMEOUT_MSEC    35
#define XML_TAG_MATCH_STYLE_NAME "xml:tag-match"

struct _IdeXmlHighlighter
{
  IdeObject           parent_instance;
  IdeHighlightEngine *engine;
  GSignalGroup       *buffer_signals;
  guint               highlight_timeout;
  guint               has_tags : 1;
};

static void highlighter_iface_init (IdeHighlighterInterface *iface);

G_DEFINE_FINAL_TYPE_WITH_CODE (IdeXmlHighlighter,
                               ide_xml_highlighter,
                               IDE_TYPE_OBJECT,
                               G_IMPLEMENT_INTERFACE (IDE_TYPE_HIGHLIGHTER, highlighter_iface_init))

static gboolean
ide_xml_highlighter_highlight_timeout_handler (gpointer data)
{
  IdeXmlHighlighter *self = data;
  GtkTextBuffer *buffer;
  GtkTextMark *insert;
  GtkTextTag *tag;
  GtkTextIter iter;
  GtkTextIter start;
  GtkTextIter end;

  g_assert (IDE_IS_XML_HIGHLIGHTER (self));

  if (self->engine == NULL)
    goto cleanup;

  buffer = GTK_TEXT_BUFFER (ide_highlight_engine_get_buffer (self->engine));
  insert = gtk_text_buffer_get_insert (buffer);
  tag = ide_highlight_engine_get_style (self->engine, XML_TAG_MATCH_STYLE_NAME);

  /*
   * Clear previous tags. We could save the previous
   * iters and clear only those locations but for
   * now this should be ok.
   */
  if (self->has_tags)
    {
      gtk_text_buffer_get_bounds (buffer, &start, &end);
      gtk_text_buffer_remove_tag (buffer, tag, &start, &end);

      self->has_tags = FALSE;
    }

  gtk_text_buffer_get_iter_at_mark (buffer, &iter, insert);

  if (ide_xml_in_element (&iter) && ide_xml_get_current_element (&iter, &start, &end))
    {
      IdeXmlElementTagType tag_type = ide_xml_get_element_tag_type (&start, &end);
      GtkTextIter next_start;
      GtkTextIter next_end;

      if ((tag_type == IDE_XML_ELEMENT_TAG_START &&
          ide_xml_find_closing_element (&start, &end, &next_start, &next_end)) ||
          (tag_type == IDE_XML_ELEMENT_TAG_END &&
          ide_xml_find_opening_element (&start, &end, &next_start, &next_end)) ||
          tag_type == IDE_XML_ELEMENT_TAG_START_END)
        {

          /*
           * All iters point to the begining of < char and the beginning of >
           * char. In our case we want to highlight everything that is between
           * those two chars.This is the reason we move one char forward from
           * the start iter.
           */
          gtk_text_iter_forward_char (&start);
          gtk_text_buffer_apply_tag (buffer, tag, &start, &end);

          if (tag_type != IDE_XML_ELEMENT_TAG_START_END)
            {
              gtk_text_iter_forward_char (&next_start);
              gtk_text_buffer_apply_tag (buffer, tag, &next_start, &next_end);
            }

          self->has_tags = TRUE;
        }
    }

cleanup:
  self->highlight_timeout = 0;

  return G_SOURCE_REMOVE;
}

static void
ide_xml_highlighter_cursor_moved (IdeXmlHighlighter *self,
                                  IdeBuffer         *buffer)
{
  g_assert (IDE_IS_XML_HIGHLIGHTER (self));
  g_assert (IDE_IS_BUFFER (buffer));

  g_clear_handle_id (&self->highlight_timeout, g_source_remove);

  self->highlight_timeout =
    g_timeout_add_full (G_PRIORITY_LOW,
                        HIGHLIGH_TIMEOUT_MSEC,
                        ide_xml_highlighter_highlight_timeout_handler,
                        g_object_ref (self),
                        g_object_unref);
}

static void
ide_xml_highlighter_destroy (IdeObject *object)
{
  IdeXmlHighlighter *self = (IdeXmlHighlighter *)object;

  g_clear_handle_id (&self->highlight_timeout, g_source_remove);
  g_clear_object (&self->buffer_signals);

  IDE_OBJECT_CLASS (ide_xml_highlighter_parent_class)->destroy (object);
}

static void
ide_xml_highlighter_class_init (IdeXmlHighlighterClass *klass)
{
  IdeObjectClass *i_object_class = IDE_OBJECT_CLASS (klass);

  i_object_class->destroy = ide_xml_highlighter_destroy;
}

static void
ide_xml_highlighter_init (IdeXmlHighlighter *self)
{
  self->buffer_signals = g_signal_group_new (IDE_TYPE_BUFFER);

  g_signal_group_connect_object (self->buffer_signals,
                                   "cursor-moved",
                                   G_CALLBACK (ide_xml_highlighter_cursor_moved),
                                   self,
                                   G_CONNECT_SWAPPED);
}

static void
ide_xml_highlighter_real_set_engine (IdeHighlighter     *highlighter,
                                     IdeHighlightEngine *engine)
{
  IdeXmlHighlighter *self = (IdeXmlHighlighter *)highlighter;
  IdeBuffer *buffer = NULL;

  g_assert (IDE_IS_XML_HIGHLIGHTER (self));
  g_assert (!engine || IDE_IS_HIGHLIGHT_ENGINE (engine));

  self->engine = engine;

  if (engine != NULL)
    {
      buffer = ide_highlight_engine_get_buffer (engine);
      ide_xml_highlighter_cursor_moved (self, buffer);
    }

  g_signal_group_set_target (self->buffer_signals, buffer);
}

static void
ide_xml_highlighter_real_update (IdeHighlighter       *highlighter,
                                 const GSList         *tags_to_remove,
                                 IdeHighlightCallback  callback,
                                 const GtkTextIter    *range_begin,
                                 const GtkTextIter    *range_end,
                                 GtkTextIter          *location)
{
  g_assert (IDE_IS_XML_HIGHLIGHTER (highlighter));
  g_assert (range_begin != NULL);
  g_assert (range_end != NULL);
  g_assert (location != NULL);

  /* We don't do any immediate processing. So let the engine thing
   * we are done immediately. Instead, we'll introduce a delay to
   * update the matching element.
   */
  *location = *range_end;
}

static void
highlighter_iface_init (IdeHighlighterInterface *iface)
{
  iface->set_engine = ide_xml_highlighter_real_set_engine;
  iface->update = ide_xml_highlighter_real_update;
}
