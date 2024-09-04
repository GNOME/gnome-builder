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

#include <adwaita.h>
#include <libide-core.h>

G_BEGIN_DECLS

typedef enum
{
  IDE_HEADER_BAR_POSITION_LEFT,
  IDE_HEADER_BAR_POSITION_RIGHT,
  IDE_HEADER_BAR_POSITION_LEFT_OF_CENTER,
  IDE_HEADER_BAR_POSITION_RIGHT_OF_CENTER,
  IDE_HEADER_BAR_POSITION_LAST,
} IdeHeaderBarPosition;

#define IDE_TYPE_HEADER_BAR (ide_header_bar_get_type())

IDE_AVAILABLE_IN_ALL
G_DECLARE_DERIVABLE_TYPE (IdeHeaderBar, ide_header_bar, IDE, HEADER_BAR, GtkWidget)

struct _IdeHeaderBarClass
{
  GtkWidgetClass parent_class;
};

IDE_AVAILABLE_IN_ALL
GtkWidget  *ide_header_bar_new         (void);
IDE_AVAILABLE_IN_ALL
void        ide_header_bar_add         (IdeHeaderBar         *self,
                                        IdeHeaderBarPosition  position,
                                        int                   priority,
                                        GtkWidget            *widget);
IDE_AVAILABLE_IN_ALL
void        ide_header_bar_remove      (IdeHeaderBar         *self,
                                        GtkWidget            *widget);
IDE_AVAILABLE_IN_ALL
const char *ide_header_bar_get_menu_id (IdeHeaderBar         *self);
IDE_AVAILABLE_IN_ALL
void        ide_header_bar_set_menu_id (IdeHeaderBar         *self,
                                        const char           *menu_id);
IDE_AVAILABLE_IN_47
void        ide_header_bar_setup_menu  (GtkPopoverMenu       *popover);

G_END_DECLS
