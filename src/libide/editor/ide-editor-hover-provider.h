/* ide-editor-hover-provider.h
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

#include "hover/ide-hover-provider.h"

G_BEGIN_DECLS

#define IDE_TYPE_EDITOR_HOVER_PROVIDER (ide_editor_hover_provider_get_type())

G_DECLARE_FINAL_TYPE (IdeEditorHoverProvider, ide_editor_hover_provider, IDE, EDITOR_HOVER_PROVIDER, GObject)

G_END_DECLS
