/* ide-editor-private.h
 *
 * Copyright 2018-2019 Christian Hergert <chergert@redhat.com>
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
#include <libide-plugins.h>
#include <libide-sourceview.h>
#include <libpeas/peas.h>

#include "ide-editor-addin.h"
#include "ide-editor-page.h"
#include "ide-editor-search-bar.h"
#include "ide-editor-search.h"
#include "ide-editor-sidebar.h"
#include "ide-editor-surface.h"

G_BEGIN_DECLS

struct _IdeEditorSurface
{
  IdeSurface           parent_instance;

  PeasExtensionSet    *addins;

  /* Template widgets */
  IdeGrid             *grid;
  GtkOverlay          *overlay;
  GtkStack            *loading_stack;

  /* State before entering focus mode */
  guint                prefocus_had_left : 1;
  guint                prefocus_had_bottom : 1;

  guint                restore_panel : 1;
};

struct _IdeEditorPage
{
  IdePage                  parent_instance;

  IdeExtensionSetAdapter  *addins;

  GSettings               *editor_settings;
  GSettings               *insight_settings;

  IdeBuffer               *buffer;
  DzlBindingGroup         *buffer_bindings;
  DzlSignalGroup          *buffer_signals;

  IdeEditorSearch         *search;

  GCancellable            *destroy_cancellable;

  GtkSourceMap            *map;
  GtkRevealer             *map_revealer;
  GtkOverlay              *overlay;
  GtkProgressBar          *progress_bar;
  IdeSourceView           *source_view;
  GtkScrolledWindow       *scroller;
  GtkBox                  *scroller_box;
  IdeEditorSearchBar      *search_bar;
  GtkRevealer             *search_revealer;
  GtkRevealer             *modified_revealer;
  GtkButton               *modified_cancel_button;

  /* Raw pointer used to determine when frame changes */
  IdeFrame                *last_frame_ptr;

  guint                    toggle_map_source;

  guint                    auto_hide_map : 1;
  guint                    show_map : 1;
};

void _ide_editor_page_init_actions         (IdeEditorPage      *self);
void _ide_editor_page_init_settings        (IdeEditorPage      *self);
void _ide_editor_page_init_shortcuts       (IdeEditorPage      *self);
void _ide_editor_page_update_actions       (IdeEditorPage      *self);
void _ide_editor_search_bar_init_shortcuts (IdeEditorSearchBar *self);
void _ide_editor_sidebar_set_open_pages    (IdeEditorSidebar   *self,
                                            GListModel         *open_pages);
void _ide_editor_surface_set_loading       (IdeEditorSurface   *self,
                                            gboolean            loading);
void _ide_editor_surface_init_actions      (IdeEditorSurface   *self);
void _ide_editor_surface_init_shortcuts    (IdeEditorSurface   *self);

G_END_DECLS
