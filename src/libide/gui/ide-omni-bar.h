/* ide-omni-bar.h
 *
 * Copyright 2018-2019 Christian Hergert <chergert@redhat.com>
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

#include <libpanel.h>

#include <libide-core.h>

G_BEGIN_DECLS

#define IDE_TYPE_OMNI_BAR (ide_omni_bar_get_type())

IDE_AVAILABLE_IN_ALL
G_DECLARE_FINAL_TYPE (IdeOmniBar, ide_omni_bar, IDE, OMNI_BAR, PanelOmniBar)

IDE_AVAILABLE_IN_ALL
GtkWidget *ide_omni_bar_new                 (void);
IDE_AVAILABLE_IN_ALL
void       ide_omni_bar_add_status_icon     (IdeOmniBar  *self,
                                             GtkWidget   *widget,
                                             int          priority);
IDE_AVAILABLE_IN_ALL
void       ide_omni_bar_set_placeholder     (IdeOmniBar  *self,
                                             GtkWidget   *placeholder);
IDE_AVAILABLE_IN_ALL
void       ide_omni_bar_add_popover_section (IdeOmniBar  *self,
                                             GtkWidget   *widget,
                                             int          priority);

G_END_DECLS
