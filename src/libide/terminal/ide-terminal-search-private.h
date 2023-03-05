/* gb-terminal-view-private.h
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

#include "ide-terminal-search.h"

#include <libide-gtk.h>

G_BEGIN_DECLS

struct _IdeTerminalSearch
{
  GtkWidget            parent_instance;

  VteTerminal         *terminal;

  GtkRevealer         *search_revealer;

  IdeSearchEntry      *search_entry;

  GtkGrid             *grid;
  GtkButton           *search_prev_button;
  GtkButton           *search_next_button;
  GtkButton           *close_button;

  /* Cached regex */
  gboolean             regex_caseless;
  gchar               *regex_pattern;
  VteRegex            *regex;

  /* Search options */
  guint                use_regex : 1;
  guint                wrap_word : 1;
  guint                match_case : 1;
  guint                entire_word : 1;

  gchar               *selected_text;
  gchar               *selection_buffer;
};

G_END_DECLS
