/* ide-editor-spell-navigator.h
 *
 * Copyright (C) 2016 Sebastien Lafargue <slafargue@gnome.org>
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

#ifndef IDE_EDITOR_SPELL_NAVIGATOR_H
#define IDE_EDITOR_SPELL_NAVIGATOR_H

#include <glib-object.h>
#include <gspell/gspell.h>

G_BEGIN_DECLS

#define IDE_TYPE_EDITOR_SPELL_NAVIGATOR (ide_editor_spell_navigator_get_type())

G_DECLARE_FINAL_TYPE (IdeEditorSpellNavigator, ide_editor_spell_navigator, IDE, EDITOR_SPELL_NAVIGATOR, GInitiallyUnowned)

GspellNavigator *ide_editor_spell_navigator_new                   (GtkTextView             *view);
guint            ide_editor_spell_navigator_get_count             (IdeEditorSpellNavigator *self,
                                                                   const gchar             *word);
gboolean         ide_editor_spell_navigator_get_is_words_counted  (IdeEditorSpellNavigator *self);
gboolean         ide_editor_spell_navigator_goto_word_start       (IdeEditorSpellNavigator *self);

G_END_DECLS

#endif /* IDE_EDITOR_SPELL_NAVIGATOR_H */

