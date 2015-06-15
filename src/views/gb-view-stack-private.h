/* gb-view-stack-private.h
 *
 * Copyright (C) 2015 Christian Hergert <christian@hergert.me>
 *
 * This file is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as
 * published by the Free Software Foundation; either version 3 of the
 * License, or (at your option) any later version.
 *
 * This file is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef GB_VIEW_STACK_PRIVATE_H
#define GB_VIEW_STACK_PRIVATE_H

#include <gtk/gtk.h>
#include <ide.h>

G_BEGIN_DECLS

struct _GbViewStack
{
  GtkBin              parent_instance;

  GList              *focus_history;
  IdeBackForwardList *back_forward_list;

  /* Weak references */
  GtkWidget          *active_view;
  IdeContext         *context;
  GBinding           *modified_binding;
  GBinding           *title_binding;

  /* Template references */
  GtkStack           *controls_stack;
  GtkButton          *close_button;
  GtkMenuButton      *document_button;
  GtkButton          *go_backward;
  GtkButton          *go_forward;
  GtkLabel           *modified_label;
  GtkPopover         *popover;
  GtkStack           *stack;
  GtkLabel           *title_label;
  GtkListBox         *views_button;
  GtkListBox         *views_listbox;
  GtkPopover         *views_popover;

  guint               destroyed : 1;
  guint               focused : 1;
};

G_END_DECLS

#endif /* GB_VIEW_STACK_PRIVATE_H */
