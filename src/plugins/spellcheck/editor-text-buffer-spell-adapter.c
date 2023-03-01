/* editor-text-buffer-spell-adapter.c
 *
 * Copyright 2021 Christian Hergert <chergert@redhat.com>
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

#include "config.h"

#include "cjhtextregionprivate.h"

#include <libide-code.h>

#include "editor-spell-checker.h"
#include "editor-spell-cursor.h"
#include "editor-spell-language.h"
#include "editor-text-buffer-spell-adapter.h"

#define RUN_UNCHECKED      GSIZE_TO_POINTER(0)
#define RUN_CHECKED        GSIZE_TO_POINTER(1)
#define UPDATE_DELAY_MSECS 100
#define UPDATE_QUANTA_USEC (G_USEC_PER_SEC/1000L*2) /* 2 msec */
/* Keyboard repeat is 30 msec by default (see
 * org.gnome.desktop.peripherals.keyboard repeat-interval) so
 * we want something longer than that so we are likely
 * to get removed/re-added on each repeat movement.
 */
#define INVALIDATE_DELAY_MSECS 100

typedef struct
{
  gint64   deadline;
  guint    has_unchecked : 1;
} Update;

typedef struct
{
  gsize offset;
  guint found : 1;
} ScanForUnchecked;

struct _EditorTextBufferSpellAdapter
{
  GObject             parent_instance;

  GtkTextBuffer      *buffer;
  EditorSpellChecker *checker;
  CjhTextRegion      *region;
  GtkTextTag         *tag;
  GtkTextTag         *no_spell_check_tag;

  guint               cursor_position;
  guint               incoming_cursor_position;
  guint               queued_cursor_moved;

  gsize               update_source;

  guint               enabled : 1;
};

G_DEFINE_TYPE (EditorTextBufferSpellAdapter, editor_text_buffer_spell_adapter, G_TYPE_OBJECT)

enum {
  PROP_0,
  PROP_BUFFER,
  PROP_CHECKER,
  PROP_ENABLED,
  PROP_LANGUAGE,
  N_PROPS
};

static GParamSpec *properties [N_PROPS];

static inline gboolean
forward_word_end (EditorTextBufferSpellAdapter *self,
                  GtkTextIter                  *iter)
{
  return editor_spell_iter_forward_word_end (iter,
                                             editor_spell_checker_get_extra_word_chars (self->checker));
}

static inline gboolean
backward_word_start (EditorTextBufferSpellAdapter *self,
                     GtkTextIter                  *iter)
{
  return editor_spell_iter_backward_word_start (iter,
                                                editor_spell_checker_get_extra_word_chars (self->checker));
}

static gboolean
get_current_word (EditorTextBufferSpellAdapter *self,
                  GtkTextIter                  *begin,
                  GtkTextIter                  *end)
{
  if (gtk_text_buffer_get_selection_bounds (self->buffer, begin, end))
    return FALSE;

  if (gtk_text_iter_ends_word (end))
    {
      backward_word_start (self, begin);
      return TRUE;
    }

  if (!gtk_text_iter_starts_word (begin))
    {
      if (!gtk_text_iter_inside_word (begin))
        return FALSE;

      backward_word_start (self, begin);
    }

  if (!gtk_text_iter_ends_word (end))
    forward_word_end (self, end);

  return TRUE;
}

static gboolean
get_word_at_position (EditorTextBufferSpellAdapter *self,
                      guint                         position,
                      GtkTextIter                  *begin,
                      GtkTextIter                  *end)
{
  gtk_text_buffer_get_iter_at_offset (self->buffer, begin, position);
  *end = *begin;

  if (gtk_text_iter_ends_word (end))
    {
      backward_word_start (self, begin);
      return TRUE;
    }

  if (!gtk_text_iter_starts_word (begin))
    {
      if (!gtk_text_iter_inside_word (begin))
        return FALSE;

      backward_word_start (self, begin);
    }

  if (!gtk_text_iter_ends_word (end))
    forward_word_end (self, end);

  return TRUE;
}

EditorTextBufferSpellAdapter *
editor_text_buffer_spell_adapter_new (GtkTextBuffer      *buffer,
                                      EditorSpellChecker *checker)
{
  g_return_val_if_fail (GTK_IS_TEXT_BUFFER (buffer), NULL);
  g_return_val_if_fail (!checker || EDITOR_IS_SPELL_CHECKER (checker), NULL);

  return g_object_new (EDITOR_TYPE_TEXT_BUFFER_SPELL_ADAPTER,
                       "buffer", buffer,
                       "checker", checker,
                       NULL);
}

static gboolean
get_unchecked_start_cb (gsize                   offset,
                        const CjhTextRegionRun *run,
                        gpointer                user_data)
{
  gsize *pos = user_data;

  if (run->data == RUN_UNCHECKED)
    {
      *pos = offset;
      return TRUE;
    }

  return FALSE;
}

static gboolean
get_unchecked_start (CjhTextRegion *region,
                     GtkTextBuffer *buffer,
                     GtkTextIter   *iter)
{
  gsize pos = G_MAXSIZE;
  _cjh_text_region_foreach (region, get_unchecked_start_cb, &pos);
  if (pos == G_MAXSIZE)
    return FALSE;
  gtk_text_buffer_get_iter_at_offset (buffer, iter, pos);
  return TRUE;
}

static gboolean
editor_text_buffer_spell_adapter_update_range (EditorTextBufferSpellAdapter *self,
                                               gint64                        deadline)
{
  g_autoptr(EditorSpellCursor) cursor = NULL;
  GtkTextIter word_begin, word_end, begin;
  const char *extra_word_chars;
  gboolean ret = FALSE;
  guint checked = 0;

  g_assert (EDITOR_IS_TEXT_BUFFER_SPELL_ADAPTER (self));

  /* Ignore while we are loading or saving */
  if (ide_buffer_get_state (IDE_BUFFER (self->buffer)) != IDE_BUFFER_STATE_READY)
    return TRUE;

  extra_word_chars = editor_spell_checker_get_extra_word_chars (self->checker);
  cursor = editor_spell_cursor_new (self->buffer, self->region, self->no_spell_check_tag, extra_word_chars);

  /* Get the first unchecked position so that we can remove the tag
   * from it up to the first word match.
   */
  if (!get_unchecked_start (self->region, self->buffer, &begin))
    {
      _cjh_text_region_replace (self->region,
                                0,
                                _cjh_text_region_get_length (self->region),
                                RUN_CHECKED);
      return FALSE;
    }

  while (editor_spell_cursor_next (cursor, &word_begin, &word_end))
    {
      g_autofree char *word = gtk_text_iter_get_slice (&word_begin, &word_end);

      checked++;

      if (!editor_spell_checker_check_word (self->checker, word, -1))
        gtk_text_buffer_apply_tag (self->buffer, self->tag, &word_begin, &word_end);

      /* Check deadline every five words */
      if (checked % 5 == 0 && deadline < g_get_monotonic_time ())
        {
          ret = TRUE;
          break;
        }
    }

  _cjh_text_region_replace (self->region,
                            gtk_text_iter_get_offset (&begin),
                            gtk_text_iter_get_offset (&word_end) - gtk_text_iter_get_offset (&begin),
                            RUN_CHECKED);

  /* Now remove any tag for the current word to be less annoying */
  if (get_current_word (self, &word_begin, &word_end))
    gtk_text_buffer_remove_tag (self->buffer, self->tag, &word_begin, &word_end);

  return ret;
}

static gboolean
editor_text_buffer_spell_adapter_run (gint64   deadline,
                                      gpointer user_data)
{
  EditorTextBufferSpellAdapter *self = user_data;

  g_assert (EDITOR_IS_TEXT_BUFFER_SPELL_ADAPTER (self));

  if (!editor_text_buffer_spell_adapter_update_range (self, deadline))
    {
      self->update_source = 0;
      return G_SOURCE_REMOVE;
    }

  return G_SOURCE_CONTINUE;
}

static void
editor_text_buffer_spell_adapter_queue_update (EditorTextBufferSpellAdapter *self)
{
  g_assert (EDITOR_IS_TEXT_BUFFER_SPELL_ADAPTER (self));

  if (self->checker == NULL || self->buffer == NULL || !self->enabled)
    {
      gtk_source_scheduler_clear (&self->update_source);
      return;
    }

  if (self->update_source == 0)
    self->update_source = gtk_source_scheduler_add (editor_text_buffer_spell_adapter_run, self);
}

void
editor_text_buffer_spell_adapter_invalidate_all (EditorTextBufferSpellAdapter *self)
{
  GtkTextIter begin, end;
  gsize length;

  g_assert (EDITOR_IS_TEXT_BUFFER_SPELL_ADAPTER (self));

  if (!self->enabled)
    return;

  /* We remove using the known length from the region */
  if ((length = _cjh_text_region_get_length (self->region)) > 0)
    {
      _cjh_text_region_remove (self->region, 0, length - 1);
      editor_text_buffer_spell_adapter_queue_update (self);
    }

  /* We add using the length from the buffer because if we were not
   * enabled previously, the textregion would be empty.
   */
  gtk_text_buffer_get_bounds (self->buffer, &begin, &end);
  if (!gtk_text_iter_equal (&begin, &end))
    {
      length = gtk_text_iter_get_offset (&end) - gtk_text_iter_get_offset (&begin);
      _cjh_text_region_insert (self->region, 0, length, RUN_UNCHECKED);
      gtk_text_buffer_remove_tag (self->buffer, self->tag, &begin, &end);
    }
}

static void
on_tag_added_cb (EditorTextBufferSpellAdapter *self,
                 GtkTextTag                   *tag,
                 GtkTextTagTable              *tag_table)
{
  g_autofree char *name = NULL;

  g_assert (EDITOR_IS_TEXT_BUFFER_SPELL_ADAPTER (self));
  g_assert (GTK_IS_TEXT_TAG (tag));
  g_assert (GTK_IS_TEXT_TAG_TABLE (tag_table));

  g_object_get (tag,
                "name", &name,
                NULL);

  if (name && strcmp (name, "gtksourceview:context-classes:no-spell-check") == 0)
    {
      g_set_object (&self->no_spell_check_tag, tag);
      editor_text_buffer_spell_adapter_invalidate_all (self);
    }
}

static void
on_tag_removed_cb (EditorTextBufferSpellAdapter *self,
                   GtkTextTag                   *tag,
                   GtkTextTagTable              *tag_table)
{
  g_assert (EDITOR_IS_TEXT_BUFFER_SPELL_ADAPTER (self));
  g_assert (GTK_IS_TEXT_TAG (tag));
  g_assert (GTK_IS_TEXT_TAG_TABLE (tag_table));

  if (tag == self->no_spell_check_tag)
    {
      g_clear_object (&self->no_spell_check_tag);
      editor_text_buffer_spell_adapter_invalidate_all (self);
    }
}

static void
invalidate_tag_region_cb (EditorTextBufferSpellAdapter *self,
                          GtkTextTag                   *tag,
                          GtkTextIter                  *begin,
                          GtkTextIter                  *end,
                          GtkTextBuffer                *buffer)
{
  g_assert (EDITOR_IS_TEXT_BUFFER_SPELL_ADAPTER (self));
  g_assert (GTK_IS_TEXT_TAG (tag));
  g_assert (GTK_IS_TEXT_BUFFER (buffer));

  if (!self->enabled)
    return;

  if (tag == self->no_spell_check_tag)
    {
      gsize begin_offset = gtk_text_iter_get_offset (begin);
      gsize end_offset = gtk_text_iter_get_offset (end);

      _cjh_text_region_replace (self->region, begin_offset, end_offset - begin_offset, RUN_UNCHECKED);
      editor_text_buffer_spell_adapter_queue_update (self);
    }
}

static void
apply_error_style_cb (GtkSourceBuffer *buffer,
                      GParamSpec      *pspec,
                      GtkTextTag      *tag)
{
  GtkSourceStyleScheme *scheme;
  GtkSourceStyle *style;
  static GdkRGBA error_color;

  g_assert (GTK_SOURCE_IS_BUFFER (buffer));
  g_assert (GTK_IS_TEXT_TAG (tag));

  if G_UNLIKELY (error_color.alpha == .0)
    gdk_rgba_parse (&error_color, "#e01b24");

  g_object_set (tag,
                "underline", PANGO_UNDERLINE_ERROR_LINE,
                "underline-rgba", &error_color,
                "background-set", FALSE,
                "foreground-set", FALSE,
                "weight-set", FALSE,
                "variant-set", FALSE,
                "style-set", FALSE,
                "indent-set", FALSE,
                "size-set", FALSE,
                NULL);

  if ((scheme = gtk_source_buffer_get_style_scheme (buffer)))
    {
      if ((style = gtk_source_style_scheme_get_style (scheme, "def:misspelled-word")))
        gtk_source_style_apply (style, tag);
    }
}

static void
editor_text_buffer_spell_adapter_set_buffer (EditorTextBufferSpellAdapter *self,
                                             GtkTextBuffer                *buffer)
{
  g_assert (EDITOR_IS_TEXT_BUFFER_SPELL_ADAPTER (self));
  g_assert (GTK_IS_TEXT_BUFFER (buffer));

  if (g_set_weak_pointer (&self->buffer, buffer))
    {
      GtkTextIter begin, end;
      GtkTextTagTable *tag_table;
      guint offset;
      guint length;

      gtk_text_buffer_get_bounds (buffer, &begin, &end);

      offset = gtk_text_iter_get_offset (&begin);
      length = gtk_text_iter_get_offset (&end) - offset;

      _cjh_text_region_insert (self->region, offset, length, RUN_UNCHECKED);

      self->tag = gtk_text_buffer_create_tag (buffer, NULL,
                                              "underline", PANGO_UNDERLINE_ERROR,
                                              NULL);

      g_signal_connect_object (buffer,
                               "notify::style-scheme",
                               G_CALLBACK (apply_error_style_cb),
                               self->tag,
                               0);
      apply_error_style_cb (GTK_SOURCE_BUFFER (buffer), NULL, self->tag);

      /* Track tag changes from the tag table and extract "no-spell-check"
       * tag from GtkSourceView so that we can avoid words with that tag.
       */
      tag_table = gtk_text_buffer_get_tag_table (buffer);
      g_signal_connect_object (tag_table,
                               "tag-added",
                               G_CALLBACK (on_tag_added_cb),
                               self,
                               G_CONNECT_SWAPPED);
      g_signal_connect_object (tag_table,
                               "tag-removed",
                               G_CALLBACK (on_tag_removed_cb),
                               self,
                               G_CONNECT_SWAPPED);

      g_signal_connect_object (buffer,
                               "apply-tag",
                               G_CALLBACK (invalidate_tag_region_cb),
                               self,
                               G_CONNECT_SWAPPED);
      g_signal_connect_object (buffer,
                               "remove-tag",
                               G_CALLBACK (invalidate_tag_region_cb),
                               self,
                               G_CONNECT_SWAPPED);

      editor_text_buffer_spell_adapter_queue_update (self);
    }
}

void
editor_text_buffer_spell_adapter_set_enabled (EditorTextBufferSpellAdapter *self,
                                              gboolean                      enabled)
{
  g_assert (EDITOR_IS_TEXT_BUFFER_SPELL_ADAPTER (self));

  enabled = !!enabled;

  if (self->enabled != enabled)
    {
      GtkTextIter begin, end;

      self->enabled = enabled;

      if (self->buffer && self->tag && !self->enabled)
        {
          gtk_text_buffer_get_bounds (self->buffer, &begin, &end);
          gtk_text_buffer_remove_tag (self->buffer, self->tag, &begin, &end);
        }

      editor_text_buffer_spell_adapter_invalidate_all (self);
      editor_text_buffer_spell_adapter_queue_update (self);

      g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_ENABLED]);
    }
}

static void
editor_text_buffer_spell_adapter_finalize (GObject *object)
{
  EditorTextBufferSpellAdapter *self = (EditorTextBufferSpellAdapter *)object;

  g_clear_object (&self->checker);
  g_clear_object (&self->no_spell_check_tag);
  g_clear_pointer (&self->region, _cjh_text_region_free);

  G_OBJECT_CLASS (editor_text_buffer_spell_adapter_parent_class)->finalize (object);
}

static void
editor_text_buffer_spell_adapter_dispose (GObject *object)
{
  EditorTextBufferSpellAdapter *self = (EditorTextBufferSpellAdapter *)object;

  g_clear_weak_pointer (&self->buffer);
  gtk_source_scheduler_clear (&self->update_source);

  G_OBJECT_CLASS (editor_text_buffer_spell_adapter_parent_class)->dispose (object);
}

static void
editor_text_buffer_spell_adapter_get_property (GObject    *object,
                                               guint       prop_id,
                                               GValue     *value,
                                               GParamSpec *pspec)
{
  EditorTextBufferSpellAdapter *self = EDITOR_TEXT_BUFFER_SPELL_ADAPTER (object);

  switch (prop_id)
    {
    case PROP_BUFFER:
      g_value_set_object (value, self->buffer);
      break;

    case PROP_CHECKER:
      g_value_set_object (value, editor_text_buffer_spell_adapter_get_checker (self));
      break;

    case PROP_ENABLED:
      g_value_set_boolean (value, self->enabled);
      break;

    case PROP_LANGUAGE:
      g_value_set_string (value, editor_text_buffer_spell_adapter_get_language (self));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
editor_text_buffer_spell_adapter_set_property (GObject      *object,
                                               guint         prop_id,
                                               const GValue *value,
                                               GParamSpec   *pspec)
{
  EditorTextBufferSpellAdapter *self = EDITOR_TEXT_BUFFER_SPELL_ADAPTER (object);

  switch (prop_id)
    {
    case PROP_BUFFER:
      editor_text_buffer_spell_adapter_set_buffer (self, g_value_get_object (value));
      break;

    case PROP_CHECKER:
      editor_text_buffer_spell_adapter_set_checker (self, g_value_get_object (value));
      break;

    case PROP_ENABLED:
      editor_text_buffer_spell_adapter_set_enabled (self, g_value_get_boolean (value));
      break;

    case PROP_LANGUAGE:
      editor_text_buffer_spell_adapter_set_language (self, g_value_get_string (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
editor_text_buffer_spell_adapter_class_init (EditorTextBufferSpellAdapterClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->dispose = editor_text_buffer_spell_adapter_dispose;
  object_class->finalize = editor_text_buffer_spell_adapter_finalize;
  object_class->get_property = editor_text_buffer_spell_adapter_get_property;
  object_class->set_property = editor_text_buffer_spell_adapter_set_property;

  properties [PROP_BUFFER] =
    g_param_spec_object ("buffer",
                         "Buffer",
                         "Buffer",
                         GTK_TYPE_TEXT_BUFFER,
                         (G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS));

  properties [PROP_CHECKER] =
    g_param_spec_object ("checker",
                         "Checker",
                         "Checker",
                         EDITOR_TYPE_SPELL_CHECKER,
                         (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  properties [PROP_ENABLED] =
    g_param_spec_boolean ("enabled",
                          "Enabled",
                          "If spellcheck is enabled",
                          TRUE,
                          (G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS));

  properties [PROP_LANGUAGE] =
    g_param_spec_string ("language",
                         "Language",
                         "The language code such as en_US",
                         NULL,
                         (G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, N_PROPS, properties);
}

static void
editor_text_buffer_spell_adapter_init (EditorTextBufferSpellAdapter *self)
{
  self->region = _cjh_text_region_new (NULL, NULL);
}

EditorSpellChecker *
editor_text_buffer_spell_adapter_get_checker (EditorTextBufferSpellAdapter *self)
{
  g_return_val_if_fail (EDITOR_IS_TEXT_BUFFER_SPELL_ADAPTER (self), NULL);

  return self->checker;
}

void
editor_text_buffer_spell_adapter_set_checker (EditorTextBufferSpellAdapter *self,
                                              EditorSpellChecker           *checker)
{
  g_return_if_fail (EDITOR_IS_TEXT_BUFFER_SPELL_ADAPTER (self));
  g_return_if_fail (!checker || EDITOR_IS_SPELL_CHECKER (checker));

  if (g_set_object (&self->checker, checker))
    {
      gsize length = _cjh_text_region_get_length (self->region);

      gtk_source_scheduler_clear (&self->update_source);

      if (length > 0)
        {
          _cjh_text_region_remove (self->region, 0, length);
          _cjh_text_region_insert (self->region, 0, length, RUN_UNCHECKED);
          g_assert_cmpint (length, ==, _cjh_text_region_get_length (self->region));
        }

      editor_text_buffer_spell_adapter_queue_update (self);

      g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_CHECKER]);
      g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_LANGUAGE]);
    }
}

GtkTextBuffer *
editor_text_buffer_spell_adapter_get_buffer (EditorTextBufferSpellAdapter *self)
{
  g_return_val_if_fail (EDITOR_IS_TEXT_BUFFER_SPELL_ADAPTER (self), NULL);

  return self->buffer;
}

static void
mark_unchecked (EditorTextBufferSpellAdapter *self,
                guint                         offset,
                guint                         length)
{
  GtkTextIter begin, end;

  g_assert (EDITOR_IS_TEXT_BUFFER_SPELL_ADAPTER (self));
  g_assert (GTK_IS_TEXT_BUFFER (self->buffer));
  g_assert (self->enabled);

  gtk_text_buffer_get_iter_at_offset (self->buffer, &begin, offset);
  gtk_text_buffer_get_iter_at_offset (self->buffer, &end, offset + length);

  if (!gtk_text_iter_starts_word (&begin))
    backward_word_start (self, &begin);

  if (!gtk_text_iter_ends_word (&end))
    forward_word_end (self, &end);

  _cjh_text_region_replace (self->region,
                            gtk_text_iter_get_offset (&begin),
                            gtk_text_iter_get_offset (&end) - gtk_text_iter_get_offset (&begin),
                            RUN_UNCHECKED);

  gtk_text_buffer_remove_tag (self->buffer, self->tag, &begin, &end);

  editor_text_buffer_spell_adapter_queue_update (self);
}

void
editor_text_buffer_spell_adapter_before_insert_text (EditorTextBufferSpellAdapter *self,
                                                     guint                         offset,
                                                     guint                         length)
{
  if (self->enabled)
    _cjh_text_region_insert (self->region, offset, length, RUN_UNCHECKED);
}


void
editor_text_buffer_spell_adapter_after_insert_text (EditorTextBufferSpellAdapter *self,
                                                    guint                         offset,
                                                    guint                         length)
{
  if (self->enabled)
    mark_unchecked (self, offset, length);
}

void
editor_text_buffer_spell_adapter_before_delete_range (EditorTextBufferSpellAdapter *self,
                                                      guint                         offset,
                                                      guint                         length)
{
  if (self->enabled)
    _cjh_text_region_remove (self->region, offset, length);
}

void
editor_text_buffer_spell_adapter_after_delete_range (EditorTextBufferSpellAdapter *self,
                                                     guint                         offset,
                                                     guint                         length)
{
  if (self->enabled)
    mark_unchecked (self, offset, 0);
}

static gboolean
editor_text_buffer_spell_adapter_cursor_moved_cb (gpointer data)
{
  EditorTextBufferSpellAdapter *self = data;
  GtkTextIter begin, end;

  g_assert (EDITOR_IS_TEXT_BUFFER_SPELL_ADAPTER (self));

  self->queued_cursor_moved = 0;

  /* Invalidate the old position */
  if (self->enabled && get_word_at_position (self, self->cursor_position, &begin, &end))
    mark_unchecked (self,
                    gtk_text_iter_get_offset (&begin),
                    gtk_text_iter_get_offset (&end) - gtk_text_iter_get_offset (&begin));

  self->cursor_position = self->incoming_cursor_position;

  /* Invalidate word at new position */
  if (self->enabled && get_word_at_position (self, self->cursor_position, &begin, &end))
    mark_unchecked (self,
                    gtk_text_iter_get_offset (&begin),
                    gtk_text_iter_get_offset (&end) - gtk_text_iter_get_offset (&begin));

  return G_SOURCE_REMOVE;
}

void
editor_text_buffer_spell_adapter_cursor_moved (EditorTextBufferSpellAdapter *self,
                                               guint                         position)
{
  g_return_if_fail (EDITOR_IS_TEXT_BUFFER_SPELL_ADAPTER (self));
  g_return_if_fail (self->buffer != NULL);

  if (!self->enabled)
    return;

  self->incoming_cursor_position = position;
  g_clear_handle_id (&self->queued_cursor_moved, g_source_remove);
  self->queued_cursor_moved = g_timeout_add_full (G_PRIORITY_LOW,
                                                  INVALIDATE_DELAY_MSECS,
                                                  editor_text_buffer_spell_adapter_cursor_moved_cb,
                                                  g_object_ref (self),
                                                  g_object_unref);
}

const char *
editor_text_buffer_spell_adapter_get_language (EditorTextBufferSpellAdapter *self)
{
  g_return_val_if_fail (EDITOR_IS_TEXT_BUFFER_SPELL_ADAPTER (self), NULL);

  return self->checker ? editor_spell_checker_get_language (self->checker) : NULL;
}

void
editor_text_buffer_spell_adapter_set_language (EditorTextBufferSpellAdapter *self,
                                               const char                   *language)
{
  g_return_if_fail (EDITOR_IS_TEXT_BUFFER_SPELL_ADAPTER (self));

  if (self->checker == NULL && language == NULL)
    return;

  if (self->checker == NULL)
    {
      self->checker = editor_spell_checker_new (NULL, language);
      g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_CHECKER]);
      g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_LANGUAGE]);
    }
  else if (g_strcmp0 (language, editor_text_buffer_spell_adapter_get_language (self)) != 0)
    {
      editor_spell_checker_set_language (self->checker, language);
      g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_LANGUAGE]);
    }

  editor_text_buffer_spell_adapter_invalidate_all (self);
}

GtkTextTag *
editor_text_buffer_spell_adapter_get_tag (EditorTextBufferSpellAdapter *self)
{
  g_return_val_if_fail (EDITOR_IS_TEXT_BUFFER_SPELL_ADAPTER (self), NULL);

  return self->tag;
}

gboolean
editor_text_buffer_spell_adapter_get_enabled (EditorTextBufferSpellAdapter *self)
{
  return self ? self->enabled : FALSE;
}
