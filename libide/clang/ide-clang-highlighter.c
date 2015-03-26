/* ide-clang-highlighter.c
 *
 * Copyright (C) 2015 Christian Hergert <christian@hergert.me>
 *
 * This file is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as
 * published by the Free Software Foundation; either version 3 of the
 * License, or (at your option) any later version.
 *
 * This file is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <glib/gi18n.h>

#include "ide-clang-highlighter.h"
#include "ide-clang-service.h"
#include "ide-clang-translation-unit.h"
#include "ide-context.h"

struct _IdeClangHighlighter
{
  IdeHighlighter parent_instance;
};

G_DEFINE_TYPE (IdeClangHighlighter, ide_clang_highlighter, IDE_TYPE_HIGHLIGHTER)

static inline gboolean
accepts_char (gunichar ch)
{
  return (ch == '_' || g_unichar_isalnum (ch));
}

static gboolean
select_next_word (GtkTextIter *begin,
                  GtkTextIter *end)
{
  g_assert (begin);
  g_assert (end);

  *end = *begin;

  while (!accepts_char (gtk_text_iter_get_char (begin)))
    if (!gtk_text_iter_forward_char (begin))
      return FALSE;

  *end = *begin;

  while (accepts_char (gtk_text_iter_get_char (end)))
    if (!gtk_text_iter_forward_char (end))
      return !gtk_text_iter_equal (begin, end);

  return TRUE;
}

static IdeHighlightKind
ide_clang_highlighter_real_next (IdeHighlighter    *highlighter,
                                 const GtkTextIter *range_begin,
                                 const GtkTextIter *range_end,
                                 GtkTextIter       *begin,
                                 GtkTextIter       *end)
{
  IdeClangHighlighter *self = (IdeClangHighlighter *)highlighter;
  GtkTextBuffer *text_buffer;
  GtkSourceBuffer *source_buffer;
  IdeHighlightIndex *index;
  IdeClangTranslationUnit *unit;
  IdeContext *context;
  IdeClangService *service;
  IdeBuffer *buffer;
  IdeFile *file;

  /*
   * TODO:
   *
   * This API has a decent bit of overhead. Instead, we should move to an API
   * design that allows us to walk through the entire buffer, and then call a
   * callback (back into the engine) to set the style name for the region.
   * This would allow us to amortize the overhead cost of getting the
   * information we need.
   */

  g_assert (IDE_IS_CLANG_HIGHLIGHTER (self));

  if (!(text_buffer = gtk_text_iter_get_buffer (range_begin)) ||
      !IDE_IS_BUFFER (text_buffer) ||
      !(source_buffer = GTK_SOURCE_BUFFER (text_buffer)) ||
      !(buffer = IDE_BUFFER (text_buffer)) ||
      !(file = ide_buffer_get_file (buffer)) ||
      !(context = ide_object_get_context (IDE_OBJECT (highlighter))) ||
      !(service = ide_context_get_service_typed (context, IDE_TYPE_CLANG_SERVICE)) ||
      !(unit = ide_clang_service_get_cached_translation_unit (service, file)) ||
      !(index = ide_clang_translation_unit_get_index (unit)))
    return IDE_HIGHLIGHT_KIND_NONE;

  *begin = *end = *range_begin;

  while (gtk_text_iter_compare (begin, range_end) < 0)
    {
      if (!select_next_word (begin, end))
        return IDE_HIGHLIGHT_KIND_NONE;

      if (gtk_text_iter_compare (begin, range_end) >= 0)
        return IDE_HIGHLIGHT_KIND_NONE;

      g_assert (!gtk_text_iter_equal (begin, end));

      if (!gtk_source_buffer_iter_has_context_class (source_buffer, begin, "string") &&
          !gtk_source_buffer_iter_has_context_class (source_buffer, begin, "path") &&
          !gtk_source_buffer_iter_has_context_class (source_buffer, begin, "comment"))
        {
          IdeHighlightKind ret;
          gchar *word;

          word = gtk_text_iter_get_slice (begin, end);
          ret = ide_highlight_index_lookup (index, word);
          g_free (word);

          if (ret != IDE_HIGHLIGHT_KIND_NONE)
            return ret;
        }

      *begin = *end;
    }

  return IDE_HIGHLIGHT_KIND_NONE;
}

static void
ide_clang_highlighter_class_init (IdeClangHighlighterClass *klass)
{
  IdeHighlighterClass *highlighter_class = IDE_HIGHLIGHTER_CLASS (klass);

  highlighter_class->next = ide_clang_highlighter_real_next;
}

static void
ide_clang_highlighter_init (IdeClangHighlighter *self)
{
}
