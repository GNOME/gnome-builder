/* ide-terminal-page.h
 *
 * Copyright 2015-2019 Christian Hergert <christian@hergert.me>
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

#include <vte/vte.h>

#include <libide-gui.h>

#include "ide-terminal.h"
#include "ide-terminal-launcher.h"

G_BEGIN_DECLS

#define IDE_TYPE_TERMINAL_PAGE (ide_terminal_page_get_type())

IDE_AVAILABLE_IN_ALL
G_DECLARE_FINAL_TYPE (IdeTerminalPage, ide_terminal_page, IDE, TERMINAL_PAGE, IdePage)

IDE_AVAILABLE_IN_ALL
IdeTerminalPage     *ide_terminal_page_new_completed             (const char          *title,
                                                                  const char          *text,
                                                                  int                  columns,
                                                                  int                  rows);
IDE_AVAILABLE_IN_ALL
void                 ide_terminal_page_set_launcher              (IdeTerminalPage     *self,
                                                                  IdeTerminalLauncher *launcher);
IDE_AVAILABLE_IN_ALL
IdeTerminalLauncher *ide_terminal_page_get_launcher              (IdeTerminalPage     *self);
IDE_AVAILABLE_IN_ALL
VtePty              *ide_terminal_page_get_pty                   (IdeTerminalPage     *self);
IDE_AVAILABLE_IN_ALL
void                 ide_terminal_page_set_pty                   (IdeTerminalPage     *self,
                                                                  VtePty              *pty);
IDE_AVAILABLE_IN_ALL
void                 ide_terminal_page_feed                      (IdeTerminalPage     *self,
                                                                  const gchar         *message);
IDE_AVAILABLE_IN_ALL
const gchar         *ide_terminal_page_get_current_directory_uri (IdeTerminalPage     *self);
IDE_AVAILABLE_IN_ALL
gboolean             ide_terminal_page_has_exited                (IdeTerminalPage     *self);
IDE_AVAILABLE_IN_ALL
IdeTerminal         *ide_terminal_page_get_terminal              (IdeTerminalPage     *self);

G_END_DECLS

