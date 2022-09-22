/* ide-url-bar.c
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

#define G_LOG_DOMAIN "ide-url-bar"

#include "config.h"

#include <libide-io.h>
#include <libide-gtk.h>

#include "ide-url-bar.h"
#include "ide-webkit-util.h"

struct _IdeUrlBar
{
  GtkWidget      parent_instance;

  /* Owned references */
  WebKitWebView *web_view;
  GBindingGroup *web_view_bindings;
  GSignalGroup  *web_view_signals;

  /* Weak References */
  IdeAnimation   *animation;

  /* Template references */
  GtkOverlay     *overlay;
  GtkStack       *stack;
  GtkInscription *url_display;
  GtkText        *url_editable;
  GtkProgressBar *load_progress;
  GtkImage       *security_image;
};

enum {
  PROP_0,
  PROP_WEB_VIEW,
  N_PROPS
};

G_DEFINE_FINAL_TYPE (IdeUrlBar, ide_url_bar, GTK_TYPE_WIDGET)

static GParamSpec *properties [N_PROPS];

static const char *
get_security_icon_name (IdeWebkitSecurityLevel security_level)
{
  switch (security_level)
    {
    case IDE_WEBKIT_SECURITY_LEVEL_LOCAL_PAGE:
    case IDE_WEBKIT_SECURITY_LEVEL_TO_BE_DETERMINED:
      return NULL;

    case IDE_WEBKIT_SECURITY_LEVEL_NONE:
    case IDE_WEBKIT_SECURITY_LEVEL_UNACCEPTABLE_CERTIFICATE:
      return "lock-small-open-symbolic";

    case IDE_WEBKIT_SECURITY_LEVEL_STRONG_SECURITY:
      return "lock-small-symbolic";

    default:
      return NULL;
    }
}

static void
on_web_view_load_changed_cb (IdeUrlBar       *self,
                             WebKitLoadEvent  load_event,
                             WebKitWebView   *web_view)
{
  g_assert (IDE_IS_URL_BAR (self));
  g_assert (WEBKIT_IS_WEB_VIEW (web_view));

  switch (load_event)
    {
    case WEBKIT_LOAD_COMMITTED:
    case WEBKIT_LOAD_FINISHED: {
      IdeWebkitSecurityLevel security_level;

      security_level = ide_webkit_util_get_security_level (web_view);
      g_object_set (self->security_image,
                    "icon-name", get_security_icon_name (security_level),
                    NULL);
      break;
    }

    case WEBKIT_LOAD_REDIRECTED:
    case WEBKIT_LOAD_STARTED:
      g_object_set (self->security_image,
                    "icon-name", "content-loading-symbolic",
                    NULL);
      break;

    default:
      break;
    }
}

static void
on_editable_focus_enter_cb (IdeUrlBar               *self,
                            GtkEventControllerFocus *focus)
{
  g_assert (IDE_IS_URL_BAR (self));
  g_assert (GTK_IS_EVENT_CONTROLLER_FOCUS (focus));

}

static void
on_editable_focus_leave_cb (IdeUrlBar               *self,
                            GtkEventControllerFocus *focus)
{
  g_assert (IDE_IS_URL_BAR (self));
  g_assert (GTK_IS_EVENT_CONTROLLER_FOCUS (focus));

  gtk_stack_set_visible_child_name (self->stack, "display");
}

static void
on_editable_activate_cb (IdeUrlBar   *self,
                         GtkEditable *editable)
{
  g_autofree char *expanded = NULL;
  g_autofree char *normalized = NULL;
  const char *uri;

  g_assert (IDE_IS_URL_BAR (self));
  g_assert (GTK_IS_EDITABLE (editable));

  if (self->web_view == NULL)
    return;

  uri = gtk_editable_get_text (editable);
  if (uri == NULL || uri[0] == 0)
    return;

  /* Expand ~/ access to home directory first */
  if (g_str_has_prefix (uri, "~/"))
    uri = expanded = ide_path_expand (uri);

  normalized = ide_webkit_util_normalize_address (uri);

  webkit_web_view_load_uri (self->web_view, normalized);
  gtk_stack_set_visible_child_name (self->stack, "display");
  gtk_widget_grab_focus (GTK_WIDGET (self->web_view));
}

static void
on_click_gesture_pressed_cb (IdeUrlBar       *self,
                             int              n_presses,
                             double           x,
                             double           y,
                             GtkGestureClick *click)
{
  const char *name;

  g_assert (IDE_IS_URL_BAR (self));
  g_assert (GTK_IS_GESTURE_CLICK (click));

  if (self->web_view == NULL)
    return;

  name = gtk_stack_get_visible_child_name (self->stack);

  /* On first click, just change to the text field immediately so that
   * we can propagate the event to that widget instead of the label.
   */
  if (n_presses == 1)
    {
      if (g_strcmp0 (name, "edit") != 0)
        {
          const char *uri = webkit_web_view_get_uri (self->web_view);

          gtk_editable_set_text (GTK_EDITABLE (self->url_editable), uri ? uri : "");
          gtk_stack_set_visible_child_name (self->stack, "edit");
          gtk_widget_grab_focus (GTK_WIDGET (self->url_editable));
          gtk_editable_select_region (GTK_EDITABLE (self->url_editable), 0, -1);
          gtk_gesture_set_state (GTK_GESTURE (click), GTK_EVENT_SEQUENCE_CLAIMED);
          return;
        }
    }

  gtk_gesture_set_state (GTK_GESTURE (click), GTK_EVENT_SEQUENCE_DENIED);
}

static void
on_web_view_notify_is_loading_cb (IdeUrlBar     *self,
                                  GParamSpec    *pspec,
                                  WebKitWebView *web_view)
{
  g_assert (IDE_IS_URL_BAR (self));
  g_assert (WEBKIT_IS_WEB_VIEW (web_view));

  if (webkit_web_view_is_loading (web_view))
    {
      gtk_progress_bar_set_fraction (self->load_progress, 0);
      gtk_widget_show (GTK_WIDGET (self->load_progress));
    }
  else
    {
      ide_gtk_widget_hide_with_fade (GTK_WIDGET (self->load_progress));
    }
}

static void
on_web_view_notify_estimated_load_progress_cb (IdeUrlBar     *self,
                                               GParamSpec    *pspec,
                                               WebKitWebView *web_view)
{
  IdeAnimation *anim;
  double progress;

  g_assert (IDE_IS_URL_BAR (self));
  g_assert (WEBKIT_IS_WEB_VIEW (web_view));

  progress = webkit_web_view_get_estimated_load_progress (web_view);

  /* First cancel any previous animation */
  if ((anim = self->animation))
    {
      g_clear_weak_pointer (&self->animation);
      ide_animation_stop (anim);
    }

  /* Short-circuit if we're not actively loading or we are jumping
   * backwards in progress instead of forwards.
   */
  if (!webkit_web_view_is_loading (web_view) ||
      progress < gtk_progress_bar_get_fraction (self->load_progress))
    {
      gtk_progress_bar_set_fraction (self->load_progress, progress);
      return;
    }

  anim = ide_object_animate (self->load_progress,
                             IDE_ANIMATION_LINEAR,
                             200,
                             NULL,
                             "fraction", progress,
                             NULL);
  g_set_weak_pointer (&self->animation, anim);
}

static gboolean
ide_url_bar_grab_focus (GtkWidget *widget)
{
  IdeUrlBar *self = (IdeUrlBar *)widget;

  g_assert (IDE_IS_URL_BAR (self));

  if (self->web_view == NULL)
    return FALSE;

  gtk_stack_set_visible_child_name (self->stack, "edit");
  gtk_widget_grab_focus (GTK_WIDGET (self->url_editable));
  gtk_editable_select_region (GTK_EDITABLE (self->url_editable), 0, -1);

  return TRUE;
}

static gboolean
focus_view_callback (GtkWidget *widget,
                     GVariant  *params,
                     gpointer   user_data)
{
  IdeUrlBar *self = (IdeUrlBar *)widget;

  g_assert (IDE_IS_URL_BAR (self));

  if (self->web_view != NULL)
    return gtk_widget_grab_focus (GTK_WIDGET (self->web_view));

  return FALSE;
}

static void
ide_url_bar_dispose (GObject *object)
{
  IdeUrlBar *self = (IdeUrlBar *)object;

  g_clear_object (&self->web_view_bindings);
  g_clear_object (&self->web_view_signals);
  g_clear_object (&self->web_view);

  g_clear_pointer ((GtkWidget **)&self->overlay, gtk_widget_unparent);

  G_OBJECT_CLASS (ide_url_bar_parent_class)->dispose (object);
}

static void
ide_url_bar_get_property (GObject    *object,
                          guint       prop_id,
                          GValue     *value,
                          GParamSpec *pspec)
{
  IdeUrlBar *self = IDE_URL_BAR (object);

  switch (prop_id)
    {
    case PROP_WEB_VIEW:
      g_value_set_object (value, ide_url_bar_get_web_view (self));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
ide_url_bar_set_property (GObject      *object,
                          guint         prop_id,
                          const GValue *value,
                          GParamSpec   *pspec)
{
  IdeUrlBar *self = IDE_URL_BAR (object);

  switch (prop_id)
    {
    case PROP_WEB_VIEW:
      ide_url_bar_set_web_view (self, g_value_get_object (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
ide_url_bar_class_init (IdeUrlBarClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  object_class->dispose = ide_url_bar_dispose;
  object_class->get_property = ide_url_bar_get_property;
  object_class->set_property = ide_url_bar_set_property;

  widget_class->grab_focus = ide_url_bar_grab_focus;

  properties [PROP_WEB_VIEW] =
    g_param_spec_object ("web-view", NULL, NULL,
                         WEBKIT_TYPE_WEB_VIEW,
                         (G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, N_PROPS, properties);

  gtk_widget_class_set_css_name (widget_class, "entry");
  gtk_widget_class_set_layout_manager_type (widget_class, GTK_TYPE_BIN_LAYOUT);
  gtk_widget_class_set_template_from_resource (widget_class, "/plugins/webkit/ide-url-bar.ui");
  gtk_widget_class_bind_template_child (widget_class, IdeUrlBar, url_display);
  gtk_widget_class_bind_template_child (widget_class, IdeUrlBar, url_editable);
  gtk_widget_class_bind_template_child (widget_class, IdeUrlBar, load_progress);
  gtk_widget_class_bind_template_child (widget_class, IdeUrlBar, overlay);
  gtk_widget_class_bind_template_child (widget_class, IdeUrlBar, security_image);
  gtk_widget_class_bind_template_child (widget_class, IdeUrlBar, stack);
  gtk_widget_class_bind_template_callback (widget_class, on_click_gesture_pressed_cb);
  gtk_widget_class_bind_template_callback (widget_class, on_editable_focus_enter_cb);
  gtk_widget_class_bind_template_callback (widget_class, on_editable_focus_leave_cb);
  gtk_widget_class_bind_template_callback (widget_class, on_editable_activate_cb);

  gtk_widget_class_add_binding (widget_class, GDK_KEY_Escape, 0, focus_view_callback, NULL);
}

static void
ide_url_bar_init (IdeUrlBar *self)
{
  gtk_widget_init_template (GTK_WIDGET (self));

  self->web_view_signals = g_signal_group_new (WEBKIT_TYPE_WEB_VIEW);
  g_signal_group_connect_object (self->web_view_signals,
                                 "notify::estimated-load-progress",
                                 G_CALLBACK (on_web_view_notify_estimated_load_progress_cb),
                                 self,
                                 G_CONNECT_SWAPPED);
  g_signal_group_connect_object (self->web_view_signals,
                                 "notify::is-loading",
                                 G_CALLBACK (on_web_view_notify_is_loading_cb),
                                 self,
                                 G_CONNECT_SWAPPED);
  g_signal_group_connect_object (self->web_view_signals,
                                 "load-changed",
                                 G_CALLBACK (on_web_view_load_changed_cb),
                                 self,
                                 G_CONNECT_SWAPPED);

  self->web_view_bindings = g_binding_group_new ();
  g_binding_group_bind (self->web_view_bindings, "uri",
                        self->url_display, "text",
                        G_BINDING_SYNC_CREATE);

  gtk_widget_set_cursor_from_name (GTK_WIDGET (self->url_display), "text");
}

WebKitWebView *
ide_url_bar_get_web_view (IdeUrlBar *self)
{
  g_return_val_if_fail (IDE_IS_URL_BAR (self), NULL);

  return self->web_view;
}

void
ide_url_bar_set_web_view (IdeUrlBar     *self,
                          WebKitWebView *web_view)
{
  g_return_if_fail (IDE_IS_URL_BAR (self));
  g_return_if_fail (!web_view || WEBKIT_IS_WEB_VIEW (web_view));

  if (g_set_object (&self->web_view, web_view))
    {
      g_binding_group_set_source (self->web_view_bindings, web_view);
      g_signal_group_set_target (self->web_view_signals, web_view);

      gtk_widget_hide (GTK_WIDGET (self->load_progress));
      gtk_widget_set_can_focus (GTK_WIDGET (self), web_view != NULL);
      g_object_set (self->security_image,
                    "icon-name", NULL,
                    NULL);

      if (self->web_view != NULL)
        {
          const char *uri = webkit_web_view_get_uri (self->web_view);

          gtk_editable_set_text (GTK_EDITABLE (self->url_editable), uri ? uri : "");

          if (gtk_widget_has_focus (GTK_WIDGET (self->url_editable)))
            gtk_editable_select_region (GTK_EDITABLE (self->url_editable), 0, -1);

          on_web_view_notify_estimated_load_progress_cb (self, NULL, self->web_view);

          /* TODO: Update security image if we ever share a url bar for multiple
           *       web views.
           */
        }

      g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_WEB_VIEW]);
    }
}
