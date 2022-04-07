/* editor-spell-language.h
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

#define EDITOR_TYPE_SPELL_LANGUAGE (editor_spell_language_get_type())

G_DECLARE_DERIVABLE_TYPE (EditorSpellLanguage, editor_spell_language, EDITOR, SPELL_LANGUAGE, GObject)

struct _EditorSpellLanguageClass
{
  GObjectClass parent_class;

  gboolean     (*contains_word)        (EditorSpellLanguage *self,
                                        const char          *word,
                                        gssize               word_len);
  char       **(*list_corrections)     (EditorSpellLanguage *self,
                                        const char          *word,
                                        gssize               word_len);
  void         (*add_word)             (EditorSpellLanguage *self,
                                        const char          *word);
  void         (*ignore_word)          (EditorSpellLanguage *self,
                                        const char          *word);
  const char  *(*get_extra_word_chars) (EditorSpellLanguage *self);

  /*< private >*/
  gpointer _reserved[8];
};

const char  *editor_spell_language_get_code             (EditorSpellLanguage *self);
gboolean     editor_spell_language_contains_word        (EditorSpellLanguage *self,
                                                         const char          *word,
                                                         gssize               word_len);
char       **editor_spell_language_list_corrections     (EditorSpellLanguage *self,
                                                         const char          *word,
                                                         gssize               word_len);
void         editor_spell_language_add_word             (EditorSpellLanguage *self,
                                                         const char          *word);
void         editor_spell_language_ignore_word          (EditorSpellLanguage *self,
                                                         const char          *word);
const char  *editor_spell_language_get_extra_word_chars (EditorSpellLanguage *self);

G_END_DECLS
