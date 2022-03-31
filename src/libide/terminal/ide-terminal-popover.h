/* ide-terminal-popover.h
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

#if !defined (IDE_TERMINAL_INSIDE) && !defined (IDE_TERMINAL_COMPILATION)
# error "Only <libide-terminal.h> can be included directly."
#endif

#include <gtk/gtk.h>

#include <libide-core.h>
#include <libide-foundry.h>

G_BEGIN_DECLS

#define IDE_TYPE_TERMINAL_POPOVER (ide_terminal_popover_get_type())

IDE_AVAILABLE_IN_ALL
G_DECLARE_FINAL_TYPE (IdeTerminalPopover, ide_terminal_popover, IDE, TERMINAL_POPOVER, GtkPopover)

IDE_AVAILABLE_IN_ALL
GtkWidget  *ide_terminal_popover_new         (void);
IDE_AVAILABLE_IN_ALL
IdeRuntime *ide_terminal_popover_get_runtime (IdeTerminalPopover *self);

G_END_DECLS
