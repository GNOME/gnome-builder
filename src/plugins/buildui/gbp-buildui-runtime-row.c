/* gbp-buildui-runtime-row.c
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

#define G_LOG_DOMAIN "gbp-buildui-runtime-row"

#include "config.h"

#include "gbp-buildui-runtime-row.h"

struct _GbpBuilduiRuntimeRow
{
  GtkListBoxRow  parent_instance;

  gchar         *runtime_id;

  GtkLabel      *label;
  GtkImage      *image;
};

G_DEFINE_TYPE (GbpBuilduiRuntimeRow, gbp_buildui_runtime_row, GTK_TYPE_LIST_BOX_ROW)

static void
gbp_buildui_runtime_row_finalize (GObject *object)
{
  GbpBuilduiRuntimeRow *self = (GbpBuilduiRuntimeRow *)object;

  g_clear_pointer (&self->runtime_id, g_free);

  G_OBJECT_CLASS (gbp_buildui_runtime_row_parent_class)->finalize (object);
}

static void
gbp_buildui_runtime_row_class_init (GbpBuilduiRuntimeRowClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = gbp_buildui_runtime_row_finalize;
}

static void
gbp_buildui_runtime_row_init (GbpBuilduiRuntimeRow *self)
{
  GtkWidget *box;

  box = g_object_new (GTK_TYPE_BOX,
                      "margin", 10,
                      "orientation", GTK_ORIENTATION_HORIZONTAL,
                      "spacing", 6,
                      "visible", TRUE,
                      NULL);
  gtk_container_add (GTK_CONTAINER (self), box);

  self->label = g_object_new (GTK_TYPE_LABEL,
                              "visible", TRUE,
                              "use-markup", TRUE,
                              "xalign", 0.0f,
                              NULL);
  gtk_container_add (GTK_CONTAINER (box), GTK_WIDGET (self->label));

  self->image = g_object_new (GTK_TYPE_IMAGE,
                              "visible", TRUE,
                              "halign", GTK_ALIGN_START,
                              "hexpand", TRUE,
                              "icon-name", "object-select-symbolic",
                              NULL);
  gtk_container_add (GTK_CONTAINER (box), GTK_WIDGET (self->image));
}

static void
notify_config_runtime_id (GbpBuilduiRuntimeRow *self,
                          GParamSpec           *pspec,
                          IdeConfiguration     *config)
{
  g_assert (GBP_IS_BUILDUI_RUNTIME_ROW (self));
  g_assert (IDE_IS_CONFIGURATION (config));

  gtk_widget_set_visible (GTK_WIDGET (self->image),
                          ide_str_equal0 (self->runtime_id,
                                          ide_configuration_get_runtime_id (config)));
}

GtkWidget *
gbp_buildui_runtime_row_new (IdeRuntime       *runtime,
                             IdeConfiguration *config)
{
  GbpBuilduiRuntimeRow *self;
  gboolean sensitive;

  g_return_val_if_fail (IDE_IS_RUNTIME (runtime), NULL);
  g_return_val_if_fail (IDE_IS_CONFIGURATION (config), NULL);

  sensitive = ide_configuration_supports_runtime (config, runtime);

  self = g_object_new (GBP_TYPE_BUILDUI_RUNTIME_ROW,
                       "sensitive", sensitive,
                       "visible", TRUE,
                       NULL);
  self->runtime_id = g_strdup (ide_runtime_get_id (runtime));
  gtk_label_set_label (self->label,
                       ide_runtime_get_display_name (runtime));

  g_signal_connect_object (config,
                           "notify::runtime-id",
                           G_CALLBACK (notify_config_runtime_id),
                           self,
                           G_CONNECT_SWAPPED);
  gtk_widget_set_visible (GTK_WIDGET (self->image),
                          ide_str_equal0 (self->runtime_id,
                                          ide_configuration_get_runtime_id (config)));

  return GTK_WIDGET (self);
}

const gchar *
gbp_buildui_runtime_row_get_id (GbpBuilduiRuntimeRow *self)
{
  g_return_val_if_fail (GBP_IS_BUILDUI_RUNTIME_ROW (self), NULL);

  return self->runtime_id;
}
