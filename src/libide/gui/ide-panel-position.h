/* ide-panel-position.h
 *
 * Copyright 2022 Christian Hergert <chergert@redhat.com>
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

#define IDE_TYPE_PANEL_POSITION (ide_panel_position_get_type())

typedef struct _IdePanelPosition IdePanelPosition;

IDE_AVAILABLE_IN_ALL
GType             ide_panel_position_get_type         (void) G_GNUC_CONST;
IDE_AVAILABLE_IN_ALL
IdePanelPosition *ide_panel_position_new              (void);
IDE_AVAILABLE_IN_ALL
IdePanelPosition *ide_panel_position_ref              (IdePanelPosition  *self);
IDE_AVAILABLE_IN_ALL
void              ide_panel_position_unref            (IdePanelPosition  *self);
IDE_AVAILABLE_IN_ALL
gboolean          ide_panel_position_get_edge         (IdePanelPosition  *self,
                                                       PanelDockPosition *edge);
IDE_AVAILABLE_IN_ALL
void              ide_panel_position_set_edge         (IdePanelPosition  *self,
                                                       PanelDockPosition  edge);
IDE_AVAILABLE_IN_ALL
gboolean          ide_panel_position_get_row          (IdePanelPosition  *self,
                                                       guint             *row);
IDE_AVAILABLE_IN_ALL
void              ide_panel_position_set_row          (IdePanelPosition  *self,
                                                       guint              row);
IDE_AVAILABLE_IN_ALL
gboolean          ide_panel_position_get_column       (IdePanelPosition  *self,
                                                       guint             *column);
IDE_AVAILABLE_IN_ALL
void              ide_panel_position_set_column       (IdePanelPosition  *self,
                                                       guint              column);
IDE_AVAILABLE_IN_ALL
gboolean          ide_panel_position_get_depth        (IdePanelPosition  *self,
                                                       guint             *depth);
IDE_AVAILABLE_IN_ALL
void              ide_panel_position_set_depth        (IdePanelPosition  *self,
                                                       guint              depth);
IDE_AVAILABLE_IN_ALL
gboolean          ide_panel_position_is_indeterminate (IdePanelPosition  *self);

G_DEFINE_AUTOPTR_CLEANUP_FUNC (IdePanelPosition, ide_panel_position_unref)

G_END_DECLS
