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

#include "ide-gui-global.h"
#include "ide-gui-private.h"
#include "ide-header-bar.h"
#include "ide-omni-bar.h"
#include "ide-primary-workspace.h"
#include "ide-run-button.h"
#include "ide-surface.h"
#include "ide-window-settings-private.h"

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
 *
 * Since: 3.32
 */

struct _IdePrimaryWorkspace
{
  IdeWorkspace       parent_instance;

  /* Template widgets */
  IdeHeaderBar       *header_bar;
  DzlMenuButton      *surface_menu_button;
  IdeRunButton       *run_button;
  GtkLabel           *project_title;
  DzlShortcutTooltip *search_tooltip;
};

G_DEFINE_TYPE (IdePrimaryWorkspace, ide_primary_workspace, IDE_TYPE_WORKSPACE)

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
    g_object_bind_property (project_info, "name", self->project_title, "label",
                            G_BINDING_SYNC_CREATE);
}

static void
ide_primary_workspace_surface_set (IdeWorkspace *workspace,
                                   IdeSurface   *surface)
{
  IdePrimaryWorkspace *self = (IdePrimaryWorkspace *)workspace;

  g_assert (IDE_IS_PRIMARY_WORKSPACE (self));
  g_assert (!surface || IDE_IS_SURFACE (surface));

  if (DZL_IS_DOCK_ITEM (surface))
    {
      g_autofree gchar *icon_name = NULL;

      icon_name = dzl_dock_item_get_icon_name (DZL_DOCK_ITEM (surface));
      g_object_set (self->surface_menu_button,
                    "icon-name", icon_name,
                    NULL);
    }

  IDE_WORKSPACE_CLASS (ide_primary_workspace_parent_class)->surface_set (workspace, surface);
}

static void
ide_primary_workspace_class_init (IdePrimaryWorkspaceClass *klass)
{
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);
  IdeWorkspaceClass *workspace_class = IDE_WORKSPACE_CLASS (klass);

  ide_workspace_class_set_kind (workspace_class, "primary");

  workspace_class->surface_set = ide_primary_workspace_surface_set;
  workspace_class->context_set = ide_primary_workspace_context_set;

  gtk_widget_class_set_template_from_resource (widget_class, "/org/gnome/libide-gui/ui/ide-primary-workspace.ui");
  gtk_widget_class_bind_template_child (widget_class, IdePrimaryWorkspace, header_bar);
  gtk_widget_class_bind_template_child (widget_class, IdePrimaryWorkspace, project_title);
  gtk_widget_class_bind_template_child (widget_class, IdePrimaryWorkspace, run_button);
  gtk_widget_class_bind_template_child (widget_class, IdePrimaryWorkspace, search_tooltip);
  gtk_widget_class_bind_template_child (widget_class, IdePrimaryWorkspace, surface_menu_button);

  g_type_ensure (IDE_TYPE_RUN_BUTTON);
}

static void
ide_primary_workspace_init (IdePrimaryWorkspace *self)
{
  gtk_widget_init_template (GTK_WIDGET (self));

  _ide_primary_workspace_init_actions (self);
  _ide_window_settings_register (GTK_WINDOW (self));
}
