/* ide-langserv-completion-item.h
 *
 * Copyright 2018 Christian Hergert <chergert@redhat.com>
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
 */

#pragma once

#include "ide-version-macros.h"

#include "completion/ide-completion-proposal.h"
#include "snippets/ide-snippet.h"

G_BEGIN_DECLS

#define IDE_TYPE_LANGSERV_COMPLETION_ITEM (ide_langserv_completion_item_get_type())

IDE_AVAILABLE_IN_3_30
G_DECLARE_FINAL_TYPE (IdeLangservCompletionItem, ide_langserv_completion_item, IDE, LANGSERV_COMPLETION_ITEM, GObject)

IDE_AVAILABLE_IN_3_30
IdeLangservCompletionItem *ide_langserv_completion_item_new             (GVariant *variant);
IDE_AVAILABLE_IN_3_30
const gchar               *ide_langserv_completion_item_get_icon_name   (IdeLangservCompletionItem *self);
IDE_AVAILABLE_IN_3_30
const gchar               *ide_langserv_completion_item_get_return_type (IdeLangservCompletionItem *self);
IDE_AVAILABLE_IN_3_30
const gchar               *ide_langserv_completion_item_get_detail      (IdeLangservCompletionItem *self);
IDE_AVAILABLE_IN_3_30
gchar                     *ide_langserv_completion_item_get_markup      (IdeLangservCompletionItem *self,
                                                                         const gchar               *typed_text);
IDE_AVAILABLE_IN_3_30
IdeSnippet                *ide_langserv_completion_item_get_snippet     (IdeLangservCompletionItem *self);

G_END_DECLS
