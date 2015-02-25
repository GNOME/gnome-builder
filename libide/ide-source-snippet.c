/* ide-source-snippet.c
 *
 * Copyright (C) 2013 Christian Hergert <christian@hergert.me>
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

#define G_LOG_DOMAIN "ide-source-snippet"

#include <glib/gi18n.h>

#include "ide-source-snippet.h"
#include "ide-source-snippet-private.h"
#include "ide-source-snippet-chunk.h"
#include "ide-source-snippet-context.h"

struct _IdeSourceSnippet
{
  IdeObject                parent_instance;

  IdeSourceSnippetContext *snippet_context;
  GtkTextBuffer           *buffer;
  GPtrArray               *chunks;
  GArray                  *runs;
  GtkTextMark             *mark_begin;
  GtkTextMark             *mark_end;
  gchar                   *trigger;
  gchar                   *language;
  gchar                   *description;

  gint                     tab_stop;
  gint                     max_tab_stop;
  gint                     current_chunk;

  guint                    inserted : 1;
};

enum {
  PROP_0,
  PROP_BUFFER,
  PROP_MARK_BEGIN,
  PROP_MARK_END,
  PROP_TAB_STOP,
  PROP_TRIGGER,
  PROP_LANGUAGE,
  PROP_DESCRIPTION,
  LAST_PROP
};

G_DEFINE_TYPE (IdeSourceSnippet, ide_source_snippet, G_TYPE_OBJECT)

static GParamSpec * gParamSpecs[LAST_PROP];

IdeSourceSnippet *
ide_source_snippet_new (const gchar *trigger,
                        const gchar *language)
{
  IdeSourceSnippet *ret;

  ret = g_object_new (IDE_TYPE_SOURCE_SNIPPET,
                      "trigger", trigger,
                      "language", language,
                      NULL);

  return ret;
}

/**
 * ide_source_snippet_copy:
 *
 * Returns: (transfer full): An #IdeSourceSnippet.
 */
IdeSourceSnippet *
ide_source_snippet_copy (IdeSourceSnippet *self)
{
  IdeSourceSnippetChunk *chunk;
  IdeSourceSnippet *ret;
  gint i;

  g_return_val_if_fail (IDE_IS_SOURCE_SNIPPET (self), NULL);

  ret = g_object_new (IDE_TYPE_SOURCE_SNIPPET,
                      "trigger", self->trigger,
                      "language", self->language,
                      "description", self->description,
                      NULL);

  for (i = 0; i < self->chunks->len; i++)
    {
      chunk = g_ptr_array_index (self->chunks, i);
      chunk = ide_source_snippet_chunk_copy (chunk);
      ide_source_snippet_add_chunk (ret, chunk);
      g_object_unref (chunk);
    }

  return ret;
}

gint
ide_source_snippet_get_tab_stop (IdeSourceSnippet *self)
{
  g_return_val_if_fail (IDE_IS_SOURCE_SNIPPET (self), -1);

  return self->tab_stop;
}

guint
ide_source_snippet_get_n_chunks (IdeSourceSnippet *self)
{
  g_return_val_if_fail (IDE_IS_SOURCE_SNIPPET (self), 0);

  return self->chunks->len;
}

/**
 * ide_source_snippet_get_nth_chunk:
 *
 * Returns: (transfer none):
 */
IdeSourceSnippetChunk *
ide_source_snippet_get_nth_chunk (IdeSourceSnippet *self,
                                  guint             n)
{
  g_return_val_if_fail (IDE_IS_SOURCE_SNIPPET (self), 0);

  if (n < self->chunks->len)
    return g_ptr_array_index (self->chunks, n);

  return NULL;
}

const gchar *
ide_source_snippet_get_trigger (IdeSourceSnippet *self)
{
  g_return_val_if_fail (IDE_IS_SOURCE_SNIPPET (self), NULL);

  return self->trigger;
}

void
ide_source_snippet_set_trigger (IdeSourceSnippet *self,
                                const gchar      *trigger)
{
  g_return_if_fail (IDE_IS_SOURCE_SNIPPET (self));

  g_free (self->trigger);
  self->trigger = g_strdup (trigger);
}

const gchar *
ide_source_snippet_get_language (IdeSourceSnippet *self)
{
  g_return_val_if_fail (IDE_IS_SOURCE_SNIPPET (self), NULL);

  return self->language;
}

void
ide_source_snippet_set_language (IdeSourceSnippet *self,
                                 const gchar      *language)
{
  g_return_if_fail (IDE_IS_SOURCE_SNIPPET (self));

  g_free (self->language);
  self->language = g_strdup (language);
}

const gchar *
ide_source_snippet_get_description (IdeSourceSnippet *self)
{
  g_return_val_if_fail (IDE_IS_SOURCE_SNIPPET (self), NULL);

  return self->description;
}

void
ide_source_snippet_set_description (IdeSourceSnippet *self,
                                    const gchar      *description)
{
  g_return_if_fail (IDE_IS_SOURCE_SNIPPET (self));

  g_free (self->description);
  self->description = g_strdup (description);
}

static gint
ide_source_snippet_get_offset (IdeSourceSnippet *self,
                               GtkTextIter      *iter)
{
  GtkTextIter begin;
  gint ret;

  g_return_val_if_fail (IDE_IS_SOURCE_SNIPPET (self), 0);
  g_return_val_if_fail (iter, 0);

  gtk_text_buffer_get_iter_at_mark (self->buffer, &begin, self->mark_begin);
  ret = gtk_text_iter_get_offset (iter) - gtk_text_iter_get_offset (&begin);
  ret = MAX (0, ret);

  return ret;
}

static gint
ide_source_snippet_get_index (IdeSourceSnippet *self,
                              GtkTextIter      *iter)
{
  gint offset;
  gint run;
  gint i;

  g_return_val_if_fail (IDE_IS_SOURCE_SNIPPET (self), 0);
  g_return_val_if_fail (iter, 0);

  offset = ide_source_snippet_get_offset (self, iter);

  for (i = 0; i < self->runs->len; i++)
    {
      run = g_array_index (self->runs, gint, i);
      offset -= run;
      if (offset <= 0)
        {
          /*
           * HACK: This is the central part of the hack by using offsets
           *       instead of textmarks (which gives us lots of gravity grief).
           *       We guess which snippet it is based on the current chunk.
           */
          if ((i + 1) == self->current_chunk)
            return (i + 1);
          return i;
        }
    }

  return (self->runs->len - 1);
}

static gboolean
ide_source_snippet_within_bounds (IdeSourceSnippet *self,
                                  GtkTextIter      *iter)
{
  GtkTextIter begin;
  GtkTextIter end;
  gboolean ret;

  g_return_val_if_fail (IDE_IS_SOURCE_SNIPPET (self), FALSE);
  g_return_val_if_fail (iter, FALSE);

  gtk_text_buffer_get_iter_at_mark (self->buffer, &begin, self->mark_begin);
  gtk_text_buffer_get_iter_at_mark (self->buffer, &end, self->mark_end);

  ret = ((gtk_text_iter_compare (&begin, iter) <= 0) &&
         (gtk_text_iter_compare (&end, iter) >= 0));

  return ret;
}

gboolean
ide_source_snippet_insert_set (IdeSourceSnippet *self,
                               GtkTextMark      *mark)
{
  GtkTextIter iter;

  g_return_val_if_fail (IDE_IS_SOURCE_SNIPPET (self), FALSE);
  g_return_val_if_fail (GTK_IS_TEXT_MARK (mark), FALSE);

  gtk_text_buffer_get_iter_at_mark (self->buffer, &iter, mark);

  if (!ide_source_snippet_within_bounds (self, &iter))
    return FALSE;

  self->current_chunk = ide_source_snippet_get_index (self, &iter);

  return TRUE;
}

static void
ide_source_snippet_get_nth_chunk_range (IdeSourceSnippet *self,
                                        gint              n,
                                        GtkTextIter      *begin,
                                        GtkTextIter      *end)
{
  gint run;
  gint i;

  g_return_if_fail (IDE_IS_SOURCE_SNIPPET (self));
  g_return_if_fail (n >= 0);
  g_return_if_fail (begin);
  g_return_if_fail (end);

  gtk_text_buffer_get_iter_at_mark (self->buffer, begin, self->mark_begin);

  for (i = 0; i < n; i++)
    {
      run = g_array_index (self->runs, gint, i);
      gtk_text_iter_forward_chars (begin, run);
    }

  gtk_text_iter_assign (end, begin);
  run = g_array_index (self->runs, gint, n);
  gtk_text_iter_forward_chars (end, run);
}

void
ide_source_snippet_get_chunk_range (IdeSourceSnippet      *self,
                                    IdeSourceSnippetChunk *chunk,
                                    GtkTextIter           *begin,
                                    GtkTextIter           *end)
{
  IdeSourceSnippetChunk *item;
  guint i;

  g_return_if_fail (IDE_IS_SOURCE_SNIPPET (self));
  g_return_if_fail (IDE_IS_SOURCE_SNIPPET_CHUNK (chunk));

  for (i = 0; i < self->chunks->len; i++)
    {
      item = g_ptr_array_index (self->chunks, i);

      if (item == chunk)
        {
          ide_source_snippet_get_nth_chunk_range (self, i, begin, end);
          return;
        }
    }

  g_warning (_("Chunk does not belong to snippet."));
}

static void
ide_source_snippet_select_chunk (IdeSourceSnippet *self,
                                 gint              n)
{
  GtkTextIter begin;
  GtkTextIter end;

  g_return_if_fail (IDE_IS_SOURCE_SNIPPET (self));
  g_return_if_fail (n >= 0);
  g_return_if_fail (n < self->runs->len);

  ide_source_snippet_get_nth_chunk_range (self, n, &begin, &end);
  gtk_text_buffer_select_range (self->buffer, &begin, &end);
  self->current_chunk = n;
}

gboolean
ide_source_snippet_move_next (IdeSourceSnippet *self)
{
  IdeSourceSnippetChunk *chunk = NULL;
  GtkTextIter iter;
  gint i;

  g_return_val_if_fail (IDE_IS_SOURCE_SNIPPET (self), FALSE);

  if (self->tab_stop > self->max_tab_stop)
    return FALSE;

  self->tab_stop++;

  for (i = 0; i < self->chunks->len; i++)
    {
      chunk = g_ptr_array_index (self->chunks, i);
      if (ide_source_snippet_chunk_get_tab_stop (chunk) == self->tab_stop)
        {
          ide_source_snippet_select_chunk (self, i);
          return TRUE;
        }
    }

  for (i = 0; i < self->chunks->len; i++)
    {
      chunk = g_ptr_array_index (self->chunks, i);
      if (!ide_source_snippet_chunk_get_tab_stop (chunk))
        {
          ide_source_snippet_select_chunk (self, i);
          return FALSE;
        }
    }

  gtk_text_buffer_get_iter_at_mark (self->buffer, &iter, self->mark_end);
  gtk_text_buffer_select_range (self->buffer, &iter, &iter);
  self->current_chunk = self->chunks->len - 1;

  return FALSE;
}

gboolean
ide_source_snippet_move_previous (IdeSourceSnippet *self)
{
  IdeSourceSnippetChunk *chunk = NULL;
  gint i;

  g_return_val_if_fail (IDE_IS_SOURCE_SNIPPET (self), FALSE);

  self->tab_stop = MAX (1, self->tab_stop - 1);

  for (i = 0; i < self->chunks->len; i++)
    {
      chunk = g_ptr_array_index (self->chunks, i);
      if (ide_source_snippet_chunk_get_tab_stop (chunk) == self->tab_stop)
        {
          ide_source_snippet_select_chunk (self, i);
          return TRUE;
        }
    }

  return FALSE;
}

static void
ide_source_snippet_update_context (IdeSourceSnippet *self)
{
  IdeSourceSnippetContext *context;
  IdeSourceSnippetChunk *chunk;
  const gchar *text;
  gchar key[12];
  guint i;
  gint tab_stop;

  g_return_if_fail (IDE_IS_SOURCE_SNIPPET (self));

  context = ide_source_snippet_get_context (self);

  ide_source_snippet_context_emit_changed (context);

  for (i = 0; i < self->chunks->len; i++)
    {
      chunk = g_ptr_array_index (self->chunks, i);
      tab_stop = ide_source_snippet_chunk_get_tab_stop (chunk);
      if (tab_stop > 0)
        {
          if ((text = ide_source_snippet_chunk_get_text (chunk)))
            {
              g_snprintf (key, sizeof key, "%d", tab_stop);
              key[sizeof key - 1] = '\0';
              ide_source_snippet_context_add_variable (context, key, text);
            }
        }
    }

  ide_source_snippet_context_emit_changed (context);
}

gboolean
ide_source_snippet_begin (IdeSourceSnippet *self,
                          GtkTextBuffer    *buffer,
                          GtkTextIter      *iter)
{
  IdeSourceSnippetContext *context;
  IdeSourceSnippetChunk *chunk;
  const gchar *text;
  gboolean ret;
  gint len;
  gint i;

  g_return_val_if_fail (IDE_IS_SOURCE_SNIPPET (self), FALSE);
  g_return_val_if_fail (!self->buffer, FALSE);
  g_return_val_if_fail (!self->mark_begin, FALSE);
  g_return_val_if_fail (!self->mark_end, FALSE);
  g_return_val_if_fail (GTK_IS_TEXT_BUFFER (buffer), FALSE);
  g_return_val_if_fail (iter, FALSE);

  self->inserted = TRUE;

  context = ide_source_snippet_get_context (self);

  ide_source_snippet_update_context (self);
  ide_source_snippet_context_emit_changed (context);
  ide_source_snippet_update_context (self);

  self->buffer = g_object_ref (buffer);
  self->mark_begin = gtk_text_buffer_create_mark (buffer, NULL, iter, TRUE);
  g_object_add_weak_pointer (G_OBJECT (self->mark_begin),
                             (gpointer *) &self->mark_begin);

  gtk_text_buffer_begin_user_action (buffer);

  for (i = 0; i < self->chunks->len; i++)
    {
      chunk = g_ptr_array_index (self->chunks, i);
      if ((text = ide_source_snippet_chunk_get_text (chunk)))
        {
          len = g_utf8_strlen (text, -1);
          g_array_append_val (self->runs, len);
          gtk_text_buffer_insert (buffer, iter, text, -1);
        }
    }

  self->mark_end = gtk_text_buffer_create_mark (buffer, NULL, iter, FALSE);
  g_object_add_weak_pointer (G_OBJECT (self->mark_end),
                             (gpointer *) &self->mark_end);

  g_object_ref (self->mark_begin);
  g_object_ref (self->mark_end);

  gtk_text_buffer_end_user_action (buffer);

  ret = ide_source_snippet_move_next (self);

  return ret;
}

void
ide_source_snippet_finish (IdeSourceSnippet *self)
{
  g_return_if_fail (IDE_IS_SOURCE_SNIPPET (self));
}

void
ide_source_snippet_pause (IdeSourceSnippet *self)
{
  g_return_if_fail (IDE_IS_SOURCE_SNIPPET (self));
}

void
ide_source_snippet_unpause (IdeSourceSnippet *self)
{
  g_return_if_fail (IDE_IS_SOURCE_SNIPPET (self));
}

void
ide_source_snippet_add_chunk (IdeSourceSnippet      *self,
                              IdeSourceSnippetChunk *chunk)
{
  gint tab_stop;

  g_return_if_fail (IDE_IS_SOURCE_SNIPPET (self));
  g_return_if_fail (IDE_IS_SOURCE_SNIPPET_CHUNK (chunk));
  g_return_if_fail (!self->inserted);

  g_ptr_array_add (self->chunks, g_object_ref (chunk));

  ide_source_snippet_chunk_set_context (chunk, self->snippet_context);

  tab_stop = ide_source_snippet_chunk_get_tab_stop (chunk);
  self->max_tab_stop = MAX (self->max_tab_stop, tab_stop);
}

gchar *
ide_source_snippet_get_nth_text (IdeSourceSnippet *self,
                                 gint              n)
{
  GtkTextIter iter;
  GtkTextIter end;
  gchar *ret;
  gint i;

  g_return_val_if_fail (IDE_IS_SOURCE_SNIPPET (self), NULL);
  g_return_val_if_fail (n >= 0, NULL);

  gtk_text_buffer_get_iter_at_mark (self->buffer, &iter, self->mark_begin);

  for (i = 0; i < n; i++)
    gtk_text_iter_forward_chars (&iter, g_array_index (self->runs, gint, i));

  gtk_text_iter_assign (&end, &iter);
  gtk_text_iter_forward_chars (&end, g_array_index (self->runs, gint, n));

  ret = gtk_text_buffer_get_text (self->buffer, &iter, &end, TRUE);

  return ret;
}

static void
ide_source_snippet_replace_chunk_text (IdeSourceSnippet *self,
                                       gint              n,
                                       const gchar      *text)
{
  GtkTextIter begin;
  GtkTextIter end;

  g_return_if_fail (IDE_IS_SOURCE_SNIPPET (self));
  g_return_if_fail (n >= 0);
  g_return_if_fail (text);

  ide_source_snippet_get_nth_chunk_range (self, n, &begin, &end);
  gtk_text_buffer_delete (self->buffer, &begin, &end);
  gtk_text_buffer_insert (self->buffer, &end, text, -1);
  g_array_index (self->runs, gint, n) = g_utf8_strlen (text, -1);
}

static void
ide_source_snippet_rewrite_updated_chunks (IdeSourceSnippet *self)
{
  IdeSourceSnippetChunk *chunk;
  const gchar *text;
  gchar *real_text;
  gint i;

  g_return_if_fail (IDE_IS_SOURCE_SNIPPET (self));

  for (i = 0; i < self->chunks->len; i++)
    {
      chunk = g_ptr_array_index (self->chunks, i);
      text = ide_source_snippet_chunk_get_text (chunk);
      real_text = ide_source_snippet_get_nth_text (self, i);
      if (!!g_strcmp0 (text, real_text))
        ide_source_snippet_replace_chunk_text (self, i, text);
      g_free (real_text);
    }
}

void
ide_source_snippet_before_insert_text (IdeSourceSnippet *self,
                                       GtkTextBuffer    *buffer,
                                       GtkTextIter      *iter,
                                       gchar            *text,
                                       gint              len)
{
  gint utf8_len;
  gint n;

  g_return_if_fail (IDE_IS_SOURCE_SNIPPET (self));
  g_return_if_fail (self->current_chunk >= 0);
  g_return_if_fail (GTK_IS_TEXT_BUFFER (buffer));
  g_return_if_fail (iter);

  n = ide_source_snippet_get_index (self, iter);
  utf8_len = g_utf8_strlen (text, len);
  g_array_index (self->runs, gint, n) += utf8_len;

#if 0
  g_print ("I: ");
  for (n = 0; n < self->runs->len; n++)
    g_print ("%d ", g_array_index (self->runs, gint, n));
  g_print ("\n");
#endif
}

void
ide_source_snippet_after_insert_text (IdeSourceSnippet *self,
                                      GtkTextBuffer    *buffer,
                                      GtkTextIter      *iter,
                                      gchar            *text,
                                      gint              len)
{
  IdeSourceSnippetChunk *chunk;
  GtkTextMark *here;
  gchar *new_text;
  gint n;

  g_return_if_fail (IDE_IS_SOURCE_SNIPPET (self));
  g_return_if_fail (self->current_chunk >= 0);
  g_return_if_fail (GTK_IS_TEXT_BUFFER (buffer));
  g_return_if_fail (iter);

  n = ide_source_snippet_get_index (self, iter);
  chunk = g_ptr_array_index (self->chunks, n);
  new_text = ide_source_snippet_get_nth_text (self, n);
  ide_source_snippet_chunk_set_text (chunk, new_text);
  ide_source_snippet_chunk_set_text_set (chunk, TRUE);
  g_free (new_text);

  here = gtk_text_buffer_create_mark (buffer, NULL, iter, TRUE);

  ide_source_snippet_update_context (self);
  ide_source_snippet_update_context (self);
  ide_source_snippet_rewrite_updated_chunks (self);

  gtk_text_buffer_get_iter_at_mark (buffer, iter, here);
  gtk_text_buffer_delete_mark (buffer, here);

#if 0
  ide_source_snippet_context_dump (self->snippet_context);
#endif
}

void
ide_source_snippet_before_delete_range (IdeSourceSnippet *self,
                                        GtkTextBuffer    *buffer,
                                        GtkTextIter      *begin,
                                        GtkTextIter      *end)
{
  IdeSourceSnippetChunk *chunk;
  gchar *new_text;
  gint *run;
  gint len;
  gint n;
  gint i;
  gint lower_bound = -1;
  gint upper_bound = -1;

  g_return_if_fail (IDE_IS_SOURCE_SNIPPET (self));
  g_return_if_fail (GTK_IS_TEXT_BUFFER (buffer));
  g_return_if_fail (begin);
  g_return_if_fail (end);

  len = gtk_text_iter_get_offset (end) - gtk_text_iter_get_offset (begin);
  n = ide_source_snippet_get_index (self, begin);
  self->current_chunk = n;

  while (len && n < self->runs->len)
    {
      if (lower_bound == -1 || n < lower_bound)
        lower_bound = n;
      if (n > upper_bound)
        upper_bound = n;
      run = &g_array_index (self->runs, gint, n);
      if (len > *run)
        {
          len -= *run;
          *run = 0;
          n++;
          continue;
        }
      *run -= len;
      len = 0;
      break;
    }

  for (i = lower_bound; i <= upper_bound; i++)
    {
      chunk = g_ptr_array_index (self->chunks, i);
      new_text = ide_source_snippet_get_nth_text (self, i);
      ide_source_snippet_chunk_set_text (chunk, new_text);
      ide_source_snippet_chunk_set_text_set (chunk, TRUE);
      g_free (new_text);
    }

#if 0
  g_print ("D: ");
  for (n = 0; n < self->runs->len; n++)
    g_print ("%d ", g_array_index (self->runs, gint, n));
  g_print ("\n");
#endif
}

void
ide_source_snippet_after_delete_range (IdeSourceSnippet *self,
                                       GtkTextBuffer    *buffer,
                                       GtkTextIter      *begin,
                                       GtkTextIter      *end)
{
  GtkTextMark *here;

  g_return_if_fail (IDE_IS_SOURCE_SNIPPET (self));
  g_return_if_fail (GTK_IS_TEXT_BUFFER (buffer));
  g_return_if_fail (begin);
  g_return_if_fail (end);

  here = gtk_text_buffer_create_mark (buffer, NULL, begin, TRUE);

  ide_source_snippet_update_context (self);
  ide_source_snippet_update_context (self);
  ide_source_snippet_rewrite_updated_chunks (self);

  gtk_text_buffer_get_iter_at_mark (buffer, begin, here);
  gtk_text_buffer_get_iter_at_mark (buffer, end, here);
  gtk_text_buffer_delete_mark (buffer, here);

#if 0
  ide_source_snippet_context_dump (self->snippet_context);
#endif
}

/**
 * ide_source_snippet_get_mark_begin:
 *
 * Returns: (transfer none):
 */
GtkTextMark *
ide_source_snippet_get_mark_begin (IdeSourceSnippet *self)
{
  g_return_val_if_fail (IDE_IS_SOURCE_SNIPPET (self), NULL);

  return self->mark_begin;
}

/**
 * ide_source_snippet_get_mark_end:
 *
 * Returns: (transfer none):
 */
GtkTextMark *
ide_source_snippet_get_mark_end (IdeSourceSnippet *self)
{
  g_return_val_if_fail (IDE_IS_SOURCE_SNIPPET (self), NULL);

  return self->mark_end;
}

/**
 * ide_source_snippet_get_context:
 *
 * Returns: (transfer none):
 */
IdeSourceSnippetContext *
ide_source_snippet_get_context (IdeSourceSnippet *self)
{
  g_return_val_if_fail (IDE_IS_SOURCE_SNIPPET (self), NULL);

  if (!self->snippet_context)
    {
      IdeSourceSnippetChunk *chunk;
      guint i;

      self->snippet_context = ide_source_snippet_context_new ();

      for (i = 0; i < self->chunks->len; i++)
        {
          chunk = g_ptr_array_index (self->chunks, i);
          ide_source_snippet_chunk_set_context (chunk, self->snippet_context);
        }
    }

  return self->snippet_context;
}

static void
ide_source_snippet_dispose (GObject *object)
{
  IdeSourceSnippet *self = (IdeSourceSnippet *)object;

  if (self->mark_begin)
    {
      g_object_remove_weak_pointer (G_OBJECT (self->mark_begin),
                                    (gpointer *) &self->mark_begin);
      gtk_text_buffer_delete_mark (self->buffer, self->mark_begin);
      self->mark_begin = NULL;
    }

  if (self->mark_end)
    {
      g_object_remove_weak_pointer (G_OBJECT (self->mark_end),
                                    (gpointer *) &self->mark_end);
      gtk_text_buffer_delete_mark (self->buffer, self->mark_end);
      self->mark_end = NULL;
    }

  g_clear_pointer (&self->runs, (GDestroyNotify) g_array_unref);
  g_clear_pointer (&self->chunks, (GDestroyNotify) g_ptr_array_unref);

  g_clear_object (&self->buffer);
  g_clear_object (&self->snippet_context);

  g_free(self->language);
  g_free(self->description);
}

static void
ide_source_snippet_finalize (GObject *object)
{
  G_OBJECT_CLASS (ide_source_snippet_parent_class)->finalize (object);
}

static void
ide_source_snippet_get_property (GObject    *object,
                                 guint       prop_id,
                                 GValue     *value,
                                 GParamSpec *pspec)
{
  IdeSourceSnippet *self = IDE_SOURCE_SNIPPET (object);

  switch (prop_id)
    {
    case PROP_BUFFER:
      g_value_set_object (value, self->buffer);
      break;

    case PROP_MARK_BEGIN:
      g_value_set_object (value, self->mark_begin);
      break;

    case PROP_MARK_END:
      g_value_set_object (value, self->mark_end);
      break;

    case PROP_TRIGGER:
      g_value_set_string (value, self->trigger);
      break;

    case PROP_LANGUAGE:
      g_value_set_string (value, self->language);
      break;

    case PROP_DESCRIPTION:
      g_value_set_string (value, self->description);
      break;

    case PROP_TAB_STOP:
      g_value_set_uint (value, self->tab_stop);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
ide_source_snippet_set_property (GObject      *object,
                                 guint         prop_id,
                                 const GValue *value,
                                 GParamSpec   *pspec)
{
  IdeSourceSnippet *self = IDE_SOURCE_SNIPPET (object);

  switch (prop_id)
    {
    case PROP_TRIGGER:
      ide_source_snippet_set_trigger (self, g_value_get_string (value));
      break;

    case PROP_LANGUAGE:
      ide_source_snippet_set_language (self, g_value_get_string (value));
      break;

    case PROP_DESCRIPTION:
      ide_source_snippet_set_description (self, g_value_get_string (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
ide_source_snippet_class_init (IdeSourceSnippetClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->dispose = ide_source_snippet_dispose;
  object_class->finalize = ide_source_snippet_finalize;
  object_class->get_property = ide_source_snippet_get_property;
  object_class->set_property = ide_source_snippet_set_property;

  gParamSpecs[PROP_BUFFER] =
    g_param_spec_object ("buffer",
                         _("Buffer"),
                         _("The GtkTextBuffer for the snippet."),
                         GTK_TYPE_TEXT_BUFFER,
                         (G_PARAM_READABLE | G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (object_class, PROP_BUFFER,
                                   gParamSpecs[PROP_BUFFER]);

  gParamSpecs[PROP_MARK_BEGIN] =
    g_param_spec_object ("mark-begin",
                         _("Mark Begin"),
                         _("The beginning text mark."),
                         GTK_TYPE_TEXT_MARK,
                         (G_PARAM_READABLE | G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (object_class, PROP_MARK_BEGIN,
                                   gParamSpecs[PROP_MARK_BEGIN]);

  gParamSpecs[PROP_MARK_END] =
    g_param_spec_object ("mark-end",
                         _("Mark End"),
                         _("The ending text mark."),
                         GTK_TYPE_TEXT_MARK,
                         (G_PARAM_READABLE | G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (object_class, PROP_MARK_END,
                                   gParamSpecs[PROP_MARK_END]);

  gParamSpecs[PROP_TRIGGER] =
    g_param_spec_string ("trigger",
                         _("Trigger"),
                         _("The trigger for the snippet."),
                         NULL,
                         (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (object_class, PROP_TRIGGER,
                                   gParamSpecs[PROP_TRIGGER]);

  gParamSpecs[PROP_LANGUAGE] =
    g_param_spec_string ("language",
                         _("Language"),
                         _("The language for the snippet."),
                         NULL,
                         (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (object_class, PROP_LANGUAGE,
                                   gParamSpecs[PROP_LANGUAGE]);

  gParamSpecs[PROP_DESCRIPTION] =
    g_param_spec_string ("description",
                         _("Description"),
                         _("The description for the snippet."),
                         NULL,
                         (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (object_class, PROP_DESCRIPTION,
                                   gParamSpecs[PROP_DESCRIPTION]);

  gParamSpecs[PROP_TAB_STOP] =
    g_param_spec_int ("tab-stop",
                      _("Tab Stop"),
                      _("The current tab stop."),
                      -1,
                      G_MAXINT,
                      -1,
                      (G_PARAM_READABLE | G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (object_class, PROP_TAB_STOP,
                                   gParamSpecs[PROP_TAB_STOP]);
}

static void
ide_source_snippet_init (IdeSourceSnippet *self)
{
  self->tab_stop = 0;
  self->max_tab_stop = -1;
  self->chunks = g_ptr_array_new_with_free_func (g_object_unref);
  self->runs = g_array_new (FALSE, FALSE, sizeof (gint));
  self->description = NULL;
}
