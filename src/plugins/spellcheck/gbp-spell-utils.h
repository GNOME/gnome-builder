/* gbp-spell-utils.h
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
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include <gtk/gtk.h>

G_BEGIN_DECLS

gboolean    gbp_spell_utils_text_iter_forward_word_end    (GtkTextIter        *iter);
gboolean    gbp_spell_utils_text_iter_backward_word_start (GtkTextIter        *iter);
gboolean    gbp_spell_utils_text_iter_starts_word         (const GtkTextIter  *iter);
gboolean    gbp_spell_utils_text_iter_ends_word           (const GtkTextIter  *iter);
gboolean    gbp_spell_utils_text_iter_inside_word         (const GtkTextIter  *iter);
GtkTextTag *gbp_spell_utils_get_no_spell_check_tag        (GtkTextBuffer      *buffer);
gboolean    gbp_spell_utils_skip_no_spell_check           (GtkTextTag         *no_spell_check_tag,
                                                           GtkTextIter        *start,
                                                           const GtkTextIter  *end);

G_END_DECLS
