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
  GtkStack           *reload_stack;
  GtkCenterBox       *toolbar;
  IdeUrlBar          *url_bar;
  WebKitSettings     *web_settings;
  WebKitWebView      *web_view;

  IdeHtmlGenerator   *generator;

  guint               dirty : 1;
  guint               generating : 1;
  guint               disposed : 1;
} IdeWebkitPagePrivate;

enum {
  PROP_0,
  PROP_ENABLE_JAVASCRIPT,
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
on_toolbar_notify_visible_cb (IdeWebkitPage *self,
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

static gboolean
on_web_view_decide_policy_cb (IdeWebkitPage            *self,
                              WebKitPolicyDecision     *decision,
                              WebKitPolicyDecisionType  decision_type,
                              WebKitWebView            *web_view)
{
  IdeWebkitPagePrivate *priv = ide_webkit_page_get_instance_private (self);

  g_assert (IDE_IS_WEBKIT_PAGE (self));
  g_assert (WEBKIT_IS_POLICY_DECISION (decision));
  g_assert (WEBKIT_IS_WEB_VIEW (web_view));

  if (priv->generator == NULL)
    return FALSE;

  if (decision_type == WEBKIT_POLICY_DECISION_TYPE_NAVIGATION_ACTION)
    {
      WebKitNavigationAction *action = webkit_navigation_policy_decision_get_navigation_action (WEBKIT_NAVIGATION_POLICY_DECISION (decision));
      WebKitURIRequest *request = webkit_navigation_action_get_request (action);
      const char *uri = webkit_uri_request_get_uri (request);
      const char *base_uri = ide_html_generator_get_base_uri (priv->generator);

      if (!ide_str_equal0 (uri, base_uri))
        {
          GtkRoot *root = gtk_widget_get_root (GTK_WIDGET (self));
          ide_gtk_show_uri_on_window (GTK_WINDOW (root), uri, g_get_monotonic_time (), NULL);
          webkit_policy_decision_ignore (decision);
          return TRUE;
        }
    }

  return FALSE;
}

static void
go_forward_action (GtkWidget  *widget,
                   const char *action_name,
                   GVariant   *param)
{
  IDE_ENTRY;
  ide_webkit_page_go_forward (IDE_WEBKIT_PAGE (widget));
  IDE_EXIT;
}

static void
go_back_action (GtkWidget  *widget,
                const char *action_name,
                GVariant   *param)
{
  IDE_ENTRY;
  ide_webkit_page_go_back (IDE_WEBKIT_PAGE (widget));
  IDE_EXIT;
}

static void
reload_action (GtkWidget  *widget,
               const char *action_name,
               GVariant   *param)
{
  IdeWebkitPage *self = (IdeWebkitPage *)widget;
  IdeWebkitPagePrivate *priv = ide_webkit_page_get_instance_private (self);

  IDE_ENTRY;

  g_assert (IDE_IS_WEBKIT_PAGE (self));

  webkit_web_view_reload (priv->web_view);

  IDE_EXIT;
}

static void
stop_action (GtkWidget  *widget,
             const char *action_name,
             GVariant   *param)
{
  IdeWebkitPage *self = (IdeWebkitPage *)widget;
  IdeWebkitPagePrivate *priv = ide_webkit_page_get_instance_private (self);

  IDE_ENTRY;

  g_assert (IDE_IS_WEBKIT_PAGE (self));

  webkit_web_view_stop_loading (priv->web_view);

  IDE_EXIT;
}

static void
on_back_forward_list_changed_cb (IdeWebkitPage             *self,
                                 WebKitBackForwardListItem *item_added,
                                 const GList               *items_removed,
                                 WebKitBackForwardList     *list)
{
  IdeWebkitPagePrivate *priv = ide_webkit_page_get_instance_private (self);

  IDE_ENTRY;

  g_assert (IDE_IS_WEBKIT_PAGE (self));
  g_assert (WEBKIT_IS_BACK_FORWARD_LIST (list));

  panel_widget_action_set_enabled (PANEL_WIDGET (self),
                                   "web.go-forward",
                                   webkit_web_view_can_go_forward (priv->web_view));
  panel_widget_action_set_enabled (PANEL_WIDGET (self),
                                   "web.go-back",
                                   webkit_web_view_can_go_back (priv->web_view));

  IDE_EXIT;
}

static void
ide_webkit_page_update_reload (IdeWebkitPage *self)
{
  IdeWebkitPagePrivate *priv = ide_webkit_page_get_instance_private (self);
  const char *uri;
  gboolean loading;

  IDE_ENTRY;

  g_assert (IDE_IS_WEBKIT_PAGE (self));

  loading = webkit_web_view_is_loading (priv->web_view);
  uri = webkit_web_view_get_uri (priv->web_view);

  panel_widget_action_set_enabled (PANEL_WIDGET (self), "web.reload", !loading && !ide_str_empty0 (uri));
  panel_widget_action_set_enabled (PANEL_WIDGET (self), "web.stop", loading);

  if (loading)
    gtk_stack_set_visible_child_name (priv->reload_stack, "stop");
  else
    gtk_stack_set_visible_child_name (priv->reload_stack, "reload");

  IDE_EXIT;
}

static void
ide_webkit_page_print_action (GtkWidget  *widget,
                              const char *action_name,
                              GVariant   *param)
{
  ide_webkit_page_print (IDE_WEBKIT_PAGE (widget));
}

static void
enable_javascript_changed_cb (IdeWebkitPage  *self,
                              GParamSpec     *pspec,
                              WebKitSettings *web_settings)
{
  IDE_ENTRY;

  g_assert (IDE_IS_WEBKIT_PAGE (self));
  g_assert (WEBKIT_IS_SETTINGS (web_settings));

  g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_ENABLE_JAVASCRIPT]);

  IDE_EXIT;
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
ide_webkit_page_dispose (GObject *object)
{
  IdeWebkitPage *self = (IdeWebkitPage *)object;
  IdeWebkitPagePrivate *priv = ide_webkit_page_get_instance_private (self);

  priv->disposed = TRUE;

  g_clear_object (&priv->generator);

  G_OBJECT_CLASS (ide_webkit_page_parent_class)->dispose (object);
}

static void
ide_webkit_page_get_property (GObject    *object,
                              guint       prop_id,
                              GValue     *value,
                              GParamSpec *pspec)
{
  IdeWebkitPage *self = IDE_WEBKIT_PAGE (object);
  IdeWebkitPagePrivate *priv = ide_webkit_page_get_instance_private (self);

  switch (prop_id)
    {
    case PROP_ENABLE_JAVASCRIPT:
      g_value_set_boolean (value, webkit_settings_get_enable_javascript (priv->web_settings));
      break;

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
  IdeWebkitPagePrivate *priv = ide_webkit_page_get_instance_private (self);

  switch (prop_id)
    {
    case PROP_ENABLE_JAVASCRIPT:
      webkit_settings_set_enable_javascript (priv->web_settings, g_value_get_boolean (value));
      break;

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
  PanelWidgetClass *p_widget_class = PANEL_WIDGET_CLASS (klass);

  object_class->constructed = ide_webkit_page_constructed;
  object_class->dispose = ide_webkit_page_dispose;
  object_class->get_property = ide_webkit_page_get_property;
  object_class->set_property = ide_webkit_page_set_property;

  widget_class->grab_focus = ide_webkit_page_grab_focus;

  /**
   * IdeWebkitPage:enable-javascript:
   *
   * The "enable-javascript" allows disabling javascript within the webview.
   * It is also exported via the "web.enable-javascript" action (although
   * should generally be used with the "page." prefix to that action.
   *
   * Only available in 43.3.
   */
  properties [PROP_ENABLE_JAVASCRIPT] =
    g_param_spec_boolean ("enable-javascript", NULL, NULL,
                          TRUE,
                          (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  properties [PROP_SHOW_TOOLBAR] =
    g_param_spec_boolean ("show-toolbar",
                          "Show Toolbar",
                          "Show Toolbar",
                          TRUE,
                          (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, N_PROPS, properties);

  gtk_widget_class_set_template_from_resource (widget_class, "/plugins/webkit/ide-webkit-page.ui");

  gtk_widget_class_bind_template_child_private (widget_class, IdeWebkitPage, reload_stack);
  gtk_widget_class_bind_template_child_private (widget_class, IdeWebkitPage, toolbar);
  gtk_widget_class_bind_template_child_private (widget_class, IdeWebkitPage, url_bar);
  gtk_widget_class_bind_template_child_private (widget_class, IdeWebkitPage, web_settings);
  gtk_widget_class_bind_template_child_private (widget_class, IdeWebkitPage, web_view);
  gtk_widget_class_bind_template_callback (widget_class, on_toolbar_notify_visible_cb);
  gtk_widget_class_bind_template_callback (widget_class, ide_webkit_page_update_reload);
  gtk_widget_class_bind_template_callback (widget_class, on_web_view_decide_policy_cb);

  panel_widget_class_install_action (p_widget_class, "web.print", NULL, ide_webkit_page_print_action);
  panel_widget_class_install_action (p_widget_class, "web.go-forward", NULL, go_forward_action);
  panel_widget_class_install_action (p_widget_class, "web.go-back", NULL, go_back_action);
  panel_widget_class_install_action (p_widget_class, "web.reload", NULL, reload_action);
  panel_widget_class_install_action (p_widget_class, "web.stop", NULL, stop_action);
  panel_widget_class_install_property_action (p_widget_class, "web.enable-javascript", "enable-javascript");

  g_type_ensure (WEBKIT_TYPE_SETTINGS);
  g_type_ensure (WEBKIT_TYPE_WEB_VIEW);
  g_type_ensure (IDE_TYPE_URL_BAR);
}

static void
ide_webkit_page_init (IdeWebkitPage *self)
{
  IdeWebkitPagePrivate *priv = ide_webkit_page_get_instance_private (self);
  WebKitBackForwardList *list;

  panel_widget_set_can_maximize (PANEL_WIDGET (self), TRUE);

  gtk_widget_init_template (GTK_WIDGET (self));

  g_object_bind_property_full (priv->web_view, "title", self, "title", 0,
                               transform_title_with_fallback,
                               NULL, self, NULL);
  g_object_bind_property_full (priv->web_view, "favicon", self, "icon", 0,
                               transform_cairo_surface_to_gicon,
                               NULL, self, NULL);

  list = webkit_web_view_get_back_forward_list (priv->web_view);
  g_signal_connect_object (list,
                           "changed",
                           G_CALLBACK (on_back_forward_list_changed_cb),
                           self,
                           G_CONNECT_SWAPPED);

  panel_widget_action_set_enabled (PANEL_WIDGET (self), "web.go-forward", FALSE);
  panel_widget_action_set_enabled (PANEL_WIDGET (self), "web.go-back", FALSE);
  panel_widget_action_set_enabled (PANEL_WIDGET (self), "web.reload", FALSE);
  panel_widget_action_set_enabled (PANEL_WIDGET (self), "web.stop", FALSE);

  g_signal_connect_object (priv->web_settings,
                           "notify::enable-javascript",
                           G_CALLBACK (enable_javascript_changed_cb),
                           self,
                           G_CONNECT_SWAPPED);
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

void
ide_webkit_page_reload (IdeWebkitPage *self)
{
  IdeWebkitPagePrivate *priv = ide_webkit_page_get_instance_private (self);

  g_return_if_fail (IDE_IS_WEBKIT_PAGE (self));

  if (webkit_web_view_is_loading (priv->web_view))
    webkit_web_view_stop_loading (priv->web_view);

  webkit_web_view_reload (priv->web_view);
}

void
ide_webkit_page_reload_ignoring_cache (IdeWebkitPage *self)
{
  IdeWebkitPagePrivate *priv = ide_webkit_page_get_instance_private (self);

  g_return_if_fail (IDE_IS_WEBKIT_PAGE (self));

  if (webkit_web_view_is_loading (priv->web_view))
    webkit_web_view_stop_loading (priv->web_view);

  webkit_web_view_reload_bypass_cache (priv->web_view);
}

static void
ide_webkit_page_generate_cb (GObject      *object,
                             GAsyncResult *result,
                             gpointer      user_data)
{
  IdeHtmlGenerator *generator = (IdeHtmlGenerator *)object;
  g_autoptr(IdeWebkitPage) self = user_data;
  IdeWebkitPagePrivate *priv = ide_webkit_page_get_instance_private (self);
  g_autoptr(GError) error = NULL;
  g_autoptr(GBytes) bytes = NULL;

  g_assert (IDE_IS_HTML_GENERATOR (generator));
  g_assert (G_IS_ASYNC_RESULT (result));
  g_assert (IDE_IS_WEBKIT_PAGE (self));

  priv->generating = FALSE;

  if (!(bytes = ide_html_generator_generate_finish (generator, result, &error)))
    {
      /* Don't try to spin again in this case by checking dirty */
      g_warning ("Failed to generate HTML: %s", error->message);
      return;
    }

  if (priv->disposed)
    return;

  webkit_web_view_load_html (priv->web_view,
                             (const char *)g_bytes_get_data (bytes, NULL),
                             ide_html_generator_get_base_uri (generator));

  /* See if we need to run again, and check for re-entrantcy */
  if (priv->dirty && !priv->generating)
    {
      priv->dirty = FALSE;
      priv->generating = TRUE;
      ide_html_generator_generate_async (generator,
                                         NULL,
                                         ide_webkit_page_generate_cb,
                                         g_steal_pointer (&self));
    }
}

static void
ide_webkit_page_generator_invalidate_cb (IdeWebkitPage    *self,
                                         IdeHtmlGenerator *generator)
{
  IdeWebkitPagePrivate *priv = ide_webkit_page_get_instance_private (self);

  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (IDE_IS_WEBKIT_PAGE (self));
  g_assert (IDE_IS_HTML_GENERATOR (generator));

  priv->dirty = TRUE;

  if (priv->generating)
    return;

  priv->generating = TRUE;
  priv->dirty = FALSE;

  ide_html_generator_generate_async (generator,
                                     NULL,
                                     ide_webkit_page_generate_cb,
                                     g_object_ref (self));
}

IdeWebkitPage *
ide_webkit_page_new_for_generator (IdeHtmlGenerator *generator)
{
  IdeWebkitPage *self;
  IdeWebkitPagePrivate *priv;

  g_return_val_if_fail (IDE_IS_HTML_GENERATOR (generator), NULL);

  self = ide_webkit_page_new ();
  priv = ide_webkit_page_get_instance_private (self);

  priv->generator = g_object_ref (generator);
  g_signal_connect_object (priv->generator,
                           "invalidate",
                           G_CALLBACK (ide_webkit_page_generator_invalidate_cb),
                           self,
                           G_CONNECT_SWAPPED);
  ide_webkit_page_generator_invalidate_cb (self, generator);

  return self;
}

gboolean
ide_webkit_page_has_generator (IdeWebkitPage *self)
{
  IdeWebkitPagePrivate *priv = ide_webkit_page_get_instance_private (self);

  g_return_val_if_fail (IDE_IS_WEBKIT_PAGE (self), FALSE);

  return priv->generator != NULL;
}

/**
 * ide_webkit_page_get_view:
 * @self: a #IdeWebkitPage
 *
 * Gets the underlying #WebKitWebView.
 *
 * Returns: (transfer none): a #WebKitWebView
 */
GtkWidget *
ide_webkit_page_get_view (IdeWebkitPage *self)
{
  IdeWebkitPagePrivate *priv = ide_webkit_page_get_instance_private (self);

  g_return_val_if_fail (IDE_IS_WEBKIT_PAGE (self), NULL);

  return GTK_WIDGET (priv->web_view);
}

void
ide_webkit_page_print (IdeWebkitPage *self)
{
  IdeWebkitPagePrivate *priv = ide_webkit_page_get_instance_private (self);
  g_autoptr(WebKitPrintOperation) operation = NULL;
  GtkRoot *root;

  g_return_if_fail (IDE_IS_WEBKIT_PAGE (self));

  operation = webkit_print_operation_new (priv->web_view);
  root = gtk_widget_get_root (GTK_WIDGET (self));

  webkit_print_operation_run_dialog (operation, GTK_WINDOW (root));
}
