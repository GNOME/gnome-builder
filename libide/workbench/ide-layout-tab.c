/* ide-layout-tab.c
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

#define G_LOG_DOMAIN "ide-layout-tab"

#include <dazzle.h>

#include "ide-macros.h"

#include "application/ide-application.h"
#include "workbench/ide-layout-view.h"
#include "workbench/ide-layout-tab.h"
#include "workbench/ide-layout-tab-private.h"

G_DEFINE_TYPE (IdeLayoutTab, ide_layout_tab, GTK_TYPE_EVENT_BOX)

enum {
  PROP_0,
  PROP_VIEW,
  LAST_PROP
};

static GParamSpec *properties [LAST_PROP];

static void
ide_layout_tab_connect (IdeLayoutTab *self)
{
  GBinding *binding;

  g_assert (IDE_IS_LAYOUT_TAB (self));

  binding = g_object_bind_property (self->view, "special-title",
                                    self->title_label, "label",
                                    G_BINDING_SYNC_CREATE);
  ide_set_weak_pointer (&self->title_binding, binding);

  binding = g_object_bind_property (self->view, "modified",
                                    self->modified_label, "visible",
                                    G_BINDING_SYNC_CREATE);
  ide_set_weak_pointer (&self->modified_binding, binding);

  g_signal_connect (self->view,
                    "destroy",
                    G_CALLBACK (gtk_widget_destroyed),
                    &self->view);

  gtk_widget_set_visible (self->close_button, TRUE);
}

static void
ide_layout_tab_disconnect (IdeLayoutTab *self)
{
  g_assert (IDE_IS_LAYOUT_TAB (self));

  g_signal_handlers_disconnect_by_func (self->view,
                                        G_CALLBACK (gtk_widget_destroyed),
                                        &self->view);

  if (self->title_binding)
    {
      g_binding_unbind (self->title_binding);
      ide_clear_weak_pointer (&self->title_binding);
    }

  gtk_label_set_text (GTK_LABEL (self->title_label), NULL);

  if (self->modified_binding)
    {
      g_binding_unbind (self->modified_binding);
      ide_clear_weak_pointer (&self->modified_binding);
    }

  gtk_widget_set_visible (self->modified_label, FALSE);
  gtk_widget_set_visible (self->close_button, FALSE);
}

GtkWidget *
ide_layout_tab_get_view (IdeLayoutTab *self)
{
  g_return_val_if_fail (IDE_IS_LAYOUT_TAB (self), NULL);

  return GTK_WIDGET (self->view);
}

void
ide_layout_tab_set_view (IdeLayoutTab *self,
                         GtkWidget    *view)
{
  g_return_if_fail (IDE_IS_LAYOUT_TAB (self));
  g_return_if_fail (!view || IDE_IS_LAYOUT_VIEW (view));

  if (view != (GtkWidget *)self->view)
    {
      if (self->view != NULL)
        {
          ide_layout_tab_disconnect (self);
          self->view = NULL;
        }

      if (view != NULL)
        {
          self->view = IDE_LAYOUT_VIEW (view);
          ide_layout_tab_connect (self);
        }

      g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_VIEW]);
    }
}

static gboolean
ide_layout_tab_enter_notify_event (GtkWidget        *widget,
                                   GdkEventCrossing *crossing)
{
  g_assert (GTK_IS_WIDGET (widget));
  g_assert (crossing != NULL);

  gtk_widget_set_state_flags (widget, GTK_STATE_FLAG_PRELIGHT, FALSE);

  return GDK_EVENT_PROPAGATE;
}

static gboolean
ide_layout_tab_leave_notify_event (GtkWidget        *widget,
                                   GdkEventCrossing *crossing)
{
  g_assert (GTK_IS_WIDGET (widget));
  g_assert (crossing != NULL);

  gtk_widget_unset_state_flags (widget, GTK_STATE_FLAG_PRELIGHT);

  return GDK_EVENT_PROPAGATE;
}

static void
ide_layout_tab_destroy (GtkWidget *widget)
{
  IdeLayoutTab *self = (IdeLayoutTab *)widget;

  if (self->view != NULL)
    {
      ide_layout_tab_disconnect (self);
      self->view = NULL;
    }

  GTK_WIDGET_CLASS (ide_layout_tab_parent_class)->destroy (widget);
}

static void
ide_layout_tab_get_property (GObject    *object,
                             guint       prop_id,
                             GValue     *value,
                             GParamSpec *pspec)
{
  IdeLayoutTab *self = IDE_LAYOUT_TAB (object);

  switch (prop_id)
    {
    case PROP_VIEW:
      g_value_set_object (value, ide_layout_tab_get_view (self));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
ide_layout_tab_set_property (GObject      *object,
                             guint         prop_id,
                             const GValue *value,
                             GParamSpec   *pspec)
{
  IdeLayoutTab *self = IDE_LAYOUT_TAB (object);

  switch (prop_id)
    {
    case PROP_VIEW:
      ide_layout_tab_set_view (self, g_value_get_object (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
ide_layout_tab_class_init (IdeLayoutTabClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  object_class->get_property = ide_layout_tab_get_property;
  object_class->set_property = ide_layout_tab_set_property;

  widget_class->destroy = ide_layout_tab_destroy;
  widget_class->enter_notify_event = ide_layout_tab_enter_notify_event;
  widget_class->leave_notify_event = ide_layout_tab_leave_notify_event;

  properties [PROP_VIEW] =
    g_param_spec_object ("view",
                         "View",
                         "The view to be represented by the tab",
                         IDE_TYPE_LAYOUT_VIEW,
                         (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, LAST_PROP, properties);

  gtk_widget_class_set_css_name (widget_class, "layouttab");
  gtk_widget_class_set_template_from_resource (widget_class, "/org/gnome/builder/ui/ide-layout-tab.ui");
  gtk_widget_class_bind_template_child (widget_class, IdeLayoutTab, backward_button);
  gtk_widget_class_bind_template_child (widget_class, IdeLayoutTab, close_button);
  gtk_widget_class_bind_template_child (widget_class, IdeLayoutTab, controls_container);
  gtk_widget_class_bind_template_child (widget_class, IdeLayoutTab, forward_button);
  gtk_widget_class_bind_template_child (widget_class, IdeLayoutTab, modified_label);
  gtk_widget_class_bind_template_child (widget_class, IdeLayoutTab, title_label);
  gtk_widget_class_bind_template_child (widget_class, IdeLayoutTab, title_menu_button);

  g_type_ensure (DZL_TYPE_PRIORITY_BOX);
}

static void
ide_layout_tab_init (IdeLayoutTab *self)
{
  GMenu *menu;
  GtkWidget *popover;

  gtk_widget_init_template (GTK_WIDGET (self));

  gtk_widget_add_events (GTK_WIDGET (self), GDK_ENTER_NOTIFY | GDK_LEAVE_NOTIFY);

  menu = dzl_application_get_menu_by_id (DZL_APPLICATION_DEFAULT, "ide-layout-stack-menu");
  popover = gtk_popover_new_from_model (self->title_menu_button, G_MENU_MODEL (menu));
  gtk_menu_button_set_popover (GTK_MENU_BUTTON (self->title_menu_button), popover);
}
