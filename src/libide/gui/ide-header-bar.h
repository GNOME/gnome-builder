/* ide-header-bar.h
 *
 * Copyright 2014-2019 Christian Hergert <chergert@redhat.com>
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

#pragma once

#if !defined (IDE_GUI_INSIDE) && !defined (IDE_GUI_COMPILATION)
# error "Only <libide-gui.h> can be included directly."
#endif

#include <gtk/gtk.h>
#include <libide-core.h>

G_BEGIN_DECLS

#define IDE_TYPE_HEADER_BAR (ide_header_bar_get_type())

IDE_AVAILABLE_IN_3_32
G_DECLARE_DERIVABLE_TYPE (IdeHeaderBar, ide_header_bar, IDE, HEADER_BAR, GtkHeaderBar)

struct _IdeHeaderBarClass
{
  GtkHeaderBarClass parent_class;

  /*< private >*/
  gpointer _reserved[16];
};

IDE_AVAILABLE_IN_3_32
GtkWidget   *ide_header_bar_new                        (void);
IDE_AVAILABLE_IN_3_32
void         ide_header_bar_add_primary                (IdeHeaderBar *self,
                                                        GtkWidget    *widget);
IDE_AVAILABLE_IN_3_32
void         ide_header_bar_add_center_left            (IdeHeaderBar *self,
                                                        GtkWidget    *widget);
IDE_AVAILABLE_IN_3_32
void         ide_header_bar_add_secondary              (IdeHeaderBar *self,
                                                        GtkWidget    *widget);
IDE_AVAILABLE_IN_3_32
const gchar *ide_header_bar_get_menu_id                (IdeHeaderBar *self);
IDE_AVAILABLE_IN_3_32
void         ide_header_bar_set_menu_id                (IdeHeaderBar *self,
                                                        const gchar  *menu_id);
IDE_AVAILABLE_IN_3_32
gboolean     ide_header_bar_get_show_fullscreen_button (IdeHeaderBar *self);
IDE_AVAILABLE_IN_3_32
void         ide_header_bar_set_show_fullscreen_button (IdeHeaderBar *self,
                                                        gboolean      show_fullscreen_button);

G_END_DECLS
