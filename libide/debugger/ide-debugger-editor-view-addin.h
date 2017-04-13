/* ide-debugger-editor-view-addin.h
 *
 * Copyright (C) 2017 Christian Hergert <chergert@redhat.com>
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

#ifndef IDE_DEBUGGER_EDITOR_VIEW_ADDIN_H
#define IDE_DEBUGGER_EDITOR_VIEW_ADDIN_H

#include <ide.h>

G_BEGIN_DECLS

#define IDE_TYPE_DEBUGGER_EDITOR_VIEW_ADDIN (ide_debugger_editor_view_addin_get_type())

G_DECLARE_FINAL_TYPE (IdeDebuggerEditorViewAddin, ide_debugger_editor_view_addin, IDE, DEBUGGER_EDITOR_VIEW_ADDIN, GObject)

G_END_DECLS

#endif /* IDE_DEBUGGER_EDITOR_VIEW_ADDIN_H */
