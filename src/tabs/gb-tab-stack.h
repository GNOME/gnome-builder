/* gb-tab-stack.h
 *
 * Copyright (C) 2011 Christian Hergert <chris@dronelabs.com>
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

#ifndef GB_TAB_STACK_H
#define GB_TAB_STACK_H

#include <gtk/gtk.h>

#include "gb-tab.h"

G_BEGIN_DECLS

#define GB_TYPE_TAB_STACK            (gb_tab_stack_get_type())
#define GB_TAB_STACK(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), GB_TYPE_TAB_STACK, GbTabStack))
#define GB_TAB_STACK_CONST(obj)      (G_TYPE_CHECK_INSTANCE_CAST ((obj), GB_TYPE_TAB_STACK, GbTabStack const))
#define GB_TAB_STACK_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass),  GB_TYPE_TAB_STACK, GbTabStackClass))
#define GB_IS_TAB_STACK(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GB_TYPE_TAB_STACK))
#define GB_IS_TAB_STACK_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass),  GB_TYPE_TAB_STACK))
#define GB_TAB_STACK_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj),  GB_TYPE_TAB_STACK, GbTabStackClass))

typedef struct _GbTabStack        GbTabStack;
typedef struct _GbTabStackClass   GbTabStackClass;
typedef struct _GbTabStackPrivate GbTabStackPrivate;

struct _GbTabStack
{
   GtkBox parent;

   /*< private >*/
   GbTabStackPrivate *priv;
};

struct _GbTabStackClass
{
   GtkBoxClass parent_class;

   void (*changed) (GbTabStack *stack);
};

GType      gb_tab_stack_get_type       (void) G_GNUC_CONST;
GbTab     *gb_tab_stack_get_active     (GbTabStack *stack);
gboolean   gb_tab_stack_contains_tab   (GbTabStack *stack,
                                        GbTab      *tab);
void       gb_tab_stack_remove_tab     (GbTabStack *stack,
                                        GbTab      *tab);
guint      gb_tab_stack_get_n_tabs     (GbTabStack *stack);
gboolean   gb_tab_stack_focus_first    (GbTabStack *stack);
gboolean   gb_tab_stack_focus_last     (GbTabStack *stack);
gboolean   gb_tab_stack_focus_next     (GbTabStack *stack);
gboolean   gb_tab_stack_focus_previous (GbTabStack *stack);
gboolean   gb_tab_stack_focus_tab      (GbTabStack *stack,
                                        GbTab      *tab);
GList     *gb_tab_stack_get_tabs       (GbTabStack *stack);

G_END_DECLS

#endif /* GB_TAB_STACK_H */
