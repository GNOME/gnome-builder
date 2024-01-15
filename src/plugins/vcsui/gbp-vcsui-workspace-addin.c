/* gbp-vcsui-workspace-addin.c
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

#define G_LOG_DOMAIN "gbp-vcsui-workspace-addin"

#include "config.h"

#include <glib/gi18n.h>

#include <libide-gui.h>
#include <libide-greeter.h>

#include "gbp-vcsui-clone-page.h"
#include "gbp-vcsui-switcher-popover.h"
#include "gbp-vcsui-workspace-addin.h"

struct _GbpVcsuiWorkspaceAddin
{
  GObject              parent_instance;

  GbpVcsuiClonePage   *clone;

  GtkMenuButton       *branch_button;
  GtkLabel            *branch_label;
  GBindingGroup       *vcs_bindings;
};

static gboolean
greeter_open_project_cb (GbpVcsuiWorkspaceAddin *self,
                         IdeProjectInfo         *project_info,
                         IdeGreeterWorkspace    *greeter)
{
  const char *vcs_uri;

  IDE_ENTRY;

  g_assert (GBP_IS_VCSUI_WORKSPACE_ADDIN (self));
  g_assert (IDE_IS_PROJECT_INFO (project_info));
  g_assert (IDE_IS_GREETER_WORKSPACE (greeter));

  if (!ide_project_info_get_file (project_info) &&
      !ide_project_info_get_directory (project_info) &&
      (vcs_uri = ide_project_info_get_vcs_uri (project_info)))
    {
      gbp_vcsui_clone_page_set_uri (self->clone, vcs_uri);
      ide_greeter_workspace_push_page_by_tag (greeter, "clone");
      IDE_RETURN (TRUE);
    }

  IDE_RETURN (FALSE);
}

static void
gbp_vcsui_workspace_addin_load (IdeWorkspaceAddin *addin,
                                IdeWorkspace      *workspace)
{
  GbpVcsuiWorkspaceAddin *self = (GbpVcsuiWorkspaceAddin *)addin;

  IDE_ENTRY;

  g_assert (GBP_IS_VCSUI_WORKSPACE_ADDIN (self));

  if (IDE_IS_GREETER_WORKSPACE (workspace))
    {
      g_signal_connect_object (workspace,
                               "open-project",
                               G_CALLBACK (greeter_open_project_cb),
                               self,
                               G_CONNECT_AFTER | G_CONNECT_SWAPPED);
      self->clone = g_object_new (GBP_TYPE_VCSUI_CLONE_PAGE,
                                  NULL);
      ide_greeter_workspace_add_page (IDE_GREETER_WORKSPACE (workspace),
                                      ADW_NAVIGATION_PAGE (self->clone));
      ide_greeter_workspace_add_button (IDE_GREETER_WORKSPACE (workspace),
                                        g_object_new (GTK_TYPE_BUTTON,
                                                      "label", _("_Clone Repository…"),
                                                      "action-name", "greeter.page",
                                                      "action-target", g_variant_new_string ("clone"),
                                                      "use-underline", TRUE,
                                                      NULL),
                                        100);
    }
  else if (IDE_IS_PRIMARY_WORKSPACE (workspace))
    {
      PanelStatusbar *statusbar;
      IdeWorkbench *workbench;
#if 0
      GtkWidget *popover;
#endif
      GtkImage *image;
      GtkBox *box;

      workbench = ide_workspace_get_workbench (workspace);
      statusbar = ide_workspace_get_statusbar (workspace);

      box = g_object_new (GTK_TYPE_BOX,
                          "orientation", GTK_ORIENTATION_HORIZONTAL,
                          "spacing", 6,
                          NULL);
      image = g_object_new (GTK_TYPE_IMAGE,
                            "icon-name", "builder-vcs-branch-symbolic",
                            "pixel-size", 16,
                            NULL);
      self->branch_label = g_object_new (GTK_TYPE_LABEL,
                                         "xalign", .0f,
                                         "ellipsize", PANGO_ELLIPSIZE_START,
                                         NULL);
      gtk_box_append (box, GTK_WIDGET (image));
      gtk_box_append (box, GTK_WIDGET (self->branch_label));

#if 0
      popover = gbp_vcsui_switcher_popover_new ();
      g_object_bind_property (workbench, "vcs",
                              popover, "vcs",
                              G_BINDING_SYNC_CREATE);

      self->branch_button = g_object_new (GTK_TYPE_MENU_BUTTON,
                                          "child", box,
                                          "direction", GTK_ARROW_UP,
                                          "popover", popover,
                                          NULL);
      panel_statusbar_add_prefix (statusbar, G_MININT, GTK_WIDGET (self->branch_button));
#else
      panel_statusbar_add_prefix (statusbar, G_MININT, GTK_WIDGET (box));
#endif

      self->vcs_bindings = g_binding_group_new ();
      g_binding_group_bind (self->vcs_bindings, "branch-name",
                              self->branch_label, "label",
                              G_BINDING_SYNC_CREATE);
      g_object_bind_property (workbench, "vcs",
                              self->vcs_bindings, "source",
                              G_BINDING_SYNC_CREATE);
    }

  IDE_EXIT;
}

static void
gbp_vcsui_workspace_addin_unload (IdeWorkspaceAddin *addin,
                                  IdeWorkspace      *workspace)
{
  GbpVcsuiWorkspaceAddin *self = (GbpVcsuiWorkspaceAddin *)addin;

  IDE_ENTRY;

  g_assert (GBP_IS_VCSUI_WORKSPACE_ADDIN (self));

  if (IDE_IS_GREETER_WORKSPACE (workspace))
    {
      ide_greeter_workspace_remove_page (IDE_GREETER_WORKSPACE (workspace),
                                         ADW_NAVIGATION_PAGE (self->clone));
      self->clone = NULL;
    }
  else if (IDE_IS_PRIMARY_WORKSPACE (workspace))
    {
      g_clear_object (&self->vcs_bindings);
    }

  IDE_EXIT;
}

static void
workspace_addin_iface_init (IdeWorkspaceAddinInterface *iface)
{
  iface->load = gbp_vcsui_workspace_addin_load;
  iface->unload = gbp_vcsui_workspace_addin_unload;
}

G_DEFINE_FINAL_TYPE_WITH_CODE (GbpVcsuiWorkspaceAddin, gbp_vcsui_workspace_addin, G_TYPE_OBJECT,
                               G_IMPLEMENT_INTERFACE (IDE_TYPE_WORKSPACE_ADDIN, workspace_addin_iface_init))

static void
gbp_vcsui_workspace_addin_class_init (GbpVcsuiWorkspaceAddinClass *klass)
{
  g_type_ensure (GBP_TYPE_VCSUI_SWITCHER_POPOVER);
}

static void
gbp_vcsui_workspace_addin_init (GbpVcsuiWorkspaceAddin *self)
{
}
