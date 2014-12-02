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

#include <gtksourceview/gtksource.h>

#include "gb-animation.h"
#include "gb-editor-document.h"
#include "gb-editor-frame.h"
#include "gb-editor-tab.h"

G_BEGIN_DECLS

struct _GbEditorTabPrivate
{
  /* Widgets owned by GtkBuilder */
  GbEditorFrame    *frame;
  GtkPaned         *paned;
  GtkProgressBar   *progress_bar;
  GtkToggleButton  *split_button;

  /* Weak references */
  GbEditorFrame    *last_frame;
  GbAnimation      *progress_animation;

  /* Objects owned by GbEditorTab */
  GbEditorDocument *document;

  guint             unsaved_id;
};

GbEditorFrame *gb_editor_tab_get_last_frame (GbEditorTab *tab);

G_END_DECLS

#endif /* GB_EDITOR_TAB_PRIVATE_H */
