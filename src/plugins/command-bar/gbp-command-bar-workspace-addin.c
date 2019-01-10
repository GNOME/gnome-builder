/* gbp-command-bar-workspace-addin.c
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

#define G_LOG_DOMAIN "gbp-command-bar-workspace-addin"

#include "config.h"

#include <libide-editor.h>
#include <libide-gui.h>
#include <libide-terminal.h>

#include "gbp-command-bar.h"
#include "gbp-command-bar-workspace-addin.h"

struct _GbpCommandBarWorkspaceAddin
{
  GObject        parent_instance;
  GbpCommandBar *command_bar;
};

static gboolean
position_command_bar_cb (GbpCommandBarWorkspaceAddin *self,
                         GtkWidget                   *child,
                         GdkRectangle                *area,
                         GtkOverlay                  *overlay)
{
  GtkRequisition min, nat;

  g_assert (GBP_IS_COMMAND_BAR_WORKSPACE_ADDIN (self));
  g_assert (GTK_IS_WIDGET (child));

  if (!GBP_IS_COMMAND_BAR (child))
    return FALSE;

  gtk_widget_get_allocation (GTK_WIDGET (overlay), area);
  gtk_widget_get_preferred_size (child, &min, &nat);

  area->x = (area->width - nat.width) / 2;
  area->y = 100;
  area->width = nat.width;
  area->height = nat.height;

  return TRUE;
}

static void
gbp_command_bar_workspace_addin_dismiss_command_bar (GSimpleAction *action,
                                                     GVariant      *param,
                                                     gpointer       user_data)
{
  GbpCommandBarWorkspaceAddin *self = user_data;

  g_assert (G_IS_SIMPLE_ACTION (action));
  g_assert (GBP_IS_COMMAND_BAR_WORKSPACE_ADDIN (self));

  if (self->command_bar)
    gbp_command_bar_dismiss (self->command_bar);
}

static void
gbp_command_bar_workspace_addin_reveal_command_bar (GSimpleAction *action,
                                                    GVariant      *param,
                                                    gpointer       user_data)
{
  GbpCommandBarWorkspaceAddin *self = user_data;

  g_assert (G_IS_SIMPLE_ACTION (action));
  g_assert (GBP_IS_COMMAND_BAR_WORKSPACE_ADDIN (self));

  if (self->command_bar)
    gbp_command_bar_reveal (self->command_bar);
}

static const GActionEntry entries[] = {
  { "dismiss-command-bar", gbp_command_bar_workspace_addin_dismiss_command_bar },
  { "reveal-command-bar", gbp_command_bar_workspace_addin_reveal_command_bar },
};

static void
gbp_command_bar_workspace_addin_load (IdeWorkspaceAddin *addin,
                                      IdeWorkspace      *workspace)
{
  GbpCommandBarWorkspaceAddin *self = (GbpCommandBarWorkspaceAddin *)addin;
  GtkOverlay *overlay;

  g_assert (IDE_IS_WORKSPACE_ADDIN (self));
  g_assert (IDE_IS_PRIMARY_WORKSPACE (workspace) ||
            IDE_IS_EDITOR_WORKSPACE (workspace) ||
            IDE_IS_TERMINAL_WORKSPACE (workspace));

  self->command_bar = g_object_new (GBP_TYPE_COMMAND_BAR,
                                    "hexpand", TRUE,
                                    "valign", GTK_ALIGN_END,
                                    "visible", FALSE,
                                    NULL);
  overlay = ide_workspace_get_overlay (workspace);
  g_signal_connect_object (overlay,
                           "get-child-position",
                           G_CALLBACK (position_command_bar_cb),
                           self,
                           G_CONNECT_SWAPPED);
  gtk_overlay_add_overlay (overlay, GTK_WIDGET (self->command_bar));

  /* Add actions for shortcuts to activate */
  g_action_map_add_action_entries (G_ACTION_MAP (workspace),
                                   entries,
                                   G_N_ELEMENTS (entries),
                                   self);
}

static void
gbp_command_bar_workspace_addin_unload (IdeWorkspaceAddin *addin,
                                        IdeWorkspace      *workspace)
{
  GbpCommandBarWorkspaceAddin *self = (GbpCommandBarWorkspaceAddin *)addin;

  g_assert (IDE_IS_WORKSPACE_ADDIN (self));
  g_assert (IDE_IS_PRIMARY_WORKSPACE (workspace) ||
            IDE_IS_EDITOR_WORKSPACE (workspace) ||
            IDE_IS_TERMINAL_WORKSPACE (workspace));

  /* Remove all the actions we added */
  for (guint i = 0; i < G_N_ELEMENTS (entries); i++)
    g_action_map_remove_action (G_ACTION_MAP (workspace), entries[i].name);

  if (self->command_bar != NULL)
    gtk_widget_destroy (GTK_WIDGET (self->command_bar));
}

static void
workspace_addin_iface_init (IdeWorkspaceAddinInterface *iface)
{
  iface->load = gbp_command_bar_workspace_addin_load;
  iface->unload = gbp_command_bar_workspace_addin_unload;
}

G_DEFINE_TYPE_WITH_CODE (GbpCommandBarWorkspaceAddin, gbp_command_bar_workspace_addin, G_TYPE_OBJECT,
                         G_IMPLEMENT_INTERFACE (IDE_TYPE_WORKSPACE_ADDIN, workspace_addin_iface_init))

static void
gbp_command_bar_workspace_addin_class_init (GbpCommandBarWorkspaceAddinClass *klass)
{
}

static void
gbp_command_bar_workspace_addin_init (GbpCommandBarWorkspaceAddin *self)
{
}
