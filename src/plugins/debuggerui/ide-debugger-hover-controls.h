/* ide-debugger-hover-controls.h
 *
 * Copyright 2018-2019 Christian Hergert <chergert@redhat.com>
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

#include <adwaita.h>

#include "ide-debug-manager.h"

G_BEGIN_DECLS

#define IDE_TYPE_DEBUGGER_HOVER_CONTROLS (ide_debugger_hover_controls_get_type())

G_DECLARE_FINAL_TYPE (IdeDebuggerHoverControls, ide_debugger_hover_controls, IDE, DEBUGGER_HOVER_CONTROLS, AdwBin)

GtkWidget *ide_debugger_hover_controls_new (IdeDebugManager *debug_manager,
                                            GFile           *file,
                                            guint            line);

G_END_DECLS
