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
#include "gbp-manuals-page.h"
#include "gbp-manuals-panel.h"
#include "gbp-manuals-pathbar.h"
#include "gbp-manuals-workspace-addin.h"

struct _GbpManualsWorkspaceAddin
{
  GObject            parent_instance;

  GBindingGroup     *bindings;

  IdeWorkspace      *workspace;

  GbpManualsPanel   *panel;
  GbpManualsPathbar *pathbar;
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
  PanelStatusbar *statusbar;
  IdeApplicationAddin *app_addin;
  DexFuture *future;

  g_assert (GBP_IS_MANUALS_WORKSPACE_ADDIN (self));
  g_assert (IDE_IS_WORKSPACE (workspace));

  app_addin = ide_application_find_addin_by_module_name (IDE_APPLICATION_DEFAULT, "manuals");

  g_assert (GBP_IS_MANUALS_APPLICATION_ADDIN (app_addin));

  self->workspace = workspace;
  self->panel = gbp_manuals_panel_new ();
  self->pathbar = gbp_manuals_pathbar_new ();

  position = panel_position_new ();
  panel_position_set_area (position, PANEL_AREA_START);

  ide_workspace_add_pane (workspace, IDE_PANE (self->panel), position);

  gtk_widget_set_hexpand (GTK_WIDGET (self->pathbar), TRUE);
  gtk_widget_hide (GTK_WIDGET (self->pathbar));

  statusbar = ide_workspace_get_statusbar (workspace);
  panel_statusbar_add_prefix (statusbar, 10000, GTK_WIDGET (self->pathbar));

  future = gbp_manuals_application_addin_load_repository (GBP_MANUALS_APPLICATION_ADDIN (app_addin));
  future = dex_future_then (future,
                            gbp_manuals_workspace_addin_repository_loaded_cb,
                            g_object_ref (self),
                            g_object_unref);
  dex_future_disown (future);

  self->bindings = g_binding_group_new ();
  g_binding_group_bind (self->bindings, "navigatable",
                        self->pathbar, "navigatable",
                        G_BINDING_SYNC_CREATE);
}

static void
gbp_manuals_workspace_addin_unload (IdeWorkspaceAddin *addin,
                                    IdeWorkspace      *workspace)
{
  GbpManualsWorkspaceAddin *self = (GbpManualsWorkspaceAddin *)addin;
  PanelStatusbar *statusbar;

  g_assert (GBP_IS_MANUALS_WORKSPACE_ADDIN (addin));
  g_assert (IDE_IS_WORKSPACE (workspace));

  statusbar = ide_workspace_get_statusbar (workspace);
  panel_statusbar_remove (statusbar, GTK_WIDGET (self->pathbar));

  g_clear_pointer ((IdePane **)&self->panel, ide_pane_destroy);
  g_clear_object (&self->bindings);

  self->pathbar = NULL;
  self->workspace = NULL;
}

static void
gbp_manuals_workspace_addin_page_changed (IdeWorkspaceAddin *addin,
                                          IdePage           *page)
{
  GbpManualsWorkspaceAddin *self = (GbpManualsWorkspaceAddin *)addin;

  g_assert (GBP_IS_MANUALS_WORKSPACE_ADDIN (self));

  if (!GBP_IS_MANUALS_PAGE (page))
    page = NULL;

  gtk_widget_set_visible (GTK_WIDGET (self->pathbar), GBP_IS_MANUALS_PAGE (page));
  g_binding_group_set_source (self->bindings, page);
}

static void
workspace_addin_iface_init (IdeWorkspaceAddinInterface *iface)
{
  iface->load = gbp_manuals_workspace_addin_load;
  iface->unload = gbp_manuals_workspace_addin_unload;
  iface->page_changed = gbp_manuals_workspace_addin_page_changed;
}

static void
gbp_manuals_workspace_addin_filter_action (GbpManualsWorkspaceAddin *self,
                                           GVariant                 *param)
{
  g_assert (GBP_IS_MANUALS_WORKSPACE_ADDIN (self));

  if (self->panel != NULL)
    {
      panel_widget_raise (PANEL_WIDGET (self->panel));
      gbp_manuals_panel_begin_search (self->panel);
    }
}

IDE_DEFINE_ACTION_GROUP (GbpManualsWorkspaceAddin, gbp_manuals_workspace_addin, {
  { "filter", gbp_manuals_workspace_addin_filter_action },
})

G_DEFINE_FINAL_TYPE_WITH_CODE (GbpManualsWorkspaceAddin, gbp_manuals_workspace_addin, G_TYPE_OBJECT,
                               G_IMPLEMENT_INTERFACE (IDE_TYPE_WORKSPACE_ADDIN, workspace_addin_iface_init)
                               G_IMPLEMENT_INTERFACE (G_TYPE_ACTION_GROUP, gbp_manuals_workspace_addin_init_action_group))

static void
gbp_manuals_workspace_addin_class_init (GbpManualsWorkspaceAddinClass *klass)
{
}

static void
gbp_manuals_workspace_addin_init (GbpManualsWorkspaceAddin *self)
{
}

static void
gbp_manuals_workspace_addin_get_page_cb (IdePage  *page,
                                         gpointer  user_data)
{
  IdePage **out_page = user_data;

  if (*out_page == NULL && GBP_IS_MANUALS_PAGE (page))
    *out_page = page;
}

GbpManualsPage *
gbp_manuals_workspace_addin_get_page (GbpManualsWorkspaceAddin *self)
{
  GbpManualsPage *page = NULL;
  IdePage *mrp;

  g_return_val_if_fail (GBP_IS_MANUALS_WORKSPACE_ADDIN (self), NULL);

  if ((mrp = ide_workspace_get_most_recent_page (self->workspace)) &&
      GBP_IS_MANUALS_PAGE (mrp))
    return GBP_MANUALS_PAGE (mrp);

  ide_workspace_foreach_page (self->workspace,
                              gbp_manuals_workspace_addin_get_page_cb,
                              &page);

  if (page == NULL)
    {
      g_autoptr(PanelPosition) position = panel_position_new ();

      panel_position_set_area (position, PANEL_AREA_CENTER);

      page = gbp_manuals_page_new ();
      ide_workspace_add_page (self->workspace, IDE_PAGE (page), position);
      panel_widget_raise (PANEL_WIDGET (page));
    }

  return page;
}

void
gbp_manuals_workspace_addin_navigate_to (GbpManualsWorkspaceAddin *self,
                                         ManualsNavigatable       *navigatable)
{
  const char *uri;

  g_return_if_fail (GBP_IS_MANUALS_WORKSPACE_ADDIN (self));
  g_return_if_fail (MANUALS_IS_NAVIGATABLE (navigatable));

  if ((uri = manuals_navigatable_get_uri (navigatable)))
    {
      GbpManualsPage *page = gbp_manuals_workspace_addin_get_page (self);

      gbp_manuals_page_navigate_to (page, navigatable);
      panel_widget_raise (PANEL_WIDGET (page));
      gtk_widget_grab_focus (GTK_WIDGET (page));
    }
  else
    {
      gbp_manuals_panel_reveal (self->panel, navigatable);
    }
}

GbpManualsPage *
gbp_manuals_workspace_addin_add_page (GbpManualsWorkspaceAddin *self)
{
  g_autoptr(PanelPosition) position = NULL;
  GbpManualsPage *page;

  g_return_val_if_fail (IDE_IS_MAIN_THREAD (), NULL);
  g_return_val_if_fail (GBP_IS_MANUALS_WORKSPACE_ADDIN (self), NULL);

  position = panel_position_new ();
  panel_position_set_area (position, PANEL_AREA_CENTER);

  page = gbp_manuals_page_new ();
  ide_workspace_add_page (self->workspace, IDE_PAGE (page), position);
  panel_widget_raise (PANEL_WIDGET (page));

  return page;
}
