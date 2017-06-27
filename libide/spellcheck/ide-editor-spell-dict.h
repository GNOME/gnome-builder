/* ide-editor-spell-dict.h
 *
 * Copyright (C) 2016 Sébastien Lafargue <slafargue@gnome.org>
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

#ifndef IDE_EDITOR_SPELL_DICT_H
#define IDE_EDITOR_SPELL_DICT_H

#include <glib-object.h>
#include <gspell/gspell.h>

G_BEGIN_DECLS

#define IDE_TYPE_EDITOR_SPELL_DICT (ide_editor_spell_dict_get_type())

G_DECLARE_FINAL_TYPE (IdeEditorSpellDict, ide_editor_spell_dict, IDE, EDITOR_SPELL_DICT, GObject)

IdeEditorSpellDict         *ide_editor_spell_dict_new                        (GspellChecker         *checker);

GspellChecker              *ide_editor_spell_dict_get_checker                (IdeEditorSpellDict    *self);
GPtrArray                  *ide_editor_spell_dict_get_words                  (IdeEditorSpellDict    *self);
void                        ide_editor_spell_dict_set_checker                (IdeEditorSpellDict    *self,
                                                                              GspellChecker         *checker);
gboolean                    ide_editor_spell_dict_add_word_to_personal       (IdeEditorSpellDict    *self,
                                                                              const gchar           *word);
gboolean                    ide_editor_spell_dict_remove_word_from_personal  (IdeEditorSpellDict    *self,
                                                                              const gchar           *word);
gboolean                    ide_editor_spell_dict_personal_contains          (IdeEditorSpellDict    *self,
                                                                              const gchar           *word);

G_END_DECLS

#endif /* IDE_EDITOR_SPELL_DICT_H */

