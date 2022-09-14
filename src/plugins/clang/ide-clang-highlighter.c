/* ide-clang-highlighter.c
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

#define G_LOG_DOMAIN "ide-clang-highlighter"

#include "config.h"

#include <glib/gi18n.h>
#include <libide-foundry.h>

#include "ide-clang-client.h"
#include "ide-clang-highlighter.h"

struct _IdeClangHighlighter
{
  IdeObject           parent_instance;
  IdeHighlightIndex  *index;
  IdeHighlightEngine *engine;
  gint64              change_seq;
  gint64              building_seq;
  guint               waiting_for_unit : 1;
  guint               queued_source;
};

static void highlighter_iface_init             (IdeHighlighterInterface *iface);
static void ide_clang_highlighter_queue_udpate (IdeClangHighlighter     *self);

G_DEFINE_FINAL_TYPE_WITH_CODE (IdeClangHighlighter, ide_clang_highlighter, IDE_TYPE_OBJECT,
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
get_highlight_index_cb (GObject      *object,
                        GAsyncResult *result,
                        gpointer      user_data)
{
  IdeClangClient *client = (IdeClangClient *)object;
  g_autoptr(IdeHighlightIndex) index = NULL;
  g_autoptr(IdeTask) task = user_data;
  g_autoptr(GError) error = NULL;
  IdeClangHighlighter *self;

  g_assert (IDE_IS_CLANG_CLIENT (client));
  g_assert (G_IS_ASYNC_RESULT (result));
  g_assert (IDE_IS_TASK (task));

  self = ide_task_get_source_object (task);

  g_assert (IDE_IS_CLANG_HIGHLIGHTER (self));

  self->waiting_for_unit = FALSE;
  self->change_seq = self->building_seq;

  if (!(index = ide_clang_client_get_highlight_index_finish (client, result, &error)))
    {
      ide_task_return_error (task, g_steal_pointer (&error));
      return;
    }

  g_clear_pointer (&self->index, ide_highlight_index_unref);
  self->index = g_steal_pointer (&index);

  if (self->engine != NULL)
    ide_highlight_engine_advance (self->engine);

  ide_task_return_boolean (task, TRUE);
}

static void
get_index_flags_cb (GObject      *object,
                    GAsyncResult *result,
                    gpointer      user_data)
{
  IdeBuildSystem *build_system = (IdeBuildSystem *)object;
  g_autoptr(IdeTask) task = user_data;
  g_autoptr(IdeClangClient) client = NULL;
  g_autoptr(GError) error = NULL;
  g_auto(GStrv) flags = NULL;
  GCancellable *cancellable;
  IdeContext *context;
  GFile *file;

  g_assert (IDE_IS_BUILD_SYSTEM (build_system));
  g_assert (G_IS_ASYNC_RESULT (result));
  g_assert (IDE_IS_TASK (task));

  file = ide_task_get_task_data (task);
  cancellable = ide_task_get_cancellable (task);

  g_assert (G_IS_FILE (file));
  g_assert (!cancellable || G_IS_CANCELLABLE (cancellable));

  flags = ide_build_system_get_build_flags_finish (build_system, result, &error);

  if (ide_task_return_error_if_cancelled (task))
    return;

  context = ide_object_get_context (IDE_OBJECT (build_system));
  client = ide_object_ensure_child_typed (IDE_OBJECT (context), IDE_TYPE_CLANG_CLIENT);

  ide_clang_client_get_highlight_index_async (client,
                                              file,
                                              (const gchar * const *)flags,
                                              cancellable,
                                              get_highlight_index_cb,
                                              g_steal_pointer (&task));
}

static IdeHighlightIndex *
ide_clang_highlighter_get_index (IdeClangHighlighter *self,
                                 IdeBuffer           *buffer,
                                 gboolean            *transient)
{
  g_autoptr(IdeHighlightIndex) index = NULL;
  gint64 seq;
  gboolean invalid = FALSE;

  g_assert (IDE_IS_CLANG_HIGHLIGHTER (self));
  g_assert (IDE_IS_BUFFER (buffer));

  /* Get the previous index if we have one. We might be able to do some
   * fast incremental highlighting of the currently line while we wait for
   * and updated index to arrive.
   */
  index = self->index ? ide_highlight_index_ref (self->index) : NULL;

  /* If the change sequence isn't up to date, we'll try to do some updating
   * for the current line using the previous index, but not update our
   * "location" to the highlight engine. We'll do an updated scan once we
   * get our updated index from the daemon.
   */
  seq = ide_buffer_get_change_count (buffer);
  if (index == NULL || seq != self->change_seq)
    invalid = TRUE;

  /* If the index is up to date, we can allow the update to advance the
   * invalid region so that we shrink the work left to do.
   *
   * Otherwise, we'll update what we can immediately, and wait for the
   * new index to come in and do a followup.
   */
  if (!invalid && index != NULL)
    {
      *transient = FALSE;
      return g_steal_pointer (&index);
    }

  *transient = TRUE;

  if (self->waiting_for_unit)
    goto finish;

  self->building_seq = seq;
  self->waiting_for_unit = TRUE;

  ide_clang_highlighter_queue_udpate (self);

finish:

  return g_steal_pointer (&index);
}

static void
remove_tags (const GtkTextIter *begin,
             const GtkTextIter *end,
             const GSList      *tags_to_remove)
{
  GtkTextBuffer *buffer;

  g_assert (begin != NULL);
  g_assert (end != NULL);

  if (tags_to_remove == NULL)
    return;

  buffer = gtk_text_iter_get_buffer (begin);
  for (const GSList *iter = tags_to_remove; iter; iter = iter->next)
    gtk_text_buffer_remove_tag (buffer, iter->data, begin, end);
}

static void
ide_clang_highlighter_real_update (IdeHighlighter       *highlighter,
                                   const GSList         *tags_to_remove,
                                   IdeHighlightCallback  callback,
                                   const GtkTextIter    *range_begin,
                                   const GtkTextIter    *range_end,
                                   GtkTextIter          *location)
{
  IdeClangHighlighter *self = (IdeClangHighlighter *)highlighter;
  g_autoptr(IdeHighlightIndex) index = NULL;
  GtkSourceBuffer *source_buffer;
  GtkTextBuffer *text_buffer;
  GtkTextIter begin;
  GtkTextIter end;
  gboolean transient = FALSE;

  g_assert (IDE_IS_CLANG_HIGHLIGHTER (highlighter));
  g_assert (callback != NULL);
  g_assert (range_begin != NULL);
  g_assert (range_end != NULL);
  g_assert (location != NULL);

  if (!(text_buffer = gtk_text_iter_get_buffer (range_begin)) ||
      !IDE_IS_BUFFER (text_buffer))
    return;

  if (!(index = ide_clang_highlighter_get_index (self, IDE_BUFFER (text_buffer), &transient)))
    return;

  source_buffer = GTK_SOURCE_BUFFER (text_buffer);

  begin = end = *location = *range_begin;

  while (gtk_text_iter_compare (&begin, range_end) < 0)
    {
      GtkTextIter last = begin;

      if (!select_next_word (&begin, &end))
        {
          remove_tags (&last, range_end, tags_to_remove);
          goto completed;
        }

      remove_tags (&last, &end, tags_to_remove);

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
                  if (!transient)
                    *location = end;
                  return;
                }
            }
        }

      begin = end;
    }

completed:
  if (!transient)
    *location = *range_end;
}

static void
ide_clang_highlighter_real_set_engine (IdeHighlighter     *highlighter,
                                       IdeHighlightEngine *engine)
{
  IdeClangHighlighter *self = (IdeClangHighlighter *)highlighter;

  g_set_weak_pointer (&self->engine, engine);
}

static void
ide_clang_highlighter_destroy (IdeObject *object)
{
  IdeClangHighlighter *self = (IdeClangHighlighter *)object;

  g_clear_handle_id (&self->queued_source, g_source_remove);
  g_clear_pointer (&self->index, ide_highlight_index_unref);
  g_clear_weak_pointer (&self->engine);

  IDE_OBJECT_CLASS (ide_clang_highlighter_parent_class)->destroy (object);
}

static void
ide_clang_highlighter_class_init (IdeClangHighlighterClass *klass)
{
  IdeObjectClass *i_object_class = IDE_OBJECT_CLASS (klass);

  i_object_class->destroy = ide_clang_highlighter_destroy;
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

static gboolean
ide_clang_highlighter_do_update (IdeClangHighlighter *self)
{
  g_autoptr(IdeTask) task = NULL;
  g_autoptr(IdeClangClient) client = NULL;
  IdeBuildSystem *build_system;
  IdeContext *context;
  IdeBuffer *buffer;
  GFile *file;

  g_assert (IDE_IS_CLANG_HIGHLIGHTER (self));

  self->queued_source = 0;

  if (!ide_object_check_ready (IDE_OBJECT (self), NULL) ||
      self->engine == NULL ||
      !(buffer = ide_highlight_engine_get_buffer (self->engine)) ||
      !(file = ide_buffer_get_file (buffer)) ||
      !(context = ide_object_get_context (IDE_OBJECT (self))) ||
      !(client = ide_object_ensure_child_typed (IDE_OBJECT (context), IDE_TYPE_CLANG_CLIENT)))
    return G_SOURCE_REMOVE;

  task = ide_task_new (self, NULL, NULL, NULL);
  ide_task_set_source_tag (task, ide_clang_highlighter_get_index);
  ide_task_set_task_data (task, g_object_ref (file), g_object_unref);

  build_system = ide_build_system_from_context (context);

  ide_build_system_get_build_flags_async (build_system,
                                          file,
                                          NULL,
                                          get_index_flags_cb,
                                          g_steal_pointer (&task));

  return G_SOURCE_REMOVE;
}

static void
ide_clang_highlighter_queue_udpate (IdeClangHighlighter *self)
{
  g_assert (IDE_IS_CLANG_HIGHLIGHTER (self));

  if (self->queued_source != 0 ||
      ide_object_in_destruction (IDE_OBJECT (self)))
    return;

  self->queued_source =
    g_timeout_add_seconds_full (G_PRIORITY_LOW,
                                3,
                                (GSourceFunc)ide_clang_highlighter_do_update,
                                g_object_ref (self),
                                g_object_unref);

}
