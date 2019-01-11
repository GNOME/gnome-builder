/* ide-lsp-completion-item.h
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

#if !defined (IDE_LSP_INSIDE) && !defined (IDE_LSP_COMPILATION)
# error "Only <libide-lsp.h> can be included directly."
#endif

#include <libide-sourceview.h>

G_BEGIN_DECLS

#define IDE_TYPE_LSP_COMPLETION_ITEM (ide_lsp_completion_item_get_type())

IDE_AVAILABLE_IN_3_32
G_DECLARE_FINAL_TYPE (IdeLspCompletionItem, ide_lsp_completion_item, IDE, LSP_COMPLETION_ITEM, GObject)

IDE_AVAILABLE_IN_3_32
IdeLspCompletionItem *ide_lsp_completion_item_new             (GVariant             *variant);
IDE_AVAILABLE_IN_3_32
const gchar          *ide_lsp_completion_item_get_icon_name   (IdeLspCompletionItem *self);
IDE_AVAILABLE_IN_3_32
const gchar          *ide_lsp_completion_item_get_return_type (IdeLspCompletionItem *self);
IDE_AVAILABLE_IN_3_32
const gchar          *ide_lsp_completion_item_get_detail      (IdeLspCompletionItem *self);
IDE_AVAILABLE_IN_3_32
gchar                *ide_lsp_completion_item_get_markup      (IdeLspCompletionItem *self,
                                                               const gchar          *typed_text);
IDE_AVAILABLE_IN_3_32
IdeSnippet           *ide_lsp_completion_item_get_snippet     (IdeLspCompletionItem *self);

G_END_DECLS
