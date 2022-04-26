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
#include "ide-workspace-private.h"

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
 */

struct _IdePrimaryWorkspace
{
  IdeWorkspace       parent_instance;

  /* Template widgets */
  IdeHeaderBar       *header_bar;
  IdeRunButton       *run_button;
  GtkLabel           *project_title;
  GtkMenuButton      *add_button;
  PanelDock          *dock;
  PanelPaned         *edge_start;
  PanelPaned         *edge_end;
  PanelPaned         *edge_bottom;
  IdeGrid            *grid;
  GtkOverlay         *overlay;
  IdeOmniBar         *omni_bar;
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

  g_assert (IDE_IS_PRIMARY_WORKSPACE (self));

  _ide_workspace_add_widget (workspace,
                             PANEL_WIDGET (page),
                             position,
                             self->edge_start,
                             self->edge_end,
                             self->edge_bottom,
                             self->grid);
}

static void
ide_primary_workspace_add_pane (IdeWorkspace     *workspace,
                                IdePane          *pane,
                                IdePanelPosition *position)
{
  IdePrimaryWorkspace *self = (IdePrimaryWorkspace *)workspace;

  g_assert (IDE_IS_PRIMARY_WORKSPACE (self));

  _ide_workspace_add_widget (workspace,
                             PANEL_WIDGET (pane),
                             position,
                             self->edge_start,
                             self->edge_end,
                             self->edge_bottom,
                             self->grid);
}

static void
ide_primary_workspace_add_overlay (IdeWorkspace *workspace,
                                   GtkWidget    *overlay)
{
  IdePrimaryWorkspace *self = (IdePrimaryWorkspace *)workspace;

  g_assert (IDE_IS_PRIMARY_WORKSPACE (self));

  gtk_overlay_add_overlay (self->overlay, overlay);
}

static void
ide_primary_workspace_remove_overlay (IdeWorkspace *workspace,
                                      GtkWidget    *overlay)
{
  IdePrimaryWorkspace *self = (IdePrimaryWorkspace *)workspace;

  g_assert (IDE_IS_PRIMARY_WORKSPACE (self));

  gtk_overlay_remove_overlay (self->overlay, overlay);
}

static IdeFrame *
ide_primary_workspace_get_most_recent_frame (IdeWorkspace *workspace)
{
  IdePrimaryWorkspace *self = (IdePrimaryWorkspace *)workspace;

  g_assert (IDE_IS_PRIMARY_WORKSPACE (self));

  return IDE_FRAME (panel_grid_get_most_recent_frame (PANEL_GRID (self->grid)));
}

static PanelFrame *
ide_primary_workspace_get_frame_at_position (IdeWorkspace     *workspace,
                                             IdePanelPosition *position)
{
  IdePrimaryWorkspace *self = (IdePrimaryWorkspace *)workspace;

  g_assert (IDE_IS_PRIMARY_WORKSPACE (self));
  g_assert (position != NULL);

  return _ide_workspace_find_frame (workspace,
                                    position,
                                    self->edge_start,
                                    self->edge_end,
                                    self->edge_bottom,
                                    self->grid);
}

static gboolean
ide_primary_workspace_can_search (IdeWorkspace *workspace)
{
  return TRUE;
}

static void
ide_primary_workspace_dispose (GObject *object)
{
  IdePrimaryWorkspace *self = (IdePrimaryWorkspace *)object;

  /* Ensure that the grid is removed first so that it will cleanup
   * addins/pages/etc before we ever get to removing the workspace
   * addins as part of the parent class.
   */
  panel_dock_remove (self->dock, GTK_WIDGET (self->grid));
  self->grid = NULL;

  G_OBJECT_CLASS (ide_primary_workspace_parent_class)->dispose (object);
}

static void
ide_primary_workspace_class_init (IdePrimaryWorkspaceClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);
  IdeWorkspaceClass *workspace_class = IDE_WORKSPACE_CLASS (klass);

  object_class->dispose = ide_primary_workspace_dispose;

  workspace_class->add_page = ide_primary_workspace_add_page;
  workspace_class->add_pane = ide_primary_workspace_add_pane;
  workspace_class->add_overlay = ide_primary_workspace_add_overlay;
  workspace_class->remove_overlay = ide_primary_workspace_remove_overlay;
  workspace_class->can_search = ide_primary_workspace_can_search;
  workspace_class->context_set = ide_primary_workspace_context_set;
  workspace_class->get_frame_at_position = ide_primary_workspace_get_frame_at_position;
  workspace_class->get_most_recent_frame = ide_primary_workspace_get_most_recent_frame;

  ide_workspace_class_set_kind (workspace_class, "primary");

  gtk_widget_class_set_template_from_resource (widget_class, "/org/gnome/libide-gui/ui/ide-primary-workspace.ui");
  gtk_widget_class_bind_template_child (widget_class, IdePrimaryWorkspace, add_button);
  gtk_widget_class_bind_template_child (widget_class, IdePrimaryWorkspace, dock);
  gtk_widget_class_bind_template_child (widget_class, IdePrimaryWorkspace, edge_bottom);
  gtk_widget_class_bind_template_child (widget_class, IdePrimaryWorkspace, edge_end);
  gtk_widget_class_bind_template_child (widget_class, IdePrimaryWorkspace, edge_start);
  gtk_widget_class_bind_template_child (widget_class, IdePrimaryWorkspace, grid);
  gtk_widget_class_bind_template_child (widget_class, IdePrimaryWorkspace, header_bar);
  gtk_widget_class_bind_template_child (widget_class, IdePrimaryWorkspace, omni_bar);
  gtk_widget_class_bind_template_child (widget_class, IdePrimaryWorkspace, overlay);
  gtk_widget_class_bind_template_child (widget_class, IdePrimaryWorkspace, project_title);
  gtk_widget_class_bind_template_child (widget_class, IdePrimaryWorkspace, run_button);

  gtk_widget_class_add_binding_action (widget_class, GDK_KEY_Return, GDK_CONTROL_MASK, "workbench.global-search", NULL);

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

/**
 * ide_primary_workspace_get_omni_bar:
 * @self: an #IdePrimaryWorkspace
 *
 * Retrieves the #IdeOmniBar of @self.
 *
 * Returns: (transfer none): an #IdeOmniBar
 */
IdeOmniBar *
ide_primary_workspace_get_omni_bar (IdePrimaryWorkspace *self)
{
  g_return_val_if_fail (IDE_IS_PRIMARY_WORKSPACE (self), NULL);

  return self->omni_bar;
}
