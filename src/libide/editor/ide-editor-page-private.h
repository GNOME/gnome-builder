/* ide-editor-page-private.h
 *
 * Copyright 2017-2022 Christian Hergert <chergert@redhat.com>
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

#include <libide-gtk.h>
#include <libide-plugins.h>

#include "ide-editor-page.h"
#include "ide-editor-search-bar-private.h"
#include "ide-scrollbar.h"
#include "ide-source-map.h"

G_BEGIN_DECLS

struct _IdeEditorPage
{
  IdePage                  parent_instance;

  /* Owned references */
  IdeExtensionSetAdapter  *addins;
  IdeBuffer               *buffer;
  IdeGutter               *gutter;

  /* Settings Management */
  GBindingGroup           *buffer_file_settings;
  GBindingGroup           *view_file_settings;

  /* Template widgets */
  IdeSourceView           *view;
  GtkScrolledWindow       *scroller;
  IdeSourceMap            *map;
  IdeScrollbar            *scrollbar;
  IdeScrubberRevealer     *scrubber_revealer;
  IdeEditorSearchBar      *search_bar;
  GtkRevealer             *search_revealer;

  guint                    completion_blocked : 1;
};

void _ide_editor_page_settings_init              (IdeEditorPage      *self);
void _ide_editor_page_settings_reload            (IdeEditorPage      *self);
void _ide_editor_page_settings_connect_gutter    (IdeEditorPage      *self,
                                                  IdeGutter          *gutter);
void _ide_editor_page_settings_disconnect_gutter (IdeEditorPage      *self,
                                                  IdeGutter          *gutter);

G_END_DECLS
