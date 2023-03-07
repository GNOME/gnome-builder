/* editor-spell-cursor.c
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

#ifndef G_LOG_DOMAIN
# define G_LOG_DOMAIN "editor-spell-cursor"
#endif

#include "config.h"

#include "cjhtextregionprivate.h"
#include "editor-spell-cursor.h"

#define RUN_UNCHECKED NULL

#if 1
# define RETURN(r) G_STMT_START { return (r); } G_STMT_END
#else
# define RETURN(r) G_STMT_START { typeof(r) _r = (r); g_debug (" EXIT: %s(): %s", G_STRFUNC, G_STRLOC); return (_r); } G_STMT_END
#endif

typedef struct
{
  CjhTextRegion *region;
  GtkTextBuffer *buffer;
  gssize pos;
} RegionIter;

typedef struct
{
  GtkTextBuffer *buffer;
  GtkTextTag *tag;
  GtkTextIter pos;
} TagIter;

typedef struct
{
  GtkTextBuffer *buffer;
  GtkTextIter word_begin;
  GtkTextIter word_end;
} WordIter;

struct _EditorSpellCursor
{
  RegionIter region;
  TagIter tag;
  WordIter word;
  const char *extra_word_chars;
};

static void
region_iter_init (RegionIter    *self,
                  GtkTextBuffer *buffer,
                  CjhTextRegion *region)
{
  self->region = region;
  self->buffer = buffer;
  self->pos = -1;
}

static gboolean
region_iter_next_cb (gsize                   position,
                     const CjhTextRegionRun *run,
                     gpointer                user_data)
{
  if (run->data == RUN_UNCHECKED)
    {
      gsize *pos = user_data;
      *pos = position;
      RETURN (TRUE);
    }

  RETURN (FALSE);
}

static gboolean
region_iter_next (RegionIter  *self,
                  GtkTextIter *iter)
{
  gsize pos, new_pos;

  if (self->pos >= (gssize)_cjh_text_region_get_length (self->region))
    {
      gtk_text_buffer_get_end_iter (self->buffer, iter);
      RETURN (FALSE);
    }

  if (self->pos < 0)
    pos = 0;
  else
    pos = self->pos;

  _cjh_text_region_foreach_in_range (self->region,
                                     pos,
                                     _cjh_text_region_get_length (self->region),
                                     region_iter_next_cb,
                                     &new_pos);

  pos = MAX (pos, new_pos);
  gtk_text_buffer_get_iter_at_offset (self->buffer, iter, pos);
  self->pos = pos;

  RETURN (TRUE);
}

static void
region_iter_seek (RegionIter        *self,
                  const GtkTextIter *iter)
{
  /* Move to position past the word */
  self->pos = gtk_text_iter_get_offset (iter) + 1;
}

static void
tag_iter_init (TagIter       *self,
               GtkTextBuffer *buffer,
               GtkTextTag    *tag)
{
  self->buffer = buffer;
  self->tag = tag;
  gtk_text_buffer_get_start_iter (buffer, &self->pos);
}

static gboolean
tag_iter_next (TagIter     *self,
               GtkTextIter *pos)
{
  if (self->tag && gtk_text_iter_has_tag (&self->pos, self->tag))
    {
      /* Should always succeed because we are within the tag */
      gtk_text_iter_forward_to_tag_toggle (&self->pos, self->tag);
    }

  *pos = self->pos;

  RETURN (TRUE);
}

static void
tag_iter_seek (TagIter           *self,
               const GtkTextIter *iter)
{
  self->pos = *iter;
}

static inline gboolean
utf8_contains_unichar (const char *utf8,
                       gunichar    ch)
{
  if (ch == 0)
    RETURN (FALSE);

  for (const char *c = utf8; *c; c = g_utf8_next_char (c))
    {
      if (ch == g_utf8_get_char (c))
        RETURN (TRUE);
    }

  RETURN (FALSE);
}

static inline gboolean
is_word_char (const GtkTextIter *iter,
              const char        *extra_word_chars)
{
  if (gtk_text_iter_starts_word (iter))
    RETURN (TRUE);

  if (gtk_text_iter_inside_word (iter))
    RETURN (TRUE);

  if (extra_word_chars != NULL)
    {
      if (utf8_contains_unichar (extra_word_chars, gtk_text_iter_get_char (iter)))
        RETURN (TRUE);
    }

  RETURN (FALSE);
}

gboolean
editor_spell_iter_forward_word_end (GtkTextIter *iter,
                                    const char  *extra_word_chars)
{
  GtkTextIter orig = *iter;
  GtkTextIter peek;

  if (!gtk_text_iter_forward_word_end (iter))
    {
      /* We might get here if we moved but are at the end of the buffer.
       * If the previous character ends a word, that is fine too.
       */
      if (gtk_text_iter_is_end (iter) &&
          !gtk_text_iter_equal (iter, &orig))
        RETURN (gtk_text_iter_ends_word (iter));

      RETURN (FALSE);
    }

  if (!is_word_char (iter, extra_word_chars))
    RETURN (TRUE);

  peek = *iter;

  while (gtk_text_iter_forward_char (&peek) &&
         is_word_char (&peek, extra_word_chars)) { /* Do nothing */ }

  *iter = peek;

  RETURN (TRUE);
}

gboolean
editor_spell_iter_backward_word_start (GtkTextIter *iter,
                                       const char  *extra_word_chars)
{
  GtkTextIter peek;

  if (gtk_text_iter_is_start (iter))
    RETURN (FALSE);

  if (!gtk_text_iter_backward_word_start (iter))
    RETURN (gtk_text_iter_starts_word (iter));

  peek = *iter;

  while (is_word_char (&peek, extra_word_chars) &&
         gtk_text_iter_backward_char (&peek)) { /* Do Nothing */ }

  if (!gtk_text_iter_is_start (&peek) || !is_word_char (&peek, extra_word_chars))
    {
      if (!gtk_text_iter_equal (&peek, iter))
        gtk_text_iter_forward_char (&peek);
    }

  *iter = peek;

  RETURN (TRUE);
}

static void
word_iter_init (WordIter      *self,
                GtkTextBuffer *buffer)
{
  self->buffer = buffer;
  gtk_text_buffer_get_start_iter (buffer, &self->word_begin);
  self->word_end = self->word_begin;
}

static gboolean
word_iter_next (WordIter    *self,
                GtkTextIter *word_begin,
                GtkTextIter *word_end,
                const char  *extra_word_chars)
{
  if (!editor_spell_iter_forward_word_end (&self->word_end, extra_word_chars))
    {
      *word_begin = self->word_end;
      *word_end = self->word_end;
      RETURN (FALSE);
    }

  self->word_begin = self->word_end;

  if (!editor_spell_iter_backward_word_start (&self->word_begin, extra_word_chars))
    {
      *word_begin = self->word_end;
      *word_end = self->word_end;
      RETURN (FALSE);
    }

  *word_begin = self->word_begin;
  *word_end = self->word_end;

  RETURN (TRUE);
}

static void
word_iter_seek (WordIter          *self,
                const GtkTextIter *iter)
{
  self->word_begin = *iter;
  self->word_end = *iter;
}

EditorSpellCursor *
editor_spell_cursor_new (GtkTextBuffer *buffer,
                         CjhTextRegion *region,
                         GtkTextTag    *no_spell_check_tag,
                         const char    *extra_word_chars)
{
  EditorSpellCursor *self;

  g_return_val_if_fail (GTK_IS_TEXT_BUFFER (buffer), NULL);
  g_return_val_if_fail (region != NULL, NULL);
  g_return_val_if_fail (!no_spell_check_tag || GTK_IS_TEXT_TAG (no_spell_check_tag), NULL);

  self = g_rc_box_new0 (EditorSpellCursor);
  region_iter_init (&self->region, buffer, region);
  tag_iter_init (&self->tag, buffer, no_spell_check_tag);
  word_iter_init (&self->word, buffer);
  self->extra_word_chars = extra_word_chars ? g_intern_string (extra_word_chars) : "";

  return self;
}

void
editor_spell_cursor_free (EditorSpellCursor *self)
{
  g_rc_box_release (self);
}

static gboolean
contains_tag (const GtkTextIter *word_begin,
              const GtkTextIter *word_end,
              GtkTextTag        *tag)
{
  GtkTextIter toggle_iter;

  if (tag == NULL)
    RETURN (FALSE);

  if (gtk_text_iter_has_tag (word_begin, tag))
    RETURN (TRUE);

  toggle_iter = *word_begin;
  if (!gtk_text_iter_forward_to_tag_toggle (&toggle_iter, tag))
    RETURN (FALSE);

  RETURN (gtk_text_iter_compare (word_end, &toggle_iter) > 0);
}

gboolean
editor_spell_cursor_next (EditorSpellCursor *self,
                          GtkTextIter       *word_begin,
                          GtkTextIter       *word_end)
{
  /* Try to advance skipping any checked region in the buffer */
  if (!region_iter_next (&self->region, word_end))
    {
      *word_begin = *word_end;
      RETURN (FALSE);
    }

  /* Pass that position to the next iter, so it can skip
   * past anything that is already checked. Then try to move
   * forward so that we can skip past regions in the text
   * buffer that are to be ignored by spellcheck.
   */
  tag_iter_seek (&self->tag, word_end);
  if (!tag_iter_next (&self->tag, word_end))
    {
      *word_begin = *word_end;
      RETURN (FALSE);
    }

  /* Now pass that information to the word iter, so that it can
   * jump forward to the next word starting from our tag/region
   * positions.
   */
  word_iter_seek (&self->word, word_end);
  if (!word_iter_next (&self->word, word_begin, word_end, self->extra_word_chars))
    RETURN (FALSE);

  /* Now pass our new position to the region so that it will
   * skip past the word when advancing.
   */
  region_iter_seek (&self->region, word_end);

  /* If this word contains the no-spell-check tag, then try
   * again to skip past even more content.
   */
  if (contains_tag (word_begin, word_end, self->tag.tag))
    RETURN (editor_spell_cursor_next (self, word_begin, word_end));

  RETURN (TRUE);
}
