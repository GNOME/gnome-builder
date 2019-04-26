/* gbp-glade-properties.c
 *
 * Copyright 2018-2019 Christian Hergert <chergert@redhat.com>
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

#define G_LOG_DOMAIN "gbp-glade-properties"

#include "config.h"

#include <glib/gi18n.h>

#include "gbp-glade-properties.h"

struct _GbpGladeProperties
{
  DzlDockWidget     parent_instance;
  GtkStackSwitcher *stack_switcher;
  GtkStack         *stack;
  DzlDockWidget    *a11y_dock;
  DzlDockWidget    *common_dock;
  DzlDockWidget    *general_dock;
  DzlDockWidget    *packing_dock;
};

G_DEFINE_TYPE (GbpGladeProperties, gbp_glade_properties, DZL_TYPE_DOCK_WIDGET)

static void
adjust_radio_hexpand (GtkWidget *widget,
                      gpointer   user_data)
{
  gtk_widget_set_hexpand (widget, TRUE);
}

static void
gbp_glade_properties_class_init (GbpGladePropertiesClass *klass)
{
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  gtk_widget_class_set_template_from_resource (widget_class, "/plugins/glade/gbp-glade-properties.ui");
  gtk_widget_class_set_css_name (widget_class, "gbpgladeproperties");
  gtk_widget_class_bind_template_child (widget_class, GbpGladeProperties, stack);
  gtk_widget_class_bind_template_child (widget_class, GbpGladeProperties, stack_switcher);
  gtk_widget_class_bind_template_child (widget_class, GbpGladeProperties, a11y_dock);
  gtk_widget_class_bind_template_child (widget_class, GbpGladeProperties, common_dock);
  gtk_widget_class_bind_template_child (widget_class, GbpGladeProperties, general_dock);
  gtk_widget_class_bind_template_child (widget_class, GbpGladeProperties, packing_dock);
}

static void
gbp_glade_properties_init (GbpGladeProperties *self)
{
  gtk_widget_init_template (GTK_WIDGET (self));

  dzl_dock_widget_set_title (DZL_DOCK_WIDGET (self), _("Unnamed Glade Project"));
  dzl_dock_widget_set_icon_name (DZL_DOCK_WIDGET (self), "org.gnome.Glade-symbolic");

  gtk_container_foreach (GTK_CONTAINER (self->stack_switcher), adjust_radio_hexpand, NULL);
}

void
gbp_glade_properties_set_widget (GbpGladeProperties *self,
                                 GladeWidget        *widget)
{
  GladeWidgetAdaptor *adapter;
  GladeEditable *editable;
  GladeWidget *parent;

  g_return_if_fail (GBP_IS_GLADE_PROPERTIES (self));
  g_return_if_fail (!widget || GLADE_IS_WIDGET (widget));

  gtk_container_foreach (GTK_CONTAINER (self->a11y_dock), (GtkCallback)gtk_widget_destroy, NULL);
  gtk_container_foreach (GTK_CONTAINER (self->general_dock), (GtkCallback)gtk_widget_destroy, NULL);
  gtk_container_foreach (GTK_CONTAINER (self->common_dock), (GtkCallback)gtk_widget_destroy, NULL);
  gtk_container_foreach (GTK_CONTAINER (self->packing_dock), (GtkCallback)gtk_widget_destroy, NULL);

  if (widget == NULL)
    return;

  adapter = glade_widget_get_adaptor (widget);

  /* General page */
  editable = glade_widget_adaptor_create_editable (adapter, GLADE_PAGE_GENERAL);
  gtk_container_add (GTK_CONTAINER (self->general_dock), GTK_WIDGET (editable));
  glade_editable_load (editable, widget);
  gtk_widget_show (GTK_WIDGET (editable));

  /* Packing page */
  if ((parent = glade_widget_get_parent (widget)))
    {
      GladeWidgetAdaptor *parent_adapter;

      parent_adapter = glade_widget_get_adaptor (parent);
      editable = glade_widget_adaptor_create_editable (parent_adapter, GLADE_PAGE_PACKING);
      gtk_container_add (GTK_CONTAINER (self->packing_dock), GTK_WIDGET (editable));
      glade_editable_load (editable, widget);
      gtk_widget_show (GTK_WIDGET (editable));
    }

  /* Common page */
  editable = glade_widget_adaptor_create_editable (adapter, GLADE_PAGE_COMMON);
  gtk_container_add (GTK_CONTAINER (self->common_dock), GTK_WIDGET (editable));
  glade_editable_load (editable, widget);
  gtk_widget_show (GTK_WIDGET (editable));

  /* A11y page */
  editable = glade_widget_adaptor_create_editable (adapter, GLADE_PAGE_ATK);
  gtk_container_add (GTK_CONTAINER (self->a11y_dock), GTK_WIDGET (editable));
  glade_editable_load (editable, widget);
  gtk_widget_show (GTK_WIDGET (editable));
}
