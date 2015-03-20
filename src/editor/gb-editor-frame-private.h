/* gb-editor-frame-private.h
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

#ifndef GB_EDITOR_FRAME_PRIVATE_H
#define GB_EDITOR_FRAME_PRIVATE_H

#include <gtk/gtk.h>
#include <ide.h>

#include "gd-tagged-entry.h"
#include "nautilus-floating-bar.h"

G_BEGIN_DECLS

struct _GbEditorFrame
{
  GtkBin               parent_instance;

  NautilusFloatingBar *floating_bar;
  GtkLabel            *overwrite_label;
  GtkScrolledWindow   *scrolled_window;
  GtkRevealer         *search_revealer;
  GdTaggedEntry       *search_entry;
  GdTaggedEntryTag    *search_entry_tag;
  IdeSourceView       *source_view;

  gulong               cursor_moved_handler;
};

G_END_DECLS

#endif /* GB_EDITOR_FRAME_PRIVATE_H */
