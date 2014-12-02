/* gb-editor-frame-private.h
 *
 * Copyright (C) 2014 Christian Hergert <christian@hergert.me>
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

#include "gb-editor-frame.h"
#include "gb-source-change-gutter-renderer.h"
#include "gb-source-code-assistant-renderer.h"
#include "gb-source-search-highlighter.h"
#include "gb-source-view.h"
#include "gd-tagged-entry.h"
#include "gca-structs.h"
#include "nautilus-floating-bar.h"

G_BEGIN_DECLS

struct _GbEditorFramePrivate
{
  /* Widgets owned by GtkBuilder */
  GtkSpinner                    *busy_spinner;
  GbSourceChangeGutterRenderer  *diff_renderer;
  GbSourceCodeAssistantRenderer *code_assistant_renderer;
  NautilusFloatingBar           *floating_bar;
  GtkButton                     *forward_search;
  GtkButton                     *backward_search;
  GtkScrolledWindow             *scrolled_window;
  GtkRevealer                   *search_revealer;
  GdTaggedEntry                 *search_entry;
  GdTaggedEntryTag              *search_entry_tag;
  GbSourceView                  *source_view;

  /* Objects owned by GbEditorFrame */
  GbEditorDocument              *document;
  GtkSourceSearchContext        *search_context;
  GtkSourceSearchSettings       *search_settings;
  GbSourceSearchHighlighter     *search_highlighter;

  /* Signal handler identifiers */
  gulong                         cursor_moved_handler;
};

G_END_DECLS

#endif /* GB_EDITOR_FRAME_PRIVATE_H */
