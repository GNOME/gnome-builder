/* gbp-devhelp-view.c
 *
 * Copyright (C) 2015 Christian Hergert <chergert@redhat.com>
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
 */

#include <devhelp/devhelp.h>
#include <glib/gi18n.h>
#include <webkit2/webkit2.h>

#include "gbp-devhelp-view.h"

struct _GbpDevhelpView
{
  IdeLayoutView  parent_instance;
  WebKitWebView *web_view1;
};

enum {
  PROP_0,
  PROP_URI,
  LAST_PROP
};

G_DEFINE_TYPE (GbpDevhelpView, gbp_devhelp_view, IDE_TYPE_LAYOUT_VIEW)

static GParamSpec *properties [LAST_PROP];

void
gbp_devhelp_view_set_uri (GbpDevhelpView *self,
                          const gchar    *uri)
{
  g_return_if_fail (GBP_IS_DEVHELP_VIEW (self));

  if (uri == NULL)
    return;

  webkit_web_view_load_uri (self->web_view1, uri);
}

static const gchar *
gbp_devhelp_view_get_title (IdeLayoutView *view)
{
  GbpDevhelpView *self = (GbpDevhelpView *)view;

  g_assert (GBP_IS_DEVHELP_VIEW (view));

  return webkit_web_view_get_title (self->web_view1);
}

static void
gbp_devhelp_view_notify_title (GbpDevhelpView *self,
                               GParamSpec     *pspec,
                               WebKitWebView  *web_view)
{
  g_assert (GBP_IS_DEVHELP_VIEW (self));
  g_assert (WEBKIT_IS_WEB_VIEW (web_view));

  g_object_notify (G_OBJECT (self), "title");
}

static void
gbp_devhelp_view_set_property (GObject      *object,
                               guint         prop_id,
                               const GValue *value,
                               GParamSpec   *pspec)
{
  GbpDevhelpView *self = GBP_DEVHELP_VIEW (object);

  switch (prop_id)
    {
    case PROP_URI:
      gbp_devhelp_view_set_uri (self, g_value_get_string (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
gbp_devhelp_view_class_init (GbpDevhelpViewClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);
  IdeLayoutViewClass *view_class = IDE_LAYOUT_VIEW_CLASS (klass);

  object_class->set_property = gbp_devhelp_view_set_property;

  view_class->get_title = gbp_devhelp_view_get_title;

  properties [PROP_URI] =
    g_param_spec_string ("uri",
                         "Uri",
                         "The uri of the documentation.",
                         NULL,
                         (G_PARAM_WRITABLE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, LAST_PROP, properties);

  gtk_widget_class_set_template_from_resource (widget_class, "/org/gnome/builder/plugins/devhelp/gbp-devhelp-view.ui");
  gtk_widget_class_bind_template_child (widget_class, GbpDevhelpView, web_view1);

  g_type_ensure (WEBKIT_TYPE_WEB_VIEW);
}

static void
gbp_devhelp_view_init (GbpDevhelpView *self)
{
  gtk_widget_init_template (GTK_WIDGET (self));

  g_signal_connect_object (self->web_view1,
                           "notify::title",
                           G_CALLBACK (gbp_devhelp_view_notify_title),
                           self,
                           G_CONNECT_SWAPPED);
}
