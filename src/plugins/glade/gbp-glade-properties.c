/* gbp-glade-properties.c
 *
 * Copyright 2018 Christian Hergert <chergert@redhat.com>
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

#define G_LOG_DOMAIN "gbp-glade-properties"

#include <glib/gi18n.h>

#include "gbp-glade-properties.h"

struct _GbpGladeProperties
{
  DzlDockWidget parent_instance;
  GladeEditor *editor;
};

G_DEFINE_TYPE (GbpGladeProperties, gbp_glade_properties, DZL_TYPE_DOCK_WIDGET)

static void
gbp_glade_properties_class_init (GbpGladePropertiesClass *klass)
{
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  gtk_widget_class_set_template_from_resource (widget_class, "/org/gnome/builder/plugins/glade-plugin/gbp-glade-properties.ui");
  gtk_widget_class_set_css_name (widget_class, "gbpgladeproperties");
  gtk_widget_class_bind_template_child (widget_class, GbpGladeProperties, editor);
}

static void
gbp_glade_properties_init (GbpGladeProperties *self)
{
  gtk_widget_init_template (GTK_WIDGET (self));

  dzl_dock_widget_set_title (DZL_DOCK_WIDGET (self), _("Unnamed Glade Project"));
  dzl_dock_widget_set_icon_name (DZL_DOCK_WIDGET (self), "glade-symbolic");
}

void
gbp_glade_properties_set_widget (GbpGladeProperties *self,
                                 GladeWidget        *widget)
{
  g_return_if_fail (GBP_IS_GLADE_PROPERTIES (self));
  g_return_if_fail (!widget || GLADE_IS_WIDGET (widget));

  if (widget != NULL)
    glade_editor_load_widget (self->editor, widget);
}
