/* gbp-grep-workspace-addin.c
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

#define G_LOG_DOMAIN "gbp-grep-workspace-addin"

#include "config.h"

#include <glib/gi18n.h>

#include <libide-foundry.h>
#include <libide-gui.h>

#include "gbp-grep-workspace-addin.h"
#include "gbp-grep-panel.h"

#define I_(s) g_intern_static_string(s)

struct _GbpGrepWorkspaceAddin
{
  GObject    parent_instance;
  GtkWidget *panel;
};

static void
gbp_grep_workspace_page_addin_show_project_panel_action (GSimpleAction *action,
                                                         GVariant      *variant,
                                                         gpointer       user_data)
{
  GbpGrepWorkspaceAddin *self = GBP_GREP_WORKSPACE_ADDIN (user_data);

  g_assert (GBP_IS_GREP_WORKSPACE_ADDIN (self));

  panel_widget_raise (PANEL_WIDGET (self->panel));
}

static const GActionEntry actions[] = {
  { "show-project-panel", gbp_grep_workspace_page_addin_show_project_panel_action },
};

static void
gbp_grep_workspace_addin_load (IdeWorkspaceAddin *addin,
                               IdeWorkspace      *workspace)
{
  GbpGrepWorkspaceAddin *self = (GbpGrepWorkspaceAddin *)addin;
  g_autoptr(GSimpleActionGroup) group = NULL;
  g_autoptr(IdePanelPosition) position = NULL;

  g_assert (GBP_IS_GREP_WORKSPACE_ADDIN (self));
  g_assert (IDE_IS_WORKSPACE (workspace));

  self->panel = gbp_grep_panel_new ();

  position = ide_panel_position_new ();
  ide_panel_position_set_edge (position, PANEL_DOCK_POSITION_BOTTOM);
  ide_workspace_add_pane (workspace, IDE_PANE (self->panel), position);

  group = g_simple_action_group_new ();
  g_action_map_add_action_entries (G_ACTION_MAP (group),
                                   actions,
                                   G_N_ELEMENTS (actions),
                                   self);
  gtk_widget_insert_action_group (GTK_WIDGET (workspace),
                                  "grep",
                                  G_ACTION_GROUP (group));
}

static void
gbp_grep_workspace_addin_unload (IdeWorkspaceAddin *addin,
                                 IdeWorkspace      *workspace)
{
  GbpGrepWorkspaceAddin *self = (GbpGrepWorkspaceAddin *)addin;

  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (GBP_IS_GREP_WORKSPACE_ADDIN (self));

  gtk_widget_insert_action_group (GTK_WIDGET (workspace), "grep", NULL);

  g_clear_pointer ((IdePane **)&self->panel, ide_pane_destroy);
}

static void
workspace_addin_iface_init (IdeWorkspaceAddinInterface *iface)
{
  iface->load = gbp_grep_workspace_addin_load;
  iface->unload = gbp_grep_workspace_addin_unload;
}

G_DEFINE_TYPE_WITH_CODE (GbpGrepWorkspaceAddin, gbp_grep_workspace_addin, G_TYPE_OBJECT,
                         G_IMPLEMENT_INTERFACE (IDE_TYPE_WORKSPACE_ADDIN, workspace_addin_iface_init))

static void
gbp_grep_workspace_addin_class_init (GbpGrepWorkspaceAddinClass *klass)
{
}

static void
gbp_grep_workspace_addin_init (GbpGrepWorkspaceAddin *self)
{
}
