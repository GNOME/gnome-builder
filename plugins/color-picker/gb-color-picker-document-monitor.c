/* gb-color-picker-document-monitor.c
 *
 * Copyright (C) 2016 sebastien lafargue <slafargue@gnome.org>
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

G_DEFINE_TYPE (GbColorPickerDocumentMonitor, gb_color_picker_document_monitor, G_TYPE_OBJECT)

enum {
  PROP_0,
  PROP_BUFFER,
  N_PROPS
};

static GParamSpec *properties [N_PROPS];

enum {
  COLOR_FOUND,
  LAST_SIGNAL
};

static guint signals [LAST_SIGNAL];

static void
block_signals (GbColorPickerDocumentMonitor *self)
{
  g_assert (GB_IS_COLOR_PICKER_DOCUMENT_MONITOR (self));

  g_signal_handler_block (self->buffer, self->cursor_notify_handler_id);
  g_signal_handler_block (self->buffer, self->insert_handler_id);
  g_signal_handler_block (self->buffer, self->insert_after_handler_id);
  g_signal_handler_block (self->buffer, self->delete_handler_id);
  g_signal_handler_block (self->buffer, self->delete_after_handler_id);
}

static void
unblock_signals (GbColorPickerDocumentMonitor *self)
{
  g_assert (GB_IS_COLOR_PICKER_DOCUMENT_MONITOR (self));

  g_signal_handler_unblock (self->buffer, self->cursor_notify_handler_id);
  g_signal_handler_unblock (self->buffer, self->insert_handler_id);
  g_signal_handler_unblock (self->buffer, self->insert_after_handler_id);
  g_signal_handler_unblock (self->buffer, self->delete_handler_id);
  g_signal_handler_unblock (self->buffer, self->delete_after_handler_id);
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

  block_signals (self);
  gb_color_picker_helper_set_color_tag_at_iter (&cursor, color, TRUE);
  unblock_signals (self);
}

static void
remove_color_tag_foreach_cb (GtkTextTag *tag,
                             GPtrArray  *taglist)
{
  const gchar *name;

  g_assert (GTK_IS_TEXT_TAG (tag));
  g_assert (taglist != NULL);

  g_object_get (G_OBJECT (tag), "name", &name, NULL);
  if (!ide_str_empty0 (name) && g_str_has_prefix (name, COLOR_TAG_PREFIX))
    g_ptr_array_add (taglist, tag);
}

void
gb_color_picker_document_monitor_uncolorize (GbColorPickerDocumentMonitor *self,
                                             GtkTextIter                  *begin,
                                             GtkTextIter                  *end)
{
  g_autoptr (GPtrArray) taglist = NULL;
  g_autofree gchar *name = NULL;
  g_autoptr (GSList) tags = NULL;
  GtkTextTagTable *tag_table;
  GtkTextIter real_begin;
  GtkTextIter real_end;
  GtkTextTag *color_tag;
  GtkTextTag *tag;
  GSList *l;
  gint n;

  g_return_if_fail (GB_IS_COLOR_PICKER_DOCUMENT_MONITOR (self));
  g_return_if_fail (self->buffer != NULL);

  tag_table = gtk_text_buffer_get_tag_table (GTK_TEXT_BUFFER (self->buffer));
  if (begin == NULL && end == NULL)
    {
      taglist = g_ptr_array_new ();
      gtk_text_tag_table_foreach (tag_table, (GtkTextTagTableForeach)remove_color_tag_foreach_cb, taglist);
      for (n = 0; n < taglist->len; ++n)
        gtk_text_tag_table_remove (tag_table, g_ptr_array_index (taglist, n));

      return;
    }

  if (begin == NULL)
    gtk_text_buffer_get_start_iter (GTK_TEXT_BUFFER (self->buffer), &real_begin);
  else
    real_begin = *begin;

  if (end == NULL)
    gtk_text_buffer_get_end_iter (GTK_TEXT_BUFFER (self->buffer), &real_end);
  else
    real_end = *end;

  while (TRUE)
    {
      color_tag = NULL;
      tags = gtk_text_iter_get_toggled_tags (&real_begin, TRUE);
      for (l = tags; l != NULL; l = g_slist_next (l))
        {
          tag = l->data;
          g_object_get (G_OBJECT (tag), "name", &name, NULL);
          if (!ide_str_empty0 (name) && g_str_has_prefix (name, COLOR_TAG_PREFIX))
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

void
gb_color_picker_document_monitor_colorize (GbColorPickerDocumentMonitor *self,
                                           GtkTextIter                  *begin,
                                           GtkTextIter                  *end)
{
  g_autofree gchar *text = NULL;
  g_autoptr(GPtrArray) items = NULL;
  GstyleColorItem *item;
  GstyleColor *color;
  GtkTextTag *tag;
  GtkTextIter real_begin;
  GtkTextIter real_end;
  GtkTextIter tag_begin;
  GtkTextIter tag_end;
  gint offset;
  gint len;
  gint n;
  gint pos;

  g_return_if_fail (GB_IS_COLOR_PICKER_DOCUMENT_MONITOR (self));
  g_return_if_fail (self->buffer != NULL);

  if (begin == NULL)
    gtk_text_buffer_get_start_iter (GTK_TEXT_BUFFER (self->buffer), &real_begin);
  else
    real_begin = *begin;

  if (end == NULL)
    gtk_text_buffer_get_end_iter (GTK_TEXT_BUFFER (self->buffer), &real_end);
  else
    real_end = *end;

  if (gtk_text_iter_equal (&real_begin, &real_end))
    return;

  offset = gtk_text_iter_get_offset (&real_begin);
  text = gtk_text_buffer_get_slice (GTK_TEXT_BUFFER (self->buffer), &real_begin, &real_end, TRUE);

  items = gstyle_color_parse (text);
  for (n = 0; n < items->len; ++n)
    {
      item = g_ptr_array_index (items, n);
      pos = offset + gstyle_color_item_get_start (item);
      gtk_text_buffer_get_iter_at_offset (GTK_TEXT_BUFFER (self->buffer), &tag_begin, pos);
      len = gstyle_color_item_get_len (item);
      gtk_text_buffer_get_iter_at_offset (GTK_TEXT_BUFFER (self->buffer), &tag_end, pos + len);
      color = (GstyleColor *)gstyle_color_item_get_color (item);

      tag = gb_color_picker_helper_create_color_tag (GTK_TEXT_BUFFER (self->buffer), color);
      gtk_text_buffer_apply_tag (GTK_TEXT_BUFFER (self->buffer), tag, &tag_begin, &tag_end);
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
  GtkTextTag *tag;
  GstyleColor *color;
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

      gb_color_picker_document_monitor_uncolorize (self, &begin, &end);
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

  gb_color_picker_document_monitor_colorize (self, &begin, &end);
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

  g_assert (GB_IS_COLOR_PICKER_DOCUMENT_MONITOR (self));
  g_assert (GTK_IS_TEXT_BUFFER (buffer));
  g_assert (GTK_IS_TEXT_TAG (tag));

  tag_table = gtk_text_buffer_get_tag_table (GTK_TEXT_BUFFER (self->buffer));
  g_object_get (G_OBJECT (tag), "name", &name, NULL);

  if (!ide_str_empty0 (name) &&
      g_str_has_prefix (name, COLOR_TAG_PREFIX) &&
      gtk_text_tag_table_lookup (tag_table, name))
    {
      gtk_text_tag_table_remove (tag_table, tag);
    }
}

static void
text_deleted_cb (GbColorPickerDocumentMonitor *self,
                 GtkTextIter                  *begin,
                 GtkTextIter                  *end,
                 GtkTextBuffer                *buffer)
{
  GtkTextIter recolor_begin;
  GtkTextIter recolor_end;

  g_assert (GB_IS_COLOR_PICKER_DOCUMENT_MONITOR (self));
  g_assert (GTK_IS_TEXT_BUFFER (buffer));

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

  gb_color_picker_document_monitor_colorize (self, &recolor_begin, &recolor_end);
}

static void
cursor_moved_cb (GbColorPickerDocumentMonitor *self,
                 GParamSpec                   *prop,
                 GtkTextBuffer                *buffer)
{
  GtkTextTag *tag;
  GtkTextMark *insert;
  GtkTextIter cursor;
  GstyleColor *current_color;
  GtkTextIter begin, end;

  g_assert (GB_IS_COLOR_PICKER_DOCUMENT_MONITOR (self));
  g_assert (GTK_IS_TEXT_BUFFER (buffer));

  if (self->is_in_user_action)
    {
      gtk_text_buffer_end_user_action (GTK_TEXT_BUFFER (self->buffer));
      self->is_in_user_action = FALSE;
    }

  insert = gtk_text_buffer_get_insert (GTK_TEXT_BUFFER(self->buffer));
  gtk_text_buffer_get_iter_at_mark (GTK_TEXT_BUFFER(self->buffer), &cursor, insert);

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

  self->cursor_notify_handler_id = g_signal_connect_object (GTK_TEXT_BUFFER (self->buffer),
                                                            "notify::cursor-position",
                                                            G_CALLBACK (cursor_moved_cb),
                                                            self,
                                                            G_CONNECT_SWAPPED);
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
  g_return_if_fail (IDE_IS_BUFFER (buffer));

  if (self->buffer != buffer)
    {
      self->buffer = buffer;
      g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_BUFFER]);

      if (buffer != NULL)
        start_monitor (self);
      else
        stop_monitor (self);
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
color_found_handler_cb (GbColorPickerDocumentMonitor *self,
                        GstyleColor                  *color)
{
  g_assert (GB_IS_COLOR_PICKER_DOCUMENT_MONITOR (self));
  g_assert (GSTYLE_IS_COLOR (color));

  g_object_unref (color);
}

static void
gb_color_picker_document_monitor_class_init (GbColorPickerDocumentMonitorClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = gb_color_picker_document_monitor_finalize;
  object_class->get_property = gb_color_picker_document_monitor_get_property;
  object_class->set_property = gb_color_picker_document_monitor_set_property;

  properties[PROP_BUFFER] =
    g_param_spec_object ("buffer",
                         "Buffer",
                         "The GtkTextBuffer for the monitor.",
                         GTK_TYPE_TEXT_BUFFER,
                         (G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, N_PROPS, properties);

/* do not work, try with class offset ? */
  signals [COLOR_FOUND] = g_signal_new_class_handler ("color-found",
                                                      G_TYPE_FROM_CLASS (klass),
                                                      G_SIGNAL_RUN_CLEANUP,
                                                      G_CALLBACK (color_found_handler_cb),
                                                      NULL, NULL, NULL,
                                                      G_TYPE_NONE,
                                                      1,
                                                      GSTYLE_TYPE_COLOR);
}

static void
gb_color_picker_document_monitor_init (GbColorPickerDocumentMonitor *self)
{
}
