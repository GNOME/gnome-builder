/* gb-editor-tab-private.h
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

#ifndef GB_EDITOR_TAB_PRIVATE_H
#define GB_EDITOR_TAB_PRIVATE_H

#include <gtk/gtk.h>
#include <gtksourceview/gtksource.h>

#include "gb-animation.h"
#include "gb-box-theatric.h"
#include "gb-editor-document.h"
#include "gb-editor-settings.h"
#include "gb-editor-vim.h"
#include "gb-markdown-preview.h"
#include "gb-notebook.h"
#include "gb-source-auto-indenter.h"
#include "gb-source-auto-indenter-c.h"
#include "gb-source-change-monitor.h"
#include "gb-source-search-highlighter.h"
#include "gb-source-snippet-completion-provider.h"
#include "gb-source-view.h"
#include "gd-tagged-entry.h"
#include "nautilus-floating-bar.h"

G_BEGIN_DECLS

struct _GbEditorTabPrivate
{
  /*
   * Our underlying document, the GtkTextBuffer.
   */
  GbEditorDocument *document;

  /*
   * Snippet related components.
   */
  GtkSourceCompletionProvider *snippets_provider;

  /*
   * Search releated components.
   */
  GbSourceSearchHighlighter *search_highlighter;
  GtkSourceSearchSettings   *search_settings;
  GtkSourceSearchContext    *search_context;

  /*
   * Change (add, change, etc) tracking of the editor.
   */
  GbSourceChangeMonitor *change_monitor;
  GtkSourceGutterRenderer *change_renderer;

  /*
   * Tab related settings.
   */
  GbEditorSettings *settings;

  /*
   * VIM mode helper.
   */
  GbEditorVim *vim;

  /*
   * Weak reference bindings for tracking settings.
   */
  GBinding *auto_indent_binding;
  GBinding *font_desc_binding;
  GBinding *highlight_current_line_binding;
  GBinding *indent_on_tab_binding;
  GBinding *indent_width_binding;
  GBinding *insert_spaces_instead_of_tabs_binding;
  GBinding *right_margin_position_binding;
  GBinding *show_line_marks_binding;
  GBinding *show_line_numbers_binding;
  GBinding *show_right_margin_binding;
  GBinding *smart_home_end_binding;
  GBinding *style_scheme_binding;
  GBinding *tab_width_binding;

  /*
   * Tab related widgets, filled in with GtkBuilder templates.
   */
  NautilusFloatingBar *floating_bar;
  GtkButton           *go_down_button;
  GtkButton           *go_up_button;
  GtkOverlay          *overlay;
  GtkBox              *preview_container;
  GtkProgressBar      *progress_bar;
  GtkRevealer         *revealer;
  GtkScrolledWindow   *scroller;
  GbSourceView        *source_view;
  GdTaggedEntry       *search_entry;
  GdTaggedEntryTag    *search_entry_tag;
  GtkEntry            *vim_command_entry;

  /*
   * Information about our target file and encoding.
   */
  GtkSourceFile *file;

  /*
   * Animation for save progress.
   */
  GbAnimation *save_animation;
};

G_END_DECLS

#endif /* GB_EDITOR_TAB_PRIVATE_H */
