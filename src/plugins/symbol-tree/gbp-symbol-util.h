/* gbp-symbol-util.h
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

#include <libide-code.h>

G_BEGIN_DECLS

void           gbp_symbol_find_nearest_scope_async  (IdeBuffer            *buffer,
                                                     GCancellable         *cancellable,
                                                     GAsyncReadyCallback   callback,
                                                     gpointer              user_data);
IdeSymbol     *gbp_symbol_find_nearest_scope_finish (IdeBuffer            *buffer,
                                                     GAsyncResult         *result,
                                                     GError              **error);
void           gbp_symbol_get_symbol_tree_async     (IdeBuffer            *buffer,
                                                     GCancellable         *cancellable,
                                                     GAsyncReadyCallback   callback,
                                                     gpointer              user_data);
IdeSymbolTree *gbp_symbol_get_symbol_tree_finish    (IdeBuffer            *buffer,
                                                     GAsyncResult         *result,
                                                     GError              **error);

G_END_DECLS
