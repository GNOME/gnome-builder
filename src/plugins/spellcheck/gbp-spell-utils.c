/* gbp-spell-utils.c
 *
 * Copyright 2016 Sebastien Lafargue <slafargue@gnome.org>
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
 * This code is mostly from:
 * https://git.gnome.org/browse/gspell/tree/gspell/gspell-utils.c
 * https://git.gnome.org/browse/gspell/tree/gspell/gspell-text-iter.c
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#define G_LOG_DOMAIN "gbp-spell-utils"

#include <gtk/gtk.h>

#include "gbp-spell-utils.h"

/* gunichar decimal value of unicode apostrophe characters. */
#define GSPELL_MODIFIER_LETTER_APOSTROPHE (700) /* U+02BC */
#define GSPELL_RIGHT_SINGLE_QUOTATION_MARK (8217) /* U+2019 */

static inline gboolean
is__text_iter_apostrophe_or_dash (const GtkTextIter *iter)
{
  gunichar ch;

  ch = gtk_text_iter_get_char (iter);

  return (ch == '-' ||
          ch == '\'' ||
          ch == GSPELL_MODIFIER_LETTER_APOSTROPHE ||
          ch == GSPELL_RIGHT_SINGLE_QUOTATION_MARK);
}

gboolean
gbp_spell_utils_text_iter_forward_word_end (GtkTextIter *iter)
{
  g_return_val_if_fail (iter != NULL, FALSE);

  while (gtk_text_iter_forward_word_end (iter))
    {
      GtkTextIter next_char;

      if (!is__text_iter_apostrophe_or_dash (iter))
        return TRUE;

      next_char = *iter;
      gtk_text_iter_forward_char (&next_char);
      if (!gtk_text_iter_starts_word (&next_char))
        return TRUE;

      *iter = next_char;
    }

  return FALSE;
}

gboolean
gbp_spell_utils_text_iter_backward_word_start (GtkTextIter *iter)
{
  g_return_val_if_fail (iter != NULL, FALSE);

  while (gtk_text_iter_backward_word_start (iter))
    {
      GtkTextIter prev_char = *iter;

      if (!gtk_text_iter_backward_char (&prev_char) ||
          !is__text_iter_apostrophe_or_dash (&prev_char) ||
          !gtk_text_iter_ends_word (&prev_char))
        return TRUE;

      *iter = prev_char;
    }

  return FALSE;
}

gboolean
gbp_spell_utils_text_iter_starts_word (const GtkTextIter *iter)
{
  GtkTextIter prev_char;

  g_return_val_if_fail (iter != NULL, FALSE);

  if (!gtk_text_iter_starts_word (iter))
    return FALSE;

  prev_char = *iter;
  if (!gtk_text_iter_backward_char (&prev_char))
    return TRUE;

  if (is__text_iter_apostrophe_or_dash (&prev_char) &&
      gtk_text_iter_ends_word (&prev_char))
    return FALSE;

  return TRUE;
}

gboolean
gbp_spell_utils_text_iter_ends_word (const GtkTextIter *iter)
{
  GtkTextIter next_char;

  g_return_val_if_fail (iter != NULL, FALSE);

  if (!gtk_text_iter_ends_word (iter))
    return FALSE;

  if (gtk_text_iter_is_end (iter))
    return TRUE;

  next_char = *iter;
  gtk_text_iter_forward_char (&next_char);

  if (is__text_iter_apostrophe_or_dash (iter) &&
      gtk_text_iter_starts_word (&next_char))
    return FALSE;

  return TRUE;
}

gboolean
gbp_spell_utils_text_iter_inside_word (const GtkTextIter *iter)
{
  g_return_val_if_fail (iter != NULL, FALSE);

  if (gtk_text_iter_inside_word (iter))
    return TRUE;

  if (gtk_text_iter_ends_word (iter) &&
      is__text_iter_apostrophe_or_dash (iter))
    {
      GtkTextIter next_char = *iter;
      gtk_text_iter_forward_char (&next_char);
      return gtk_text_iter_starts_word (&next_char);
    }

  return FALSE;
}

GtkTextTag *
gbp_spell_utils_get_no_spell_check_tag (GtkTextBuffer *buffer)
{
  GtkTextTagTable *tag_table;

  g_return_val_if_fail (GTK_IS_TEXT_BUFFER (buffer), NULL);

  tag_table = gtk_text_buffer_get_tag_table (buffer);

  return gtk_text_tag_table_lookup (tag_table, "gtksourceview:context-classes:no-spell-check");
}

gboolean
gbp_spell_utils_skip_no_spell_check (GtkTextTag        *no_spell_check_tag,
                                     GtkTextIter       *start,
                                     const GtkTextIter *end)
{
  g_return_val_if_fail (start != NULL, FALSE);
  g_return_val_if_fail (end != NULL, FALSE);

  if (no_spell_check_tag == NULL)
    return TRUE;

  g_return_val_if_fail (GTK_IS_TEXT_TAG (no_spell_check_tag), FALSE);

  while (gtk_text_iter_has_tag (start, no_spell_check_tag))
    {
      GtkTextIter last = *start;

      if (!gtk_text_iter_forward_to_tag_toggle (start, no_spell_check_tag))
        return FALSE;

      if (gtk_text_iter_compare (start, &last) <= 0)
        return FALSE;

      gbp_spell_utils_text_iter_forward_word_end (start);
      gbp_spell_utils_text_iter_backward_word_start (start);

      if (gtk_text_iter_compare (start, &last) <= 0)
        return FALSE;

      if (gtk_text_iter_compare (start, end) >= 0)
        return FALSE;
  }

  return TRUE;
}
