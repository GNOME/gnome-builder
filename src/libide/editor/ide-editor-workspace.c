/* ide-editor-workspace.c
 *
 * Copyright 2018-2019 Christian Hergert <chergert@redhat.com>
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
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#define G_LOG_DOMAIN "ide-editor-workspace"

#include "config.h"

#include "ide-editor-surface.h"
#include "ide-editor-workspace.h"

/**
 * SECTION:ide-editor-workspace
 * @title: IdeEditorWorkspace
 * @short_description: A simplified workspace for dedicated editing
 *
 * The #IdeEditorWorkspace is a secondary workspace that can be used to
 * add additional #IdePage to. It does not contain the full contents of
 * the #IdePrimaryWorkspace. It is suitable for using on an additional
 * monitor as well as a dedicated editor in simplified Builder mode when
 * running directly from the command line.
 *
 * Since: 3.32
 */

struct _IdeEditorWorkspace
{
  IdeWorkspace        parent_instance;
  DzlMenuButton      *surface_menu_button;
  DzlShortcutTooltip *search_tooltip;
};

G_DEFINE_TYPE (IdeEditorWorkspace, ide_editor_workspace, IDE_TYPE_WORKSPACE)

static void
ide_editor_workspace_surface_set (IdeWorkspace *workspace,
                                   IdeSurface   *surface)
{
  IdeEditorWorkspace *self = (IdeEditorWorkspace *)workspace;

  g_assert (IDE_IS_EDITOR_WORKSPACE (self));
  g_assert (!surface || IDE_IS_SURFACE (surface));

  if (DZL_IS_DOCK_ITEM (surface))
    {
      g_autofree gchar *icon_name = NULL;

      icon_name = dzl_dock_item_get_icon_name (DZL_DOCK_ITEM (surface));
      g_object_set (self->surface_menu_button,
                    "icon-name", icon_name,
                    NULL);
    }

  IDE_WORKSPACE_CLASS (ide_editor_workspace_parent_class)->surface_set (workspace, surface);
}

static void
ide_editor_workspace_class_init (IdeEditorWorkspaceClass *klass)
{
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);
  IdeWorkspaceClass *workspace_class = IDE_WORKSPACE_CLASS (klass);

  ide_workspace_class_set_kind (workspace_class, "editor");

  workspace_class->surface_set = ide_editor_workspace_surface_set;

  gtk_widget_class_set_template_from_resource (widget_class, "/org/gnome/libide-editor/ui/ide-editor-workspace.ui");
  gtk_widget_class_bind_template_child (widget_class, IdeEditorWorkspace, surface_menu_button);
  gtk_widget_class_bind_template_child (widget_class, IdeEditorWorkspace, search_tooltip);
}

static void
ide_editor_workspace_init (IdeEditorWorkspace *self)
{
  gtk_widget_init_template (GTK_WIDGET (self));
}

/**
 * ide_editor_workspace_new:
 * @app: an #IdeApplication
 *
 * Creates a new #IdeEditorWorkspace.
 *
 * You'll need to add this to a workbench to be functional.
 *
 * Returns: (transfer full): an #IdeEditorWorkspace
 *
 * Since: 3.32
 */
IdeEditorWorkspace *
ide_editor_workspace_new (IdeApplication *app)
{
  return g_object_new (IDE_TYPE_EDITOR_WORKSPACE,
                       "application", app,
                       NULL);
}
