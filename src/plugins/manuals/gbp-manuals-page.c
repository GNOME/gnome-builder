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

#include "gbp-manuals-page.h"

struct _GbpManualsPage
{
  IdeWebkitPage parent_instance;
};

G_DEFINE_FINAL_TYPE (GbpManualsPage, gbp_manuals_page, IDE_TYPE_WEBKIT_PAGE)

static const char style_sheet_css[] =
  "#main { box-shadow: none !important; }\n"
  ".devhelp-hidden { display: none; }\n"
  ".toc { background: transparent !important; }\n"
  ":root { --body-bg: #ffffff !important; }\n"
  "@media (prefers-color-scheme: dark) {\n"
  "  :root { --body-bg: #1e1e1e !important; }\n"
  "}\n"
  ;

static void
gbp_manuals_page_constructed (GObject *object)
{
  GbpManualsPage *self = (GbpManualsPage *)object;
  g_autoptr(WebKitUserStyleSheet) style_sheet = NULL;
  WebKitUserContentManager *ucm;
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

  style_sheet = webkit_user_style_sheet_new (style_sheet_css,
                                             WEBKIT_USER_CONTENT_INJECT_ALL_FRAMES,
                                             WEBKIT_USER_STYLE_LEVEL_USER,
                                             NULL, NULL);
  ucm = webkit_web_view_get_user_content_manager (web_view);
  webkit_user_content_manager_add_style_sheet (ucm, style_sheet);

  session = webkit_web_view_get_network_session (web_view);
  manager = webkit_network_session_get_website_data_manager (session);
  webkit_website_data_manager_set_favicons_enabled (manager, TRUE);
}

static void
gbp_manuals_page_class_init (GbpManualsPageClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->constructed = gbp_manuals_page_constructed;
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

  if (!(uri = manuals_navigatable_get_uri (navigatable)))
    return;

  ide_webkit_page_load_uri (IDE_WEBKIT_PAGE (self), uri);
}
