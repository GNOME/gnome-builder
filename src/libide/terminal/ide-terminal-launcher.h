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

#include <libide-gui.h>
#include <libide-threading.h>

G_BEGIN_DECLS

#define IDE_TYPE_TERMINAL_LAUNCHER (ide_terminal_launcher_get_type())

IDE_AVAILABLE_IN_3_34
G_DECLARE_FINAL_TYPE (IdeTerminalLauncher, ide_terminal_launcher, IDE, TERMINAL_LAUNCHER, GObject)

IDE_AVAILABLE_IN_3_34
IdeTerminalLauncher *ide_terminal_launcher_new              (IdeContext             *context);
IDE_AVAILABLE_IN_3_34
IdeTerminalLauncher *ide_terminal_launcher_new_for_launcher (IdeSubprocessLauncher  *launcher);
IDE_AVAILABLE_IN_3_34
IdeTerminalLauncher *ide_terminal_launcher_new_for_debug    (void);
IDE_AVAILABLE_IN_3_34
IdeTerminalLauncher *ide_terminal_launcher_new_for_runtime  (IdeRuntime             *runtime);
IDE_AVAILABLE_IN_3_34
IdeTerminalLauncher *ide_terminal_launcher_new_for_runner   (IdeRuntime             *runtime);
IDE_AVAILABLE_IN_3_34
gboolean             ide_terminal_launcher_can_respawn      (IdeTerminalLauncher    *self);
IDE_AVAILABLE_IN_3_34
const gchar * const *ide_terminal_launcher_get_args         (IdeTerminalLauncher    *self);
IDE_AVAILABLE_IN_3_34
void                 ide_terminal_launcher_set_args         (IdeTerminalLauncher    *self,
                                                             const gchar * const    *args);
IDE_AVAILABLE_IN_3_34
const gchar         *ide_terminal_launcher_get_cwd          (IdeTerminalLauncher    *self);
IDE_AVAILABLE_IN_3_34
void                 ide_terminal_launcher_set_cwd          (IdeTerminalLauncher    *self,
                                                             const gchar            *cwd);
IDE_AVAILABLE_IN_3_34
const gchar         *ide_terminal_launcher_get_shell        (IdeTerminalLauncher    *self);
IDE_AVAILABLE_IN_3_34
void                 ide_terminal_launcher_set_shell        (IdeTerminalLauncher    *self,
                                                             const gchar            *shell);
IDE_AVAILABLE_IN_3_34
const gchar         *ide_terminal_launcher_get_title        (IdeTerminalLauncher    *self);
IDE_AVAILABLE_IN_3_34
void                 ide_terminal_launcher_set_title        (IdeTerminalLauncher    *self,
                                                             const gchar            *title);
IDE_AVAILABLE_IN_3_34
void                 ide_terminal_launcher_spawn_async      (IdeTerminalLauncher    *self,
                                                             VtePty                 *pty,
                                                             GCancellable           *cancellable,
                                                             GAsyncReadyCallback     callback,
                                                             gpointer                user_data);
IDE_AVAILABLE_IN_3_34
gboolean             ide_terminal_launcher_spawn_finish     (IdeTerminalLauncher    *self,
                                                             GAsyncResult           *result,
                                                             GError                **error);

G_END_DECLS
