/* pnl-tab.c
 *
 * Copyright (C) 2016 Christian Hergert <chergert@redhat.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU Lesser General Public
 * License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 */

#include "pnl-tab.h"

struct _PnlTab
{
  GtkToggleButton  parent;
  GtkPositionType  edge : 2;
  GtkLabel        *title;
  GtkWidget       *widget;
};

G_DEFINE_TYPE (PnlTab, pnl_tab, GTK_TYPE_TOGGLE_BUTTON)

enum {
  PROP_0,
  PROP_EDGE,
  PROP_TITLE,
  PROP_WIDGET,
  N_PROPS
};

static GParamSpec *properties [N_PROPS];

static void
pnl_tab_destroy (GtkWidget *widget)
{
  PnlTab *self = (PnlTab *)widget;

  if (self->widget)
    {
      g_object_remove_weak_pointer (G_OBJECT (self->widget), (gpointer *)&self->widget);
      self->widget = NULL;
    }

  GTK_WIDGET_CLASS (pnl_tab_parent_class)->destroy (widget);
}

static void
pnl_tab_update_edge (PnlTab *self)
{
  g_assert (PNL_IS_TAB (self));

  switch (self->edge)
    {
    case GTK_POS_TOP:
      gtk_label_set_angle (self->title, 0.0);
      gtk_widget_set_hexpand (GTK_WIDGET (self), TRUE);
      gtk_widget_set_vexpand (GTK_WIDGET (self), FALSE);
      break;

    case GTK_POS_BOTTOM:
      gtk_label_set_angle (self->title, 0.0);
      gtk_widget_set_hexpand (GTK_WIDGET (self), TRUE);
      gtk_widget_set_vexpand (GTK_WIDGET (self), FALSE);
      break;

    case GTK_POS_LEFT:
      gtk_label_set_angle (self->title, -90.0);
      gtk_widget_set_hexpand (GTK_WIDGET (self), FALSE);
      gtk_widget_set_vexpand (GTK_WIDGET (self), TRUE);
      break;

    case GTK_POS_RIGHT:
      gtk_label_set_angle (self->title, 90.0);
      gtk_widget_set_hexpand (GTK_WIDGET (self), FALSE);
      gtk_widget_set_vexpand (GTK_WIDGET (self), TRUE);
      break;

    default:
      g_assert_not_reached ();
    }
}

static void
pnl_tab_get_property (GObject    *object,
                      guint       prop_id,
                      GValue     *value,
                      GParamSpec *pspec)
{
  PnlTab *self = PNL_TAB (object);

  switch (prop_id)
    {
    case PROP_EDGE:
      g_value_set_enum (value, pnl_tab_get_edge (self));
      break;

    case PROP_TITLE:
      g_value_set_string (value, pnl_tab_get_title (self));
      break;

    case PROP_WIDGET:
      g_value_set_object (value, pnl_tab_get_widget (self));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
pnl_tab_set_property (GObject      *object,
                      guint         prop_id,
                      const GValue *value,
                      GParamSpec   *pspec)
{
  PnlTab *self = PNL_TAB (object);

  switch (prop_id)
    {
    case PROP_EDGE:
      pnl_tab_set_edge (self, g_value_get_enum (value));
      break;

    case PROP_TITLE:
      pnl_tab_set_title (self, g_value_get_string (value));
      break;

    case PROP_WIDGET:
      pnl_tab_set_widget (self, g_value_get_object (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
pnl_tab_class_init (PnlTabClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  object_class->get_property = pnl_tab_get_property;
  object_class->set_property = pnl_tab_set_property;

  widget_class->destroy = pnl_tab_destroy;

  gtk_widget_class_set_css_name (widget_class, "tab");

  properties [PROP_EDGE] =
    g_param_spec_enum ("edge",
                       "Edge",
                       "Edge",
                       GTK_TYPE_POSITION_TYPE,
                       GTK_POS_TOP,
                       (G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS));

  properties [PROP_TITLE] =
    g_param_spec_string ("title",
                         "Title",
                         "Title",
                         NULL,
                         (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  properties [PROP_WIDGET] =
    g_param_spec_object ("widget",
                         "Widget",
                         "The widget the tab represents",
                         GTK_TYPE_WIDGET,
                         (G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, N_PROPS, properties);
}

static void
pnl_tab_init (PnlTab *self)
{
  self->edge = GTK_POS_TOP;

  gtk_widget_set_hexpand (GTK_WIDGET (self), TRUE);
  gtk_widget_set_vexpand (GTK_WIDGET (self), FALSE);

  self->title = g_object_new (GTK_TYPE_LABEL,
                              "ellipsize", PANGO_ELLIPSIZE_END,
                              "use-underline", TRUE,
                              "visible", TRUE,
                              NULL);

  gtk_container_add (GTK_CONTAINER (self), GTK_WIDGET (self->title));
}

const gchar *
pnl_tab_get_title (PnlTab *self)
{
  g_return_val_if_fail (PNL_IS_TAB (self), NULL);

  return gtk_label_get_label (self->title);
}

void
pnl_tab_set_title (PnlTab      *self,
                   const gchar *title)
{
  g_return_if_fail (PNL_IS_TAB (self));

  gtk_label_set_label (self->title, title);
}

GtkPositionType
pnl_tab_get_edge (PnlTab *self)
{
  g_return_val_if_fail (PNL_IS_TAB (self), 0);

  return self->edge;
}

void
pnl_tab_set_edge (PnlTab          *self,
                  GtkPositionType  edge)
{
  g_return_if_fail (PNL_IS_TAB (self));
  g_return_if_fail (edge >= 0);
  g_return_if_fail (edge <= 3);

  if (self->edge != edge)
    {
      self->edge = edge;
      pnl_tab_update_edge (self);
      g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_EDGE]);
    }
}

/**
 * pnl_tab_get_widget:
 *
 * Returns: (transfer none) (nullable): A #GtkWidget or %NULL.
 */
GtkWidget *
pnl_tab_get_widget (PnlTab *self)
{
  g_return_val_if_fail (PNL_IS_TAB (self), NULL);

  return self->widget;
}

void
pnl_tab_set_widget (PnlTab    *self,
                    GtkWidget *widget)
{
  g_return_if_fail (PNL_IS_TAB (self));

  if (self->widget != widget)
    {
      if (self->widget)
        g_object_remove_weak_pointer (G_OBJECT (self->widget), (gpointer *)&self->widget);

      self->widget = widget;

      if (widget)
        g_object_add_weak_pointer (G_OBJECT (self->widget), (gpointer *)&self->widget);

      gtk_label_set_mnemonic_widget (self->title, widget);

      g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_WIDGET]);
    }
}
