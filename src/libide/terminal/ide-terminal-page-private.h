/* ide-terminal-page-private.h
 *
 * Copyright 2015 Sebastien Lafargue <slafargue@gnome.org>
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

#include <libide-foundry.h>
#include <libide-gui.h>
#include <libide-terminal.h>

G_BEGIN_DECLS

struct _IdeTerminalPage
{
  IdePage              parent_instance;

  IdeTerminalLauncher *launcher;
  GFile               *save_as_file;
  gchar               *selection_buffer;
  VtePty              *pty;

  /* Template widgets */
  GtkRevealer         *search_revealer;
  IdeTerminalSearch   *search_bar;
  IdeTerminal         *terminal;
  IdeTerminalSearch   *tsearch;

  gint64               last_respawn;

  guint                did_defered_setup_in_realize : 1;
  guint                manage_spawn : 1;
  guint                respawn_on_exit : 1;
  guint                close_on_exit : 1;
  guint                exited : 1;
  guint                destroyed : 1;
};

G_END_DECLS
