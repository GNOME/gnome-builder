/* gbp-devhelp-page.c
 *
 * Copyright 2015-2019 Christian Hergert <chergert@redhat.com>
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

#define I_ g_intern_string

#include <devhelp/devhelp.h>
#include <glib/gi18n.h>
#include <webkit2/webkit2.h>

#include <libportal/portal.h>
#include <libportal-gtk3/portal-gtk3.h>

#include "gbp-devhelp-page.h"
#include "gbp-devhelp-search.h"

struct _GbpDevhelpPage
{
  IdePage         parent_instance;

  WebKitWebView        *web_view;
  WebKitFindController *web_controller;
  GtkClipboard         *clipboard;

  GtkOverlay           *devhelp_overlay;
  GtkRevealer          *search_revealer;
  GbpDevhelpSearch     *search;
 };

enum {
  PROP_0,
  PROP_URI,
  LAST_PROP
};

G_DEFINE_FINAL_TYPE (GbpDevhelpPage, gbp_devhelp_page, IDE_TYPE_PAGE)

static GParamSpec *properties [LAST_PROP];

void
gbp_devhelp_page_set_uri (GbpDevhelpPage *self,
                          const gchar    *uri)
{
  g_return_if_fail (GBP_IS_DEVHELP_PAGE (self));

  if (uri == NULL || g_strcmp0 (uri, gbp_devhelp_page_get_uri (self)) == 0)
    return;

  webkit_web_view_load_uri (self->web_view, uri);
  g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_URI]);
}

/**
 * gbp_devhelp_page_get_uri:
 * @self: a #GbpDevhelpPage
 *
 * Returns: (nullable): the documentation URI loaded for page @self
 */
const char *
gbp_devhelp_page_get_uri (GbpDevhelpPage *self)
{
  g_return_val_if_fail (GBP_IS_DEVHELP_PAGE (self), NULL);

  return webkit_web_view_get_uri (self->web_view);
}

static void
gbp_devhelp_page_notify_title (GbpDevhelpPage *self,
                               GParamSpec     *pspec,
                               WebKitWebView  *web_view)
{
  const gchar *title;

  g_assert (GBP_IS_DEVHELP_PAGE (self));
  g_assert (WEBKIT_IS_WEB_VIEW (web_view));

  title = webkit_web_view_get_title (self->web_view);

  ide_page_set_title (IDE_PAGE (self), title);
}

static IdePage *
gbp_devhelp_page_create_split (IdePage *view)
{
  GbpDevhelpPage *self = (GbpDevhelpPage *)view;
  GbpDevhelpPage *other;
  const gchar *uri;

  g_assert (GBP_IS_DEVHELP_PAGE (self));

  uri = webkit_web_view_get_uri (self->web_view);
  other = g_object_new (GBP_TYPE_DEVHELP_PAGE,
                        "visible", TRUE,
                        "uri", uri,
                        NULL);

  return IDE_PAGE (other);
}

static void
gbp_devhelp_page_actions_print (GSimpleAction *action,
                                GVariant      *param,
                                gpointer       user_data)
{
  GbpDevhelpPage *self = user_data;
  WebKitPrintOperation *operation;
  GtkWidget *window;

  g_assert (GBP_IS_DEVHELP_PAGE (self));

  operation = webkit_print_operation_new (self->web_view);
  window = gtk_widget_get_ancestor (GTK_WIDGET (self), GTK_TYPE_WINDOW);
  webkit_print_operation_run_dialog (operation, GTK_WINDOW (window));
  g_object_unref (operation);
}

static void
gbp_devhelp_page_set_property (GObject      *object,
                               guint         prop_id,
                               const GValue *value,
                               GParamSpec   *pspec)
{
  GbpDevhelpPage *self = GBP_DEVHELP_PAGE (object);

  switch (prop_id)
    {
    case PROP_URI:
      gbp_devhelp_page_set_uri (self, g_value_get_string (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
gbp_devhelp_page_get_property (GObject    *object,
                               guint       prop_id,
                               GValue     *value,
                               GParamSpec *pspec)
{
  GbpDevhelpPage *self = GBP_DEVHELP_PAGE (object);

  switch (prop_id)
    {
    case PROP_URI:
      g_value_set_string (value, gbp_devhelp_page_get_uri (self));
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
gbp_devhelp_page_actions_history_next (GSimpleAction *action,
                                       GVariant      *param,
                                       gpointer       user_data)
{
  GbpDevhelpPage *self = (GbpDevhelpPage *)user_data;

  g_assert (GBP_IS_DEVHELP_PAGE (self));

  webkit_web_view_go_forward (self->web_view);
}

static void
gbp_devhelp_page_actions_history_previous (GSimpleAction *action,
                                           GVariant      *param,
                                           gpointer       user_data)
{
  GbpDevhelpPage *self = (GbpDevhelpPage *)user_data;

  g_assert (GBP_IS_DEVHELP_PAGE (self));

  webkit_web_view_go_back (self->web_view);
}

static void
gbp_devhelp_page_actions_reveal_search (GSimpleAction *action,
                                        GVariant      *param,
                                        gpointer       user_data)
{
  GbpDevhelpPage *self = (GbpDevhelpPage *)user_data;

  g_assert (GBP_IS_DEVHELP_PAGE (self));

  webkit_web_view_can_execute_editing_command (self->web_view, WEBKIT_EDITING_COMMAND_COPY, NULL, NULL, NULL);
  gtk_revealer_set_reveal_child (self->search_revealer, TRUE);
  gtk_widget_grab_focus (GTK_WIDGET (self->search));
}

static void
gbp_devhelp_focus_in_event (GbpDevhelpPage *self,
                            GdkEvent       *event)
{
  g_assert (GBP_IS_DEVHELP_PAGE (self));

  webkit_find_controller_search_finish (self->web_controller);
  gtk_revealer_set_reveal_child (self->search_revealer, FALSE);
}

static void
gbp_devhelp_page_finalize (GObject *object)
{
  GbpDevhelpPage *self = (GbpDevhelpPage *)object;

  g_assert (GBP_IS_DEVHELP_PAGE (self));

  gtk_widget_insert_action_group (GTK_WIDGET (self), "devhelp-view", NULL);

  G_OBJECT_CLASS (gbp_devhelp_page_parent_class)->finalize (object);
}

static void
gbp_devhelp_page_class_init (GbpDevhelpPageClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);
  IdePageClass *view_class = IDE_PAGE_CLASS (klass);

  object_class->set_property = gbp_devhelp_page_set_property;
  object_class->get_property = gbp_devhelp_page_get_property;
  object_class->finalize = gbp_devhelp_page_finalize;

  view_class->create_split = gbp_devhelp_page_create_split;

  properties [PROP_URI] =
    g_param_spec_string ("uri",
                         "Uri",
                         "The uri of the documentation.",
                         NULL,
                         (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS | G_PARAM_EXPLICIT_NOTIFY));

  g_object_class_install_properties (object_class, LAST_PROP, properties);

  gtk_widget_class_set_template_from_resource (widget_class, "/plugins/devhelp/gbp-devhelp-page.ui");
  gtk_widget_class_bind_template_child (widget_class, GbpDevhelpPage, web_view);
  gtk_widget_class_bind_template_child (widget_class, GbpDevhelpPage, devhelp_overlay);

  g_type_ensure (WEBKIT_TYPE_WEB_VIEW);
}

static void
on_uri_opened_with_portal_cb (GObject      *source_object,
                              GAsyncResult *res,
                              gpointer      user_data)
{
  XdpPortal *portal = (XdpPortal *)source_object;
  g_autoptr(GError) error = NULL;

  g_assert (XDP_IS_PORTAL (portal));

  if (!xdp_portal_open_uri_finish (portal, res, &error))
    g_warning ("Couldn't open URI with portal, from devhelp's webview: %s", error->message);
}

/* We can't use g_app_info_launch_default_for_uri() because of https://gitlab.gnome.org/GNOME/glib/-/issues/1960
 * Builder gets opened for HTML pages if we do so, and crashes because gvfs support in Builder is not really working.
 */
static void
open_uri_with_portal (GtkWidget  *widget,
                      const char *uri)
{
  g_autoptr(XdpPortal) portal = xdp_portal_new ();
  XdpParent *parent = xdp_parent_new_gtk (GTK_WINDOW (gtk_widget_get_ancestor (widget, GTK_TYPE_WINDOW)));

  xdp_portal_open_uri (portal, parent,
                       uri, XDP_OPEN_URI_FLAG_NONE,
                       NULL,
                       on_uri_opened_with_portal_cb, NULL);
  xdp_parent_free (parent);
}

static gboolean
webview_decide_policy_cb (WebKitWebView           *web_view,
                          WebKitPolicyDecision    *decision,
                          WebKitPolicyDecisionType decision_type,
                          gpointer                 user_data)
{
  WebKitURIRequest *request = NULL;
  g_autoptr(GUri) uri = NULL;
  gboolean launch_in_browser;

  g_assert (WEBKIT_IS_WEB_VIEW (web_view));

  switch (decision_type) {
    case WEBKIT_POLICY_DECISION_TYPE_NEW_WINDOW_ACTION:
    case WEBKIT_POLICY_DECISION_TYPE_NAVIGATION_ACTION:
      {
        WebKitNavigationAction *navigation_action =
          webkit_navigation_policy_decision_get_navigation_action (WEBKIT_NAVIGATION_POLICY_DECISION (decision));
        request = webkit_navigation_action_get_request (navigation_action);
        launch_in_browser = TRUE;
      }
      break;
    case WEBKIT_POLICY_DECISION_TYPE_RESPONSE:
      request = webkit_response_policy_decision_get_request (WEBKIT_RESPONSE_POLICY_DECISION (decision));
      launch_in_browser = FALSE;
      break;
    default:
      return FALSE;
  }

  uri = g_uri_parse (webkit_uri_request_get_uri (request), G_URI_FLAGS_NONE, NULL);
  if (!uri)
    /* Don't let invalid URIs go through the firewall. */
    {
      webkit_policy_decision_ignore (decision);
      return TRUE;
    }
  else
    {
      /* Allow the integrated devhelp web view to handle local documentation links,
       * but open any non-local ones with the user's web browser (e.g. library's homepage),
       * and deny any other remote resource.
       */
      if (g_strcmp0 (g_uri_get_scheme (uri), "file") == 0)
        return FALSE;
      else
        {
          if (launch_in_browser)
            open_uri_with_portal (GTK_WIDGET (web_view), webkit_uri_request_get_uri (request));

          webkit_policy_decision_ignore (decision);
          return TRUE;
        }
    }
}

static void
setup_webview (WebKitWebView *web_view)
{
  g_autoptr(WebKitUserStyleSheet) stylesheet = NULL;

  /* Both gi-docgen and gtk-doc use the devhelp-hidden style class to give indications of what
   * elements should be hidden for use by devhelp. Generally it's for the sidebar but it allows
   * to hide really anything not useful for devhelp (e.g. the TOC which already has native GTK
   * widgets in Builder/Devhelp. So follow Devhelp here and hide them.
   */
  stylesheet = webkit_user_style_sheet_new (".devhelp-hidden { display: none; }",
                                            WEBKIT_USER_CONTENT_INJECT_ALL_FRAMES,
                                            WEBKIT_USER_STYLE_LEVEL_USER,
                                            NULL, NULL);
  webkit_user_content_manager_add_style_sheet (webkit_web_view_get_user_content_manager (web_view),
                                               stylesheet);

  g_signal_connect (web_view, "decide-policy", G_CALLBACK (webview_decide_policy_cb), NULL);
}

static const GActionEntry actions[] = {
  { "print", gbp_devhelp_page_actions_print },
  { "reveal-search", gbp_devhelp_page_actions_reveal_search },
  { "history-next", gbp_devhelp_page_actions_history_next },
  { "history-previous", gbp_devhelp_page_actions_history_previous },
};

static void
gbp_devhelp_page_init (GbpDevhelpPage *self)
{
  g_autoptr(GSimpleActionGroup) group = NULL;
  DzlShortcutController *controller;

  gtk_widget_init_template (GTK_WIDGET (self));

  ide_page_set_title (IDE_PAGE (self), _("Documentation"));
  ide_page_set_can_split (IDE_PAGE (self), TRUE);
  ide_page_set_icon_name (IDE_PAGE (self), "org.gnome.Devhelp-symbolic");
  ide_page_set_menu_id (IDE_PAGE (self), "devhelp-view-document-menu");

  self->search = g_object_new (GBP_TYPE_DEVHELP_SEARCH, NULL);
  self->search_revealer = gbp_devhelp_search_get_revealer (self->search);
  self->clipboard = gtk_clipboard_get (GDK_SELECTION_PRIMARY);
  self->web_controller = webkit_web_view_get_find_controller (self->web_view);

  setup_webview (self->web_view);

  gtk_widget_show (GTK_WIDGET (self->search));
  gtk_overlay_add_overlay (self->devhelp_overlay, GTK_WIDGET (self->search));

  gbp_devhelp_search_set_devhelp (self->search,
                                  self->web_controller,
                                  self->clipboard);

  g_signal_connect_object (self->web_view,
                           "notify::title",
                           G_CALLBACK (gbp_devhelp_page_notify_title),
                           self,
                           G_CONNECT_SWAPPED);
  g_signal_connect_object (self->web_view,
                          "focus-in-event",
                           G_CALLBACK (gbp_devhelp_focus_in_event),
                           self,
                           G_CONNECT_SWAPPED);

  group = g_simple_action_group_new ();
  g_action_map_add_action_entries (G_ACTION_MAP (group),
                                   actions,
                                   G_N_ELEMENTS (actions),
                                   self);
  gtk_widget_insert_action_group (GTK_WIDGET (self),
                                  "devhelp-view",
                                  G_ACTION_GROUP (group));

  controller = dzl_shortcut_controller_find (GTK_WIDGET (self));
  dzl_shortcut_controller_add_command_action (controller,
                                              I_("org.gnome.builder.devhelp-view.reveal-search"),
                                              "<Primary>f",
                                              DZL_SHORTCUT_PHASE_CAPTURE,
                                              I_("devhelp-view.reveal-search"));
  dzl_shortcut_controller_add_command_action (controller,
                                              I_("org.gnome.builder.devhelp-view.history-next"),
                                              "<Alt>Right",
                                              DZL_SHORTCUT_PHASE_CAPTURE,
                                              I_("devhelp-view.history-next"));
  dzl_shortcut_controller_add_command_action (controller,
                                              I_("org.gnome.builder.devhelp-view.history-previous"),
                                              "<Alt>Left",
                                              DZL_SHORTCUT_PHASE_CAPTURE,
                                              I_("devhelp-view.history-previous"));
  dzl_shortcut_controller_add_command_action (controller,
                                              I_("org.gnome.builder.devhelp-view.history-next"),
                                              "<Alt>KP_Right",
                                              DZL_SHORTCUT_PHASE_CAPTURE,
                                              I_("devhelp-view.history-next"));
  dzl_shortcut_controller_add_command_action (controller,
                                              I_("org.gnome.builder.devhelp-view.history-previous"),
                                              "<Alt>KP_Left",
                                              DZL_SHORTCUT_PHASE_CAPTURE,
                                              I_("devhelp-view.history-previous"));
}
