/* gb-simple-popover.h
 *
 * Copyright (C) 2015 Christian Hergert <christian@hergert.me>
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

#ifndef GB_SIMPLE_POPOVER_H
#define GB_SIMPLE_POPOVER_H

#include <gtk/gtk.h>

G_BEGIN_DECLS

#define GB_TYPE_SIMPLE_POPOVER (gb_simple_popover_get_type())

G_DECLARE_DERIVABLE_TYPE (GbSimplePopover, gb_simple_popover, GB, SIMPLE_POPOVER, GtkPopover)

struct _GbSimplePopoverClass
{
  GtkPopoverClass parent;

  /**
   * GbSimplePopover::activate:
   * @self: The #GbSimplePopover instance.
   * @text: The text at the time of activation.
   *
   * This signal is emitted when the popover's forward button is activated.
   * Connect to this signal to perform your forward progress.
   */
  void (*activate) (GbSimplePopover *self,
                    const gchar     *text);

  /**
   * GbSimplePopover::insert-text:
   * @self: A #GbSimplePopover.
   * @position: the position in UTF-8 characters.
   * @chars: the NULL terminated UTF-8 text to insert.
   * @n_chars: the number of UTF-8 characters in chars.
   *
   * Use this signal to determine if text should be allowed to be inserted
   * into the text buffer. Return GDK_EVENT_STOP to prevent the text from
   * being inserted.
   */
  gboolean (*insert_text) (GbSimplePopover *self,
                           guint            position,
                           const gchar     *chars,
                           guint            n_chars);

  
  /**
   * GbSimplePopover::changed:
   * @self: A #GbSimplePopover.
   *
   * This signal is emitted when the entry text changes.
   */
  void (*changed) (GbSimplePopover *self);
};

GtkWidget   *gb_simple_popover_new             (void);
const gchar *gb_simple_popover_get_text        (GbSimplePopover *self);
void         gb_simple_popover_set_text        (GbSimplePopover *self,
                                                const gchar     *text);
const gchar *gb_simple_popover_get_message     (GbSimplePopover *self);
void         gb_simple_popover_set_message     (GbSimplePopover *self,
                                                const gchar     *message);
const gchar *gb_simple_popover_get_title       (GbSimplePopover *self);
void         gb_simple_popover_set_title       (GbSimplePopover *self,
                                                const gchar     *title);
const gchar *gb_simple_popover_get_button_text (GbSimplePopover *self);
void         gb_simple_popover_set_button_text (GbSimplePopover *self,
                                                const gchar     *button_text);
gboolean     gb_simple_popover_get_ready       (GbSimplePopover *self);
void         gb_simple_popover_set_ready       (GbSimplePopover *self,
                                                gboolean         ready);

G_END_DECLS

#endif /* GB_SIMPLE_POPOVER_H */
