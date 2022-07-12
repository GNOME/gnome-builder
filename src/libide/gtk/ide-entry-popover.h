/* ide-entry-popover.h
 *
 * Copyright (C) 2015-2022 Christian Hergert <christian@hergert.me>
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

#pragma once

#include <gtk/gtk.h>

#include <libide-core.h>

G_BEGIN_DECLS

#define IDE_TYPE_ENTRY_POPOVER (ide_entry_popover_get_type())

IDE_AVAILABLE_IN_ALL
G_DECLARE_DERIVABLE_TYPE (IdeEntryPopover, ide_entry_popover, IDE, ENTRY_POPOVER, GtkPopover)

struct _IdeEntryPopoverClass
{
  GtkPopoverClass parent;

  /**
   * IdeEntryPopover::activate:
   * @self: The #IdeEntryPopover instance.
   * @text: The text at the time of activation.
   *
   * This signal is emitted when the popover's forward button is activated.
   * Connect to this signal to perform your forward progress.
   */
  void (*activate) (IdeEntryPopover *self,
                    const gchar      *text);

  /**
   * IdeEntryPopover::insert-text:
   * @self: A #IdeEntryPopover.
   * @position: the position in UTF-8 characters.
   * @chars: the NULL terminated UTF-8 text to insert.
   * @n_chars: the number of UTF-8 characters in chars.
   *
   * Use this signal to determine if text should be allowed to be inserted
   * into the text buffer. Return GDK_EVENT_STOP to prevent the text from
   * being inserted.
   */
  gboolean (*insert_text) (IdeEntryPopover *self,
                           guint             position,
                           const gchar      *chars,
                           guint             n_chars);


  /**
   * IdeEntryPopover::changed:
   * @self: A #IdeEntryPopover.
   *
   * This signal is emitted when the entry text changes.
   */
  void (*changed) (IdeEntryPopover *self);
};

IDE_AVAILABLE_IN_ALL
GtkWidget   *ide_entry_popover_new             (void);
IDE_AVAILABLE_IN_ALL
const gchar *ide_entry_popover_get_text        (IdeEntryPopover *self);
IDE_AVAILABLE_IN_ALL
void         ide_entry_popover_set_text        (IdeEntryPopover *self,
                                                const gchar     *text);
IDE_AVAILABLE_IN_ALL
const gchar *ide_entry_popover_get_message     (IdeEntryPopover *self);
IDE_AVAILABLE_IN_ALL
void         ide_entry_popover_set_message     (IdeEntryPopover *self,
                                                const gchar     *message);
IDE_AVAILABLE_IN_ALL
const gchar *ide_entry_popover_get_title       (IdeEntryPopover *self);
IDE_AVAILABLE_IN_ALL
void         ide_entry_popover_set_title       (IdeEntryPopover *self,
                                                const gchar     *title);
IDE_AVAILABLE_IN_ALL
const gchar *ide_entry_popover_get_button_text (IdeEntryPopover *self);
IDE_AVAILABLE_IN_ALL
void         ide_entry_popover_set_button_text (IdeEntryPopover *self,
                                                const gchar     *button_text);
IDE_AVAILABLE_IN_ALL
gboolean     ide_entry_popover_get_ready       (IdeEntryPopover *self);
IDE_AVAILABLE_IN_ALL
void         ide_entry_popover_set_ready       (IdeEntryPopover *self,
                                                gboolean         ready);
IDE_AVAILABLE_IN_ALL
void         ide_entry_popover_select_all      (IdeEntryPopover *self);

G_END_DECLS
