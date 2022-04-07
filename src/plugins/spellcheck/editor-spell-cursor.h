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

#pragma once

#include <gtk/gtk.h>

G_BEGIN_DECLS

typedef struct _EditorSpellCursor EditorSpellCursor;
typedef struct _CjhTextRegion     CjhTextRegion;

EditorSpellCursor *editor_spell_cursor_new               (GtkTextBuffer     *buffer,
                                                          CjhTextRegion     *region,
                                                          GtkTextTag        *no_spell_check_tag,
                                                          const char        *extra_word_chars);
void               editor_spell_cursor_free              (EditorSpellCursor *cursor);
gboolean           editor_spell_cursor_next              (EditorSpellCursor *cursor,
                                                          GtkTextIter       *word_begin,
                                                          GtkTextIter       *word_end);
gboolean           editor_spell_iter_forward_word_end    (GtkTextIter       *iter,
                                                          const char        *extra_word_chars);
gboolean           editor_spell_iter_backward_word_start (GtkTextIter       *iter,
                                                          const char        *extra_word_chars);

G_DEFINE_AUTOPTR_CLEANUP_FUNC (EditorSpellCursor, editor_spell_cursor_free)

G_END_DECLS
