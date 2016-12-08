/* ide-editor-spell-widget.h
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

#ifndef IDE_EDITOR_SPELL_WIDGET_H
#define IDE_EDITOR_SPELL_WIDGET_H

#include <glib-object.h>
#include <gtk/gtk.h>

#include "sourceview/ide-source-view.h"

G_BEGIN_DECLS

#define IDE_TYPE_EDITOR_SPELL_WIDGET (ide_editor_spell_widget_get_type())

G_DECLARE_FINAL_TYPE (IdeEditorSpellWidget, ide_editor_spell_widget, IDE, EDITOR_SPELL_WIDGET, GtkBin)

GtkWidget       *ide_editor_spell_widget_new          (IdeSourceView           *source_view);
GtkWidget       *ide_editor_spell_widget_get_entry    (IdeEditorSpellWidget    *self);

G_END_DECLS

#endif /* IDE_EDITOR_SPELL_WIDGET_H */
