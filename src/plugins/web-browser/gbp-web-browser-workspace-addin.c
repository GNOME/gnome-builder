/* gbp-web-browser-workspace-addin.c
 *
 * Copyright 2022-2023 Christian Hergert <chergert@redhat.com>
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

#define G_LOG_DOMAIN "gbp-web-browser-workspace-addin"

#include "config.h"

#include <libide-gui.h>
#include <libide-webkit.h>

#include "gbp-web-browser-workspace-addin.h"

struct _GbpWebBrowserWorkspaceAddin
{
  GObject       parent_instance;
  IdeWorkspace *workspace;
};

static void web_browser_new_page_action      (GbpWebBrowserWorkspaceAddin *self,
                                              GVariant                    *param);
static void web_browser_focus_address_action (GbpWebBrowserWorkspaceAddin *self,
                                              GVariant                    *param);
static void web_browser_reload_action        (GbpWebBrowserWorkspaceAddin *self,
                                              GVariant                    *param);

IDE_DEFINE_ACTION_GROUP (GbpWebBrowserWorkspaceAddin, gbp_web_browser_workspace_addin, {
  { "page.new", web_browser_new_page_action },
  { "page.location.focus", web_browser_focus_address_action },
  { "page.reload", web_browser_reload_action, "b" },
})

static void
gbp_web_browser_workspace_addin_load (IdeWorkspaceAddin *addin,
                                      IdeWorkspace      *workspace)
{
  GBP_WEB_BROWSER_WORKSPACE_ADDIN (addin)->workspace = workspace;
}

static void
gbp_web_browser_workspace_addin_unload (IdeWorkspaceAddin *addin,
                                        IdeWorkspace      *workspace)
{
  GBP_WEB_BROWSER_WORKSPACE_ADDIN (addin)->workspace = NULL;
}

static void
gbp_web_browser_workspace_addin_save_session_page_cb (IdePage  *page,
                                                      gpointer  user_data)
{
  IdeSession *session = user_data;

  g_assert (IDE_IS_PAGE (page));
  g_assert (IDE_IS_SESSION (session));

  /* Ignore subclasses of IdeWebkitPage, they need to handle session
   * saving themselves.
   */
  if (G_OBJECT_TYPE (page) == IDE_TYPE_WEBKIT_PAGE &&
      !ide_webkit_page_has_generator (IDE_WEBKIT_PAGE (page)))
    {
      g_autoptr(PanelPosition) position = ide_page_get_position (page);
      g_autoptr(IdeSessionItem) item = ide_session_item_new ();
      GtkWidget *web_view = ide_webkit_page_get_view (IDE_WEBKIT_PAGE (page));
      g_autoptr(WebKitWebViewSessionState) state = webkit_web_view_get_session_state (WEBKIT_WEB_VIEW (web_view));
      IdeWorkspace *workspace = ide_widget_get_workspace (GTK_WIDGET (page));
      const char *workspace_id = ide_workspace_get_id (workspace);
      g_autoptr(GBytes) bytes = NULL;
      GVariant *state_value = NULL;

      if (state == NULL ||
          !(bytes = webkit_web_view_session_state_serialize (state)) ||
          !(state_value = g_variant_new_fixed_array (G_VARIANT_TYPE_BYTE,
                                                     g_bytes_get_data (bytes, NULL),
                                                     g_bytes_get_size (bytes),
                                                     sizeof (guint8))))
        return;

      ide_session_item_set_module_name (item, "web-browser");
      ide_session_item_set_type_hint (item, "IdeWebkitPage");
      ide_session_item_set_workspace (item, workspace_id);
      ide_session_item_set_position (item, position);
      ide_session_item_set_metadata_value (item, "state", g_steal_pointer (&state_value));

      if (page == ide_workspace_get_most_recent_page (workspace))
        ide_session_item_set_metadata (item, "has-focus", "b", TRUE);

      ide_session_append (session, item);
    }
}

static void
gbp_web_browser_workspace_addin_save_session (IdeWorkspaceAddin *addin,
                                              IdeSession        *session)
{
  GbpWebBrowserWorkspaceAddin *self = (GbpWebBrowserWorkspaceAddin *)addin;

  IDE_ENTRY;

  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (GBP_IS_WEB_BROWSER_WORKSPACE_ADDIN (self));
  g_assert (IDE_IS_WORKSPACE (self->workspace));
  g_assert (IDE_IS_SESSION (session));

  ide_workspace_foreach_page (self->workspace,
                              gbp_web_browser_workspace_addin_save_session_page_cb,
                              session);

  IDE_EXIT;
}

static void
gbp_web_browser_workspace_addin_restore_session_item (IdeWorkspaceAddin *addin,
                                                      IdeSession        *session,
                                                      IdeSessionItem    *item)
{
  GbpWebBrowserWorkspaceAddin *self = (GbpWebBrowserWorkspaceAddin *)addin;
  g_autoptr(WebKitWebViewSessionState) state = NULL;
  g_autoptr(GBytes) bytes = NULL;
  WebKitBackForwardList *bf_list;
  WebKitBackForwardListItem *current;
  PanelPosition *position;
  IdeWebkitPage *page;
  GtkWidget *view;
  GVariant *state_value;
  const guint8 *data;
  gboolean has_focus;
  gsize n_elements = 0;

  IDE_ENTRY;

  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (GBP_IS_WEB_BROWSER_WORKSPACE_ADDIN (self));
  g_assert (IDE_IS_WORKSPACE (self->workspace));
  g_assert (IDE_IS_SESSION (session));

  if (!ide_str_equal0 ("IdeWebkitPage", ide_session_item_get_type_hint (item)))
    IDE_EXIT;

  if (!(state_value = ide_session_item_get_metadata_value (item, "state", G_VARIANT_TYPE ("ay"))))
    IDE_EXIT;

  if (!(data = g_variant_get_fixed_array (state_value, &n_elements, sizeof (guint8))))
    IDE_EXIT;

  if (n_elements == 0)
    IDE_EXIT;

  /* Make a copy of the bytes because we can't guarantee the lifetime of
   * the bytes and we don't want to possibly keep state_value alive.
   */
  bytes = g_bytes_new (data, n_elements);

  g_assert (bytes != NULL);
  g_assert (g_bytes_get_size (bytes) == n_elements);

  IDE_DUMP_BYTES (state, data, n_elements);

  /* Create the WebkitWebView _BEFORE_ we deserialize the session state or
   * we risk Webkit assertions due to RunLoop::isMain() failure simply due
   * to missing initialization paths.
   *
   * See #2005 and https://bugs.webkit.org/show_bug.cgi?id=253858
   */
  page = ide_webkit_page_new ();
  view = ide_webkit_page_get_view (page);

  if (!(state = webkit_web_view_session_state_new (bytes)))
    {
      g_object_unref (page);
      IDE_EXIT;
    }

  position = ide_session_item_get_position (item);

  webkit_web_view_restore_session_state (WEBKIT_WEB_VIEW (view), state);

  bf_list = webkit_web_view_get_back_forward_list (WEBKIT_WEB_VIEW (view));

  if ((current = webkit_back_forward_list_get_current_item (bf_list)))
    webkit_web_view_go_to_back_forward_list_item (WEBKIT_WEB_VIEW (view), current);

  ide_workspace_add_page (self->workspace, IDE_PAGE (page), position);

  if (ide_session_item_get_metadata (item, "has-focus", "b", &has_focus) && has_focus)
    {
      panel_widget_raise (PANEL_WIDGET (page));
      gtk_widget_grab_focus (GTK_WIDGET (page));
    }

  IDE_EXIT;
}

static void
workspace_addin_iface_init (IdeWorkspaceAddinInterface *iface)
{
  iface->load = gbp_web_browser_workspace_addin_load;
  iface->unload = gbp_web_browser_workspace_addin_unload;
  iface->save_session = gbp_web_browser_workspace_addin_save_session;
  iface->restore_session_item = gbp_web_browser_workspace_addin_restore_session_item;
}

G_DEFINE_FINAL_TYPE_WITH_CODE (GbpWebBrowserWorkspaceAddin, gbp_web_browser_workspace_addin, G_TYPE_OBJECT,
                               G_IMPLEMENT_INTERFACE (G_TYPE_ACTION_GROUP, gbp_web_browser_workspace_addin_init_action_group)
                               G_IMPLEMENT_INTERFACE (IDE_TYPE_WORKSPACE_ADDIN, workspace_addin_iface_init))

static void
gbp_web_browser_workspace_addin_class_init (GbpWebBrowserWorkspaceAddinClass *klass)
{
}

static void
gbp_web_browser_workspace_addin_init (GbpWebBrowserWorkspaceAddin *self)
{
}

static void
web_browser_new_page_action (GbpWebBrowserWorkspaceAddin *self,
                             GVariant                    *param)
{
  g_autoptr(PanelPosition) position = NULL;
  IdeWebkitPage *page;

  IDE_ENTRY;

  g_assert (GBP_IS_WEB_BROWSER_WORKSPACE_ADDIN (self));
  g_assert (IDE_IS_WORKSPACE (self->workspace));

  page = ide_webkit_page_new ();
  position = panel_position_new ();

  ide_workspace_add_page (self->workspace, IDE_PAGE (page), position);
  panel_widget_raise (PANEL_WIDGET (page));
  gtk_widget_grab_focus (GTK_WIDGET (page));

  IDE_EXIT;
}

static void
web_browser_focus_address_action (GbpWebBrowserWorkspaceAddin *self,
                                  GVariant                    *param)
{
  IdePage *page;

  IDE_ENTRY;

  g_assert (GBP_IS_WEB_BROWSER_WORKSPACE_ADDIN (self));
  g_assert (IDE_IS_WORKSPACE (self->workspace));

  if (!(page = ide_workspace_get_most_recent_page (self->workspace)))
    IDE_EXIT;

  if (!IDE_IS_WEBKIT_PAGE (page))
    IDE_EXIT;

  ide_webkit_page_focus_address (IDE_WEBKIT_PAGE (page));

  IDE_EXIT;
}

static void
web_browser_reload_action (GbpWebBrowserWorkspaceAddin *self,
                           GVariant                    *param)
{
  IdePage *page;
  gboolean ignore_cache;

  IDE_ENTRY;

  g_assert (GBP_IS_WEB_BROWSER_WORKSPACE_ADDIN (self));
  g_assert (param != NULL);
  g_assert (g_variant_is_of_type (param, G_VARIANT_TYPE_BOOLEAN));
  g_assert (IDE_IS_WORKSPACE (self->workspace));

  if (!(page = ide_workspace_get_most_recent_page (self->workspace)))
    IDE_EXIT;

  if (!IDE_IS_WEBKIT_PAGE (page))
    IDE_EXIT;

  ignore_cache = g_variant_get_boolean (param);

  if (ignore_cache)
    ide_webkit_page_reload_ignoring_cache (IDE_WEBKIT_PAGE (page));
  else
    ide_webkit_page_reload (IDE_WEBKIT_PAGE (page));

  IDE_EXIT;
}
