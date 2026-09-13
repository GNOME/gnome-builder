/* gbp-acp-workspace-addin.c
 *
 * Copyright 2026 GNOME Builder contributors
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

#define G_LOG_DOMAIN "gbp-acp-workspace-addin"

#include "config.h"

#include <glib/gi18n.h>

#include <libide-gui.h>

#include "gbp-acp-pane.h"
#include "gbp-acp-workspace-addin.h"

struct _GbpAcpWorkspaceAddin
{
  GObject     parent_instance;
  GbpAcpPane *pane;
};

static void
gbp_acp_workspace_addin_load (IdeWorkspaceAddin *addin,
                              IdeWorkspace      *workspace)
{
  GbpAcpWorkspaceAddin *self = (GbpAcpWorkspaceAddin *)addin;
  g_autoptr(PanelPosition) position = NULL;
  IdeContext *context;

  g_assert (GBP_IS_ACP_WORKSPACE_ADDIN (self));
  g_assert (IDE_IS_WORKSPACE (workspace));

  if (!(context = ide_workspace_get_context (workspace)))
    return;

  ide_pane_observe (IDE_PANE (gbp_acp_pane_new (context)),
                    (IdePane **)&self->pane);

  if (self->pane == NULL)
    return;

  position = panel_position_new ();
  panel_position_set_area (position, PANEL_AREA_END);
  panel_position_set_row (position, 0);
  panel_position_set_depth (position, 0);

  ide_workspace_add_pane (workspace, IDE_PANE (self->pane), position);

  panel_widget_raise (PANEL_WIDGET (self->pane));
}

static void
gbp_acp_workspace_addin_unload (IdeWorkspaceAddin *addin,
                                IdeWorkspace      *workspace)
{
  GbpAcpWorkspaceAddin *self = (GbpAcpWorkspaceAddin *)addin;

  g_assert (GBP_IS_ACP_WORKSPACE_ADDIN (self));
  g_assert (IDE_IS_WORKSPACE (workspace));

  ide_clear_pane ((IdePane **)&self->pane);
}

static void
workspace_addin_iface_init (IdeWorkspaceAddinInterface *iface)
{
  iface->load = gbp_acp_workspace_addin_load;
  iface->unload = gbp_acp_workspace_addin_unload;
}

G_DEFINE_FINAL_TYPE_WITH_CODE (GbpAcpWorkspaceAddin, gbp_acp_workspace_addin, G_TYPE_OBJECT,
                               G_IMPLEMENT_INTERFACE (IDE_TYPE_WORKSPACE_ADDIN, workspace_addin_iface_init))

static void
gbp_acp_workspace_addin_class_init (GbpAcpWorkspaceAddinClass *klass)
{
}

static void
gbp_acp_workspace_addin_init (GbpAcpWorkspaceAddin *self)
{
}
