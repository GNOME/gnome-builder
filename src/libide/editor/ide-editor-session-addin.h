/* ide-editor-session-addin.h
 *
 * Copyright 2018 Christian Hergert <chergert@redhat.com>
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

#pragma once

#include "session/ide-session-addin.h"

G_BEGIN_DECLS

#define IDE_TYPE_EDITOR_SESSION_ADDIN (ide_editor_session_addin_get_type())

G_DECLARE_FINAL_TYPE (IdeEditorSessionAddin, ide_editor_session_addin, IDE, EDITOR_SESSION_ADDIN, IdeObject)

G_END_DECLS
