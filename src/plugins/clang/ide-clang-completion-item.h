/* ide-clang-completion-item.h
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

#include "proposals.h"

G_BEGIN_DECLS

#define IDE_TYPE_CLANG_COMPLETION_ITEM (ide_clang_completion_item_get_type())

G_DECLARE_FINAL_TYPE (IdeClangCompletionItem, ide_clang_completion_item, IDE, CLANG_COMPLETION_ITEM, GObject)

struct _IdeClangCompletionItem
{
  GObject        parent_instance;

  /* Owned reference to ensure @ref validity */
  GVariant      *results;

  /* Updated during search/filter */
  guint          priority;

  /* Extracted from @ref */
  IdeSymbolKind  kind;

  /* Raw access into @results */
  ProposalRef    ref;

  /* Cached on first generation */
  char          *params;

  /* Unowned references */
  const char    *keyword;
  const char    *return_type;
  const char    *icon_name;
  const char    *typed_text;
};

IdeClangCompletionItem *ide_clang_completion_item_new         (GVariant                *results,
                                                               ProposalRef              ref);
GtkSourceSnippet       *ide_clang_completion_item_get_snippet (IdeClangCompletionItem  *self,
                                                               IdeFileSettings         *file_settings);
void                    ide_clang_completion_item_display     (IdeClangCompletionItem  *self,
                                                               GtkSourceCompletionCell *cell,
                                                               const char              *typed_text);

G_END_DECLS
