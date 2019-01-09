/* gbp-devhelp-view-private.h
 *
 * Copyright 2017 Lucie Charvat <luci.charvat@gmail.com>
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

#include <libide-editor.h>

G_BEGIN_DECLS

struct _GbpDevhelpSearch
{
  GtkBin                parent_instance;

  WebKitWebView        *web_view;
  WebKitFindController *web_controller;

  GtkRevealer          *search_revealer;

  IdeTaggedEntry       *search_entry;

  GtkButton            *search_prev_button;
  GtkButton            *search_next_button;
  GtkButton            *close_button;

  GtkClipboard         *clipboard;
  gchar                *selected_text;
};

G_END_DECLS
