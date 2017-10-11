/* ide-debugger-gutter-renderer.h
 *
 * Copyright © 2017 Christian Hergert <chergert@redhat.com>
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

#include <gtksourceview/gtksource.h>

#include "ide-types.h"

#include "debugger/ide-debug-manager.h"
#include "debugger/ide-debugger-breakpoints.h"

G_BEGIN_DECLS

#define IDE_TYPE_DEBUGGER_GUTTER_RENDERER (ide_debugger_gutter_renderer_get_type())

G_DECLARE_FINAL_TYPE (IdeDebuggerGutterRenderer, ide_debugger_gutter_renderer, IDE, DEBUGGER_GUTTER_RENDERER, GtkSourceGutterRendererPixbuf)

GtkSourceGutterRenderer *ide_debugger_gutter_renderer_new             (IdeDebugManager           *debug_manager);
void                     ide_debugger_gutter_renderer_set_breakpoints (IdeDebuggerGutterRenderer *self,
                                                                       IdeDebuggerBreakpoints    *breakpoints);

G_END_DECLS
