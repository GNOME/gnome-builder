/* ide-clang-completion-item-private.h
 *
 * Copyright (C) 2015 Christian Hergert <christian@hergert.me>
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

#include <clang-c/Index.h>
#include <glib-object.h>
#include <ide.h>
#include <string.h>

#include "ide-clang-completion-item.h"

G_BEGIN_DECLS

struct _IdeClangCompletionItem
{
  GObject           parent_instance;

  GList             link;

  guint             index;
  guint             priority;
  gint              typed_text_index : 16;
  guint             initialized : 1;

  const gchar      *icon_name;
  gchar            *brief_comment;
  gchar            *markup;
  IdeRefPtr        *results;
  IdeSourceSnippet *snippet;
  gchar            *typed_text;
};

static inline CXCompletionResult *
ide_clang_completion_item_get_result (const IdeClangCompletionItem *self)
{
  return &((CXCodeCompleteResults *)ide_ref_ptr_get (self->results))->Results [self->index];
}

static inline gboolean
ide_clang_completion_item_match (IdeClangCompletionItem *self,
                                 const gchar            *lower_is_ascii)
{
  const gchar *haystack = self->typed_text;
  const gchar *needle = lower_is_ascii;
  const gchar *tmp;
  char ch = *needle;

  if (G_UNLIKELY (haystack == NULL))
    haystack = ide_clang_completion_item_get_typed_text (self);

  /*
   * Optimization to require that we find the first character of
   * needle within the first 4 characters of typed_text. Otherwise,
   * we get way too many bogus results. It's okay to check past
   * the trailing null byte since we know that malloc allocations
   * are aligned to a pointer size (and therefore minimum allocation
   * is 4 byte on 32-bit and 8 byte on 64-bit). If we hit a bogus
   * condition, oh well, it's just one more compare below.
   */
  if (haystack [0] != ch && haystack [1] != ch && haystack [2] != ch && haystack [3] != ch)
    return FALSE;

  for (; *needle; needle++)
    {
      tmp = strchr (haystack, *needle);
      if (tmp == NULL)
        tmp = strchr (haystack, g_ascii_toupper (*needle));
      if (tmp == NULL)
        return FALSE;
      haystack = tmp;
    }

  return TRUE;
}

IdeClangCompletionItem *ide_clang_completion_item_new (IdeRefPtr *results,
                                                       guint      index);

G_END_DECLS
