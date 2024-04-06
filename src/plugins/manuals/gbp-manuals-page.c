/* gbp-manuals-page.c
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

#include <glib/gi18n.h>

#include <webkit/webkit.h>

#include "gbp-manuals-application-addin.h"
#include "gbp-manuals-workspace-addin.h"
#include "gbp-manuals-page.h"

#include "manuals-sdk.h"
#include "manuals-heading.h"
#include "manuals-keyword.h"

struct _GbpManualsPage
{
  IdeWebkitPage         parent_instance;
  ManualsNavigatable   *navigatable;
  WebKitUserStyleSheet *style_sheet;
};

G_DEFINE_FINAL_TYPE (GbpManualsPage, gbp_manuals_page, IDE_TYPE_WEBKIT_PAGE)

enum {
  PROP_0,
  PROP_NAVIGATABLE,
  N_PROPS
};

static GParamSpec *properties[N_PROPS];

static const char style_sheet_css_template[] =
  "#main { box-shadow: none !important; }\n"
  ".devhelp-hidden { display: none; }\n"
  ".toc { background: transparent !important; }\n"
  ":root { --body-bg: @BODY_BG@ !important; }\n"
  "@media (prefers-color-scheme: dark) {\n"
  "  :root { --body-bg: @BODY_BG@ !important; }\n"
  "}\n"
  ;

typedef struct _DecidePolicy
{
  GbpManualsPage           *self;
  WebKitPolicyDecision     *decision;
  WebKitPolicyDecisionType  decision_type;
} DecidePolicy;

static void
decide_policy_free (DecidePolicy *state)
{
  g_clear_object (&state->self);
  g_clear_object (&state->decision);
  g_free (state);
}

static DexFuture *
gbp_manuals_page_decide_policy_fiber (gpointer user_data)
{
  WebKitNavigationPolicyDecision *navigation_decision;
  WebKitNavigationAction *navigation_action;
  g_autoptr(GObject) resource = NULL;
  g_auto(GValue) uri_value = G_VALUE_INIT;
  IdeApplicationAddin *app_addin;
  ManualsRepository *repository;
  DecidePolicy *state = user_data;
  IdeWorkspaceAddin *addin;
  IdeWorkspace *workspace;
  const char *uri;
  gboolean open_new_tab = FALSE;
  int button;
  int modifiers;

  g_assert (state != NULL);
  g_assert (GBP_IS_MANUALS_PAGE (state->self));
  g_assert (WEBKIT_IS_NAVIGATION_POLICY_DECISION (state->decision));

  if (!(app_addin = ide_application_find_addin_by_module_name (IDE_APPLICATION_DEFAULT, "manuals")) ||
      !(workspace = ide_widget_get_workspace (GTK_WIDGET (state->self))) ||
      !(addin = ide_workspace_addin_find_by_module_name (workspace, "manuals")) ||
      !(repository = dex_await_object (gbp_manuals_application_addin_load_repository (GBP_MANUALS_APPLICATION_ADDIN (app_addin)), NULL)))
    goto ignore;

  navigation_decision = WEBKIT_NAVIGATION_POLICY_DECISION (state->decision);
  navigation_action = webkit_navigation_policy_decision_get_navigation_action (navigation_decision);
  uri = webkit_uri_request_get_uri (webkit_navigation_action_get_request (navigation_action));

  /* middle click or ctrl-click -> new tab */
  button = webkit_navigation_action_get_mouse_button (navigation_action);
  modifiers = webkit_navigation_action_get_modifiers (navigation_action);
  open_new_tab = (button == 2 || (button == 1 && modifiers == GDK_CONTROL_MASK));

  /* Pass-through API requested things */
  if (button == 0 && modifiers == 0)
    {
      webkit_policy_decision_use (state->decision);
      return dex_future_new_for_boolean (TRUE);
    }

  if (g_str_equal (uri, "about:blank"))
    {
      gbp_manuals_workspace_addin_add_page (GBP_MANUALS_WORKSPACE_ADDIN (addin));
      goto ignore;
    }

  if (g_strcmp0 ("file", g_uri_peek_scheme (uri)) != 0)
    {
      g_autoptr(GtkUriLauncher) launcher = gtk_uri_launcher_new (uri);
      gtk_uri_launcher_launch (launcher, GTK_WINDOW (workspace), NULL, NULL, NULL);
      goto ignore;
    }

  if ((resource = dex_await_object (manuals_heading_find_by_uri (repository, uri), NULL)) ||
      (resource = dex_await_object (manuals_keyword_find_by_uri (repository, uri), NULL)))
    {
      g_autoptr(ManualsNavigatable) navigatable = NULL;
      GbpManualsPage *page = state->self;

      if (open_new_tab)
        page = gbp_manuals_workspace_addin_add_page (GBP_MANUALS_WORKSPACE_ADDIN (addin));

      navigatable = manuals_navigatable_new_for_resource (resource);
      gbp_manuals_page_navigate_to (page, navigatable);

      goto ignore;
    }

  webkit_policy_decision_use (state->decision);

  return dex_future_new_for_boolean (TRUE);

ignore:
  webkit_policy_decision_ignore (state->decision);

  return dex_future_new_for_boolean (TRUE);
}

static gboolean
manuals_tab_web_view_decide_policy_cb (GbpManualsPage           *self,
                                       WebKitPolicyDecision     *decision,
                                       WebKitPolicyDecisionType  decision_type,
                                       WebKitWebView            *web_view)
{
  DecidePolicy *state;

  g_assert (GBP_IS_MANUALS_PAGE (self));
  g_assert (WEBKIT_IS_POLICY_DECISION (decision));
  g_assert (WEBKIT_IS_WEB_VIEW (web_view));

  if (decision_type != WEBKIT_POLICY_DECISION_TYPE_NAVIGATION_ACTION)
    return GDK_EVENT_PROPAGATE;

  state = g_new0 (DecidePolicy, 1);
  state->self = g_object_ref (self);
  state->decision = g_object_ref (decision);
  state->decision_type = decision_type;

  dex_future_disown (dex_scheduler_spawn (NULL, 0,
                                          gbp_manuals_page_decide_policy_fiber,
                                          state,
                                          (GDestroyNotify)decide_policy_free));

  return GDK_EVENT_STOP;
}

static void
gbp_manuals_page_update_style_sheet (GbpManualsPage *self)
{
  g_autoptr(WebKitUserStyleSheet) old_style_sheet = NULL;
  g_autoptr(GString) style_sheet_css = NULL;
  g_autofree char *bg_color_str = NULL;
  GtkSourceStyleSchemeManager *style_scheme_manager;
  WebKitUserContentManager *ucm;
  GtkSourceStyleScheme *style_scheme;
  GtkSourceStyle *style;
  WebKitWebView *web_view;
  const char *style_scheme_name;

  g_assert (GBP_IS_MANUALS_PAGE (self));

  style_scheme_name = ide_application_get_style_scheme (IDE_APPLICATION_DEFAULT);
  style_scheme_manager = gtk_source_style_scheme_manager_get_default ();
  style_scheme = gtk_source_style_scheme_manager_get_scheme (style_scheme_manager, style_scheme_name);

  g_assert (style_scheme != NULL);

  /* Unlikely, but bail if there is no text style */
  if (!(style = gtk_source_style_scheme_get_style (style_scheme, "text")))
    return;

  g_object_get (style,
                "background", &bg_color_str,
                NULL);

  web_view = WEBKIT_WEB_VIEW (ide_webkit_page_get_view (IDE_WEBKIT_PAGE (self)));
  ucm = webkit_web_view_get_user_content_manager (web_view);

  if ((old_style_sheet = g_steal_pointer (&self->style_sheet)))
    webkit_user_content_manager_remove_style_sheet (ucm, old_style_sheet);

  style_sheet_css = g_string_new (style_sheet_css_template);
  g_string_replace (style_sheet_css, "@BODY_BG@", bg_color_str, 0);

  self->style_sheet = webkit_user_style_sheet_new (style_sheet_css->str,
                                                   WEBKIT_USER_CONTENT_INJECT_ALL_FRAMES,
                                                   WEBKIT_USER_STYLE_LEVEL_USER,
                                                   NULL, NULL);
  webkit_user_content_manager_add_style_sheet (ucm, self->style_sheet);
}

static void
gbp_manuals_page_constructed (GObject *object)
{
  GbpManualsPage *self = (GbpManualsPage *)object;
  WebKitWebsiteDataManager *manager;
  WebKitNetworkSession *session;
  WebKitSettings *webkit_settings;
  WebKitWebView *web_view;

  G_OBJECT_CLASS (gbp_manuals_page_parent_class)->constructed (object);

  web_view = WEBKIT_WEB_VIEW (ide_webkit_page_get_view (IDE_WEBKIT_PAGE (self)));

  webkit_settings = webkit_web_view_get_settings (web_view);
  webkit_settings_set_enable_back_forward_navigation_gestures (webkit_settings, TRUE);
  webkit_settings_set_enable_html5_database (webkit_settings, FALSE);
  webkit_settings_set_enable_html5_local_storage (webkit_settings, FALSE);
  webkit_settings_set_user_agent_with_application_details (webkit_settings, "GNOME-Builder", PACKAGE_VERSION);

  session = webkit_web_view_get_network_session (web_view);
  manager = webkit_network_session_get_website_data_manager (session);
  webkit_website_data_manager_set_favicons_enabled (manager, TRUE);

  g_signal_connect_object (web_view,
                           "decide-policy",
                           G_CALLBACK (manuals_tab_web_view_decide_policy_cb),
                           self,
                           G_CONNECT_SWAPPED);

  g_signal_connect_object (IDE_APPLICATION_DEFAULT,
                           "notify::style-scheme",
                           G_CALLBACK (gbp_manuals_page_update_style_sheet),
                           self,
                           G_CONNECT_SWAPPED);

  gbp_manuals_page_update_style_sheet (self);
}

static void
gbp_manuals_page_dispose (GObject *object)
{
  GbpManualsPage *self = (GbpManualsPage *)object;

  g_clear_object (&self->navigatable);
  g_clear_pointer (&self->style_sheet, webkit_user_style_sheet_unref);

  G_OBJECT_CLASS (gbp_manuals_page_parent_class)->dispose (object);
}

static void
gbp_manuals_page_get_property (GObject    *object,
                               guint       prop_id,
                               GValue     *value,
                               GParamSpec *pspec)
{
  GbpManualsPage *self = GBP_MANUALS_PAGE (object);

  switch (prop_id)
    {
    case PROP_NAVIGATABLE:
      g_value_set_object (value, self->navigatable);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
gbp_manuals_page_class_init (GbpManualsPageClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->constructed = gbp_manuals_page_constructed;
  object_class->dispose = gbp_manuals_page_dispose;
  object_class->get_property = gbp_manuals_page_get_property;

  properties[PROP_NAVIGATABLE] =
    g_param_spec_object ("navigatable", NULL, NULL,
                         MANUALS_TYPE_NAVIGATABLE,
                         (G_PARAM_READABLE |
                          G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, N_PROPS, properties);
}

static void
gbp_manuals_page_init (GbpManualsPage *self)
{
  panel_widget_set_icon_name (PANEL_WIDGET (self), "builder-documentation-symbolic");
  panel_widget_set_title (PANEL_WIDGET (self), _("Manuals"));
  ide_webkit_page_set_show_toolbar (IDE_WEBKIT_PAGE (self), FALSE);
}

GbpManualsPage *
gbp_manuals_page_new (void)
{
  return g_object_new (GBP_TYPE_MANUALS_PAGE, NULL);
}

void
gbp_manuals_page_navigate_to (GbpManualsPage     *self,
                              ManualsNavigatable *navigatable)
{
  const char *uri;

  g_return_if_fail (GBP_IS_MANUALS_PAGE (self));
  g_return_if_fail (MANUALS_IS_NAVIGATABLE (navigatable));

  if (g_set_object (&self->navigatable, navigatable))
    g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_NAVIGATABLE]);

  if ((uri = manuals_navigatable_get_uri (navigatable)))
    ide_webkit_page_load_uri (IDE_WEBKIT_PAGE (self), uri);
}
