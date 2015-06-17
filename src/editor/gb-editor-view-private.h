/* gb-editor-view-private.h
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

#ifndef GB_EDITOR_VIEW_PRIVATE_H
#define GB_EDITOR_VIEW_PRIVATE_H

#include <ide.h>
#include <libpeas/peas.h>

#include "gb-editor-document.h"
#include "gb-editor-frame.h"
#include "gb-editor-tweak-widget.h"
#include "gb-view.h"

G_BEGIN_DECLS

struct _GbEditorView
{
  GbView               parent_instance;

  GbEditorDocument    *document;
  PeasExtensionSet    *extensions;
  GSettings           *settings;
  IdePatternSpec      *symbol_spec;

  GbEditorFrame       *frame1;
  GbEditorFrame       *frame2;
  GtkButton           *modified_cancel_button;
  GtkRevealer         *modified_revealer;
  GtkPaned            *paned;
  GtkProgressBar      *progress_bar;
  GtkMenuButton       *symbols_button;
  GtkListBox          *symbols_listbox;
  GtkPopover          *symbols_popover;
  GtkSearchEntry      *symbols_search_entry;
  GtkMenuButton       *tweak_button;
  GbEditorTweakWidget *tweak_widget;

  guint                symbol_timeout;
};

G_END_DECLS

#endif /* GB_EDITOR_VIEW_PRIVATE_H */
