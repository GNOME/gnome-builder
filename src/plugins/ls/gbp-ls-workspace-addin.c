/* gbp-ls-workspace-addin.c
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

#define G_LOG_DOMAIN "gbp-ls-workspace-addin"

#include "config.h"

#include "gbp-ls-page.h"
#include "gbp-ls-workspace-addin.h"

struct _GbpLsWorkspaceAddin
{
  GObject       parent_instance;
  IdeWorkspace *workspace;
};

static void
gbp_ls_workspace_addin_load (IdeWorkspaceAddin *addin,
                             IdeWorkspace      *workspace)
{
  GbpLsWorkspaceAddin *self = (GbpLsWorkspaceAddin *)addin;

  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (GBP_IS_LS_WORKSPACE_ADDIN (self));
  g_assert (IDE_IS_WORKSPACE (workspace));

  self->workspace = workspace;
}

static void
gbp_ls_workspace_addin_unload (IdeWorkspaceAddin *addin,
                               IdeWorkspace      *workspace)
{
  GbpLsWorkspaceAddin *self = (GbpLsWorkspaceAddin *)addin;

  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (GBP_IS_LS_WORKSPACE_ADDIN (self));
  g_assert (IDE_IS_WORKSPACE (workspace));

  self->workspace = NULL;
}

static void
gbp_ls_workspace_addin_save_session_page_cb (IdePage  *page,
                                             gpointer  user_data)
{
  IdeSession *session = user_data;

  g_assert (IDE_IS_PAGE (page));
  g_assert (IDE_IS_SESSION (session));

  if (GBP_IS_LS_PAGE (page))
    {
      g_autoptr(PanelPosition) position = ide_page_get_position (page);
      g_autoptr(IdeSessionItem) item = ide_session_item_new ();
      GFile *file = gbp_ls_page_get_directory (GBP_LS_PAGE (page));
      IdeWorkspace *workspace = ide_widget_get_workspace (GTK_WIDGET (page));
      const char *id = ide_workspace_get_id (workspace);
      g_autofree char *uri = g_file_get_uri (file);

      ide_session_item_set_module_name (item, "ls");
      ide_session_item_set_type_hint (item, "GbpLsPage");
      ide_session_item_set_workspace (item, id);
      ide_session_item_set_position (item, position);
      ide_session_item_set_metadata (item, "uri", "s", uri);

      if (page == ide_workspace_get_most_recent_page (workspace))
        ide_session_item_set_metadata (item, "has-focus", "b", TRUE);

      ide_session_append (session, item);
    }
}

static void
gbp_ls_workspace_addin_save_session (IdeWorkspaceAddin *addin,
                                     IdeSession        *session)
{
  GbpLsWorkspaceAddin *self = (GbpLsWorkspaceAddin *)addin;

  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (GBP_IS_LS_WORKSPACE_ADDIN (self));
  g_assert (IDE_IS_WORKSPACE (self->workspace));
  g_assert (IDE_IS_SESSION (session));

  ide_workspace_foreach_page (self->workspace,
                              gbp_ls_workspace_addin_save_session_page_cb,
                              session);
}

static void
gbp_ls_workspace_addin_restore_page (GbpLsWorkspaceAddin *self,
                                     IdeSessionItem      *item)
{
  g_autofree char *uri = NULL;
  g_autoptr(GFile) file = NULL;
  PanelPosition *position;
  GtkWidget *page;
  gboolean has_focus;

  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (GBP_IS_LS_WORKSPACE_ADDIN (self));
  g_assert (IDE_IS_SESSION_ITEM (item));

  if (!(position = ide_session_item_get_position (item)) ||
      !ide_session_item_get_metadata (item, "uri", "s", &uri))
    return;

  file = g_file_new_for_uri (uri);
  page = gbp_ls_page_new ();
  gbp_ls_page_set_directory (GBP_LS_PAGE (page), file);

  ide_workspace_add_page (self->workspace, IDE_PAGE (page), position);

  if (ide_session_item_get_metadata (item, "has-focus", "b", &has_focus) && has_focus)
    {
      panel_widget_raise (PANEL_WIDGET (page));
      gtk_widget_grab_focus (GTK_WIDGET (page));
    }
}

static void
gbp_ls_workspace_addin_restore_session_item (IdeWorkspaceAddin *addin,
                                             IdeSession        *session,
                                             IdeSessionItem    *item)
{
  GbpLsWorkspaceAddin *self = (GbpLsWorkspaceAddin *)addin;
  const char *type_hint;

  IDE_ENTRY;

  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (GBP_IS_LS_WORKSPACE_ADDIN (self));
  g_assert (IDE_IS_SESSION (session));
  g_assert (IDE_IS_WORKSPACE (self->workspace));

  type_hint = ide_session_item_get_type_hint (item);

  if (ide_str_equal0 (type_hint, "GbpLsPage"))
    gbp_ls_workspace_addin_restore_page (self, item);

  IDE_EXIT;
}

static void
workspace_addin_iface_init (IdeWorkspaceAddinInterface *iface)
{
  iface->load = gbp_ls_workspace_addin_load;
  iface->unload = gbp_ls_workspace_addin_unload;
  iface->save_session = gbp_ls_workspace_addin_save_session;
  iface->restore_session_item = gbp_ls_workspace_addin_restore_session_item;
}

G_DEFINE_FINAL_TYPE_WITH_CODE (GbpLsWorkspaceAddin, gbp_ls_workspace_addin, G_TYPE_OBJECT,
                               G_IMPLEMENT_INTERFACE (IDE_TYPE_WORKSPACE_ADDIN, workspace_addin_iface_init))

static void
gbp_ls_workspace_addin_class_init (GbpLsWorkspaceAddinClass *klass)
{
}

static void
gbp_ls_workspace_addin_init (GbpLsWorkspaceAddin *self)
{
}
