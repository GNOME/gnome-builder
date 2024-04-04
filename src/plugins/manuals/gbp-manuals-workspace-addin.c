/* gbp-manuals-workspace-addin.c
 *
 * Copyright 2024 Christian Hergert <chergert@redhat.com>
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

#include "config.h"

#include "gbp-manuals-workspace-addin.h"

struct _GbpManualsWorkspaceAddin
{
  GObject       parent_instance;
  IdeWorkspace *workspace;
};

static void
gbp_manuals_workspace_addin_load (IdeWorkspaceAddin *addin,
                                  IdeWorkspace      *workspace)
{
  GbpManualsWorkspaceAddin *self = (GbpManualsWorkspaceAddin *)addin;

  g_assert (GBP_IS_MANUALS_WORKSPACE_ADDIN (self));
  g_assert (IDE_IS_WORKSPACE (workspace));

  self->workspace = workspace;
}

static void
gbp_manuals_workspace_addin_unload (IdeWorkspaceAddin *addin,
                                    IdeWorkspace      *workspace)
{
  GbpManualsWorkspaceAddin *self = (GbpManualsWorkspaceAddin *)addin;

  g_assert (GBP_IS_MANUALS_WORKSPACE_ADDIN (addin));
  g_assert (IDE_IS_WORKSPACE (workspace));

  self->workspace = NULL;
}

static void
workspace_addin_iface_init (IdeWorkspaceAddinInterface *iface)
{
  iface->load = gbp_manuals_workspace_addin_load;
  iface->unload = gbp_manuals_workspace_addin_unload;
}

G_DEFINE_FINAL_TYPE_WITH_CODE (GbpManualsWorkspaceAddin, gbp_manuals_workspace_addin, G_TYPE_OBJECT,
                               G_IMPLEMENT_INTERFACE (IDE_TYPE_WORKSPACE_ADDIN, workspace_addin_iface_init))

static void
gbp_manuals_workspace_addin_class_init (GbpManualsWorkspaceAddinClass *klass)
{
}

static void
gbp_manuals_workspace_addin_init (GbpManualsWorkspaceAddin *self)
{

}
