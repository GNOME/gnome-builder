/* gb-terminal-view.h
 *
 * Copyright © 2015 Christian Hergert <christian@hergert.me>
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

#include <ide.h>
#include <vte/vte.h>

G_BEGIN_DECLS

#define GB_TYPE_TERMINAL_VIEW (gb_terminal_view_get_type())

G_DECLARE_FINAL_TYPE (GbTerminalView, gb_terminal_view, GB, TERMINAL_VIEW, IdeLayoutView)

void      gb_terminal_view_set_pty        (GbTerminalView *self,
                                           VtePty         *pty);
void      gb_terminal_view_feed           (GbTerminalView *self,
                                           const gchar    *message);

G_END_DECLS
