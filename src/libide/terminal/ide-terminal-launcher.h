/* ide-terminal-launcher.h
 *
 * Copyright 2019 Christian Hergert <unknown@domain.org>
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

#if !defined (IDE_TERMINAL_INSIDE) && !defined (IDE_TERMINAL_COMPILATION)
# error "Only <libide-terminal.h> can be included directly."
#endif

#include <libide-foundry.h>
#include <libide-threading.h>

G_BEGIN_DECLS

#define IDE_TYPE_TERMINAL_LAUNCHER (ide_terminal_launcher_get_type())

IDE_AVAILABLE_IN_ALL
G_DECLARE_FINAL_TYPE (IdeTerminalLauncher, ide_terminal_launcher, IDE, TERMINAL_LAUNCHER, GObject)

IDE_AVAILABLE_IN_ALL
IdeTerminalLauncher *ide_terminal_launcher_new                   (IdeContext             *context,
                                                                  IdeRunCommand          *run_command);
IDE_AVAILABLE_IN_ALL
IdeTerminalLauncher *ide_terminal_launcher_copy                  (IdeTerminalLauncher    *self);
IDE_AVAILABLE_IN_ALL
void                 ide_terminal_launcher_spawn_async           (IdeTerminalLauncher    *self,
                                                                  VtePty                 *pty,
                                                                  GCancellable           *cancellable,
                                                                  GAsyncReadyCallback     callback,
                                                                  gpointer                user_data);
IDE_AVAILABLE_IN_ALL
gboolean             ide_terminal_launcher_spawn_finish          (IdeTerminalLauncher    *self,
                                                                  GAsyncResult           *result,
                                                                  GError                **error);
IDE_AVAILABLE_IN_47
void                 ide_terminal_launcher_set_override_environ  (IdeTerminalLauncher    *self,
                                                                  const char * const     *override_environ);

G_END_DECLS
