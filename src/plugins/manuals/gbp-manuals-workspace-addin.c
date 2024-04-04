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

#include "gbp-manuals-application-addin.h"
#include "gbp-manuals-panel.h"
#include "gbp-manuals-workspace-addin.h"

struct _GbpManualsWorkspaceAddin
{
  GObject          parent_instance;
  IdeWorkspace    *workspace;
  GbpManualsPanel *panel;
};

static DexFuture *
gbp_manuals_workspace_addin_repository_loaded_cb (DexFuture *completed,
                                                  gpointer   user_data)
{
  GbpManualsWorkspaceAddin *self = user_data;
  g_autoptr(ManualsRepository) repository = NULL;

  g_assert (DEX_IS_FUTURE (completed));
  g_assert (GBP_IS_MANUALS_WORKSPACE_ADDIN (self));

  if (self->panel == NULL)
    return NULL;

  repository = dex_await_object (dex_ref (completed), NULL);
  gbp_manuals_panel_set_repository (self->panel, repository);

  return NULL;
}

static void
gbp_manuals_workspace_addin_load (IdeWorkspaceAddin *addin,
                                  IdeWorkspace      *workspace)
{
  GbpManualsWorkspaceAddin *self = (GbpManualsWorkspaceAddin *)addin;
  g_autoptr(PanelPosition) position = NULL;
  IdeApplicationAddin *app_addin;
  DexFuture *future;

  g_assert (GBP_IS_MANUALS_WORKSPACE_ADDIN (self));
  g_assert (IDE_IS_WORKSPACE (workspace));

  app_addin = ide_application_find_addin_by_module_name (IDE_APPLICATION_DEFAULT, "manuals");

  g_assert (GBP_IS_MANUALS_APPLICATION_ADDIN (app_addin));

  self->workspace = workspace;
  self->panel = gbp_manuals_panel_new ();

  position = panel_position_new ();
  panel_position_set_area (position, PANEL_AREA_START);

  ide_workspace_add_pane (workspace, IDE_PANE (self->panel), position);

  future = gbp_manuals_application_addin_load_repository (GBP_MANUALS_APPLICATION_ADDIN (app_addin));
  future = dex_future_then (future,
                            gbp_manuals_workspace_addin_repository_loaded_cb,
                            g_object_ref (self),
                            g_object_unref);
  dex_future_disown (future);
}

static void
gbp_manuals_workspace_addin_unload (IdeWorkspaceAddin *addin,
                                    IdeWorkspace      *workspace)
{
  GbpManualsWorkspaceAddin *self = (GbpManualsWorkspaceAddin *)addin;

  g_assert (GBP_IS_MANUALS_WORKSPACE_ADDIN (addin));
  g_assert (IDE_IS_WORKSPACE (workspace));

  g_clear_pointer ((IdePane **)&self->panel, ide_pane_destroy);

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
