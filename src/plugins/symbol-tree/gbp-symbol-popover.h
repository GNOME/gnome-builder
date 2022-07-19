/* gbp-symbol-popover.h
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

#include <gtk/gtk.h>

#include <libide-code.h>

G_BEGIN_DECLS

#define GBP_TYPE_SYMBOL_POPOVER (gbp_symbol_popover_get_type())

G_DECLARE_FINAL_TYPE (GbpSymbolPopover, gbp_symbol_popover, GBP, SYMBOL_POPOVER, GtkPopover)

GtkWidget     *gbp_symbol_popover_new             (void);
IdeSymbolTree *gbp_symbol_popover_get_symbol_tree (GbpSymbolPopover *self);
void           gbp_symbol_popover_set_symbol_tree (GbpSymbolPopover *self,
                                                   IdeSymbolTree    *symbol_tree);
GListModel    *gbp_symbol_popover_get_model       (GbpSymbolPopover *self);

G_END_DECLS
