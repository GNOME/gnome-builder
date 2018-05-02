/* ide-clang-completion-item.h
 *
 * Copyright 2015 Christian Hergert <christian@hergert.me>
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

#include <ide.h>

G_BEGIN_DECLS

#define IDE_TYPE_CLANG_COMPLETION_ITEM (ide_clang_completion_item_get_type())

G_DECLARE_FINAL_TYPE (IdeClangCompletionItem, ide_clang_completion_item, IDE, CLANG_COMPLETION_ITEM, GObject)

struct _IdeClangCompletionItem
{
  GObject           parent_instance;

  GList             link;

  guint             index;
  guint             priority;
  gint              typed_text_index : 16;
  guint             initialized : 1;

  const gchar      *icon_name;
  gchar            *markup;
  GVariant         *results;
  const gchar      *typed_text;
};

static inline GVariant *
ide_clang_completion_item_get_result (const IdeClangCompletionItem *self)
{
  g_autoptr(GVariant) child = g_variant_get_child_value (self->results, self->index);

  if (g_variant_is_of_type (child, G_VARIANT_TYPE_VARIANT))
    return g_variant_get_variant (child);

  return g_steal_pointer (&child);
}

IdeClangCompletionItem *ide_clang_completion_item_new         (GVariant               *results,
                                                               guint                   index,
                                                               const gchar            *typed_text);
IdeSourceSnippet       *ide_clang_completion_item_get_snippet (IdeClangCompletionItem *self,
                                                               IdeFileSettings        *file_settings);

G_END_DECLS
