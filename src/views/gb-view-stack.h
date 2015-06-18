/* gb-view-stack.h
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

#ifndef GB_VIEW_STACK_H
#define GB_VIEW_STACK_H

#include <gtk/gtk.h>
#include <ide.h>

#include "gb-document.h"
#include "gb-view.h"

G_BEGIN_DECLS

#define GB_TYPE_VIEW_STACK (gb_view_stack_get_type())

G_DECLARE_FINAL_TYPE (GbViewStack, gb_view_stack, GB, VIEW_STACK, GtkBin)

GtkWidget  *gb_view_stack_new                 (void);
void        gb_view_stack_remove              (GbViewStack       *self,
                                               GbView            *view);
GtkWidget  *gb_view_stack_get_active_view     (GbViewStack       *self);
void        gb_view_stack_set_active_view     (GbViewStack       *self,
                                               GtkWidget         *active_view);
GtkWidget  *gb_view_stack_find_with_document  (GbViewStack       *self,
                                               GbDocument        *document);
GbDocument *gb_view_stack_find_document_typed (GbViewStack       *self,
                                               GType              document_type);
void        gb_view_stack_focus_document      (GbViewStack       *self,
                                               GbDocument        *document);
void        gb_view_stack_focus_location      (GbViewStack       *self,
                                               IdeSourceLocation *location);
GList      *gb_view_stack_get_views           (GbViewStack       *self);
void        gb_view_stack_raise_document      (GbViewStack       *self,
                                               GbDocument        *document,
                                               gboolean           focus);

G_END_DECLS

#endif /* GB_VIEW_STACK_H */
