/* ide-editor-frame-private.h
 *
 * Copyright (C) 2015 Christian Hergert <christian@hergert.me>
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

#ifndef IDE_EDITOR_FRAME_PRIVATE_H
#define IDE_EDITOR_FRAME_PRIVATE_H

#include <libgd/gd.h>
#include <gtk/gtk.h>
#include <nautilus-floating-bar.h>

#include "ide-types.h"
#include "editor/ide-editor-map-bin.h"
#include "ide-editor-spell-widget.h"
#include "sourceview/ide-source-map.h"
#include "sourceview/ide-source-view.h"

G_BEGIN_DECLS

struct _IdeEditorFrame
{
  GtkBin               parent_instance;

  gchar               *previous_search_string;

  NautilusFloatingBar *floating_bar;
  GtkRevealer         *map_revealer;
  GtkLabel            *mode_name_label;
  GtkLabel            *overwrite_label;
  GtkScrolledWindow   *scrolled_window;
  GtkRevealer         *search_revealer;
  GtkFrame            *search_frame;
  GdTaggedEntry       *search_entry;
  GtkSearchEntry      *replace_entry;
  GtkButton           *replace_button;
  GtkButton           *replace_all_button;
  GtkGrid             *search_options;
  GdTaggedEntryTag    *search_entry_tag;
  IdeSourceView       *source_view;
  IdeEditorMapBin      *source_map_container;
  IdeSourceMap        *source_map;
  GtkOverlay          *source_overlay;

  gulong               cursor_moved_handler;

  guint                pending_replace_confirm;
  guint                auto_hide_map : 1;
  guint                show_ruler : 1;
};

G_END_DECLS

#endif /* IDE_EDITOR_FRAME_PRIVATE_H */
