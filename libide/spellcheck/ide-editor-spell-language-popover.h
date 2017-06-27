/* ide-editor-spell-language-popover.h
 *
 * Copyright (C) 2017 Sébastien Lafargue <slafargue@gnome.org>
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

#ifndef IDE_EDITOR_SPELL_LANGUAGE_POPOVER_H
#define IDE_EDITOR_SPELL_LANGUAGE_POPOVER_H

#include <glib-object.h>
#include <gtk/gtk.h>
#include <gspell/gspell.h>

G_BEGIN_DECLS

#define IDE_TYPE_EDITOR_SPELL_LANGUAGE_POPOVER (ide_editor_spell_language_popover_get_type())

G_DECLARE_FINAL_TYPE (IdeEditorSpellLanguagePopover, ide_editor_spell_language_popover, IDE, EDITOR_SPELL_LANGUAGE_POPOVER, GtkButton)

IdeEditorSpellLanguagePopover *ide_editor_spell_language_popover_new (const GspellLanguage *current_language);

G_END_DECLS

#endif /* IDE_EDITOR_SPELL_LANGUAGE_POPOVER_H */
