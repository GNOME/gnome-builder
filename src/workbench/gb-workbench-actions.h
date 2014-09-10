/* gb-workbench-actions.h
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

#ifndef GB_WORKBENCH_ACTIONS_H
#define GB_WORKBENCH_ACTIONS_H

#include <gtk/gtk.h>

#include "gb-workbench.h"

G_BEGIN_DECLS

#define GB_TYPE_WORKBENCH_ACTIONS            (gb_workbench_actions_get_type())
#define GB_WORKBENCH_ACTIONS(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), GB_TYPE_WORKBENCH_ACTIONS, GbWorkbenchActions))
#define GB_WORKBENCH_ACTIONS_CONST(obj)      (G_TYPE_CHECK_INSTANCE_CAST ((obj), GB_TYPE_WORKBENCH_ACTIONS, GbWorkbenchActions const))
#define GB_WORKBENCH_ACTIONS_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass),  GB_TYPE_WORKBENCH_ACTIONS, GbWorkbenchActionsClass))
#define GB_IS_WORKBENCH_ACTIONS(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GB_TYPE_WORKBENCH_ACTIONS))
#define GB_IS_WORKBENCH_ACTIONS_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass),  GB_TYPE_WORKBENCH_ACTIONS))
#define GB_WORKBENCH_ACTIONS_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj),  GB_TYPE_WORKBENCH_ACTIONS, GbWorkbenchActionsClass))

typedef struct _GbWorkbenchActions        GbWorkbenchActions;
typedef struct _GbWorkbenchActionsClass   GbWorkbenchActionsClass;
typedef struct _GbWorkbenchActionsPrivate GbWorkbenchActionsPrivate;

struct _GbWorkbenchActions
{
   GSimpleActionGroup parent;

   /*< private >*/
   GbWorkbenchActionsPrivate *priv;
};

struct _GbWorkbenchActionsClass
{
   GSimpleActionGroupClass parent_class;
};

GType               gb_workbench_actions_get_type (void) G_GNUC_CONST;
GbWorkbenchActions *gb_workbench_actions_new      (GbWorkbench *workbench);

G_END_DECLS

#endif /* GB_WORKBENCH_ACTIONS_H */
