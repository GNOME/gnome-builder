/* ide-highlight-engine.c
 *
 * Copyright (C) 2015 Christian Hergert <christian@hergert.me>
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

#define G_LOG_DOMAIN "ide-highlight-engine"

#include <glib/gi18n.h>

#include "ide-debug.h"
#include "ide-highlight-engine.h"
#include "ide-types.h"

#define HIGHLIGHT_QUANTA_USEC 1000
#define WORK_TIMEOUT_MSEC     50

struct _IdeHighlightEngine
{
  GObject         parent_instance;

  IdeBuffer      *buffer;
  IdeHighlighter *highlighter;

  GtkTextTag     *tags[IDE_HIGHLIGHT_KIND_LAST];

  GtkTextMark    *invalid_begin;
  GtkTextMark    *invalid_end;

  guint           work_timeout;
};

G_DEFINE_TYPE (IdeHighlightEngine, ide_highlight_engine, G_TYPE_OBJECT)

enum {
  PROP_0,
  PROP_BUFFER,
  PROP_HIGHLIGHTER,
  LAST_PROP
};

static GParamSpec *gParamSpecs [LAST_PROP];

static GtkTextTag *
create_tag_from_style (IdeHighlightEngine *self,
                       const gchar        *style_name)
{
  GtkSourceStyleScheme *style_scheme;
  GtkSourceStyle *style;
  GtkTextTag *tag;
  g_autofree gchar *foreground = NULL;
  g_autofree gchar *background = NULL;
  gboolean foreground_set = FALSE;
  gboolean background_set = FALSE;
  gboolean bold = FALSE;
  gboolean bold_set = FALSE;
  gboolean underline = FALSE;
  gboolean underline_set = FALSE;
  gboolean italic = FALSE;
  gboolean italic_set = FALSE;

  g_assert (IDE_IS_HIGHLIGHT_ENGINE (self));
  g_assert (self->buffer != NULL);
  g_assert (IDE_IS_BUFFER (self->buffer));

  tag = gtk_text_buffer_create_tag (GTK_TEXT_BUFFER (self->buffer), "", NULL);

  style_scheme = gtk_source_buffer_get_style_scheme (GTK_SOURCE_BUFFER (self->buffer));
  if (style_scheme == NULL)
    return tag;

  style = gtk_source_style_scheme_get_style (style_scheme, style_name);
  if (style == NULL)
    return tag;

  g_object_get (style,
                "background", &background,
                "background-set", &background_set,
                "foreground", &foreground,
                "foreground-set", &foreground_set,
                "bold", &bold,
                "bold-set", &bold_set,
                "underline", &underline,
                "underline-set", &underline_set,
                "italic", &italic,
                "italic-set", &italic_set,
                NULL);

  if (background_set)
    g_object_set (tag, "background", background, NULL);

  if (foreground_set)
    g_object_set (tag, "foreground", foreground, NULL);

  if (bold_set && bold)
    g_object_set (tag, "weight", PANGO_WEIGHT_BOLD, NULL);

  if (italic_set && italic)
    g_object_set (tag, "style", PANGO_STYLE_ITALIC, NULL);

  if (underline_set && underline)
    g_object_set (tag, "underline", PANGO_UNDERLINE_SINGLE, NULL);

  return tag;
}

static GtkTextTag *
get_kind_tag (IdeHighlightEngine *self,
              IdeHighlightKind    kind)
{
  const gchar *name = NULL;

  g_assert (IDE_IS_HIGHLIGHT_ENGINE (self));

  switch (kind)
    {
    case IDE_HIGHLIGHT_KIND_TYPE_NAME:
    case IDE_HIGHLIGHT_KIND_FUNCTION_NAME:
    case IDE_HIGHLIGHT_KIND_CLASS_NAME:
    case IDE_HIGHLIGHT_KIND_MACRO_NAME:
      name = "def:type";
      break;

    case IDE_HIGHLIGHT_KIND_NONE:
    default:
      return NULL;
    }

    if ((self->tags [kind] == NULL) && (name != NULL))
      self->tags [kind] = create_tag_from_style (self, name);

    return self->tags [kind];
}

static gboolean
ide_highlight_engine_tick (IdeHighlightEngine *self)
{
  GtkTextBuffer *buffer;
  GtkTextIter iter;
  GtkTextIter begin;
  GtkTextIter end;
  guint64 quanta_expiration;
  IdeHighlightKind kind;

  IDE_PROBE;

  g_assert (IDE_IS_HIGHLIGHT_ENGINE (self));
  g_assert (self->buffer != NULL);
  g_assert (self->highlighter != NULL);
  g_assert (self->invalid_begin != NULL);
  g_assert (self->invalid_end != NULL);

  quanta_expiration = g_get_monotonic_time () + HIGHLIGHT_QUANTA_USEC;

  buffer = GTK_TEXT_BUFFER (self->buffer);

  do
    {
      GtkTextTag *tag;
      GtkTextIter invalid_begin;
      GtkTextIter invalid_end;

      gtk_text_buffer_get_iter_at_mark (buffer, &invalid_begin, self->invalid_begin);
      gtk_text_buffer_get_iter_at_mark (buffer, &invalid_end, self->invalid_end);

      IDE_TRACE_MSG ("Highlight Range (%u:%u <=> %u:%u)",
                     gtk_text_iter_get_line (&invalid_begin),
                     gtk_text_iter_get_line_offset (&invalid_begin),
                     gtk_text_iter_get_line (&invalid_end),
                     gtk_text_iter_get_line_offset (&invalid_end));

      if (gtk_text_iter_compare (&invalid_begin, &invalid_end) >= 0)
        IDE_GOTO (up_to_date);

      iter = invalid_begin;

      kind = ide_highlighter_next (self->highlighter, &iter, &invalid_end, &begin, &end);

      if (kind == IDE_HIGHLIGHT_KIND_NONE)
        IDE_GOTO (up_to_date);

      IDE_TRACE_MSG ("Found tag of kind: %d\n", kind);

      tag = get_kind_tag (self, kind);

      gtk_text_buffer_apply_tag (buffer, tag, &begin, &end);
      gtk_text_buffer_move_mark (buffer, self->invalid_begin, &end);
    }
  while (g_get_monotonic_time () < quanta_expiration);

  return TRUE;

up_to_date:
  gtk_text_buffer_get_start_iter (buffer, &begin);
  gtk_text_buffer_move_mark (buffer, self->invalid_begin, &begin);
  gtk_text_buffer_move_mark (buffer, self->invalid_end, &begin);

  return FALSE;
}

static gboolean
ide_highlight_engine_work_timeout_handler (gpointer data)
{
  IdeHighlightEngine *self = data;

  g_assert (IDE_IS_HIGHLIGHT_ENGINE (self));

  if (ide_highlight_engine_tick (self))
    return G_SOURCE_CONTINUE;

  self->work_timeout = 0;

  return G_SOURCE_REMOVE;
}

static void
ide_highlight_engine_queue_work (IdeHighlightEngine *self)
{
  g_assert (IDE_IS_HIGHLIGHT_ENGINE (self));

  if ((self->highlighter == NULL) || (self->buffer == NULL) || (self->work_timeout != 0))
    return;

  self->work_timeout = g_timeout_add (WORK_TIMEOUT_MSEC,
                                      ide_highlight_engine_work_timeout_handler,
                                      self);
}

static void
ide_highlight_engine_reload (IdeHighlightEngine *self)
{
  GtkTextBuffer *buffer;
  GtkTextIter begin;
  GtkTextIter end;
  gsize i;

  IDE_ENTRY;

  g_assert (IDE_IS_HIGHLIGHT_ENGINE (self));

  if (self->work_timeout != 0)
    {
      g_source_remove (self->work_timeout);
      self->work_timeout = 0;
    }

  if (self->buffer == NULL)
    IDE_EXIT;

  buffer = GTK_TEXT_BUFFER (self->buffer);

  gtk_text_buffer_get_bounds (buffer, &begin, &end);

  /*
   * Invalidate the whole buffer.
   */
  gtk_text_buffer_move_mark (buffer, self->invalid_begin, &begin);
  gtk_text_buffer_move_mark (buffer, self->invalid_end, &end);

  /*
   * Remove our highlight tags from the buffer.
   */
  for (i = 0; i < G_N_ELEMENTS (self->tags); i++)
    if (self->tags [i] != NULL)
      gtk_text_buffer_remove_tag (buffer, self->tags [i], &begin, &end);

  if (self->highlighter == NULL)
    IDE_EXIT;

  ide_highlight_engine_queue_work (self);

  IDE_EXIT;
}

static void
ide_highlight_engine__buffer_insert_text_cb (IdeHighlightEngine *self,
                                             GtkTextIter        *location,
                                             gchar              *text,
                                             gint                len,
                                             IdeBuffer          *buffer)
{
  GtkTextBuffer *text_buffer = (GtkTextBuffer *)buffer;
  GtkTextIter begin;
  GtkTextIter end;

  IDE_ENTRY;

  g_assert (IDE_IS_HIGHLIGHT_ENGINE (self));
  g_assert (location);
  g_assert (text);
  g_assert (IDE_IS_BUFFER (buffer));
  g_assert (GTK_IS_TEXT_BUFFER (text_buffer));

  gtk_text_buffer_get_iter_at_mark (text_buffer, &begin, self->invalid_begin);
  gtk_text_buffer_get_iter_at_mark (text_buffer, &end, self->invalid_end);

  if (gtk_text_iter_equal (&begin, &end))
    {
      begin = *location;
      end = *location;

      gtk_text_iter_backward_lines (&begin, 2);
      gtk_text_iter_forward_lines (&end, 2);
      gtk_text_buffer_move_mark (text_buffer, self->invalid_begin, &begin);
      gtk_text_buffer_move_mark (text_buffer, self->invalid_end, &end);
    }
  else
    {
      if (gtk_text_iter_compare (location, &begin) < 0)
        gtk_text_buffer_move_mark (text_buffer, self->invalid_begin, location);
      if (gtk_text_iter_compare (location, &end) > 0)
        gtk_text_buffer_move_mark (text_buffer, self->invalid_end, location);
    }

  ide_highlight_engine_queue_work (self);

  IDE_EXIT;
}

static void
ide_highlight_engine__buffer_delete_range_cb (IdeHighlightEngine *self,
                                              GtkTextIter        *range_begin,
                                              GtkTextIter        *range_end,
                                              IdeBuffer          *buffer)
{
  GtkTextBuffer *text_buffer = (GtkTextBuffer *)buffer;
  GtkTextIter begin;
  GtkTextIter end;

  IDE_ENTRY;

  g_assert (IDE_IS_HIGHLIGHT_ENGINE (self));
  g_assert (range_begin);
  g_assert (range_end);
  g_assert (IDE_IS_BUFFER (buffer));
  g_assert (GTK_IS_TEXT_BUFFER (text_buffer));

  gtk_text_buffer_get_iter_at_mark (text_buffer, &begin, self->invalid_begin);
  gtk_text_buffer_get_iter_at_mark (text_buffer, &end, self->invalid_end);

  if (gtk_text_iter_equal (&begin, &end))
    {
      begin = *range_begin;
      end = *range_end;

      gtk_text_iter_backward_lines (&begin, 2);
      gtk_text_iter_forward_lines (&end, 2);
      gtk_text_buffer_move_mark (text_buffer, self->invalid_begin, &begin);
      gtk_text_buffer_move_mark (text_buffer, self->invalid_end, &end);
    }
  else
    {
      if (gtk_text_iter_compare (range_begin, &begin) < 0)
        gtk_text_buffer_move_mark (text_buffer, self->invalid_begin, range_begin);
      if (gtk_text_iter_compare (range_end, &end) > 0)
        gtk_text_buffer_move_mark (text_buffer, self->invalid_end, range_end);
    }

  ide_highlight_engine_queue_work (self);

  IDE_EXIT;
}

static void
ide_highlight_engine_connect_buffer (IdeHighlightEngine *self,
                                     IdeBuffer          *buffer)
{
  GtkTextBuffer *text_buffer = (GtkTextBuffer *)buffer;
  GtkTextIter begin;
  GtkTextIter end;

  IDE_ENTRY;

  g_assert (IDE_IS_HIGHLIGHT_ENGINE (self));
  g_assert (IDE_IS_BUFFER (buffer));

  gtk_text_buffer_get_bounds (text_buffer, &begin, &end);

  self->invalid_begin = gtk_text_buffer_create_mark (text_buffer, NULL, &begin, TRUE);
  self->invalid_end = gtk_text_buffer_create_mark (text_buffer, NULL, &end, FALSE);

  g_signal_connect_object (buffer,
                           "insert-text",
                           G_CALLBACK (ide_highlight_engine__buffer_insert_text_cb),
                           self,
                           G_CONNECT_SWAPPED);

  g_signal_connect_object (buffer,
                           "delete-range",
                           G_CALLBACK (ide_highlight_engine__buffer_delete_range_cb),
                           self,
                           G_CONNECT_SWAPPED);

  ide_highlight_engine_reload (self);

  IDE_EXIT;
}

static void
ide_highlight_engine_disconnect_buffer (IdeHighlightEngine *self,
                                        IdeBuffer          *buffer)
{
  GtkTextBuffer *text_buffer = (GtkTextBuffer *)buffer;
  GtkTextTagTable *tag_table;
  GtkTextIter begin;
  GtkTextIter end;
  gsize i;

  IDE_ENTRY;

  g_assert (IDE_IS_HIGHLIGHT_ENGINE (self));
  g_assert (IDE_IS_BUFFER (buffer));

  if (self->work_timeout)
    {
      g_source_remove (self->work_timeout);
      self->work_timeout = 0;
    }

  g_signal_handlers_disconnect_by_func (buffer,
                                        G_CALLBACK (ide_highlight_engine__buffer_delete_range_cb),
                                        self);

  g_signal_handlers_disconnect_by_func (buffer,
                                        G_CALLBACK (ide_highlight_engine__buffer_insert_text_cb),
                                        self);

  tag_table = gtk_text_buffer_get_tag_table (text_buffer);

  gtk_text_buffer_delete_mark (text_buffer, self->invalid_begin);
  gtk_text_buffer_delete_mark (text_buffer, self->invalid_end);

  self->invalid_begin = NULL;
  self->invalid_end = NULL;

  gtk_text_buffer_get_bounds (text_buffer, &begin, &end);

  for (i = 0; i < G_N_ELEMENTS (self->tags); i++)
    {
      if (self->tags [i] != NULL)
        {
          gtk_text_buffer_remove_tag (text_buffer, self->tags [i], &begin, &end);
          gtk_text_tag_table_remove (tag_table, self->tags [i]);
          self->tags [i] = NULL;
        }
    }

  IDE_EXIT;
}

static void
ide_highlight_engine_set_buffer (IdeHighlightEngine *self,
                                 IdeBuffer          *buffer)
{
  g_return_if_fail (IDE_IS_HIGHLIGHT_ENGINE (self));
  g_return_if_fail (IDE_IS_BUFFER (buffer));

  if (self->buffer != buffer)
    {
      if (self->buffer != NULL)
        {
          ide_highlight_engine_disconnect_buffer (self, self->buffer);
          ide_clear_weak_pointer (&self->buffer);
        }

      if (buffer != NULL)
        {
          ide_set_weak_pointer (&self->buffer, buffer);
          ide_highlight_engine_connect_buffer (self, self->buffer);
        }

      g_object_notify_by_pspec (G_OBJECT (self), gParamSpecs [PROP_BUFFER]);
    }
}

static void
ide_highlight_engine_dispose (GObject *object)
{
  IdeHighlightEngine *self = (IdeHighlightEngine *)object;

  ide_highlight_engine_set_buffer (self, NULL);

  G_OBJECT_CLASS (ide_highlight_engine_parent_class)->dispose (object);
}

static void
ide_highlight_engine_finalize (GObject *object)
{
  IdeHighlightEngine *self = (IdeHighlightEngine *)object;

  g_clear_object (&self->highlighter);
  ide_clear_weak_pointer (&self->buffer);

  G_OBJECT_CLASS (ide_highlight_engine_parent_class)->finalize (object);
}

static void
ide_highlight_engine_get_property (GObject    *object,
                                   guint       prop_id,
                                   GValue     *value,
                                   GParamSpec *pspec)
{
  IdeHighlightEngine *self = IDE_HIGHLIGHT_ENGINE (object);

  switch (prop_id)
    {
    case PROP_BUFFER:
      g_value_set_object (value, ide_highlight_engine_get_buffer (self));
      break;

    case PROP_HIGHLIGHTER:
      g_value_set_object (value, ide_highlight_engine_get_highlighter (self));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
ide_highlight_engine_set_property (GObject      *object,
                                   guint         prop_id,
                                   const GValue *value,
                                   GParamSpec   *pspec)
{
  IdeHighlightEngine *self = IDE_HIGHLIGHT_ENGINE (object);

  switch (prop_id)
    {
    case PROP_BUFFER:
      ide_highlight_engine_set_buffer (self, g_value_get_object (value));
      break;

    case PROP_HIGHLIGHTER:
      ide_highlight_engine_set_highlighter (self, g_value_get_object (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
ide_highlight_engine_class_init (IdeHighlightEngineClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->dispose = ide_highlight_engine_dispose;
  object_class->finalize = ide_highlight_engine_finalize;
  object_class->get_property = ide_highlight_engine_get_property;
  object_class->set_property = ide_highlight_engine_set_property;

  gParamSpecs [PROP_BUFFER] =
    g_param_spec_object ("buffer",
                         _("Buffer"),
                         _("The buffer to highlight."),
                         IDE_TYPE_BUFFER,
                         (G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (object_class, PROP_BUFFER, gParamSpecs [PROP_BUFFER]);

  gParamSpecs [PROP_HIGHLIGHTER] =
    g_param_spec_object ("highlighter",
                         _("Highlighter"),
                         _("The highlighter to use for type information."),
                         IDE_TYPE_HIGHLIGHTER,
                         (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (object_class, PROP_HIGHLIGHTER, gParamSpecs [PROP_HIGHLIGHTER]);
}

static void
ide_highlight_engine_init (IdeHighlightEngine *self)
{
}

IdeHighlightEngine *
ide_highlight_engine_new (IdeBuffer *buffer)
{
  return g_object_new (IDE_TYPE_HIGHLIGHT_ENGINE,
                       "buffer", buffer,
                       NULL);
}

/**
 * ide_highlight_engine_get_highlighter:
 * @self: A #IdeHighlightEngine.
 *
 * Gets the IdeHighlightEngine:highlighter property.
 *
 * Returns: (transfer none): An #IdeHighlighter.
 */
IdeHighlighter *
ide_highlight_engine_get_highlighter (IdeHighlightEngine *self)
{
  g_return_val_if_fail (IDE_IS_HIGHLIGHT_ENGINE (self), NULL);

  return self->highlighter;
}

void
ide_highlight_engine_set_highlighter (IdeHighlightEngine *self,
                                      IdeHighlighter     *highlighter)
{
  g_return_if_fail (IDE_IS_HIGHLIGHT_ENGINE (self));
  g_return_if_fail (!highlighter || IDE_IS_HIGHLIGHTER (highlighter));

  if (g_set_object (&self->highlighter, highlighter))
    {
      ide_highlight_engine_reload (self);
      g_object_notify_by_pspec (G_OBJECT (self), gParamSpecs [PROP_HIGHLIGHTER]);
    }
}

/**
 * ide_highlight_engine_get_buffer:
 * @self: A #IdeHighlightEngine.
 *
 * Gets the IdeHighlightEngine:buffer property.
 *
 * Returns: (transfer none): An #IdeBuffer.
 */
IdeBuffer *
ide_highlight_engine_get_buffer (IdeHighlightEngine *self)
{
  g_return_val_if_fail (IDE_IS_HIGHLIGHT_ENGINE (self), NULL);

  return self->buffer;
}
