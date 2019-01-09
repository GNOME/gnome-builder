/* gbp-history-item.c
 *
 * Copyright 2017-2019 Christian Hergert <chergert@redhat.com>
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

#define G_LOG_DOMAIN "gbp-history-item"

#include "gbp-history-item.h"

#define DISTANCE_LINES_THRESH 10

struct _GbpHistoryItem
{
  GObject      parent_instance;

  IdeContext  *context;
  GtkTextMark *mark;
  GFile       *file;

  guint        line;
};

G_DEFINE_TYPE (GbpHistoryItem, gbp_history_item, G_TYPE_OBJECT)

static void
gbp_history_item_dispose (GObject *object)
{
  GbpHistoryItem *self = (GbpHistoryItem *)object;

  g_clear_weak_pointer (&self->context);

  if (self->mark != NULL)
    {
      GtkTextBuffer *buffer = gtk_text_mark_get_buffer (self->mark);

      if (buffer != NULL)
        gtk_text_buffer_delete_mark (buffer, self->mark);
    }

  g_clear_object (&self->mark);
  g_clear_object (&self->file);

  G_OBJECT_CLASS (gbp_history_item_parent_class)->dispose (object);
}

static void
gbp_history_item_class_init (GbpHistoryItemClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->dispose = gbp_history_item_dispose;
}

static void
gbp_history_item_init (GbpHistoryItem *self)
{
}

GbpHistoryItem *
gbp_history_item_new (GtkTextMark *mark)
{
  g_autoptr(IdeContext) context = NULL;
  GbpHistoryItem *item;
  GtkTextBuffer *buffer;
  GtkTextIter iter;
  GFile *file;

  g_return_val_if_fail (GTK_IS_TEXT_MARK (mark), NULL);

  buffer = gtk_text_mark_get_buffer (mark);
  g_return_val_if_fail (IDE_IS_BUFFER (buffer), NULL);

  item = g_object_new (GBP_TYPE_HISTORY_ITEM, NULL);
  item->mark = g_object_ref (mark);

  context = ide_buffer_ref_context (IDE_BUFFER (buffer));
  g_set_weak_pointer (&item->context, context);

  gtk_text_buffer_get_iter_at_mark (buffer, &iter, mark);
  item->line = gtk_text_iter_get_line (&iter);

  file = ide_buffer_get_file (IDE_BUFFER (buffer));
  item->file = g_object_ref (file);

  return g_steal_pointer (&item);
}

gboolean
gbp_history_item_chain (GbpHistoryItem *self,
                        GbpHistoryItem *other)
{
  GtkTextBuffer *buffer;

  g_return_val_if_fail (GBP_IS_HISTORY_ITEM (self), FALSE);
  g_return_val_if_fail (GBP_IS_HISTORY_ITEM (other), FALSE);

  if (self->mark != NULL && other->mark != NULL &&
      NULL != (buffer = gtk_text_mark_get_buffer (self->mark)) &&
      buffer == gtk_text_mark_get_buffer (other->mark))
    {
      GtkTextIter self_iter;
      GtkTextIter other_iter;

      gtk_text_buffer_get_iter_at_mark (buffer, &self_iter, self->mark);
      gtk_text_buffer_get_iter_at_mark (buffer, &other_iter, other->mark);

      if (ABS (gtk_text_iter_get_line (&self_iter) -
               gtk_text_iter_get_line (&other_iter)) < DISTANCE_LINES_THRESH)
        return TRUE;
    }

  if (self->file != NULL &&
      other->file != NULL &&
      g_file_equal (self->file, other->file))
    {
      if (ABS ((gint)self->line - (gint)other->line) < DISTANCE_LINES_THRESH)
        return TRUE;
    }

  return FALSE;
}

gchar *
gbp_history_item_get_label (GbpHistoryItem *self)
{
  g_autofree gchar *title = NULL;
  GtkTextBuffer *buffer;
  GtkTextIter iter;
  guint line;

  g_return_val_if_fail (GBP_IS_HISTORY_ITEM (self), NULL);
  g_return_val_if_fail (self->mark != NULL, NULL);

  buffer = gtk_text_mark_get_buffer (self->mark);
  if (buffer == NULL)
    return NULL;
  g_return_val_if_fail (IDE_IS_BUFFER (buffer), NULL);

  gtk_text_buffer_get_iter_at_mark (buffer, &iter, self->mark);
  line = gtk_text_iter_get_line (&iter) + 1;
  title = ide_buffer_dup_title (IDE_BUFFER (buffer));

  return g_strdup_printf ("%s <span fgcolor='32767'>%u</span>", title, line);
}

/**
 * gbp_history_item_get_location:
 * @self: a #GbpHistoryItem
 *
 * Gets an #IdeLocation represented by this item.
 *
 * Returns: (transfer full): A new #IdeLocation
 *
 * Since: 3.32
 */
IdeLocation *
gbp_history_item_get_location (GbpHistoryItem *self)
{
  GtkTextBuffer *buffer;
  GtkTextIter iter;

  g_return_val_if_fail (GBP_IS_HISTORY_ITEM (self), NULL);
  g_return_val_if_fail (self->mark != NULL, NULL);

  if (self->context == NULL)
    return NULL;

  if (!(buffer = gtk_text_mark_get_buffer (self->mark)))
    return ide_location_new (self->file, self->line, 0);

  g_return_val_if_fail (IDE_IS_BUFFER (buffer), NULL);
  gtk_text_buffer_get_iter_at_mark (buffer, &iter, self->mark);

  return ide_buffer_get_iter_location (IDE_BUFFER (buffer), &iter);
}

/**
 * gbp_history_item_get_file:
 *
 * Returns: (transfer none): a #GFile.
 *
 * Since: 3.32
 */
GFile *
gbp_history_item_get_file (GbpHistoryItem *self)
{
  g_return_val_if_fail (GBP_IS_HISTORY_ITEM (self), NULL);

  return self->file;
}

/**
 * gbp_history_item_get_line:
 *
 * Gets the line for the history item.
 *
 * If the text mark is still valid, it will be used to locate the
 * mark which may have moved.
 *
 * Since: 3.32
 */
guint
gbp_history_item_get_line (GbpHistoryItem *self)
{
  GtkTextBuffer *buffer;

  g_return_val_if_fail (GBP_IS_HISTORY_ITEM (self), 0);

  buffer = gtk_text_mark_get_buffer (self->mark);

  if (buffer != NULL)
    {
      GtkTextIter iter;

      gtk_text_buffer_get_iter_at_mark (buffer, &iter, self->mark);
      return gtk_text_iter_get_line (&iter);
    }

  return self->line;
}
