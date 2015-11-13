/* ide-layout-stack-private.h
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

#ifndef IDE_LAYOUT_STACK_PRIVATE_H
#define IDE_LAYOUT_STACK_PRIVATE_H

#include <gtk/gtk.h>

#include "ide-context.h"
#include "ide-back-forward-list.h"

G_BEGIN_DECLS

struct _IdeLayoutStack
{
  GtkBin              parent_instance;

  GList              *focus_history;
  IdeBackForwardList *back_forward_list;
  GtkGesture         *swipe_gesture;

  /* Weak references */
  GtkWidget          *active_view;
  IdeContext         *context;
  GBinding           *modified_binding;
  GBinding           *title_binding;

  /* Template references */
  GtkBox             *controls;
  GtkButton          *close_button;
  GtkMenuButton      *document_button;
  GtkButton          *go_backward;
  GtkButton          *go_forward;
  GtkEventBox        *header_event_box;
  GtkLabel           *modified_label;
  GtkStack           *stack;
  GtkLabel           *title_label;
  GtkListBox         *views_button;
  GtkListBox         *views_listbox;
  GtkPopover         *views_popover;

  guint               destroyed : 1;
  guint               focused : 1;
};

void ide_layout_stack_add (GtkContainer *container,
                           GtkWidget    *child);

G_END_DECLS

#endif /* IDE_LAYOUT_STACK_PRIVATE_H */
