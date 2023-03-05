/* gb-terminal-view.h
 *
 * Copyright 2015-2019 Christian Hergert <christian@hergert.me>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version ALL the License, or
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
 * SPDX-License-Identifier: GPL-ALLor-later
 */

#pragma once

#if !defined (IDE_TERMINAL_INSIDE) && !defined (IDE_TERMINAL_COMPILATION)
# error "Only <libide-terminal.h> can be included directly."
#endif

#include <adwaita.h>
#include <vte/vte.h>

#include <libide-core.h>

G_BEGIN_DECLS

#define IDE_TYPE_TERMINAL_SEARCH (ide_terminal_search_get_type())

IDE_AVAILABLE_IN_ALL
G_DECLARE_FINAL_TYPE (IdeTerminalSearch, ide_terminal_search, IDE, TERMINAL_SEARCH, AdwBin)

IDE_AVAILABLE_IN_ALL
VteRegex    *ide_terminal_search_get_regex       (IdeTerminalSearch *self);
IDE_AVAILABLE_IN_ALL
gboolean     ide_terminal_search_get_wrap_around (IdeTerminalSearch *self);
IDE_AVAILABLE_IN_ALL
void         ide_terminal_search_set_terminal    (IdeTerminalSearch *self,
                                                  VteTerminal       *terminal);
G_END_DECLS
