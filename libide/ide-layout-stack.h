/* ide-layout-stack.h
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

#ifndef IDE_LAYOUT_STACK_H
#define IDE_LAYOUT_STACK_H

#include <gtk/gtk.h>

G_BEGIN_DECLS

#define IDE_TYPE_LAYOUT_STACK (ide_layout_stack_get_type())

G_DECLARE_FINAL_TYPE (IdeLayoutStack, ide_layout_stack, IDE, LAYOUT_STACK, GtkBin)

GtkWidget  *ide_layout_stack_new             (void);
void        ide_layout_stack_remove          (IdeLayoutStack    *self,
                                              GtkWidget         *view);
GtkWidget  *ide_layout_stack_get_active_view (IdeLayoutStack    *self);
void        ide_layout_stack_set_active_view (IdeLayoutStack    *self,
                                              GtkWidget         *active_view);
void        ide_layout_stack_foreach_view    (IdeLayoutStack    *self,
                                              GtkCallback        callback,
                                              gpointer           user_data);

G_END_DECLS

#endif /* IDE_LAYOUT_STACK_H */
