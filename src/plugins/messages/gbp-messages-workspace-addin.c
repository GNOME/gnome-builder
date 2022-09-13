/* gbp-messages-workspace-addin.c
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

#define G_LOG_DOMAIN "gbp-messages-workspace-addin"

#include <libide-gui.h>

#include "gbp-messages-workspace-addin.h"
#include "gbp-messages-panel.h"

struct _GbpMessagesWorkspaceAddin
{
  GObject           parent_instance;
  GbpMessagesPanel *panel;
};

static void
gbp_messages_workspace_addin_load (IdeWorkspaceAddin *addin,
                                   IdeWorkspace      *workspace)
{
  GbpMessagesWorkspaceAddin *self = (GbpMessagesWorkspaceAddin *)addin;
  g_autoptr(PanelPosition) position = NULL;

  g_assert (GBP_IS_MESSAGES_WORKSPACE_ADDIN (self));
  g_assert (IDE_IS_WORKSPACE (workspace));

  position = panel_position_new ();
  panel_position_set_area (position, PANEL_AREA_BOTTOM);

  self->panel = g_object_new (GBP_TYPE_MESSAGES_PANEL, NULL);
  ide_workspace_add_pane (workspace, IDE_PANE (self->panel), position);
}

static void
gbp_messages_workspace_addin_unload (IdeWorkspaceAddin *addin,
                                     IdeWorkspace      *workspace)
{
  GbpMessagesWorkspaceAddin *self = (GbpMessagesWorkspaceAddin *)addin;
  GtkWidget *frame;

  g_assert (GBP_IS_MESSAGES_WORKSPACE_ADDIN (self));
  g_assert (IDE_IS_WORKSPACE (workspace));

  frame = gtk_widget_get_ancestor (GTK_WIDGET (self->panel), PANEL_TYPE_FRAME);
  panel_frame_remove (PANEL_FRAME (frame), PANEL_WIDGET (self->panel));

  self->panel = NULL;
}

static void
workspace_addin_iface_init (IdeWorkspaceAddinInterface *iface)
{
  iface->load = gbp_messages_workspace_addin_load;
  iface->unload = gbp_messages_workspace_addin_unload;
}

G_DEFINE_FINAL_TYPE_WITH_CODE (GbpMessagesWorkspaceAddin, gbp_messages_workspace_addin, G_TYPE_OBJECT,
                               G_IMPLEMENT_INTERFACE (IDE_TYPE_WORKSPACE_ADDIN, workspace_addin_iface_init))

static void
gbp_messages_workspace_addin_class_init (GbpMessagesWorkspaceAddinClass *klass)
{
}

static void
gbp_messages_workspace_addin_init (GbpMessagesWorkspaceAddin *self)
{
}
