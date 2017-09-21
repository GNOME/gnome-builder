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

#include <dazzle.h>
#include <glib/gi18n.h>

#include "ide-xml.h"
#include "ide-xml-highlighter.h"

#define HIGHLIGH_TIMEOUT_MSEC    35
#define XML_TAG_MATCH_STYLE_NAME "xml:tag-match"

struct _IdeXmlHighlighter
{
  IdeObject           parent_instance;

  DzlSignalGroup     *signal_group;
  GtkTextMark        *iter_mark;
  IdeHighlightEngine *engine;
  GtkTextBuffer      *buffer;
  guint               highlight_timeout;
  guint               has_tags : 1;
};

static void highlighter_iface_init (IdeHighlighterInterface *iface);

G_DEFINE_DYNAMIC_TYPE_EXTENDED (IdeXmlHighlighter,
                                ide_xml_highlighter,
                                IDE_TYPE_OBJECT,
                                0,
                                G_IMPLEMENT_INTERFACE (IDE_TYPE_HIGHLIGHTER,
                                                       highlighter_iface_init))

static gboolean
ide_xml_highlighter_highlight_timeout_handler (gpointer data)
{
  IdeXmlHighlighter *self = data;
  GtkTextTag *tag;
  GtkTextIter iter;
  GtkTextIter start;
  GtkTextIter end;

  g_assert (IDE_IS_XML_HIGHLIGHTER (self));
  g_assert (self->buffer != NULL);
  g_assert (self->iter_mark != NULL);

  if (self->engine == NULL)
    goto cleanup;

  tag = ide_highlight_engine_get_style (self->engine, XML_TAG_MATCH_STYLE_NAME);

  /*
   * Clear previous tags. We could save the previous
   * iters and clear only those locations but for
   * now this should be ok.
   */
  if (self->has_tags)
    {
      gtk_text_buffer_get_bounds (self->buffer, &start, &end);
      gtk_text_buffer_remove_tag (self->buffer, tag, &start, &end);

      self->has_tags = FALSE;
    }


  gtk_text_buffer_get_iter_at_mark (self->buffer, &iter, self->iter_mark);
  if (ide_xml_in_element (&iter) && ide_xml_get_current_element (&iter,
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
          gtk_text_buffer_apply_tag (GTK_TEXT_BUFFER (self->buffer),
                                     tag,
                                     &start,
                                     &end);

          if (tag_type != IDE_XML_ELEMENT_TAG_START_END)
            {
              gtk_text_iter_forward_char (&next_start);
              gtk_text_buffer_apply_tag (GTK_TEXT_BUFFER (self->buffer),
                                         tag,
                                         &next_start,
                                         &next_end);
            }

          self->has_tags = TRUE;
        }
    }

cleanup:
  self->highlight_timeout = 0;

  return G_SOURCE_REMOVE;
}

static void
ide_xml_highlighter_bind_buffer_cb (IdeXmlHighlighter  *self,
                                    IdeBuffer          *buffer,
                                    DzlSignalGroup     *group)
{
  GtkTextIter begin;

  g_assert (IDE_IS_XML_HIGHLIGHTER (self));
  g_assert (IDE_IS_BUFFER (buffer));
  g_assert (DZL_IS_SIGNAL_GROUP (group));

  ide_set_weak_pointer (&self->buffer, GTK_TEXT_BUFFER (buffer));

  gtk_text_buffer_get_start_iter (self->buffer, &begin);
  self->iter_mark = gtk_text_buffer_create_mark (self->buffer, NULL, &begin, TRUE);
}

static void
ide_xml_highlighter_unbind_buffer_cb (IdeXmlHighlighter  *self,
                                      DzlSignalGroup     *group)
{
  g_assert (IDE_IS_XML_HIGHLIGHTER (self));
  g_assert (DZL_IS_SIGNAL_GROUP (group));
  g_assert (self->buffer != NULL);

  if (self->highlight_timeout != 0)
    {
      g_source_remove (self->highlight_timeout);
      self->highlight_timeout = 0;
    }

  gtk_text_buffer_delete_mark (self->buffer, self->iter_mark);
  self->iter_mark = NULL;

  ide_clear_weak_pointer (&self->buffer);
}


static void
ide_xml_highlighter_cursor_moved_cb (GtkTextBuffer     *buffer,
                                     GtkTextIter       *iter,
                                     IdeXmlHighlighter *self)
{
  g_assert (IDE_IS_HIGHLIGHTER (self));
  g_assert (GTK_IS_TEXT_BUFFER (buffer) && self->buffer == buffer);

  if (self->highlight_timeout != 0)
    g_source_remove (self->highlight_timeout);

  gtk_text_buffer_move_mark (buffer, self->iter_mark, iter);
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

  dzl_signal_group_set_target (self->signal_group, buffer);
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
ide_xml_highlighter_real_set_engine (IdeHighlighter     *highlighter,
                                     IdeHighlightEngine *engine)
{
  IdeXmlHighlighter *self = (IdeXmlHighlighter *)highlighter;
  IdeBuffer *buffer = NULL;

  g_return_if_fail (IDE_IS_XML_HIGHLIGHTER (self));
  g_return_if_fail (IDE_IS_HIGHLIGHT_ENGINE (engine));

  if (ide_set_weak_pointer (&self->engine, engine))
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

  ide_xml_highlighter_set_buffer (self, buffer);
}

static void
ide_xml_highlighter_dispose (GObject *object)
{
  IdeXmlHighlighter *self = (IdeXmlHighlighter *)object;

  if (self->highlight_timeout != 0)
    {
      g_source_remove (self->highlight_timeout);
      self->highlight_timeout = 0;
    }

  ide_clear_weak_pointer (&self->engine);
  g_clear_object (&self->signal_group);

  G_OBJECT_CLASS (ide_xml_highlighter_parent_class)->dispose (object);
}

static void
ide_xml_highlighter_class_init (IdeXmlHighlighterClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->dispose = ide_xml_highlighter_dispose;
}

static void
ide_xml_highlighter_class_finalize (IdeXmlHighlighterClass *klass)
{
}

static void
ide_xml_highlighter_init (IdeXmlHighlighter *self)
{
  self->signal_group = dzl_signal_group_new (IDE_TYPE_BUFFER);
  dzl_signal_group_connect_object (self->signal_group,
                                   "cursor-moved",
                                   G_CALLBACK (ide_xml_highlighter_cursor_moved_cb),
                                   self,
                                   0);

  g_signal_connect_object (self->signal_group,
                           "bind",
                           G_CALLBACK (ide_xml_highlighter_bind_buffer_cb),
                           self,
                           G_CONNECT_SWAPPED);

  g_signal_connect_object (self->signal_group,
                           "unbind",
                           G_CALLBACK (ide_xml_highlighter_unbind_buffer_cb),
                           self,
                           G_CONNECT_SWAPPED);
}

static void
highlighter_iface_init (IdeHighlighterInterface *iface)
{
  iface->set_engine = ide_xml_highlighter_real_set_engine;
}

void
_ide_xml_highlighter_register_type (GTypeModule *module)
{
  ide_xml_highlighter_register_type (module);
}
