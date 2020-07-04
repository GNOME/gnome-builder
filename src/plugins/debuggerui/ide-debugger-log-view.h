/* ide-debugger-log-view.h
 *
 * Copyright 2020 Günther Wagner <info@gunibert.de>
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

#include <gtk/gtk.h>
#include <libide-debugger.h>

G_BEGIN_DECLS

#define IDE_TYPE_DEBUGGER_LOG_VIEW (ide_debugger_log_view_get_type())

G_DECLARE_FINAL_TYPE (IdeDebuggerLogView, ide_debugger_log_view, IDE, DEBUGGER_LOG_VIEW, GtkBox)

IdeDebuggerLogView *ide_debugger_log_view_new          (void);
void                ide_debugger_log_view_debugger_log (IdeDebuggerLogView *self,
                                                        IdeDebuggerStream   stream,
                                                        GBytes             *content,
                                                        IdeDebugger        *debugger);
void                ide_debugger_log_view_set_debugger (IdeDebuggerLogView *self,
                                                        IdeDebugger        *debugger);
IdeDebugger        *ide_debugger_log_view_get_debugger (IdeDebuggerLogView *self);
G_END_DECLS
