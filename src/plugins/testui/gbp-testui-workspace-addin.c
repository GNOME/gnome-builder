/* gbp-testui-workspace-addin.c
 *
 * Copyright 2022 Christian Hergert <chergert@redhat.com>
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

#define G_LOG_DOMAIN "gbp-testui-workspace-addin"

#include "config.h"

#include <libide-foundry.h>
#include <libide-gui.h>

#include "gbp-testui-output-panel.h"
#include "gbp-testui-panel.h"
#include "gbp-testui-workspace-addin.h"

struct _GbpTestuiWorkspaceAddin
{
  GObject               parent_instance;
  IdeWorkspace         *workspace;
  GbpTestuiPanel       *panel;
  GbpTestuiOutputPanel *output_panel;
};

static void
on_test_activated_cb (GbpTestuiWorkspaceAddin *self,
                      IdeTest                 *test,
                      GbpTestuiPanel          *panel)
{
  IdeTestManager *test_manager;
  IdeContext *context;

  IDE_ENTRY;

  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (GBP_IS_TESTUI_WORKSPACE_ADDIN (self));
  g_assert (IDE_IS_TEST (test));
  g_assert (GBP_IS_TESTUI_PANEL (panel));

  gbp_testui_output_panel_reset (self->output_panel);
  panel_widget_raise (PANEL_WIDGET (self->output_panel));

  context = ide_workspace_get_context (self->workspace);
  test_manager = ide_test_manager_from_context (context);
  ide_test_manager_run_async (test_manager, test, NULL, NULL, NULL);

  IDE_EXIT;
}

static void
gbp_testui_workspace_addin_load (IdeWorkspaceAddin *addin,
                                 IdeWorkspace      *workspace)
{
  GbpTestuiWorkspaceAddin *self = (GbpTestuiWorkspaceAddin *)addin;
  g_autoptr(IdePanelPosition) position = NULL;
  g_autoptr(IdePanelPosition) output_position = NULL;
  IdeTestManager *test_manager;
  IdeContext *context;
  VtePty *pty;

  IDE_ENTRY;

  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (GBP_IS_TESTUI_WORKSPACE_ADDIN (self));
  g_assert (IDE_IS_PRIMARY_WORKSPACE (workspace));

  self->workspace = workspace;

  context = ide_workspace_get_context (workspace);
  test_manager = ide_test_manager_from_context (context);
  pty = ide_test_manager_get_pty (test_manager);

  self->panel = gbp_testui_panel_new (test_manager);
  g_signal_connect_object (self->panel,
                           "test-activated",
                           G_CALLBACK (on_test_activated_cb),
                           self,
                           G_CONNECT_SWAPPED);
  position = ide_panel_position_new ();
  ide_panel_position_set_edge (position, PANEL_DOCK_POSITION_END);
  ide_workspace_add_pane (workspace, IDE_PANE (self->panel), position);

  self->output_panel = gbp_testui_output_panel_new (pty);
  output_position = ide_panel_position_new ();
  ide_panel_position_set_edge (output_position, PANEL_DOCK_POSITION_BOTTOM);
  ide_workspace_add_pane (workspace, IDE_PANE (self->output_panel), output_position);

  IDE_EXIT;
}

static void
gbp_testui_workspace_addin_unload (IdeWorkspaceAddin *addin,
                                   IdeWorkspace      *workspace)
{
  GbpTestuiWorkspaceAddin *self = (GbpTestuiWorkspaceAddin *)addin;

  IDE_ENTRY;

  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (GBP_IS_TESTUI_WORKSPACE_ADDIN (self));
  g_assert (IDE_IS_PRIMARY_WORKSPACE (workspace));

  g_clear_pointer ((IdePane **)&self->panel, ide_pane_destroy);
  g_clear_pointer ((IdePane **)&self->output_panel, ide_pane_destroy);

  self->workspace = NULL;

  IDE_EXIT;
}

static void
workspace_addin_iface_init (IdeWorkspaceAddinInterface *iface)
{
  iface->load = gbp_testui_workspace_addin_load;
  iface->unload = gbp_testui_workspace_addin_unload;
}

G_DEFINE_FINAL_TYPE_WITH_CODE (GbpTestuiWorkspaceAddin, gbp_testui_workspace_addin, G_TYPE_OBJECT,
                               G_IMPLEMENT_INTERFACE (IDE_TYPE_WORKSPACE_ADDIN, workspace_addin_iface_init))

static void
gbp_testui_workspace_addin_class_init (GbpTestuiWorkspaceAddinClass *klass)
{
}

static void
gbp_testui_workspace_addin_init (GbpTestuiWorkspaceAddin *self)
{
}
