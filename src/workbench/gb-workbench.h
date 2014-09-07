/* gb-workbench.h
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

#ifndef GB_WORKBENCH_H
#define GB_WORKBENCH_H

#include <gtk/gtk.h>

#include "gb-workspace.h"

G_BEGIN_DECLS

#define GB_TYPE_WORKBENCH            (gb_workbench_get_type())
#define GB_WORKBENCH(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), GB_TYPE_WORKBENCH, GbWorkbench))
#define GB_WORKBENCH_CONST(obj)      (G_TYPE_CHECK_INSTANCE_CAST ((obj), GB_TYPE_WORKBENCH, GbWorkbench const))
#define GB_WORKBENCH_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass),  GB_TYPE_WORKBENCH, GbWorkbenchClass))
#define GB_IS_WORKBENCH(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GB_TYPE_WORKBENCH))
#define GB_IS_WORKBENCH_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass),  GB_TYPE_WORKBENCH))
#define GB_WORKBENCH_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj),  GB_TYPE_WORKBENCH, GbWorkbenchClass))

typedef struct _GbWorkbench        GbWorkbench;
typedef struct _GbWorkbenchClass   GbWorkbenchClass;
typedef struct _GbWorkbenchPrivate GbWorkbenchPrivate;

struct _GbWorkbench
{
  GtkApplicationWindow parent;

  /*< private >*/
  GbWorkbenchPrivate *priv;
};

struct _GbWorkbenchClass
{
  GtkApplicationWindowClass parent_class;

  void (*workspace_changed) (GbWorkbench *workbench,
                             GbWorkspace *workspace);
};

GType gb_workbench_get_type (void) G_GNUC_CONST;

G_END_DECLS

#endif /* GB_WORKBENCH_H */
