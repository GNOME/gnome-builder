/* ide-editor-dict-widget.h
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

#ifndef IDE_EDITOR_DICT_WIDGET_H
#define IDE_EDITOR_DICT_WIDGET_H

#include <glib-object.h>
#include <gspell/gspell.h>
#include <gtk/gtk.h>

G_BEGIN_DECLS

#define IDE_TYPE_EDITOR_DICT_WIDGET (ide_editor_dict_widget_get_type())

G_DECLARE_FINAL_TYPE (IdeEditorDictWidget, ide_editor_dict_widget, IDE, EDITOR_DICT_WIDGET, GtkBin)

IdeEditorDictWidget        *ide_editor_dict_widget_new               (GspellChecker         *checker);

GspellChecker              *ide_editor_dict_widget_get_checker       (IdeEditorDictWidget   *self);
void                        ide_editor_dict_widget_set_checker       (IdeEditorDictWidget   *self,
                                                                      GspellChecker         *checker);
void                        ide_editor_dict_widget_add_word          (IdeEditorDictWidget   *self,
                                                                      const gchar           *word);

G_END_DECLS

#endif /* IDE_EDITOR_DICT_WIDGET_H */

