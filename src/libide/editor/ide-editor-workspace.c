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

#include "ide-editor-workspace.h"
#include "ide-workspace-private.h"

/**
 * SECTION:ide-editor-workspace
 * @title: IdeEditorWorkspace
 * @short_description: The editor IDE window
 *
 * The editor workspace is a secondary workspace that may be added to
 * supplement the IdePrimaryWorkspace for additional editors. It may
 * also be used in an "editor" mode without a project.
 */

struct _IdeEditorWorkspace
{
  IdeWorkspace       parent_instance;

  /* Template widgets */
  IdeHeaderBar       *header_bar;
  AdwWindowTitle     *project_title;
  GtkMenuButton      *add_button;
  GtkMenuButton      *sidebar_menu_button;
  PanelStatusbar     *statusbar;
  IdeWorkspaceDock    dock;
};

G_DEFINE_FINAL_TYPE (IdeEditorWorkspace, ide_editor_workspace, IDE_TYPE_WORKSPACE)

static gboolean
file_to_short_path (GBinding     *binding,
                    const GValue *from,
                    GValue       *to,
                    gpointer      user_data)
{
  GFile *file;

  g_assert (G_IS_BINDING (binding));
  g_assert (G_VALUE_HOLDS (from, G_TYPE_FILE));
  g_assert (G_VALUE_HOLDS (to, G_TYPE_STRING));
  g_assert (user_data == NULL);

  if ((file = g_value_get_object (from)))
    {
      if (g_file_is_native (file))
        g_value_take_string (to, ide_path_collapse (g_file_peek_path (file)));
      else
        g_value_take_string (to, g_file_get_uri (file));
    }

  return TRUE;
}

static void
ide_editor_workspace_context_set (IdeWorkspace *workspace,
                                  IdeContext   *context)
{
  IdeEditorWorkspace *self = (IdeEditorWorkspace *)workspace;
  IdeProjectInfo *project_info;
  IdeWorkbench *workbench;

  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (IDE_IS_EDITOR_WORKSPACE (self));
  g_assert (IDE_IS_CONTEXT (context));

  IDE_WORKSPACE_CLASS (ide_editor_workspace_parent_class)->context_set (workspace, context);

  workbench = ide_widget_get_workbench (GTK_WIDGET (self));
  project_info = ide_workbench_get_project_info (workbench);

  if (project_info)
    {
      g_object_bind_property (project_info, "name",
                              self->project_title, "title",
                              G_BINDING_SYNC_CREATE);
      g_object_bind_property_full (context, "workdir",
                                   self->project_title, "subtitle",
                                   G_BINDING_SYNC_CREATE,
                                   file_to_short_path, NULL, NULL, NULL);
    }
  else
    {
      g_object_bind_property_full (context, "workdir",
                                   self->project_title, "title",
                                   G_BINDING_SYNC_CREATE,
                                   file_to_short_path, NULL, NULL, NULL);
    }
}

static void
ide_editor_workspace_add_page (IdeWorkspace  *workspace,
                               IdePage       *page,
                               PanelPosition *position)
{
  IdeEditorWorkspace *self = (IdeEditorWorkspace *)workspace;

  g_assert (IDE_IS_EDITOR_WORKSPACE (self));

  _ide_workspace_add_widget (workspace, PANEL_WIDGET (page), position, &self->dock);
}

static void
ide_editor_workspace_add_pane (IdeWorkspace  *workspace,
                               IdePane       *pane,
                               PanelPosition *position)
{
  IdeEditorWorkspace *self = (IdeEditorWorkspace *)workspace;

  g_assert (IDE_IS_EDITOR_WORKSPACE (self));

  _ide_workspace_add_widget (workspace, PANEL_WIDGET (pane), position, &self->dock);
}

static void
ide_editor_workspace_add_grid_column (IdeWorkspace *workspace,
                                      guint         position)
{
  panel_grid_insert_column (PANEL_GRID (IDE_EDITOR_WORKSPACE (workspace)->dock.grid), position);
}

static IdeFrame *
ide_editor_workspace_get_most_recent_frame (IdeWorkspace *workspace)
{
  IdeEditorWorkspace *self = (IdeEditorWorkspace *)workspace;

  g_assert (IDE_IS_EDITOR_WORKSPACE (self));

  return IDE_FRAME (panel_grid_get_most_recent_frame (PANEL_GRID (self->dock.grid)));
}

static PanelFrame *
ide_editor_workspace_get_frame_at_position (IdeWorkspace  *workspace,
                                            PanelPosition *position)
{
  IdeEditorWorkspace *self = (IdeEditorWorkspace *)workspace;

  g_assert (IDE_IS_EDITOR_WORKSPACE (self));
  g_assert (position != NULL);

  return _ide_workspace_find_frame (workspace, position, &self->dock);
}

static gboolean
ide_editor_workspace_can_search (IdeWorkspace *workspace)
{
  return TRUE;
}

static IdeHeaderBar *
ide_editor_workspace_get_header_bar (IdeWorkspace *workspace)
{
  return IDE_EDITOR_WORKSPACE (workspace)->header_bar;
}

static void
ide_editor_workspace_foreach_page (IdeWorkspace    *workspace,
                                   IdePageCallback  callback,
                                   gpointer         user_data)
{
  ide_grid_foreach_page (IDE_EDITOR_WORKSPACE (workspace)->dock.grid, callback, user_data);
}

static void
toggle_panel_action (gpointer    instance,
                     const char *action_name,
                     GVariant   *param)
{
  IdeEditorWorkspace *self = instance;
  g_autofree char *can_property = NULL;
  const char *property = NULL;
  gboolean reveal;
  gboolean can_reveal;

  g_assert (IDE_IS_EDITOR_WORKSPACE (self));

  if (ide_str_equal0 (action_name, "panel.toggle-start"))
    property = "reveal-start";
  else if (ide_str_equal0 (action_name, "panel.toggle-end"))
    property = "reveal-end";
  else if (ide_str_equal0 (action_name, "panel.toggle-bottom"))
    property = "reveal-bottom";
  else
    return;

  can_property = g_strconcat ("can-", property, NULL);

  g_object_get (self->dock.dock,
                can_property, &can_reveal,
                property, &reveal,
                NULL);

  reveal = !reveal;

  if (reveal && can_reveal)
    g_object_set (self->dock.dock, property, TRUE, NULL);
  else
    g_object_set (self->dock.dock, property, FALSE, NULL);

  if (!reveal)
    gtk_widget_grab_focus (GTK_WIDGET (self->dock.grid));
}

static void
ide_editor_workspace_agree_to_close_async (IdeWorkspace        *workspace,
                                           GCancellable        *cancellable,
                                           GAsyncReadyCallback  callback,
                                           gpointer             user_data)
{
  _ide_workspace_agree_to_close_async (workspace,
                                       IDE_EDITOR_WORKSPACE (workspace)->dock.grid,
                                       cancellable,
                                       callback,
                                       user_data);
}

static gboolean
ide_editor_workspace_agree_to_close_finish (IdeWorkspace  *workspace,
                                            GAsyncResult  *result,
                                            GError       **error)
{
  return _ide_workspace_agree_to_close_finish (workspace, result, error);
}

static void
ide_editor_workspace_save_session (IdeWorkspace *workspace,
                                   IdeSession   *session)
{
  IdeEditorWorkspace *self = (IdeEditorWorkspace *)workspace;

  g_assert (IDE_IS_EDITOR_WORKSPACE (self));
  g_assert (IDE_IS_SESSION (session));

  _ide_workspace_save_session_simple (workspace, session, &self->dock);
}

static void
ide_editor_workspace_restore_session (IdeWorkspace *workspace,
                                      IdeSession   *session)
{
  IdeEditorWorkspace *self = (IdeEditorWorkspace *)workspace;

  g_assert (IDE_IS_EDITOR_WORKSPACE (self));
  g_assert (IDE_IS_SESSION (session));

  _ide_workspace_restore_session_simple (workspace, session, &self->dock);
}

static PanelStatusbar *
ide_editor_workspace_get_statusbar (IdeWorkspace *workspace)
{
  return IDE_EDITOR_WORKSPACE (workspace)->statusbar;
}

static gboolean
invert_boolean (gpointer object,
                gboolean value)
{
  return !value;
}

static void
ide_editor_workspace_dispose (GObject *object)
{
  IdeEditorWorkspace *self = (IdeEditorWorkspace *)object;

  /* Ensure that the grid is removed first so that it will cleanup
   * addins/pages/etc before we ever get to removing the workspace
   * addins as part of the parent class.
   */
  panel_dock_remove (self->dock.dock, GTK_WIDGET (self->dock.grid));
  self->dock.grid = NULL;

  G_OBJECT_CLASS (ide_editor_workspace_parent_class)->dispose (object);
}

static void
ide_editor_workspace_class_init (IdeEditorWorkspaceClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);
  IdeWorkspaceClass *workspace_class = IDE_WORKSPACE_CLASS (klass);

  object_class->dispose = ide_editor_workspace_dispose;

  workspace_class->add_grid_column = ide_editor_workspace_add_grid_column;
  workspace_class->add_page = ide_editor_workspace_add_page;
  workspace_class->add_pane = ide_editor_workspace_add_pane;
  workspace_class->agree_to_close_async = ide_editor_workspace_agree_to_close_async;
  workspace_class->agree_to_close_finish = ide_editor_workspace_agree_to_close_finish;
  workspace_class->can_search = ide_editor_workspace_can_search;
  workspace_class->context_set = ide_editor_workspace_context_set;
  workspace_class->foreach_page = ide_editor_workspace_foreach_page;
  workspace_class->get_frame_at_position = ide_editor_workspace_get_frame_at_position;
  workspace_class->get_header_bar = ide_editor_workspace_get_header_bar;
  workspace_class->get_most_recent_frame = ide_editor_workspace_get_most_recent_frame;
  workspace_class->save_session = ide_editor_workspace_save_session;
  workspace_class->restore_session = ide_editor_workspace_restore_session;
  workspace_class->get_statusbar = ide_editor_workspace_get_statusbar;

  ide_workspace_class_set_kind (workspace_class, "editor");

  gtk_widget_class_set_template_from_resource (widget_class, "/org/gnome/libide-editor/ide-editor-workspace.ui");
  gtk_widget_class_bind_template_child (widget_class, IdeEditorWorkspace, add_button);
  gtk_widget_class_bind_template_child (widget_class, IdeEditorWorkspace, header_bar);
  gtk_widget_class_bind_template_child (widget_class, IdeEditorWorkspace, project_title);
  gtk_widget_class_bind_template_child (widget_class, IdeEditorWorkspace, sidebar_menu_button);
  gtk_widget_class_bind_template_child (widget_class, IdeEditorWorkspace, statusbar);
  gtk_widget_class_bind_template_callback (widget_class, _ide_workspace_adopt_widget);
  gtk_widget_class_bind_template_callback (widget_class, invert_boolean);

  _ide_workspace_class_bind_template_dock (widget_class, G_STRUCT_OFFSET (IdeEditorWorkspace, dock));

  ide_workspace_class_install_action (workspace_class, "panel.toggle-start", NULL, toggle_panel_action);
  ide_workspace_class_install_action (workspace_class, "panel.toggle-end", NULL, toggle_panel_action);
  ide_workspace_class_install_action (workspace_class, "panel.toggle-bottom", NULL, toggle_panel_action);

  g_type_ensure (IDE_TYPE_GRID);
  g_type_ensure (IDE_TYPE_NOTIFICATIONS_BUTTON);
  g_type_ensure (IDE_TYPE_OMNI_BAR);
}

static void
ide_editor_workspace_init (IdeEditorWorkspace *self)
{
  GtkPopover *popover;
  GMenu *menu;

  gtk_widget_init_template (GTK_WIDGET (self));

  menu = ide_application_get_menu_by_id (IDE_APPLICATION_DEFAULT, "new-document-menu");
  gtk_menu_button_set_menu_model (self->add_button, G_MENU_MODEL (menu));

  menu = ide_application_get_menu_by_id (IDE_APPLICATION_DEFAULT, "ide-editor-workspace-menu");
  gtk_menu_button_set_menu_model (self->sidebar_menu_button, G_MENU_MODEL (menu));

  popover = gtk_menu_button_get_popover (self->sidebar_menu_button);
  ide_header_bar_setup_menu (GTK_POPOVER_MENU (popover));
}

/**
 * ide_editor_workspace_new:
 * @application: an #IdeApplication such as %IDE_APPLICATION_DEFAULT
 *
 * Creates a new #IdeEditorWorkspace
 *
 * Returns: (transfer full): an #IdeEditorWorkspace
 */
IdeEditorWorkspace *
ide_editor_workspace_new (IdeApplication *application)
{
  g_return_val_if_fail (IDE_IS_APPLICATION (application), NULL);

  return g_object_new (IDE_TYPE_EDITOR_WORKSPACE,
                       "application", application,
                       NULL);
}
