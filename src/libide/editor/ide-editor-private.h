/* ide-editor-private.h
 *
 * Copyright (C) 2017 Christian Hergert <chergert@redhat.com>
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

#include <dazzle.h>
#include <libpeas/peas.h>
#include <libgd/gd-tagged-entry.h>

#include "editor/ide-editor-perspective.h"
#include "editor/ide-editor-properties.h"
#include "editor/ide-editor-search-bar.h"
#include "editor/ide-editor-sidebar.h"
#include "editor/ide-editor-view-addin.h"
#include "editor/ide-editor-view.h"
#include "layout/ide-layout-grid.h"
#include "layout/ide-layout-view.h"
#include "plugins/ide-extension-set-adapter.h"

G_BEGIN_DECLS

struct _IdeEditorPerspective
{
  IdeLayout            parent_instance;

  PeasExtensionSet    *addins;

  /* Template widgets */
  IdeLayoutGrid       *grid;
  GtkOverlay          *overlay;
  IdeEditorProperties *properties;

  /* State before entering focus mode */
  guint                prefocus_had_left : 1;
  guint                prefocus_had_bottom : 1;
};

struct _IdeEditorView
{
  IdeLayoutView            parent_instance;

  IdeExtensionSetAdapter  *addins;

  GSettings               *editor_settings;
  GSettings               *insight_settings;

  IdeBuffer               *buffer;
  DzlBindingGroup         *buffer_bindings;
  DzlSignalGroup          *buffer_signals;

  GtkSourceSearchSettings *search_settings;
  GtkSourceSearchContext  *search_context;

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

  /* Raw pointer used to determine when stack changes */
  IdeLayoutStack          *last_stack_ptr;

  guint                    toggle_map_source;

  guint                    auto_hide_map : 1;
  guint                    show_map : 1;
};

struct _IdeEditorSearchBar
{
  DzlBin                   parent_instance;

  /* Owned references */
  DzlSignalGroup          *buffer_signals;
  GtkSourceSearchContext  *context;
  DzlSignalGroup          *context_signals;
  GtkSourceSearchSettings *settings;
  DzlSignalGroup          *settings_signals;
  GdTaggedEntryTag        *search_entry_tag;

  /* Template widgets */
  GtkCheckButton          *case_sensitive;
  GtkButton               *replace_all_button;
  GtkButton               *replace_button;
  GtkSearchEntry          *replace_entry;
  GdTaggedEntry           *search_entry;
  GtkGrid                 *search_options;
  GtkCheckButton          *use_regex;
  GtkCheckButton          *whole_word;

  GSettings               *quick_highlight_settings;

  guint                    quick_highlight_enabled : 1;
};

void _ide_editor_view_init_actions           (IdeEditorView        *self);
void _ide_editor_view_init_settings          (IdeEditorView        *self);
void _ide_editor_view_init_shortcuts         (IdeEditorView        *self);
void _ide_editor_view_update_actions         (IdeEditorView        *self);
void _ide_editor_search_bar_init_actions     (IdeEditorSearchBar   *self);
void _ide_editor_search_bar_init_shortcuts   (IdeEditorSearchBar   *self);
void _ide_editor_sidebar_set_open_pages      (IdeEditorSidebar     *self,
                                              GListModel           *open_pages);
void _ide_editor_perspective_show_properties (IdeEditorPerspective *self,
                                              IdeEditorView        *view);
void _ide_editor_perspective_init_actions    (IdeEditorPerspective *self);
void _ide_editor_perspective_init_shortcuts  (IdeEditorPerspective *self);

G_END_DECLS
