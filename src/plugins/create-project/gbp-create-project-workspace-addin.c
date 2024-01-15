/* gbp-create-project-workspace-addin.c
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

#define G_LOG_DOMAIN "gbp-create-project-workspace-addin"

#include "config.h"

#include <glib/gi18n.h>

#include <libide-greeter.h>

#include "gbp-create-project-widget.h"
#include "gbp-create-project-workspace-addin.h"

struct _GbpCreateProjectWorkspaceAddin
{
  GObject                 parent_instance;
  GbpCreateProjectWidget *widget;
};

static void
gbp_create_project_workspace_addin_load (IdeWorkspaceAddin *addin,
                                         IdeWorkspace      *workspace)
{
  GbpCreateProjectWorkspaceAddin *self = (GbpCreateProjectWorkspaceAddin *)addin;

  IDE_ENTRY;

  g_assert (GBP_IS_CREATE_PROJECT_WORKSPACE_ADDIN (self));
  g_assert (IDE_IS_GREETER_WORKSPACE (workspace));

  ide_greeter_workspace_add_button (IDE_GREETER_WORKSPACE (workspace),
                                    g_object_new (GTK_TYPE_BUTTON,
                                                  "action-name", "greeter.page",
                                                  "action-target", g_variant_new_string ("create-project"),
                                                  "label", _("Create _New Project…"),
                                                  "use-underline", TRUE,
                                                  "visible", TRUE,
                                                  NULL),
                                    -10);

  self->widget = g_object_new (GBP_TYPE_CREATE_PROJECT_WIDGET, NULL);
  ide_greeter_workspace_add_page (IDE_GREETER_WORKSPACE (workspace),
                                  ADW_NAVIGATION_PAGE (self->widget));

  IDE_EXIT;
}

static void
gbp_create_project_workspace_addin_unload (IdeWorkspaceAddin *addin,
                                           IdeWorkspace      *workspace)
{
  GbpCreateProjectWorkspaceAddin *self = (GbpCreateProjectWorkspaceAddin *)addin;

  IDE_ENTRY;

  g_assert (GBP_IS_CREATE_PROJECT_WORKSPACE_ADDIN (self));
  g_assert (IDE_IS_GREETER_WORKSPACE (workspace));

  ide_greeter_workspace_remove_page (IDE_GREETER_WORKSPACE (workspace),
                                     ADW_NAVIGATION_PAGE (self->widget));
  self->widget = NULL;

  IDE_EXIT;
}

static void
workspace_addin_iface_init (IdeWorkspaceAddinInterface *iface)
{
  iface->load = gbp_create_project_workspace_addin_load;
  iface->unload = gbp_create_project_workspace_addin_unload;
}

G_DEFINE_FINAL_TYPE_WITH_CODE (GbpCreateProjectWorkspaceAddin, gbp_create_project_workspace_addin, G_TYPE_OBJECT,
                               G_IMPLEMENT_INTERFACE (IDE_TYPE_WORKSPACE_ADDIN, workspace_addin_iface_init))

static void
gbp_create_project_workspace_addin_class_init (GbpCreateProjectWorkspaceAddinClass *klass)
{
}

static void
gbp_create_project_workspace_addin_init (GbpCreateProjectWorkspaceAddin *self)
{
}
