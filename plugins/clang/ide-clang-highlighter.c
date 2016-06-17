/* ide-clang-highlighter.c
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

#include <glib/gi18n.h>

#include "ide-clang-highlighter.h"
#include "ide-clang-service.h"
#include "ide-clang-translation-unit.h"

struct _IdeClangHighlighter
{
  IdeObject           parent_instance;
  IdeHighlightEngine *engine;
  guint               waiting_for_unit : 1;
};

static void highlighter_iface_init (IdeHighlighterInterface *iface);

G_DEFINE_TYPE_EXTENDED (IdeClangHighlighter, ide_clang_highlighter, IDE_TYPE_OBJECT, 0,
                        G_IMPLEMENT_INTERFACE (IDE_TYPE_HIGHLIGHTER, highlighter_iface_init))

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
get_unit_cb (GObject      *object,
             GAsyncResult *result,
             gpointer      user_data)
{
  IdeClangService *service = (IdeClangService *)object;
  g_autoptr(IdeClangHighlighter) self = user_data;
  g_autoptr(IdeClangTranslationUnit) unit = NULL;

  g_assert (IDE_IS_CLANG_SERVICE (service));
  g_assert (IDE_IS_CLANG_HIGHLIGHTER (self));

  self->waiting_for_unit = FALSE;

  if (!(unit = ide_clang_service_get_translation_unit_finish (service, result, NULL)))
    return;

  if (self->engine != NULL)
    ide_highlight_engine_rebuild (self->engine);
}

static void
ide_clang_highlighter_real_update (IdeHighlighter       *highlighter,
                                   IdeHighlightCallback  callback,
                                   const GtkTextIter    *range_begin,
                                   const GtkTextIter    *range_end,
                                   GtkTextIter          *location)
{
  g_autoptr(IdeClangTranslationUnit) unit = NULL;
  IdeClangHighlighter *self = (IdeClangHighlighter *)highlighter;
  GtkTextBuffer *text_buffer;
  GtkSourceBuffer *source_buffer;
  IdeHighlightIndex *index;
  IdeContext *context;
  IdeClangService *service = NULL;
  IdeBuffer *buffer;
  IdeFile *file;
  GtkTextIter begin;
  GtkTextIter end;

  g_assert (IDE_IS_CLANG_HIGHLIGHTER (highlighter));
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
      !(service = ide_context_get_service_typed (context, IDE_TYPE_CLANG_SERVICE)))
    return;

  if (!(unit = ide_clang_service_get_cached_translation_unit (service, file)))
    {
      if (!self->waiting_for_unit)
        {
          self->waiting_for_unit = TRUE;
          ide_clang_service_get_translation_unit_async (service,
                                                        file,
                                                        0,
                                                        NULL,
                                                        get_unit_cb,
                                                        g_object_ref (self));
        }

      return;
    }

  if (!(index = ide_clang_translation_unit_get_index (unit)))
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
ide_clang_highlighter_real_set_engine (IdeHighlighter     *highlighter,
                                       IdeHighlightEngine *engine)
{
  IdeClangHighlighter *self = (IdeClangHighlighter *)highlighter;

  ide_set_weak_pointer (&self->engine, engine);
}

static void
ide_clang_highlighter_finalize (GObject *object)
{
  IdeClangHighlighter *self = (IdeClangHighlighter *)object;

  ide_clear_weak_pointer (&self->engine);

  G_OBJECT_CLASS (ide_clang_highlighter_parent_class)->finalize (object);
}

static void
ide_clang_highlighter_class_init (IdeClangHighlighterClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = ide_clang_highlighter_finalize;
}

static void
ide_clang_highlighter_init (IdeClangHighlighter *self)
{
}

static void
highlighter_iface_init (IdeHighlighterInterface *iface)
{
  iface->update = ide_clang_highlighter_real_update;
  iface->set_engine = ide_clang_highlighter_real_set_engine;
}
