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

#include <devhelp/devhelp.h>
#include <glib/gi18n.h>
#include <webkit2/webkit2.h>

#include "gbp-devhelp-page.h"
#include "gbp-devhelp-search.h"

struct _GbpDevhelpPage
{
  IdePage         parent_instance;

  WebKitWebView        *web_view1;
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

enum {
  SEARCH_REVEAL,
  LAST_SIGNAL
};

G_DEFINE_TYPE (GbpDevhelpPage, gbp_devhelp_page, IDE_TYPE_PAGE)

static GParamSpec *properties [LAST_PROP];
static guint signals [LAST_SIGNAL];

void
gbp_devhelp_page_set_uri (GbpDevhelpPage *self,
                          const gchar    *uri)
{
  g_return_if_fail (GBP_IS_DEVHELP_PAGE (self));

  if (uri == NULL)
    return;

  webkit_web_view_load_uri (self->web_view1, uri);
}

static void
gbp_devhelp_page_notify_title (GbpDevhelpPage *self,
                               GParamSpec     *pspec,
                               WebKitWebView  *web_view)
{
  const gchar *title;

  g_assert (GBP_IS_DEVHELP_PAGE (self));
  g_assert (WEBKIT_IS_WEB_VIEW (web_view));

  title = webkit_web_view_get_title (self->web_view1);

  ide_page_set_title (IDE_PAGE (self), title);
}

static IdePage *
gbp_devhelp_page_create_split (IdePage *view)
{
  GbpDevhelpPage *self = (GbpDevhelpPage *)view;
  GbpDevhelpPage *other;
  const gchar *uri;

  g_assert (GBP_IS_DEVHELP_PAGE (self));

  uri = webkit_web_view_get_uri (self->web_view1);
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

  operation = webkit_print_operation_new (self->web_view1);
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
gbp_devhelp_search_reveal (GbpDevhelpPage *self)
{
  g_assert (GBP_IS_DEVHELP_PAGE (self));

  webkit_web_view_can_execute_editing_command (self->web_view1, WEBKIT_EDITING_COMMAND_COPY, NULL, NULL, NULL);
  gtk_revealer_set_reveal_child (self->search_revealer, TRUE);
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
gbp_devhelp_page_class_init (GbpDevhelpPageClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);
  IdePageClass *view_class = IDE_PAGE_CLASS (klass);

  object_class->set_property = gbp_devhelp_page_set_property;

  view_class->create_split = gbp_devhelp_page_create_split;

  properties [PROP_URI] =
    g_param_spec_string ("uri",
                         "Uri",
                         "The uri of the documentation.",
                         NULL,
                         (G_PARAM_WRITABLE | G_PARAM_STATIC_STRINGS));

  signals [SEARCH_REVEAL] =
    g_signal_new_class_handler ("search-reveal",
                                G_TYPE_FROM_CLASS (klass),
                                G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION,
                                G_CALLBACK (gbp_devhelp_search_reveal),
                                NULL, NULL, NULL,
                                G_TYPE_NONE, 0);

  gtk_binding_entry_add_signal (gtk_binding_set_by_class (klass),
                                GDK_KEY_f,
                                GDK_CONTROL_MASK,
                                "search-reveal", 0);

  g_object_class_install_properties (object_class, LAST_PROP, properties);

  gtk_widget_class_set_template_from_resource (widget_class, "/plugins/devhelp/gbp-devhelp-page.ui");
  gtk_widget_class_bind_template_child (widget_class, GbpDevhelpPage, web_view1);
  gtk_widget_class_bind_template_child (widget_class, GbpDevhelpPage, devhelp_overlay);

  g_type_ensure (WEBKIT_TYPE_WEB_VIEW);
}

static const GActionEntry actions[] = {
  { "print", gbp_devhelp_page_actions_print },
};

static void
gbp_devhelp_page_init (GbpDevhelpPage *self)
{
  g_autoptr(GSimpleActionGroup) group = NULL;

  gtk_widget_init_template (GTK_WIDGET (self));

  ide_page_set_title (IDE_PAGE (self), _("Documentation"));
  ide_page_set_can_split (IDE_PAGE (self), TRUE);
  ide_page_set_icon_name (IDE_PAGE (self), "org.gnome.Devhelp-symbolic");
  ide_page_set_menu_id (IDE_PAGE (self), "devhelp-view-document-menu");

  self->search = g_object_new (GBP_TYPE_DEVHELP_SEARCH, NULL);
  self->search_revealer = gbp_devhelp_search_get_revealer (self->search);
  self->clipboard = gtk_clipboard_get (GDK_SELECTION_PRIMARY);
  self->web_controller = webkit_web_view_get_find_controller (self->web_view1);

  gtk_overlay_add_overlay (self->devhelp_overlay,
                           GTK_WIDGET (self->search_revealer));

  gbp_devhelp_search_set_devhelp (self->search,
                                  self->web_controller,
                                  self->clipboard);

  g_signal_connect_object (self->web_view1,
                           "notify::title",
                           G_CALLBACK (gbp_devhelp_page_notify_title),
                           self,
                           G_CONNECT_SWAPPED);
  g_signal_connect_object (self->web_view1,
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
}
