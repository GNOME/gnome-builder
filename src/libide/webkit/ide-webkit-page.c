/* ide-webkit-page.c
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

#define G_LOG_DOMAIN "ide-webkit-page"

#include "config.h"

#include <webkit2/webkit2.h>

#include "ide-webkit-page.h"
#include "ide-url-bar.h"

typedef struct
{
  GtkCenterBox  *toolbar;
  IdeUrlBar     *url_bar;
  WebKitWebView *web_view;
} IdeWebkitPagePrivate;

enum {
  PROP_0,
  PROP_SHOW_TOOLBAR,
  N_PROPS
};

G_DEFINE_TYPE_WITH_PRIVATE (IdeWebkitPage, ide_webkit_page, IDE_TYPE_PAGE)

static GParamSpec *properties [N_PROPS];

static gboolean
transform_title_with_fallback (GBinding     *binding,
                                  const GValue *from_value,
                                  GValue       *to_value,
                                  gpointer      user_data)
{
  IdeWebkitPage *self = user_data;
  IdeWebkitPagePrivate *priv = ide_webkit_page_get_instance_private (self);
  const char *title;

  g_assert (G_IS_BINDING (binding));
  g_assert (G_VALUE_HOLDS_STRING (from_value));
  g_assert (G_VALUE_HOLDS_STRING (to_value));
  g_assert (IDE_IS_WEBKIT_PAGE (self));

  title = g_value_get_string (from_value);
  if (ide_str_empty0 (title))
    title = webkit_web_view_get_uri (priv->web_view);
  g_value_set_string (to_value, title);
  return TRUE;
}

static gboolean
transform_cairo_surface_to_gicon (GBinding     *binding,
                                  const GValue *from_value,
                                  GValue       *to_value,
                                  gpointer      user_data)
{
  IdeWebkitPage *self = user_data;
  cairo_surface_t *surface;
  GdkPixbuf *pixbuf;
  int favicon_width;
  int favicon_height;
  int width;
  int height;

  g_assert (G_IS_BINDING (binding));
  g_assert (G_VALUE_HOLDS_POINTER (from_value));
  g_assert (G_VALUE_HOLDS_OBJECT (to_value));
  g_assert (IDE_IS_WEBKIT_PAGE (self));

  /* No ownership transfer */
  surface = g_value_get_pointer (from_value);

  if (surface == NULL)
    {
      g_value_take_object (to_value, g_themed_icon_new ("web-browser-symbolic"));
      return TRUE;
    }

  width = 16 * gtk_widget_get_scale_factor (GTK_WIDGET (self));
  height = 16 * gtk_widget_get_scale_factor (GTK_WIDGET (self));
  favicon_width = cairo_image_surface_get_width (surface);
  favicon_height = cairo_image_surface_get_height (surface);
  pixbuf = gdk_pixbuf_get_from_surface (surface, 0, 0, favicon_width, favicon_height);

  if ((favicon_width != width || favicon_height != height))
    {
      GdkPixbuf *scaled_pixbuf = gdk_pixbuf_scale_simple (pixbuf, width, height, GDK_INTERP_BILINEAR);
      g_object_unref (pixbuf);
      pixbuf = scaled_pixbuf;
    }

  g_assert (!pixbuf || G_IS_ICON (pixbuf));

  g_value_take_object (to_value, pixbuf);

  return TRUE;
}

static void
toolbar_notify_visible_cb (IdeWebkitPage *self,
                           GParamSpec    *pspec,
                           GtkWidget     *toolbar)
{
  g_assert (IDE_IS_WEBKIT_PAGE (self));
  g_assert (GTK_IS_WIDGET (toolbar));

  g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_SHOW_TOOLBAR]);
}

static gboolean
ide_webkit_page_grab_focus (GtkWidget *widget)
{
  IdeWebkitPage *self = (IdeWebkitPage *)widget;
  IdeWebkitPagePrivate *priv = ide_webkit_page_get_instance_private (self);
  const char *uri;

  g_assert (IDE_IS_WEBKIT_PAGE (self));

  uri = webkit_web_view_get_uri (priv->web_view);

  if (ide_str_empty0 (uri))
    return gtk_widget_grab_focus (GTK_WIDGET (priv->url_bar));
  else
    return gtk_widget_grab_focus (GTK_WIDGET (priv->web_view));
}

static void
ide_webkit_page_constructed (GObject *object)
{
  IdeWebkitPage *self = (IdeWebkitPage *)object;
  IdeWebkitPagePrivate *priv = ide_webkit_page_get_instance_private (self);
  GtkStyleContext *context;
  GdkRGBA color;

  G_OBJECT_CLASS (ide_webkit_page_parent_class)->constructed (object);

  context = gtk_widget_get_style_context (GTK_WIDGET (priv->web_view));
  if (gtk_style_context_lookup_color (context, "theme_base_color", &color))
    webkit_web_view_set_background_color (WEBKIT_WEB_VIEW (priv->web_view), &color);
}

static void
ide_webkit_page_get_property (GObject    *object,
                              guint       prop_id,
                              GValue     *value,
                              GParamSpec *pspec)
{
  IdeWebkitPage *self = IDE_WEBKIT_PAGE (object);

  switch (prop_id)
    {
    case PROP_SHOW_TOOLBAR:
      g_value_set_boolean (value, ide_webkit_page_get_show_toolbar (self));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
ide_webkit_page_set_property (GObject      *object,
                              guint         prop_id,
                              const GValue *value,
                              GParamSpec   *pspec)
{
  IdeWebkitPage *self = IDE_WEBKIT_PAGE (object);

  switch (prop_id)
    {
    case PROP_SHOW_TOOLBAR:
      ide_webkit_page_set_show_toolbar (self, g_value_get_boolean (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
ide_webkit_page_class_init (IdeWebkitPageClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  object_class->constructed = ide_webkit_page_constructed;
  object_class->get_property = ide_webkit_page_get_property;
  object_class->set_property = ide_webkit_page_set_property;

  widget_class->grab_focus = ide_webkit_page_grab_focus;

  properties [PROP_SHOW_TOOLBAR] =
    g_param_spec_boolean ("show-toolbar",
                          "Show Toolbar",
                          "Show Toolbar",
                          TRUE,
                          (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, N_PROPS, properties);

  gtk_widget_class_set_template_from_resource (widget_class, "/plugins/webkit/ide-webkit-page.ui");

  gtk_widget_class_bind_template_child_private (widget_class, IdeWebkitPage, toolbar);
  gtk_widget_class_bind_template_child_private (widget_class, IdeWebkitPage, url_bar);
  gtk_widget_class_bind_template_child_private (widget_class, IdeWebkitPage, web_view);
  gtk_widget_class_bind_template_callback (widget_class, toolbar_notify_visible_cb);

  g_type_ensure (WEBKIT_TYPE_WEB_VIEW);
  g_type_ensure (IDE_TYPE_URL_BAR);
}

static void
ide_webkit_page_init (IdeWebkitPage *self)
{
  IdeWebkitPagePrivate *priv = ide_webkit_page_get_instance_private (self);

  gtk_widget_init_template (GTK_WIDGET (self));

  g_object_bind_property_full (priv->web_view, "title", self, "title", 0,
                               transform_title_with_fallback,
                               NULL, self, NULL);
  g_object_bind_property_full (priv->web_view, "favicon", self, "icon", 0,
                               transform_cairo_surface_to_gicon,
                               NULL, self, NULL);
}

IdeWebkitPage *
ide_webkit_page_new (void)
{
  return g_object_new (IDE_TYPE_WEBKIT_PAGE, NULL);
}

void
ide_webkit_page_load_uri (IdeWebkitPage *self,
                          const char    *uri)
{
  IdeWebkitPagePrivate *priv = ide_webkit_page_get_instance_private (self);

  g_return_if_fail (IDE_IS_WEBKIT_PAGE (self));
  g_return_if_fail (uri != NULL);

  webkit_web_view_load_uri (priv->web_view, uri);
}

gboolean
ide_webkit_page_get_show_toolbar (IdeWebkitPage *self)
{
  IdeWebkitPagePrivate *priv = ide_webkit_page_get_instance_private (self);

  g_return_val_if_fail (IDE_IS_WEBKIT_PAGE (self), FALSE);

  return gtk_widget_get_visible (GTK_WIDGET (priv->toolbar));
}

void
ide_webkit_page_set_show_toolbar (IdeWebkitPage *self,
                                  gboolean       show_toolbar)
{
  IdeWebkitPagePrivate *priv = ide_webkit_page_get_instance_private (self);

  g_return_if_fail (IDE_IS_WEBKIT_PAGE (self));

  gtk_widget_set_visible (GTK_WIDGET (priv->toolbar), show_toolbar);
}

gboolean
ide_webkit_page_focus_address (IdeWebkitPage *self)
{
  IdeWebkitPagePrivate *priv = ide_webkit_page_get_instance_private (self);

  g_return_val_if_fail (IDE_IS_WEBKIT_PAGE (self), FALSE);

  return gtk_widget_grab_focus (GTK_WIDGET (priv->url_bar));
}

void
ide_webkit_page_go_back (IdeWebkitPage *self)
{
  IdeWebkitPagePrivate *priv = ide_webkit_page_get_instance_private (self);
  WebKitBackForwardList *list;
  WebKitBackForwardListItem *item;

  IDE_ENTRY;

  g_return_if_fail (IDE_IS_WEBKIT_PAGE (self));

  list = webkit_web_view_get_back_forward_list (priv->web_view);
  item = webkit_back_forward_list_get_back_item (list);

  g_return_if_fail (item != NULL);

  webkit_web_view_go_to_back_forward_list_item (priv->web_view, item);

  IDE_EXIT;
}

void
ide_webkit_page_go_forward (IdeWebkitPage *self)
{
  IdeWebkitPagePrivate *priv = ide_webkit_page_get_instance_private (self);
  WebKitBackForwardList *list;
  WebKitBackForwardListItem *item;

  IDE_ENTRY;

  g_return_if_fail (IDE_IS_WEBKIT_PAGE (self));

  list = webkit_web_view_get_back_forward_list (priv->web_view);
  item = webkit_back_forward_list_get_forward_item (list);

  g_return_if_fail (item != NULL);

  webkit_web_view_go_to_back_forward_list_item (priv->web_view, item);

  IDE_EXIT;
}
