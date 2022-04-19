/* ide-ctags-completion-item.h
 *
 * Copyright 2015-2019 Christian Hergert <christian@hergert.me>
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
#include <libide-sourceview.h>

#include "ide-ctags-index.h"
#include "ide-ctags-results.h"

G_BEGIN_DECLS

#define IDE_TYPE_CTAGS_COMPLETION_ITEM (ide_ctags_completion_item_get_type())

G_DECLARE_FINAL_TYPE (IdeCtagsCompletionItem, ide_ctags_completion_item, IDE, CTAGS_COMPLETION_ITEM, GObject)

struct _IdeCtagsCompletionItem
{
  GObject                   parent_instance;
  const IdeCtagsIndexEntry *entry;
  IdeCtagsResults          *results;
};

IdeCtagsCompletionItem *ide_ctags_completion_item_new         (IdeCtagsResults          *results,
                                                               const IdeCtagsIndexEntry *entry);
gboolean                ide_ctags_completion_item_is_function (IdeCtagsCompletionItem   *self);
GtkSourceSnippet       *ide_ctags_completion_item_get_snippet (IdeCtagsCompletionItem   *self,
                                                               IdeFileSettings          *file_settings);

G_END_DECLS
