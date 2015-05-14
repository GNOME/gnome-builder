/* ide-xml-highlighter.c
 *
 * Copyright (C) 2015 Dimitris Zenios <dimitris.zenios@gmail.com>
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

#include "egg-signal-group.h"

#include "ide-xml-highlighter.h"
#include "ide-context.h"
#include "ide-buffer.h"
#include "ide-xml.h"
#include "ide-highlight-engine.h"

#define HIGHLIGH_TIMEOUT_MSEC    35
#define XML_TAG_MATCH_STYLE_NAME "xml:tag-match"

struct _IdeXmlHighlighter
{
  IdeHighlighter  parent_instance;

  EggSignalGroup *signal_group;
  GtkTextIter     iter;
  guint           highlight_timeout;
  guint           has_tags : 1;
};

G_DEFINE_TYPE (IdeXmlHighlighter, ide_xml_highlighter, IDE_TYPE_HIGHLIGHTER)

static gboolean
ide_xml_highlighter_highlight_timeout_handler (gpointer data)
{
  IdeXmlHighlighter *self = data;
  IdeHighlightEngine *engine;
  GtkTextTag *tag;
  GtkTextIter start;
  GtkTextIter end;
  GtkTextBuffer *buffer;

  g_assert (IDE_IS_XML_HIGHLIGHTER (self));

  /*
   * If we lost our buffer handle, nothing we can do!
   */
  buffer = egg_signal_group_get_target (self->signal_group);
  if (!GTK_IS_TEXT_BUFFER (buffer))
    goto finished;

  engine = ide_highlighter_get_highlight_engine (IDE_HIGHLIGHTER (self));
  tag = ide_highlight_engine_get_style (engine, XML_TAG_MATCH_STYLE_NAME);

  /*
   * Clear previous tags. We could save the previous
   * iters and clear only those locations but for
   * now this should be ok.
   */
  if (self->has_tags)
    {
      gtk_text_buffer_get_start_iter (buffer, &start);
      gtk_text_buffer_get_end_iter (buffer, &end);
      gtk_text_buffer_remove_tag (buffer,tag,&start,&end);

      self->has_tags = FALSE;
    }


  if (ide_xml_in_element (&self->iter) && ide_xml_get_current_element (&self->iter,
                                                                       &start,
                                                                       &end))
    {
      GtkTextIter next_start;
      GtkTextIter next_end;
      IdeXmlElementTagType tag_type = ide_xml_get_element_tag_type (&start,
                                                                    &end);

      if ((tag_type == IDE_XML_ELEMENT_TAG_START &&
          ide_xml_find_closing_element (&start,&end,
                                      &next_start,&next_end)) ||
          (tag_type == IDE_XML_ELEMENT_TAG_END &&
          ide_xml_find_opening_element (&start,&end,
                                      &next_start,&next_end)) ||
          tag_type == IDE_XML_ELEMENT_TAG_START_END)
        {

          /*
           * All iters point to the begining of < char and the
           * beginning of > char.In our case we want to highlight everything that is
           * between those two chars.This is the reason we move one char forward
           * from the start iter
           */
          gtk_text_iter_forward_char (&start);
          gtk_text_buffer_apply_tag (GTK_TEXT_BUFFER (buffer),
                                     tag,
                                     &start,
                                     &end);

          if (tag_type != IDE_XML_ELEMENT_TAG_START_END)
            {
              gtk_text_iter_forward_char (&next_start);
              gtk_text_buffer_apply_tag (GTK_TEXT_BUFFER (buffer),
                                         tag,
                                         &next_start,
                                         &next_end);
            }

          self->has_tags = TRUE;
        }
    }

finished:
  self->highlight_timeout = 0;
  return G_SOURCE_REMOVE;
}

static void
ide_xml_highlighter_cursor_moved_cb (GtkTextBuffer     *buffer,
                                     GtkTextIter       *iter,
                                     IdeXmlHighlighter *self)
{
  g_assert (IDE_IS_HIGHLIGHTER (self));

  self->iter = *iter;

  if (self->highlight_timeout != 0)
    g_source_remove (self->highlight_timeout);
  self->highlight_timeout = g_timeout_add (HIGHLIGH_TIMEOUT_MSEC,
                                           ide_xml_highlighter_highlight_timeout_handler,
                                           self);
}

static void
ide_xml_highlighter_set_buffer (IdeXmlHighlighter *highlighter,
                                IdeBuffer         *buffer)
{
  IdeXmlHighlighter *self = (IdeXmlHighlighter *)highlighter;

  g_assert (IDE_IS_HIGHLIGHTER (self));
  g_assert (!buffer || IDE_IS_BUFFER (buffer));

  egg_signal_group_set_target (self->signal_group, buffer);
}

static void
ide_xml_highlighter_on_buffer_set (IdeXmlHighlighter  *self,
                                   GParamSpec         *pspec,
                                   IdeHighlightEngine *engine)
{
  IdeBuffer *buffer;

  g_assert (IDE_IS_XML_HIGHLIGHTER (self));
  g_assert (IDE_IS_HIGHLIGHT_ENGINE (engine));

  buffer = ide_highlight_engine_get_buffer (engine);
  ide_xml_highlighter_set_buffer (self, buffer);
}

static void
ide_xml_highlighter_on_highlight_engine_set (IdeHighlighter  *self,
                                             GParamSpec      *pspec,
                                             gpointer        *data)
{
  IdeXmlHighlighter *highlighter = (IdeXmlHighlighter *)self;
  IdeHighlightEngine *engine;
  IdeBuffer *buffer = NULL;

  g_assert (IDE_IS_XML_HIGHLIGHTER (highlighter));

  if ((engine = ide_highlighter_get_highlight_engine (self)))
    {
      buffer = ide_highlight_engine_get_buffer (engine);
      /*
       * TODO: technically we should connect/disconnect when the
       *       highlighter changes. but in practice, it is set only
       *       once.
       */
      g_signal_connect_object (engine,
                               "notify::buffer",
                               G_CALLBACK (ide_xml_highlighter_on_buffer_set),
                               self,
                               G_CONNECT_SWAPPED);
    }

  ide_xml_highlighter_set_buffer (highlighter, buffer);
}

static void
ide_xml_highlighter_engine_dispose (GObject *object)
{
  IdeXmlHighlighter *self = (IdeXmlHighlighter *)object;

  if (self->highlight_timeout != 0)
    {
      g_source_remove (self->highlight_timeout);
      self->highlight_timeout = 0;
    }

  g_clear_object (&self->signal_group);

  G_OBJECT_CLASS (ide_xml_highlighter_parent_class)->dispose (object);
}

static void
ide_xml_highlighter_class_init (IdeXmlHighlighterClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->dispose = ide_xml_highlighter_engine_dispose;
}

static void
ide_xml_highlighter_init (IdeXmlHighlighter *self)
{
  g_signal_connect (self,
                    "notify::highlight-engine",
                    G_CALLBACK (ide_xml_highlighter_on_highlight_engine_set),
                    NULL);

  self->signal_group = egg_signal_group_new (IDE_TYPE_BUFFER);
  egg_signal_group_connect_object (self->signal_group,
                                   "cursor-moved",
                                   G_CALLBACK (ide_xml_highlighter_cursor_moved_cb),
                                   self,
                                   0);
}
