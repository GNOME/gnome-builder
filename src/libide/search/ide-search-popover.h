/* ide-search-popover.h
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

#if !defined (IDE_SEARCH_INSIDE) && !defined (IDE_SEARCH_COMPILATION)
# error "Only <libide-search.h> can be included directly."
#endif

#include <gtk/gtk.h>

#include <libide-core.h>

G_BEGIN_DECLS

#define IDE_TYPE_SEARCH_POPOVER (ide_search_popover_get_type())

IDE_AVAILABLE_IN_ALL
G_DECLARE_FINAL_TYPE (IdeSearchPopover, ide_search_popover, IDE, SEARCH_POPOVER, GtkPopover)

IDE_AVAILABLE_IN_ALL
GtkWidget  *ide_search_popover_new         (IdeContext       *context);
IDE_AVAILABLE_IN_ALL
IdeContext *ide_search_popover_get_context (IdeSearchPopover *self);

G_END_DECLS
