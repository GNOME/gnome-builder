/* gbp-vim-workspace-addin.c
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

#define G_LOG_DOMAIN "gbp-vim-workspace-addin"

#include "config.h"

#include <gtk/gtk.h>
#include <libpanel.h>

#include <libide-gui.h>

#include "gbp-vim-workspace-addin.h"

struct _GbpVimWorkspaceAddin
{
  GObject parent_instance;

  GtkLabel *command_bar;
  GtkLabel *command;
};

static void
gbp_vim_workspace_addin_load (IdeWorkspaceAddin *addin,
                              IdeWorkspace      *workspace)
{
  GbpVimWorkspaceAddin *self = (GbpVimWorkspaceAddin *)addin;
  PanelStatusbar *statusbar;
  PangoAttrList *attrs;

  IDE_ENTRY;

  g_assert (IDE_IS_WORKSPACE_ADDIN (self));
  g_assert (IDE_IS_WORKSPACE (workspace));

  attrs = pango_attr_list_new ();
  pango_attr_list_insert (attrs, pango_attr_family_new ("Monospace"));

  self->command_bar = g_object_new (GTK_TYPE_LABEL,
                                    "attributes", attrs,
                                    "hexpand", TRUE,
                                    "selectable", TRUE,
                                    "xalign", .0f,
                                    NULL);
  self->command = g_object_new (GTK_TYPE_LABEL,
                                "attributes", attrs,
                                "visible", FALSE,
                                "xalign", 1.f,
                                NULL);

  statusbar = ide_workspace_get_statusbar (workspace);

  /* TODO: priorities for packing */
  panel_statusbar_add_prefix (statusbar, GTK_WIDGET (self->command_bar));
  panel_statusbar_add_prefix (statusbar, GTK_WIDGET (self->command));

  pango_attr_list_unref (attrs);

  IDE_EXIT;
}

static void
gbp_vim_workspace_addin_unload (IdeWorkspaceAddin *addin,
                                IdeWorkspace      *workspace)
{
  GbpVimWorkspaceAddin *self = (GbpVimWorkspaceAddin *)addin;
  PanelStatusbar *statusbar;

  IDE_ENTRY;

  g_assert (IDE_IS_WORKSPACE_ADDIN (self));
  g_assert (IDE_IS_WORKSPACE (workspace));

  statusbar = ide_workspace_get_statusbar (workspace);

  panel_statusbar_remove (statusbar, GTK_WIDGET (self->command_bar));
  panel_statusbar_remove (statusbar, GTK_WIDGET (self->command));

  self->command_bar = NULL;
  self->command = NULL;

  IDE_EXIT;
}

static void
workspace_addin_iface_init (IdeWorkspaceAddinInterface *iface)
{
  iface->load = gbp_vim_workspace_addin_load;
  iface->unload = gbp_vim_workspace_addin_unload;
}

G_DEFINE_FINAL_TYPE_WITH_CODE (GbpVimWorkspaceAddin, gbp_vim_workspace_addin, G_TYPE_OBJECT,
                               G_IMPLEMENT_INTERFACE (IDE_TYPE_WORKSPACE_ADDIN, workspace_addin_iface_init))

static void
gbp_vim_workspace_addin_class_init (GbpVimWorkspaceAddinClass *klass)
{
}

static void
gbp_vim_workspace_addin_init (GbpVimWorkspaceAddin *self)
{
}

void
gbp_vim_workspace_addin_set_command (GbpVimWorkspaceAddin *self,
                                     const char           *command)
{
  g_return_if_fail (GBP_IS_VIM_WORKSPACE_ADDIN (self));

  gtk_label_set_label (self->command, command);
  gtk_widget_set_visible (GTK_WIDGET (self->command), command && *command);
}

void
gbp_vim_workspace_addin_set_command_bar (GbpVimWorkspaceAddin *self,
                                         const char           *command_bar)
{
  g_return_if_fail (GBP_IS_VIM_WORKSPACE_ADDIN (self));

  if (self->command_bar != NULL)
    gtk_label_set_label (self->command_bar, command_bar);
}
