/* gb-color-picker-document-monitor.c
 *
 * Copyright 2016 sebastien lafargue <slafargue@gnome.org>
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

#include "gb-color-picker-helper.h"
#include "gb-color-picker-private.h"
#include "gstyle-color-item.h"

#include "gb-color-picker-document-monitor.h"

struct _GbColorPickerDocumentMonitor
{
  GObject       parent_instance;

  IdeBuffer    *buffer;

  gulong        insert_handler_id;
  gulong        insert_after_handler_id;
  gulong        delete_handler_id;
  gulong        delete_after_handler_id;
  gulong        cursor_notify_handler_id;

  gulong        remove_tag_handler_id;

  guint         is_in_user_action : 1;
};

typedef struct
{
  guint line;
  guint line_offset;
} Position;

typedef struct
{
  GbColorPickerDocumentMonitor *self;
  GtkTextBuffer                *buffer;
  GtkTextMark                  *begin;
  GtkTextMark                  *end;
  guint                         uncolorize : 1;
} QueuedColorize;

G_DEFINE_TYPE (GbColorPickerDocumentMonitor, gb_color_picker_document_monitor, G_TYPE_OBJECT)

enum {
  PROP_0,
  PROP_BUFFER,
  N_PROPS
};

enum {
  COLOR_FOUND,
  N_SIGNALS
};

static GParamSpec *properties [N_PROPS];
static guint signals [N_SIGNALS];

static void block_signals   (GbColorPickerDocumentMonitor *self,
                             IdeBuffer                    *buffer);

static void unblock_signals (GbColorPickerDocumentMonitor *self,
                             IdeBuffer                    *buffer);

static void
position_save (Position          *pos,
              const GtkTextIter *iter)
{
  pos->line = gtk_text_iter_get_line (iter);
  pos->line_offset = gtk_text_iter_get_line_offset (iter);
}

static void
position_restore (Position      *pos,
                  GtkTextBuffer *buffer,
                  GtkTextIter   *iter)
{
  gtk_text_buffer_get_iter_at_line_offset (buffer, iter, pos->line, pos->line_offset);
}

void
gb_color_picker_document_monitor_set_color_tag_at_cursor (GbColorPickerDocumentMonitor *self,
                                                          GstyleColor                  *color)
{
  GtkTextMark *insert;
  GtkTextIter cursor;

  g_return_if_fail (GB_IS_COLOR_PICKER_DOCUMENT_MONITOR (self));
  g_return_if_fail (GSTYLE_IS_COLOR (color));
  g_return_if_fail (self->buffer != NULL);

  insert = gtk_text_buffer_get_insert (GTK_TEXT_BUFFER(self->buffer));
  gtk_text_buffer_get_iter_at_mark (GTK_TEXT_BUFFER(self->buffer), &cursor, insert);

  if (!self->is_in_user_action)
    {
      gtk_text_buffer_begin_user_action (GTK_TEXT_BUFFER (self->buffer));
      self->is_in_user_action = TRUE;
    }

  block_signals (self, self->buffer);
  gb_color_picker_helper_set_color_tag_at_iter (&cursor, color, TRUE);
  unblock_signals (self, self->buffer);
}

static void
collect_tag_names (GtkTextTag *tag,
                   GPtrArray  *taglist)
{
  g_autofree gchar *name = NULL;

  g_assert (GTK_IS_TEXT_TAG (tag));
  g_assert (taglist != NULL);

  g_object_get (G_OBJECT (tag), "name", &name, NULL);
  if (!dzl_str_empty0 (name) && g_str_has_prefix (name, COLOR_TAG_PREFIX))
    g_ptr_array_add (taglist, g_steal_pointer (&name));
}

static void
gb_color_picker_document_monitor_uncolorize (GbColorPickerDocumentMonitor *self,
                                             GtkTextBuffer                *buffer,
                                             GtkTextIter                  *begin,
                                             GtkTextIter                  *end)
{
  GtkTextTagTable *tag_table;
  GtkTextIter real_begin;
  GtkTextIter real_end;

  g_return_if_fail (GB_IS_COLOR_PICKER_DOCUMENT_MONITOR (self));
  g_return_if_fail (GTK_IS_TEXT_BUFFER (buffer));

  tag_table = gtk_text_buffer_get_tag_table (GTK_TEXT_BUFFER (buffer));

  if (begin == NULL && end == NULL)
    {
      g_autoptr(GPtrArray) taglist = g_ptr_array_new_with_free_func (g_free);

      gtk_text_tag_table_foreach (tag_table, (GtkTextTagTableForeach)collect_tag_names, taglist);
      for (guint n = 0; n < taglist->len; ++n)
        gtk_text_tag_table_remove (tag_table, g_ptr_array_index (taglist, n));

      return;
    }

  if (begin == NULL)
    gtk_text_buffer_get_start_iter (GTK_TEXT_BUFFER (buffer), &real_begin);
  else
    real_begin = *begin;

  if (end == NULL)
    gtk_text_buffer_get_end_iter (GTK_TEXT_BUFFER (buffer), &real_end);
  else
    real_end = *end;

  while (TRUE)
    {
      g_autoptr(GSList) tags = NULL;
      GtkTextTag *color_tag = NULL;

      tags = gtk_text_iter_get_toggled_tags (&real_begin, TRUE);

      for (const GSList *l = tags; l != NULL; l = l->next)
        {
          g_autofree gchar *name = NULL;
          GtkTextTag *tag = l->data;

          g_object_get (G_OBJECT (tag), "name", &name, NULL);

          if (name != NULL && g_str_has_prefix (name, COLOR_TAG_PREFIX))
            {
              color_tag = tag;
              break;
            }
        }

      if (color_tag != NULL)
        {
          gtk_text_iter_forward_to_tag_toggle (&real_begin, color_tag);
          gtk_text_tag_table_remove (tag_table, color_tag);
        }

      if (!gtk_text_iter_forward_to_tag_toggle (&real_begin, NULL))
        break;

      if (gtk_text_iter_compare (&real_begin, &real_end) != -1)
        break;
    }
}

static void
gb_color_picker_document_monitor_colorize (GbColorPickerDocumentMonitor *self,
                                           GtkTextBuffer                *buffer,
                                           GtkTextIter                  *begin,
                                           GtkTextIter                  *end)
{
  g_autofree gchar *text = NULL;
  g_autoptr(GPtrArray) items = NULL;
  GstyleColorItem *item;
  GtkTextTag *tag;
  GtkTextIter real_begin;
  GtkTextIter real_end;
  GtkTextIter tag_begin;
  GtkTextIter tag_end;
  gint offset;
  gint len;
  gint pos;

  g_return_if_fail (GB_IS_COLOR_PICKER_DOCUMENT_MONITOR (self));
  g_return_if_fail (GTK_IS_TEXT_BUFFER (buffer));

  if (begin == NULL)
    gtk_text_buffer_get_start_iter (GTK_TEXT_BUFFER (buffer), &real_begin);
  else
    real_begin = *begin;

  if (end == NULL)
    gtk_text_buffer_get_end_iter (GTK_TEXT_BUFFER (buffer), &real_end);
  else
    real_end = *end;

  if (gtk_text_iter_equal (&real_begin, &real_end))
    return;

  offset = gtk_text_iter_get_offset (&real_begin);
  text = gtk_text_buffer_get_slice (GTK_TEXT_BUFFER (buffer), &real_begin, &real_end, TRUE);

  items = gstyle_color_parse (text);
  for (guint n = 0; n < items->len; ++n)
    {
      GstyleColor *color;

      item = g_ptr_array_index (items, n);
      pos = offset + gstyle_color_item_get_start (item);
      gtk_text_buffer_get_iter_at_offset (GTK_TEXT_BUFFER (buffer), &tag_begin, pos);
      len = gstyle_color_item_get_len (item);
      gtk_text_buffer_get_iter_at_offset (GTK_TEXT_BUFFER (buffer), &tag_end, pos + len);
      color = (GstyleColor *)gstyle_color_item_get_color (item);

      tag = gb_color_picker_helper_create_color_tag (GTK_TEXT_BUFFER (buffer), color);
      gtk_text_buffer_apply_tag (GTK_TEXT_BUFFER (buffer), tag, &tag_begin, &tag_end);
      /* FIXME: is the tag added to the tag table or should we handle a hash table/tag table ourself ? */
    }
}

static void
text_inserted_cb (GbColorPickerDocumentMonitor *self,
                  GtkTextIter                  *cursor,
                  gchar                        *text,
                  gint                          len,
                  GtkTextBuffer                *buffer)
{
  g_autoptr(GstyleColor) color = NULL;
  GtkTextTag *tag;
  GtkTextIter begin, end;

  g_assert (GB_IS_COLOR_PICKER_DOCUMENT_MONITOR (self));
  g_assert (GTK_IS_TEXT_BUFFER (buffer));
  g_assert (cursor != NULL);

  tag = gb_color_picker_helper_get_tag_at_iter (cursor, &color, &begin, &end);

  if (tag != NULL )
    {
      gtk_text_iter_set_line_offset (&begin, 0);
      if (!gtk_text_iter_ends_line (&end))
        gtk_text_iter_forward_to_line_end (&end);

      gb_color_picker_document_monitor_queue_uncolorize (self, &begin, &end);
    }
}

static void
text_inserted_after_cb (GbColorPickerDocumentMonitor *self,
                        GtkTextIter                  *iter,
                        gchar                        *text,
                        gint                          len,
                        GtkTextBuffer                *buffer)
{
  GtkTextIter begin, end;
  gint offset;

  g_assert (GB_IS_COLOR_PICKER_DOCUMENT_MONITOR (self));
  g_assert (GTK_IS_TEXT_BUFFER (buffer));
  g_assert (iter != NULL);

  begin = *iter;
  offset = gtk_text_iter_get_offset (&begin);
  gtk_text_iter_set_offset (&begin, offset - len);
  gtk_text_iter_set_line_offset (&begin, 0);

  end = *iter;
  if (!gtk_text_iter_ends_line (&end))
    gtk_text_iter_forward_to_line_end (&end);

  gb_color_picker_document_monitor_queue_colorize (self, &begin, &end);
}

static void
remove_tag_cb (GbColorPickerDocumentMonitor *self,
               GtkTextTag                   *tag,
               GtkTextIter                  *start,
               GtkTextIter                  *end,
               GtkTextBuffer                *buffer)
{
  GtkTextTagTable *tag_table;
  g_autofree gchar *name = NULL;
  Position spos;
  Position epos;

  g_assert (GB_IS_COLOR_PICKER_DOCUMENT_MONITOR (self));
  g_assert (GTK_IS_TEXT_BUFFER (buffer));
  g_assert (GTK_IS_TEXT_TAG (tag));

  position_save (&spos, start);
  position_save (&epos, end);

  tag_table = gtk_text_buffer_get_tag_table (GTK_TEXT_BUFFER (self->buffer));
  g_object_get (G_OBJECT (tag), "name", &name, NULL);

  if (!dzl_str_empty0 (name) &&
      g_str_has_prefix (name, COLOR_TAG_PREFIX) &&
      gtk_text_tag_table_lookup (tag_table, name))
    gtk_text_tag_table_remove (tag_table, tag);

  position_restore (&spos, buffer, start);
  position_restore (&epos, buffer, end);
}

static void
text_deleted_cb (GbColorPickerDocumentMonitor *self,
                 GtkTextIter                  *begin,
                 GtkTextIter                  *end,
                 GtkTextBuffer                *buffer)
{
  GtkTextIter recolor_begin;
  GtkTextIter recolor_end;
  Position spos;
  Position epos;

  g_assert (GB_IS_COLOR_PICKER_DOCUMENT_MONITOR (self));
  g_assert (GTK_IS_TEXT_BUFFER (buffer));

  position_save (&spos, begin);
  position_save (&epos, end);

  self->remove_tag_handler_id = g_signal_connect_object (GTK_TEXT_BUFFER (self->buffer),
                                                         "remove-tag",
                                                         G_CALLBACK (remove_tag_cb),
                                                         self,
                                                         G_CONNECT_SWAPPED | G_CONNECT_AFTER);

  recolor_begin = *begin;
  gtk_text_iter_set_line_offset (&recolor_begin, 0);

  recolor_end = *end;
  if (!gtk_text_iter_ends_line (&recolor_end))
    gtk_text_iter_forward_to_line_end (&recolor_end);

  /* FIXME: we only need to remove color tag */
  gtk_text_buffer_remove_all_tags (buffer, &recolor_begin, &recolor_end);
  g_signal_handler_disconnect (GTK_TEXT_BUFFER (self->buffer), self->remove_tag_handler_id);

  position_restore (&spos, buffer, begin);
  position_restore (&epos, buffer, end);
}

static void
text_deleted_after_cb (GbColorPickerDocumentMonitor *self,
                       GtkTextIter                  *begin,
                       GtkTextIter                  *end,
                       GtkTextBuffer                *buffer)
{
  GtkTextIter recolor_begin;
  GtkTextIter recolor_end;

  g_assert (GB_IS_COLOR_PICKER_DOCUMENT_MONITOR (self));
  g_assert (GTK_IS_TEXT_BUFFER (buffer));
  g_assert (begin != NULL);
  g_assert (end != NULL);

  recolor_begin = *begin;
  gtk_text_iter_set_line_offset (&recolor_begin, 0);

  recolor_end = *end;
  if (!gtk_text_iter_ends_line (&recolor_end))
    gtk_text_iter_forward_to_line_end (&recolor_end);

  gb_color_picker_document_monitor_queue_colorize (self, &recolor_begin, &recolor_end);
}

static void
cursor_moved_cb (GbColorPickerDocumentMonitor *self,
                 const GtkTextIter            *location,
                 GtkTextBuffer                *buffer)
{
  g_autoptr(GstyleColor) current_color = NULL;
  GtkTextTag *tag;
  GtkTextIter cursor;
  GtkTextIter begin, end;

  g_assert (GB_IS_COLOR_PICKER_DOCUMENT_MONITOR (self));
  g_assert (GTK_IS_TEXT_BUFFER (buffer));

  if (self->is_in_user_action)
    {
      gtk_text_buffer_end_user_action (GTK_TEXT_BUFFER (self->buffer));
      self->is_in_user_action = FALSE;
    }

  cursor = *location;

  /* TODO: fast path: check if we are in the last already detected color tag */
  tag = gb_color_picker_helper_get_tag_at_iter (&cursor, &current_color, &begin, &end);
  if (tag != NULL)
    g_signal_emit (self, signals [COLOR_FOUND], 0, current_color);
}

static void
start_monitor (GbColorPickerDocumentMonitor *self)
{
  g_assert (GB_IS_COLOR_PICKER_DOCUMENT_MONITOR (self));

  self->insert_handler_id = g_signal_connect_object (GTK_TEXT_BUFFER (self->buffer),
                                                     "insert-text",
                                                     G_CALLBACK (text_inserted_cb),
                                                     self,
                                                     G_CONNECT_SWAPPED);

  self->insert_after_handler_id = g_signal_connect_object (GTK_TEXT_BUFFER (self->buffer),
                                                           "insert-text",
                                                           G_CALLBACK (text_inserted_after_cb),
                                                           self,
                                                           G_CONNECT_SWAPPED | G_CONNECT_AFTER);

  self->delete_handler_id = g_signal_connect_object (GTK_TEXT_BUFFER (self->buffer),
                                                     "delete-range",
                                                     G_CALLBACK (text_deleted_cb),
                                                     self,
                                                     G_CONNECT_SWAPPED);

  self->delete_after_handler_id = g_signal_connect_object (GTK_TEXT_BUFFER (self->buffer),
                                                           "delete-range",
                                                           G_CALLBACK (text_deleted_after_cb),
                                                           self,
                                                           G_CONNECT_SWAPPED | G_CONNECT_AFTER);

  self->cursor_notify_handler_id = g_signal_connect_object (self->buffer,
                                                            "cursor-moved",
                                                            G_CALLBACK (cursor_moved_cb),
                                                            self,
                                                            G_CONNECT_SWAPPED | G_CONNECT_AFTER);
}

static void
block_signals (GbColorPickerDocumentMonitor *self,
               IdeBuffer                    *buffer)
{
  g_assert (GB_IS_COLOR_PICKER_DOCUMENT_MONITOR (self));

  g_signal_handlers_block_by_func (buffer, text_inserted_cb, self);
  g_signal_handlers_block_by_func (buffer, text_inserted_after_cb, self);
  g_signal_handlers_block_by_func (buffer, text_deleted_cb, self);
  g_signal_handlers_block_by_func (buffer, text_deleted_after_cb, self);
  g_signal_handlers_block_by_func (buffer, cursor_moved_cb, self);
}

static void
unblock_signals (GbColorPickerDocumentMonitor *self,
                 IdeBuffer                    *buffer)
{
  g_assert (GB_IS_COLOR_PICKER_DOCUMENT_MONITOR (self));

  g_signal_handlers_unblock_by_func (buffer, text_inserted_cb, self);
  g_signal_handlers_unblock_by_func (buffer, text_inserted_after_cb, self);
  g_signal_handlers_unblock_by_func (buffer, text_deleted_cb, self);
  g_signal_handlers_unblock_by_func (buffer, text_deleted_after_cb, self);
  g_signal_handlers_unblock_by_func (buffer, cursor_moved_cb, self);
}

static void
stop_monitor (GbColorPickerDocumentMonitor *self)
{
  g_assert (GB_IS_COLOR_PICKER_DOCUMENT_MONITOR (self));

  g_signal_handlers_disconnect_by_func (self->buffer, text_inserted_cb, self);
  g_signal_handlers_disconnect_by_func (self->buffer, text_inserted_after_cb, self);
  g_signal_handlers_disconnect_by_func (self->buffer, text_deleted_cb, self);
  g_signal_handlers_disconnect_by_func (self->buffer, text_deleted_after_cb, self);
  g_signal_handlers_disconnect_by_func (self->buffer, cursor_moved_cb, self);
}

void
gb_color_picker_document_monitor_set_buffer (GbColorPickerDocumentMonitor *self,
                                             IdeBuffer                    *buffer)
{
  g_return_if_fail (GB_IS_COLOR_PICKER_DOCUMENT_MONITOR (self));
  g_return_if_fail (!buffer || IDE_IS_BUFFER (buffer));

  if (self->buffer != buffer && self->buffer != NULL)
    stop_monitor (self);

  if (g_set_weak_pointer (&self->buffer, buffer))
    {
      g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_BUFFER]);

      if (buffer != NULL)
        start_monitor (self);
    }
}

IdeBuffer *
gb_color_picker_document_monitor_get_buffer (GbColorPickerDocumentMonitor *self)
{
  g_return_val_if_fail (GB_IS_COLOR_PICKER_DOCUMENT_MONITOR (self), NULL);

  return self->buffer;
}

GbColorPickerDocumentMonitor *
gb_color_picker_document_monitor_new (IdeBuffer *buffer)
{
  return g_object_new (GB_TYPE_COLOR_PICKER_DOCUMENT_MONITOR,
                       "buffer", buffer,
                       NULL);
}

static void
gb_color_picker_document_monitor_finalize (GObject *object)
{
  GbColorPickerDocumentMonitor *self = (GbColorPickerDocumentMonitor *)object;

  g_clear_weak_pointer (&self->buffer);

  G_OBJECT_CLASS (gb_color_picker_document_monitor_parent_class)->finalize (object);
}

static void
gb_color_picker_document_monitor_get_property (GObject    *object,
                                               guint       prop_id,
                                               GValue     *value,
                                               GParamSpec *pspec)
{
  GbColorPickerDocumentMonitor *self = GB_COLOR_PICKER_DOCUMENT_MONITOR (object);


  switch (prop_id)
    {
    case PROP_BUFFER:
      g_value_set_object (value, gb_color_picker_document_monitor_get_buffer (self));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
gb_color_picker_document_monitor_set_property (GObject      *object,
                                               guint         prop_id,
                                               const GValue *value,
                                               GParamSpec   *pspec)
{
  GbColorPickerDocumentMonitor *self = GB_COLOR_PICKER_DOCUMENT_MONITOR (object);

  switch (prop_id)
    {
    case PROP_BUFFER:
      gb_color_picker_document_monitor_set_buffer (self, g_value_get_object (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
gb_color_picker_document_monitor_class_init (GbColorPickerDocumentMonitorClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = gb_color_picker_document_monitor_finalize;
  object_class->get_property = gb_color_picker_document_monitor_get_property;
  object_class->set_property = gb_color_picker_document_monitor_set_property;

  properties [PROP_BUFFER] =
    g_param_spec_object ("buffer",
                         "Buffer",
                         "The GtkTextBuffer for the monitor.",
                         GTK_TYPE_TEXT_BUFFER,
                         (G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, N_PROPS, properties);

  signals [COLOR_FOUND] =
    g_signal_new_class_handler ("color-found",
                                G_TYPE_FROM_CLASS (klass),
                                G_SIGNAL_RUN_LAST,
                                NULL, NULL, NULL, NULL,
                                G_TYPE_NONE, 1, GSTYLE_TYPE_COLOR);
}

static void
gb_color_picker_document_monitor_init (GbColorPickerDocumentMonitor *self)
{
}

static void
queued_colorize_free (gpointer data)
{
  QueuedColorize *qc = data;

  g_clear_object (&qc->self);
  g_clear_object (&qc->buffer);
  g_clear_object (&qc->begin);
  g_clear_object (&qc->end);
  g_slice_free (QueuedColorize, qc);
}

static gboolean
gb_color_picker_document_monitor_queue_oper_cb (gpointer data)
{
  QueuedColorize *qc = data;

  g_assert (qc != NULL);
  g_assert (GB_IS_COLOR_PICKER_DOCUMENT_MONITOR (qc->self));
  g_assert (GTK_IS_TEXT_MARK (qc->begin));
  g_assert (GTK_IS_TEXT_MARK (qc->end));
  g_assert (GTK_TEXT_BUFFER (qc->buffer));

  block_signals (qc->self, IDE_BUFFER (qc->buffer));

  if (qc->buffer != NULL)
    {
      GtkTextIter begin;
      GtkTextIter end;

      gtk_text_buffer_get_iter_at_mark (qc->buffer, &begin, qc->begin);
      gtk_text_buffer_get_iter_at_mark (qc->buffer, &end, qc->end);

      if (qc->uncolorize)
        gb_color_picker_document_monitor_uncolorize (qc->self, qc->buffer, &begin, &end);
      else
        gb_color_picker_document_monitor_colorize (qc->self, qc->buffer, &begin, &end);

      gtk_text_buffer_delete_mark (qc->buffer, qc->begin);
      gtk_text_buffer_delete_mark (qc->buffer, qc->end);
    }

  unblock_signals (qc->self, IDE_BUFFER (qc->buffer));

  return G_SOURCE_REMOVE;
}

/**
 * gb_color_picker_document_monitor_queue_colorize:
 * @self: a #GbColorPickerDocumentMonitor
 *
 * This queues a region to be recolorized but does so after returning
 * to the main loop. This can be useful for situations where you do not
 * know if you are in a path that must retain a valid GtkTextIter.
 *
 * Since: 3.26
 */
static void
gb_color_picker_document_monitor_queue_oper (GbColorPickerDocumentMonitor *self,
                                             const GtkTextIter            *begin,
                                             const GtkTextIter            *end,
                                             gboolean                      uncolorize)
{
  QueuedColorize queued = { 0 };
  GtkTextBuffer *buffer;
  GtkTextIter real_begin;
  GtkTextIter real_end;

  g_return_if_fail (GB_IS_COLOR_PICKER_DOCUMENT_MONITOR (self));
  g_return_if_fail (self->buffer != NULL);
  g_return_if_fail (begin == NULL || GTK_TEXT_BUFFER (self->buffer) == gtk_text_iter_get_buffer (begin));
  g_return_if_fail (end == NULL || GTK_TEXT_BUFFER (self->buffer) == gtk_text_iter_get_buffer (end));

  buffer = GTK_TEXT_BUFFER (self->buffer);

  gtk_text_buffer_get_bounds (buffer, &real_begin, &real_end);

  if (begin)
    real_begin = *begin;

  if (end)
    real_end = *end;

  queued.self = g_object_ref (self);
  queued.buffer = g_object_ref (buffer);
  queued.begin = g_object_ref (gtk_text_buffer_create_mark (buffer, NULL, &real_begin, TRUE));
  queued.end = g_object_ref (gtk_text_buffer_create_mark (buffer, NULL, &real_end, FALSE));
  queued.uncolorize = !!uncolorize;

  gdk_threads_add_idle_full (G_PRIORITY_LOW,
                             gb_color_picker_document_monitor_queue_oper_cb,
                             g_slice_dup (QueuedColorize, &queued),
                             queued_colorize_free);
}

/**
 * gb_color_picker_document_monitor_queue_colorize:
 * @self: a #GbColorPickerDocumentMonitor
 *
 * This queues a region to be recolorized but does so after returning
 * to the main loop. This can be useful for situations where you do not
 * know if you are in a path that must retain a valid GtkTextIter.
 *
 * Since: 3.26
 */
void
gb_color_picker_document_monitor_queue_colorize (GbColorPickerDocumentMonitor *self,
                                                 const GtkTextIter            *begin,
                                                 const GtkTextIter            *end)
{
  gb_color_picker_document_monitor_queue_oper (self, begin, end, FALSE);
}

/**
 * gb_color_picker_document_monitor_queue_uncolorize:
 * @self: a #GbColorPickerDocumentMonitor
 *
 * This queues a region to be uncolorized but does so after returning
 * to the main loop. This can be useful for situations where you do not
 * know if you are in a path that must retain a valid GtkTextIter.
 *
 * Since: 3.26
 */
void
gb_color_picker_document_monitor_queue_uncolorize (GbColorPickerDocumentMonitor *self,
                                                   const GtkTextIter            *begin,
                                                   const GtkTextIter            *end)
{
  gb_color_picker_document_monitor_queue_oper (self, begin, end, TRUE);
}
