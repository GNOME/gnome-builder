/* gb-workspace.h
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

#ifndef GB_WORKSPACE_H
#define GB_WORKSPACE_H

#include <gtk/gtk.h>

G_BEGIN_DECLS

#define GB_TYPE_WORKSPACE            (gb_workspace_get_type())
#define GB_WORKSPACE(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), GB_TYPE_WORKSPACE, GbWorkspace))
#define GB_WORKSPACE_CONST(obj)      (G_TYPE_CHECK_INSTANCE_CAST ((obj), GB_TYPE_WORKSPACE, GbWorkspace const))
#define GB_WORKSPACE_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass),  GB_TYPE_WORKSPACE, GbWorkspaceClass))
#define GB_IS_WORKSPACE(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GB_TYPE_WORKSPACE))
#define GB_IS_WORKSPACE_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass),  GB_TYPE_WORKSPACE))
#define GB_WORKSPACE_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj),  GB_TYPE_WORKSPACE, GbWorkspaceClass))

typedef struct _GbWorkspace        GbWorkspace;
typedef struct _GbWorkspaceClass   GbWorkspaceClass;
typedef struct _GbWorkspacePrivate GbWorkspacePrivate;

struct _GbWorkspace
{
  GtkBin parent;

  /*< private >*/
  GbWorkspacePrivate *priv;
};

struct _GbWorkspaceClass
{
  GtkBinClass parent_class;

  void (*new_document) (GbWorkspace *workspace);
  void (*open)         (GbWorkspace *workspace);
};

GType         gb_workspace_get_type      (void) G_GNUC_CONST;
const gchar  *gb_workspace_get_icon_name (GbWorkspace *workspace);
void          gb_workspace_set_icon_name (GbWorkspace *workspace,
                                          const gchar *icon_name);
const gchar  *gb_workspace_get_title     (GbWorkspace *workspace);
void          gb_workspace_set_title     (GbWorkspace *workspace,
                                          const gchar *title);
void          gb_workspace_new_document  (GbWorkspace *workspace);
void          gb_workspace_open          (GbWorkspace *workspace);

G_END_DECLS

#endif /* GB_WORKSPACE_H */
