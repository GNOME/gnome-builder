/* gbp-gjs-symbol-node.h
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

#include <json-glib/json-glib.h>

#include <libide-code.h>

G_BEGIN_DECLS

#define GBP_TYPE_GJS_SYMBOL_NODE (gbp_gjs_symbol_node_get_type())

G_DECLARE_FINAL_TYPE (GbpGjsSymbolNode, gbp_gjs_symbol_node, GBP, GJS_SYMBOL_NODE, IdeSymbolNode)

GbpGjsSymbolNode *gbp_gjs_symbol_node_new            (JsonObject       *object);
guint             gbp_gjs_symbol_node_get_n_children (GbpGjsSymbolNode *self);
IdeSymbolNode    *gbp_gjs_symbol_node_get_nth_child  (GbpGjsSymbolNode *self,
                                                      guint             nth_child);

G_END_DECLS
