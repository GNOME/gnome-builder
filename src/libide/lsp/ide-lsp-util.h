/* ide-lsp-util.h
 *
 * Copyright 2017-2019 Christian Hergert <chergert@redhat.com>
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

#if !defined (IDE_LSP_INSIDE) && !defined (IDE_LSP_COMPILATION)
# error "Only <libide-lsp.h> can be included directly."
#endif

#include <libide-code.h>

G_BEGIN_DECLS

IDE_AVAILABLE_IN_ALL
IdeSymbolKind ide_lsp_decode_symbol_kind     (guint     kind);
IDE_AVAILABLE_IN_ALL
IdeSymbolKind ide_lsp_decode_completion_kind (guint     kind);
IDE_AVAILABLE_IN_ALL
IdeTextEdit  *ide_lsp_decode_text_edit       (GVariant *text_edit,
                                              GFile    *gfile);

G_END_DECLS
