/* ide-primary-workspace.c
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

#define G_LOG_DOMAIN "ide-primary-workspace"

#include "config.h"

#include "ide-frame.h"
#include "ide-grid.h"
#include "ide-gui-global.h"
#include "ide-header-bar.h"
#include "ide-notifications-button.h"
#include "ide-omni-bar.h"
#include "ide-primary-workspace-private.h"
#include "ide-run-button.h"

/**
 * SECTION:ide-primary-workspace
 * @title: IdePrimaryWorkspace
 * @short_description: The primary IDE window
 *
 * The primary workspace is the main workspace window for the user. This is the
 * "IDE experience" workspace. It is generally created by the workbench when
 * opening a project (unless another workspace type has been requested).
 *
 * See ide_workbench_open_async() for how to select another workspace type
 * when opening a project.
 *
 * Returns: (transfer full): an #IdePrimaryWorkspace
 */

struct _IdePrimaryWorkspace
{
  IdeWorkspace       parent_instance;

  /* Template widgets */
  IdeHeaderBar       *header_bar;
  IdeRunButton       *run_button;
  GtkLabel           *project_title;
  GtkMenuButton      *add_button;
  PanelPaned         *edge_start;
  PanelPaned         *edge_end;
  PanelPaned         *edge_bottom;
  IdeGrid            *grid;
};

G_DEFINE_FINAL_TYPE (IdePrimaryWorkspace, ide_primary_workspace, IDE_TYPE_WORKSPACE)

static void
ide_primary_workspace_context_set (IdeWorkspace *workspace,
                                   IdeContext   *context)
{
  IdePrimaryWorkspace *self = (IdePrimaryWorkspace *)workspace;
  IdeProjectInfo *project_info;
  IdeWorkbench *workbench;

  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (IDE_IS_PRIMARY_WORKSPACE (self));
  g_assert (IDE_IS_CONTEXT (context));

  IDE_WORKSPACE_CLASS (ide_primary_workspace_parent_class)->context_set (workspace, context);

  workbench = ide_widget_get_workbench (GTK_WIDGET (self));
  project_info = ide_workbench_get_project_info (workbench);

  if (project_info)
    g_object_bind_property (project_info, "name",
                            self->project_title, "label",
                            G_BINDING_SYNC_CREATE);
}

static void
ide_primary_workspace_add_page (IdeWorkspace     *workspace,
                                IdePage          *page,
                                IdePanelPosition *position)
{
  IdePrimaryWorkspace *self = (IdePrimaryWorkspace *)workspace;
  PanelFrame *frame;
  PanelDockPosition edge;
  guint column;
  guint row;

  g_assert (IDE_IS_PRIMARY_WORKSPACE (self));
  g_assert (IDE_IS_PAGE (page));
  g_assert (position != NULL);

  ide_panel_position_get_edge (position, &edge);

  switch (edge)
    {
    case PANEL_DOCK_POSITION_START:
    case PANEL_DOCK_POSITION_END:
    case PANEL_DOCK_POSITION_BOTTOM:
    case PANEL_DOCK_POSITION_TOP:
    default:
      g_warning ("Primary workspace only supports center position");
      return;

    case PANEL_DOCK_POSITION_CENTER:
      break;
    }

  if (!ide_panel_position_get_column (position, &column))
    column = 0;

  if (!ide_panel_position_get_row (position, &row))
    row = 0;

  frame = panel_grid_column_get_row (panel_grid_get_column (PANEL_GRID (self->grid), column), row);

  /* TODO: Handle depth */
  panel_frame_add (frame, PANEL_WIDGET (page));
}

static void
ide_primary_workspace_add_pane (IdeWorkspace     *workspace,
                                IdePane          *pane,
                                IdePanelPosition *position)
{
  IdePrimaryWorkspace *self = (IdePrimaryWorkspace *)workspace;
  PanelDockPosition edge;
  PanelPaned *paned;
  GtkWidget *parent;
  guint depth;
  guint nth = 0;

  g_assert (IDE_IS_PRIMARY_WORKSPACE (self));
  g_assert (IDE_IS_PANE (pane));
  g_assert (position != NULL);

  ide_panel_position_get_edge (position, &edge);

  switch (edge)
    {
    case PANEL_DOCK_POSITION_START:
      paned = self->edge_start;
      ide_panel_position_get_row (position, &nth);
      break;

    case PANEL_DOCK_POSITION_END:
      paned = self->edge_end;
      ide_panel_position_get_row (position, &nth);
      break;

    case PANEL_DOCK_POSITION_BOTTOM:
      paned = self->edge_bottom;
      ide_panel_position_get_column (position, &nth);
      break;

    case PANEL_DOCK_POSITION_TOP:
    case PANEL_DOCK_POSITION_CENTER:
    default:
      g_warning ("Primary workspace only supports left/right/bottom edges");
      return;
    }

  while (!(parent = panel_paned_get_nth_child (paned, nth)))
    {
      parent = panel_frame_new ();

      if (edge == PANEL_DOCK_POSITION_START ||
          edge == PANEL_DOCK_POSITION_END)
        gtk_orientable_set_orientation (GTK_ORIENTABLE (parent), GTK_ORIENTATION_VERTICAL);
      else
        gtk_orientable_set_orientation (GTK_ORIENTABLE (parent), GTK_ORIENTATION_HORIZONTAL);

      panel_paned_append (paned, parent);
    }

  if (ide_panel_position_get_depth (position, &depth))
    {
      /* TODO: setup position */
      panel_frame_add (PANEL_FRAME (parent), PANEL_WIDGET (pane));
    }
  else
    {
      panel_frame_add (PANEL_FRAME (parent), PANEL_WIDGET (pane));
    }
}

static IdeFrame *
ide_primary_workspace_get_most_recent_frame (IdeWorkspace *workspace)
{
  IdePrimaryWorkspace *self = (IdePrimaryWorkspace *)workspace;

  g_assert (IDE_IS_PRIMARY_WORKSPACE (self));

  return IDE_FRAME (panel_grid_get_most_recent_frame (PANEL_GRID (self->grid)));
}

static void
ide_primary_workspace_class_init (IdePrimaryWorkspaceClass *klass)
{
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);
  IdeWorkspaceClass *workspace_class = IDE_WORKSPACE_CLASS (klass);

  ide_workspace_class_set_kind (workspace_class, "primary");

  workspace_class->context_set = ide_primary_workspace_context_set;
  workspace_class->add_page = ide_primary_workspace_add_page;
  workspace_class->add_pane = ide_primary_workspace_add_pane;
  workspace_class->get_most_recent_frame = ide_primary_workspace_get_most_recent_frame;

  gtk_widget_class_set_template_from_resource (widget_class, "/org/gnome/libide-gui/ui/ide-primary-workspace.ui");
  gtk_widget_class_bind_template_child (widget_class, IdePrimaryWorkspace, add_button);
  gtk_widget_class_bind_template_child (widget_class, IdePrimaryWorkspace, edge_bottom);
  gtk_widget_class_bind_template_child (widget_class, IdePrimaryWorkspace, edge_end);
  gtk_widget_class_bind_template_child (widget_class, IdePrimaryWorkspace, edge_start);
  gtk_widget_class_bind_template_child (widget_class, IdePrimaryWorkspace, grid);
  gtk_widget_class_bind_template_child (widget_class, IdePrimaryWorkspace, header_bar);
  gtk_widget_class_bind_template_child (widget_class, IdePrimaryWorkspace, project_title);
  gtk_widget_class_bind_template_child (widget_class, IdePrimaryWorkspace, run_button);

  g_type_ensure (IDE_TYPE_GRID);
  g_type_ensure (IDE_TYPE_NOTIFICATIONS_BUTTON);
  g_type_ensure (IDE_TYPE_OMNI_BAR);
  g_type_ensure (IDE_TYPE_RUN_BUTTON);
}

static void
ide_primary_workspace_init (IdePrimaryWorkspace *self)
{
  GMenu *menu;

  gtk_widget_init_template (GTK_WIDGET (self));

  menu = ide_application_get_menu_by_id (IDE_APPLICATION_DEFAULT, "new-document-menu");
  gtk_menu_button_set_menu_model (self->add_button, G_MENU_MODEL (menu));

  _ide_primary_workspace_init_actions (self);
}
