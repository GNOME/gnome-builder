/* ide-radio-box.h
 *
 * Copyright (C) 2016-2022 Christian Hergert <chergert@redhat.com>
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

#if !defined (IDE_GTK_INSIDE) && !defined (IDE_GTK_COMPILATION)
# error "Only <libide-gtk.h> can be included directly."
#endif

#include <gtk/gtk.h>

#include <libide-core.h>

G_BEGIN_DECLS

#define IDE_TYPE_RADIO_BOX (ide_radio_box_get_type())

IDE_AVAILABLE_IN_ALL
G_DECLARE_FINAL_TYPE (IdeRadioBox, ide_radio_box, IDE, RADIO_BOX, GtkWidget)

IDE_AVAILABLE_IN_ALL
GtkWidget   *ide_radio_box_new           (void);
IDE_AVAILABLE_IN_ALL
void         ide_radio_box_add_item      (IdeRadioBox *self,
                                          const gchar *id,
                                          const gchar *text);
IDE_AVAILABLE_IN_ALL
void         ide_radio_box_remove_item   (IdeRadioBox *self,
                                          const gchar *id);
IDE_AVAILABLE_IN_ALL
const gchar *ide_radio_box_get_active_id (IdeRadioBox *self);
IDE_AVAILABLE_IN_ALL
void         ide_radio_box_set_active_id (IdeRadioBox *self,
                                          const gchar *id);

G_END_DECLS
