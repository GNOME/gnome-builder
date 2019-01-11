/* ide-debug-manager.h
 *
 * Copyright 2017-2019 Christian Hergert <chergert@redhat.com>
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

#include <libide-core.h>
#include <libide-foundry.h>

#include "ide-debugger.h"
#include "ide-debugger-breakpoints.h"
#include "ide-debugger-types.h"

G_BEGIN_DECLS

#define IDE_TYPE_DEBUG_MANAGER (ide_debug_manager_get_type())

IDE_AVAILABLE_IN_3_32
G_DECLARE_FINAL_TYPE (IdeDebugManager, ide_debug_manager, IDE, DEBUG_MANAGER, IdeObject)

IDE_AVAILABLE_IN_3_32
IdeDebugManager        *ide_debug_manager_from_context             (IdeContext       *context);
IDE_AVAILABLE_IN_3_32
IdeDebugger            *ide_debug_manager_get_debugger             (IdeDebugManager  *self);
IDE_AVAILABLE_IN_3_32
gboolean                ide_debug_manager_get_active               (IdeDebugManager  *self);
IDE_AVAILABLE_IN_3_32
gboolean                ide_debug_manager_start                    (IdeDebugManager  *self,
                                                                    IdeRunner        *runner,
                                                                    GError          **error);
IDE_AVAILABLE_IN_3_32
void                    ide_debug_manager_stop                     (IdeDebugManager  *self);
IDE_AVAILABLE_IN_3_32
IdeDebuggerBreakpoints *ide_debug_manager_get_breakpoints_for_file (IdeDebugManager  *self,
                                                                    GFile            *file);
IDE_AVAILABLE_IN_3_32
gboolean                ide_debug_manager_supports_language        (IdeDebugManager  *self,
                                                                    const gchar      *language_id);
IDE_AVAILABLE_IN_3_32
IdeDebugger            *ide_debug_manager_find_debugger            (IdeDebugManager  *self,
                                                                    IdeRunner        *runner);

G_END_DECLS
