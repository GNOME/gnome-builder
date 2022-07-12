/* editor-spell-checker.h
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

#include "editor-types.h"

G_BEGIN_DECLS

#define EDITOR_TYPE_SPELL_CHECKER (editor_spell_checker_get_type())

G_DECLARE_FINAL_TYPE (EditorSpellChecker, editor_spell_checker, EDITOR, SPELL_CHECKER, GObject)

EditorSpellChecker   *editor_spell_checker_new                  (EditorSpellProvider *provider,
                                                                 const char          *language);
EditorSpellProvider  *editor_spell_checker_get_provider         (EditorSpellChecker  *self);
const char           *editor_spell_checker_get_language         (EditorSpellChecker  *self);
void                  editor_spell_checker_set_language         (EditorSpellChecker  *self,
                                                                 const char          *language);
gboolean              editor_spell_checker_check_word           (EditorSpellChecker  *self,
                                                                 const char          *word,
                                                                 gssize               word_len);
char                **editor_spell_checker_list_corrections     (EditorSpellChecker  *self,
                                                                 const char          *word);
void                  editor_spell_checker_add_word             (EditorSpellChecker  *self,
                                                                 const char          *word);
void                  editor_spell_checker_ignore_word          (EditorSpellChecker  *self,
                                                                 const char          *word);
const char           *editor_spell_checker_get_extra_word_chars (EditorSpellChecker  *self);

G_END_DECLS
