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

#include "ide-url-bar.h"

struct _IdeUrlBar
{
  GtkWidget      parent_instance;

  /* Owned references */
  WebKitWebView *web_view;
  GBindingGroup *web_view_bindings;

  /* Template references */
  GtkStack      *stack;
  GtkLabel      *url_display;
  GtkText       *url_editable;
};

enum {
  PROP_0,
  PROP_WEB_VIEW,
  N_PROPS
};

G_DEFINE_FINAL_TYPE (IdeUrlBar, ide_url_bar, GTK_TYPE_WIDGET)

static GParamSpec *properties [N_PROPS];

static void
on_editable_focus_enter_cb (IdeUrlBar               *self,
                            GtkEventControllerFocus *focus)
{
  const char *uri;

  g_assert (IDE_IS_URL_BAR (self));
  g_assert (GTK_IS_EVENT_CONTROLLER_FOCUS (focus));

  if (self->web_view == NULL)
    return;

  uri = webkit_web_view_get_uri (self->web_view);
  gtk_editable_set_text (GTK_EDITABLE (self->url_editable), uri);
  gtk_editable_select_region (GTK_EDITABLE (self->url_editable), 0, -1);
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
  g_assert (IDE_IS_URL_BAR (self));
  g_assert (GTK_IS_EDITABLE (editable));

  if (self->web_view == NULL)
    return;

  webkit_web_view_load_uri (self->web_view,
                            gtk_editable_get_text (editable));
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
  if (g_strcmp0 (name, "edit") == 0)
    return;

  gtk_gesture_set_state (GTK_GESTURE (click), GTK_EVENT_SEQUENCE_CLAIMED);
  gtk_stack_set_visible_child_name (self->stack, "edit");
  gtk_widget_grab_focus (GTK_WIDGET (self->url_editable));
}

static void
ide_url_bar_dispose (GObject *object)
{
  IdeUrlBar *self = (IdeUrlBar *)object;

  g_clear_object (&self->web_view_bindings);
  g_clear_object (&self->web_view);

  g_clear_pointer ((GtkWidget **)&self->stack, gtk_widget_unparent);

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
  gtk_widget_class_bind_template_child (widget_class, IdeUrlBar, stack);
  gtk_widget_class_bind_template_callback (widget_class, on_click_gesture_pressed_cb);
  gtk_widget_class_bind_template_callback (widget_class, on_editable_focus_enter_cb);
  gtk_widget_class_bind_template_callback (widget_class, on_editable_focus_leave_cb);
  gtk_widget_class_bind_template_callback (widget_class, on_editable_activate_cb);
}

static void
ide_url_bar_init (IdeUrlBar *self)
{
  gtk_widget_init_template (GTK_WIDGET (self));

  self->web_view_bindings = g_binding_group_new ();
  g_binding_group_bind (self->web_view_bindings, "uri",
                        self->url_display, "label",
                        G_BINDING_SYNC_CREATE);
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
      gtk_widget_set_can_focus (GTK_WIDGET (self), web_view != NULL);
      g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_WEB_VIEW]);
    }
}
