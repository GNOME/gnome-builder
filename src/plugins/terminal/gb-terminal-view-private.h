/* gb-terminal-view-private.h
 *
 * Copyright © 2015 Sebastien Lafargue <slafargue@gnome.org>
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

#include <libgd/gd.h>
#include <ide.h>
#include <vte/vte.h>

#include "gb-terminal-search.h"
#include "gb-terminal-search-private.h"

G_BEGIN_DECLS

struct _GbTerminalView
{
  IdeLayoutView        parent_instance;

  /*
   * If we are spawning a process in a runtime instead of the
   * host, then we will have a runtime pointer here.
   */
  IdeRuntime          *runtime;

  GtkOverlay          *terminal_overlay_top;

  GtkRevealer         *search_revealer_top;

  VteTerminal         *terminal_top;

  GtkScrollbar        *top_scrollbar;

  GbTerminalSearch    *tsearch;
  GbTerminalSearch    *bsearch;

  GFile               *save_as_file_top;

  gchar               *selection_buffer;

  VtePty              *pty;

  gint64               last_respawn;

  guint                manage_spawn : 1;
  guint                top_has_spawned : 1;
  guint                top_has_needs_attention : 1;
  guint                run_on_host : 1;
};

G_END_DECLS
