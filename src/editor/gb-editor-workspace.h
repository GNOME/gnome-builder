/* gb-editor-workspace.h
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

#ifndef GB_EDITOR_WORKSPACE_H
#define GB_EDITOR_WORKSPACE_H

#include "gb-workspace.h"

G_BEGIN_DECLS

#define GB_TYPE_EDITOR_WORKSPACE            (gb_editor_workspace_get_type())
#define GB_EDITOR_WORKSPACE(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), GB_TYPE_EDITOR_WORKSPACE, GbEditorWorkspace))
#define GB_EDITOR_WORKSPACE_CONST(obj)      (G_TYPE_CHECK_INSTANCE_CAST ((obj), GB_TYPE_EDITOR_WORKSPACE, GbEditorWorkspace const))
#define GB_EDITOR_WORKSPACE_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass),  GB_TYPE_EDITOR_WORKSPACE, GbEditorWorkspaceClass))
#define GB_IS_EDITOR_WORKSPACE(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GB_TYPE_EDITOR_WORKSPACE))
#define GB_IS_EDITOR_WORKSPACE_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass),  GB_TYPE_EDITOR_WORKSPACE))
#define GB_EDITOR_WORKSPACE_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj),  GB_TYPE_EDITOR_WORKSPACE, GbEditorWorkspaceClass))

typedef struct _GbEditorWorkspace        GbEditorWorkspace;
typedef struct _GbEditorWorkspaceClass   GbEditorWorkspaceClass;
typedef struct _GbEditorWorkspacePrivate GbEditorWorkspacePrivate;

struct _GbEditorWorkspace
{
  GbWorkspace parent;

  /*< private >*/
  GbEditorWorkspacePrivate *priv;
};

struct _GbEditorWorkspaceClass
{
  GbWorkspaceClass parent_class;
};

GType gb_editor_workspace_get_type (void);
void  gb_editor_workspace_open     (GbEditorWorkspace *workspace,
                                    GFile             *file);

G_END_DECLS

#endif /* GB_EDITOR_WORKSPACE_H */
