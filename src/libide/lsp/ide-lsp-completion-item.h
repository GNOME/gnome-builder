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

IDE_AVAILABLE_IN_ALL
G_DECLARE_FINAL_TYPE (IdeLspCompletionItem, ide_lsp_completion_item, IDE, LSP_COMPLETION_ITEM, GObject)

IDE_AVAILABLE_IN_ALL
IdeLspCompletionItem *ide_lsp_completion_item_new                       (GVariant                *variant);
IDE_AVAILABLE_IN_ALL
const gchar          *ide_lsp_completion_item_get_icon_name             (IdeLspCompletionItem    *self);
IDE_AVAILABLE_IN_ALL
const gchar          *ide_lsp_completion_item_get_return_type           (IdeLspCompletionItem    *self);
IDE_AVAILABLE_IN_ALL
const gchar          *ide_lsp_completion_item_get_detail                (IdeLspCompletionItem    *self);
IDE_AVAILABLE_IN_ALL
GtkSourceSnippet     *ide_lsp_completion_item_get_snippet               (IdeLspCompletionItem    *self);

IDE_AVAILABLE_IN_ALL
GPtrArray            *ide_lsp_completion_item_get_additional_text_edits (IdeLspCompletionItem    *self,
                                                                         GFile                   *file);
IDE_AVAILABLE_IN_ALL
void                  ide_lsp_completion_item_display                   (IdeLspCompletionItem    *self,
                                                                         GtkSourceCompletionCell *cell,
                                                                         const char              *typed_text);

G_END_DECLS
