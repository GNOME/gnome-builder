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

static void
ide_clang_highlighter_real_update (IdeHighlighter       *highlighter,
                                   IdeHighlightCallback  callback,
                                   const GtkTextIter    *range_begin,
                                   const GtkTextIter    *range_end,
                                   GtkTextIter          *location)
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
  GtkTextIter begin;
  GtkTextIter end;

  g_assert (IDE_IS_CLANG_HIGHLIGHTER (self));
  g_assert (callback != NULL);
  g_assert (range_begin != NULL);
  g_assert (range_end != NULL);
  g_assert (location != NULL);

  if (!(text_buffer = gtk_text_iter_get_buffer (range_begin)) ||
      !IDE_IS_BUFFER (text_buffer) ||
      !(source_buffer = GTK_SOURCE_BUFFER (text_buffer)) ||
      !(buffer = IDE_BUFFER (text_buffer)) ||
      !(file = ide_buffer_get_file (buffer)) ||
      !(context = ide_object_get_context (IDE_OBJECT (highlighter))) ||
      !(service = ide_context_get_service_typed (context, IDE_TYPE_CLANG_SERVICE)) ||
      !(unit = ide_clang_service_get_cached_translation_unit (service, file)) ||
      !(index = ide_clang_translation_unit_get_index (unit)))
    return;

  begin = end = *location = *range_begin;

  while (gtk_text_iter_compare (&begin, range_end) < 0)
    {
      if (!select_next_word (&begin, &end))
        goto completed;

      if (gtk_text_iter_compare (&begin, range_end) >= 0)
        goto completed;

      g_assert (!gtk_text_iter_equal (&begin, &end));

      if (!gtk_source_buffer_iter_has_context_class (source_buffer, &begin, "string") &&
          !gtk_source_buffer_iter_has_context_class (source_buffer, &begin, "path") &&
          !gtk_source_buffer_iter_has_context_class (source_buffer, &begin, "comment"))
        {
          const gchar *tag;
          gchar *word;

          word = gtk_text_iter_get_slice (&begin, &end);
          tag = ide_highlight_index_lookup (index, word);
          g_free (word);

          if (tag != NULL)
            {
              if (callback (&begin, &end, tag) == IDE_HIGHLIGHT_STOP)
                {
                  *location = end;
                  return;
                }
            }
        }

      begin = end;
    }

completed:
  *location = *range_end;
}

static void
ide_clang_highlighter_class_init (IdeClangHighlighterClass *klass)
{
  IdeHighlighterClass *highlighter_class = IDE_HIGHLIGHTER_CLASS (klass);

  highlighter_class->update = ide_clang_highlighter_real_update;
}

static void
ide_clang_highlighter_init (IdeClangHighlighter *self)
{
}
