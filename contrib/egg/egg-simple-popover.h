/* egg-simple-popover.h
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

#ifndef EGG_SIMPLE_POPOVER_H
#define EGG_SIMPLE_POPOVER_H

#include <gtk/gtk.h>

G_BEGIN_DECLS

#define EGG_TYPE_SIMPLE_POPOVER (egg_simple_popover_get_type())

G_DECLARE_DERIVABLE_TYPE (EggSimplePopover, egg_simple_popover, EGG, SIMPLE_POPOVER, GtkPopover)

struct _EggSimplePopoverClass
{
  GtkPopoverClass parent;

  /**
   * EggSimplePopover::activate:
   * @self: The #EggSimplePopover instance.
   * @text: The text at the time of activation.
   *
   * This signal is emitted when the popover's forward button is activated.
   * Connect to this signal to perform your forward progress.
   */
  void (*activate) (EggSimplePopover *self,
                    const gchar     *text);

  /**
   * EggSimplePopover::insert-text:
   * @self: A #EggSimplePopover.
   * @position: the position in UTF-8 characters.
   * @chars: the NULL terminated UTF-8 text to insert.
   * @n_chars: the number of UTF-8 characters in chars.
   *
   * Use this signal to determine if text should be allowed to be inserted
   * into the text buffer. Return GDK_EVENT_STOP to prevent the text from
   * being inserted.
   */
  gboolean (*insert_text) (EggSimplePopover *self,
                           guint            position,
                           const gchar     *chars,
                           guint            n_chars);

  
  /**
   * EggSimplePopover::changed:
   * @self: A #EggSimplePopover.
   *
   * This signal is emitted when the entry text changes.
   */
  void (*changed) (EggSimplePopover *self);
};

GtkWidget   *egg_simple_popover_new             (void);
const gchar *egg_simple_popover_get_text        (EggSimplePopover *self);
void         egg_simple_popover_set_text        (EggSimplePopover *self,
                                                const gchar     *text);
const gchar *egg_simple_popover_get_message     (EggSimplePopover *self);
void         egg_simple_popover_set_message     (EggSimplePopover *self,
                                                const gchar     *message);
const gchar *egg_simple_popover_get_title       (EggSimplePopover *self);
void         egg_simple_popover_set_title       (EggSimplePopover *self,
                                                const gchar     *title);
const gchar *egg_simple_popover_get_button_text (EggSimplePopover *self);
void         egg_simple_popover_set_button_text (EggSimplePopover *self,
                                                const gchar     *button_text);
gboolean     egg_simple_popover_get_ready       (EggSimplePopover *self);
void         egg_simple_popover_set_ready       (EggSimplePopover *self,
                                                gboolean         ready);

G_END_DECLS

#endif /* EGG_SIMPLE_POPOVER_H */
