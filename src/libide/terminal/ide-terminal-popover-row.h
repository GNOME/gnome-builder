/* ide-terminal-popover-row.h
 *
 * Copyright 2019 Christian Hergert <chergert@redhat.com>
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

#include <gtk/gtk.h>

#include <libide-foundry.h>

G_BEGIN_DECLS

#define IDE_TYPE_TERMINAL_POPOVER_ROW (ide_terminal_popover_row_get_type())

G_DECLARE_FINAL_TYPE (IdeTerminalPopoverRow, ide_terminal_popover_row, IDE, TERMINAL_POPOVER_ROW, GtkListBoxRow)

GtkWidget  *ide_terminal_popover_row_new          (IdeRuntime            *runtime);
IdeRuntime *ide_terminal_popover_row_get_runtime  (IdeTerminalPopoverRow *self);
void        ide_terminal_popover_row_set_selected (IdeTerminalPopoverRow *self,
                                                   gboolean               selected);

G_END_DECLS
