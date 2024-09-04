/* ide-primary-workspace.c
 *
 * Copyright 2018-2023 Christian Hergert <chergert@redhat.com>
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

#include <glib/gi18n.h>

#include "ide-frame.h"
#include "ide-grid.h"
#include "ide-gui-global.h"
#include "ide-header-bar.h"
#include "ide-notifications-button.h"
#include "ide-omni-bar.h"
#include "ide-primary-workspace.h"
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
  GtkMenuButton      *sidebar_menu_button;
  GtkOverlay         *overlay;
  IdeOmniBar         *omni_bar;
  IdeJoinedMenu      *build_menu;
  PanelStatusbar     *statusbar;

  IdeWorkspaceDock    dock;
};

G_DEFINE_FINAL_TYPE (IdePrimaryWorkspace, ide_primary_workspace, IDE_TYPE_WORKSPACE)

static PanelStatusbar *
ide_primary_workspace_get_statusbar (IdeWorkspace *workspace)
{
  return IDE_PRIMARY_WORKSPACE (workspace)->statusbar;
}

static gboolean
config_to_title (GBinding     *binding,
                 const GValue *value,
                 GValue       *to_value,
                 gpointer      data)
{
  IdeConfig *config = g_value_get_object (value);

  if (config)
    g_value_set_string (to_value, ide_config_get_display_name (config));
  else
    g_value_set_static_string (to_value, _("Invalid configuration"));

  return TRUE;
}

static void
ide_primary_workspace_context_set (IdeWorkspace *workspace,
                                   IdeContext   *context)
{
  IdePrimaryWorkspace *self = (IdePrimaryWorkspace *)workspace;
  IdeConfigManager *config_manager;
  GMenuModel *config_menu;

  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (IDE_IS_PRIMARY_WORKSPACE (self));
  g_assert (IDE_IS_CONTEXT (context));

  IDE_WORKSPACE_CLASS (ide_primary_workspace_parent_class)->context_set (workspace, context);

  config_manager = ide_config_manager_from_context (context);

  g_object_bind_property_full (config_manager, "current",
                               self->project_title, "label",
                               G_BINDING_SYNC_CREATE,
                               config_to_title, NULL, NULL, NULL);

  config_menu = ide_config_manager_get_menu (config_manager);
  ide_joined_menu_prepend_menu (self->build_menu, G_MENU_MODEL (config_menu));
}

static void
ide_primary_workspace_add_page (IdeWorkspace  *workspace,
                                IdePage       *page,
                                PanelPosition *position)
{
  IdePrimaryWorkspace *self = (IdePrimaryWorkspace *)workspace;

  g_assert (IDE_IS_PRIMARY_WORKSPACE (self));

  _ide_workspace_add_widget (workspace, PANEL_WIDGET (page), position, &self->dock);
}

static void
ide_primary_workspace_add_pane (IdeWorkspace  *workspace,
                                IdePane       *pane,
                                PanelPosition *position)
{
  IdePrimaryWorkspace *self = (IdePrimaryWorkspace *)workspace;

  g_assert (IDE_IS_PRIMARY_WORKSPACE (self));

  _ide_workspace_add_widget (workspace, PANEL_WIDGET (pane), position, &self->dock);
}

static void
ide_primary_workspace_add_grid_column (IdeWorkspace *workspace,
                                       guint         position)
{
  panel_grid_insert_column (PANEL_GRID (IDE_PRIMARY_WORKSPACE (workspace)->dock.grid), position);
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

  return IDE_FRAME (panel_grid_get_most_recent_frame (PANEL_GRID (self->dock.grid)));
}

static PanelFrame *
ide_primary_workspace_get_frame_at_position (IdeWorkspace  *workspace,
                                             PanelPosition *position)
{
  IdePrimaryWorkspace *self = (IdePrimaryWorkspace *)workspace;

  g_assert (IDE_IS_PRIMARY_WORKSPACE (self));
  g_assert (position != NULL);

  return _ide_workspace_find_frame (workspace, position, &self->dock);
}

static gboolean
ide_primary_workspace_can_search (IdeWorkspace *workspace)
{
  return TRUE;
}

static IdeHeaderBar *
ide_primary_workspace_get_header_bar (IdeWorkspace *workspace)
{
  return IDE_PRIMARY_WORKSPACE (workspace)->header_bar;
}

static void
ide_primary_workspace_foreach_page (IdeWorkspace    *workspace,
                                    IdePageCallback  callback,
                                    gpointer         user_data)
{
  ide_grid_foreach_page (IDE_PRIMARY_WORKSPACE (workspace)->dock.grid, callback, user_data);
}

static void
toggle_panel_action (gpointer    instance,
                     const char *action_name,
                     GVariant   *param)
{
  IdePrimaryWorkspace *self = instance;
  g_autofree char *can_property = NULL;
  const char *property = NULL;
  gboolean reveal;
  gboolean can_reveal;

  g_assert (IDE_IS_PRIMARY_WORKSPACE (self));

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
ide_primary_workspace_agree_to_close_async (IdeWorkspace        *workspace,
                                            GCancellable        *cancellable,
                                            GAsyncReadyCallback  callback,
                                            gpointer             user_data)
{
  _ide_workspace_agree_to_close_async (workspace,
                                       IDE_PRIMARY_WORKSPACE (workspace)->dock.grid,
                                       cancellable,
                                       callback,
                                       user_data);
}

static gboolean
ide_primary_workspace_agree_to_close_finish (IdeWorkspace  *workspace,
                                             GAsyncResult  *result,
                                             GError       **error)
{
  return _ide_workspace_agree_to_close_finish (workspace, result, error);
}

static void
ide_primary_workspace_save_session (IdeWorkspace *workspace,
                                    IdeSession   *session)
{
  IdePrimaryWorkspace *self = (IdePrimaryWorkspace *)workspace;

  g_assert (IDE_IS_PRIMARY_WORKSPACE (self));
  g_assert (IDE_IS_SESSION (session));

  _ide_workspace_save_session_simple (workspace, session, &self->dock);
}

static void
ide_primary_workspace_restore_session (IdeWorkspace *workspace,
                                       IdeSession   *session)
{
  IdePrimaryWorkspace *self = (IdePrimaryWorkspace *)workspace;

  g_assert (IDE_IS_PRIMARY_WORKSPACE (self));
  g_assert (IDE_IS_SESSION (session));

  _ide_workspace_restore_session_simple (workspace, session, &self->dock);
}

static gboolean
invert_boolean (gpointer object,
                gboolean value)
{
  return !value;
}

static void
ide_primary_workspace_dispose (GObject *object)
{
  IdePrimaryWorkspace *self = (IdePrimaryWorkspace *)object;

  /* Ensure that the grid is removed first so that it will cleanup
   * addins/pages/etc before we ever get to removing the workspace
   * addins as part of the parent class.
   */
  panel_dock_remove (self->dock.dock, GTK_WIDGET (self->dock.grid));
  self->dock.grid = NULL;

  G_OBJECT_CLASS (ide_primary_workspace_parent_class)->dispose (object);
}

static void
ide_primary_workspace_class_init (IdePrimaryWorkspaceClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);
  IdeWorkspaceClass *workspace_class = IDE_WORKSPACE_CLASS (klass);

  object_class->dispose = ide_primary_workspace_dispose;

  workspace_class->add_grid_column = ide_primary_workspace_add_grid_column;
  workspace_class->add_overlay = ide_primary_workspace_add_overlay;
  workspace_class->add_page = ide_primary_workspace_add_page;
  workspace_class->add_pane = ide_primary_workspace_add_pane;
  workspace_class->agree_to_close_async = ide_primary_workspace_agree_to_close_async;
  workspace_class->agree_to_close_finish = ide_primary_workspace_agree_to_close_finish;
  workspace_class->can_search = ide_primary_workspace_can_search;
  workspace_class->context_set = ide_primary_workspace_context_set;
  workspace_class->foreach_page = ide_primary_workspace_foreach_page;
  workspace_class->get_frame_at_position = ide_primary_workspace_get_frame_at_position;
  workspace_class->get_header_bar = ide_primary_workspace_get_header_bar;
  workspace_class->get_most_recent_frame = ide_primary_workspace_get_most_recent_frame;
  workspace_class->remove_overlay = ide_primary_workspace_remove_overlay;
  workspace_class->save_session = ide_primary_workspace_save_session;
  workspace_class->restore_session = ide_primary_workspace_restore_session;
  workspace_class->get_statusbar = ide_primary_workspace_get_statusbar;

  ide_workspace_class_set_kind (workspace_class, "primary");

  gtk_widget_class_set_template_from_resource (widget_class, "/org/gnome/libide-gui/ui/ide-primary-workspace.ui");
  gtk_widget_class_bind_template_child (widget_class, IdePrimaryWorkspace, add_button);
  gtk_widget_class_bind_template_child (widget_class, IdePrimaryWorkspace, build_menu);
  gtk_widget_class_bind_template_child (widget_class, IdePrimaryWorkspace, header_bar);
  gtk_widget_class_bind_template_child (widget_class, IdePrimaryWorkspace, omni_bar);
  gtk_widget_class_bind_template_child (widget_class, IdePrimaryWorkspace, overlay);
  gtk_widget_class_bind_template_child (widget_class, IdePrimaryWorkspace, project_title);
  gtk_widget_class_bind_template_child (widget_class, IdePrimaryWorkspace, run_button);
  gtk_widget_class_bind_template_child (widget_class, IdePrimaryWorkspace, statusbar);
  gtk_widget_class_bind_template_child (widget_class, IdePrimaryWorkspace, sidebar_menu_button);
  gtk_widget_class_bind_template_callback (widget_class, _ide_workspace_adopt_widget);
  gtk_widget_class_bind_template_callback (widget_class, invert_boolean);

  _ide_workspace_class_bind_template_dock (widget_class, G_STRUCT_OFFSET (IdePrimaryWorkspace, dock));

  ide_workspace_class_install_action (workspace_class, "panel.toggle-start", NULL, toggle_panel_action);
  ide_workspace_class_install_action (workspace_class, "panel.toggle-end", NULL, toggle_panel_action);
  ide_workspace_class_install_action (workspace_class, "panel.toggle-bottom", NULL, toggle_panel_action);

  g_type_ensure (IDE_TYPE_GRID);
  g_type_ensure (IDE_TYPE_NOTIFICATIONS_BUTTON);
  g_type_ensure (IDE_TYPE_OMNI_BAR);
  g_type_ensure (IDE_TYPE_RUN_BUTTON);
}

static void
ide_primary_workspace_init (IdePrimaryWorkspace *self)
{
  GtkPopover *popover;
  GMenu *menu;

  ide_workspace_set_id (IDE_WORKSPACE (self), "primary");

  gtk_widget_init_template (GTK_WIDGET (self));

  menu = ide_application_get_menu_by_id (IDE_APPLICATION_DEFAULT, "new-document-menu");
  gtk_menu_button_set_menu_model (self->add_button, G_MENU_MODEL (menu));

  menu = ide_application_get_menu_by_id (IDE_APPLICATION_DEFAULT, "ide-primary-workspace-menu");
  gtk_menu_button_set_menu_model (self->sidebar_menu_button, G_MENU_MODEL (menu));

  menu = ide_application_get_menu_by_id (IDE_APPLICATION_DEFAULT, "build-menu");
  ide_joined_menu_append_menu (self->build_menu, G_MENU_MODEL (menu));

  popover = gtk_menu_button_get_popover (self->sidebar_menu_button);
  ide_header_bar_setup_menu (GTK_POPOVER_MENU (popover));
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
