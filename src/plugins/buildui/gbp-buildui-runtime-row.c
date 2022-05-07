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
  AdwActionRow  parent_instance;

  gchar         *runtime_id;

  GtkImage      *image;
};

G_DEFINE_FINAL_TYPE (GbpBuilduiRuntimeRow, gbp_buildui_runtime_row, ADW_TYPE_ACTION_ROW)

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
  self->image = g_object_new (GTK_TYPE_IMAGE,
                              "valign", GTK_ALIGN_CENTER,
                              "icon-name", "object-select-symbolic",
                              NULL);
  adw_action_row_add_suffix (ADW_ACTION_ROW (self), GTK_WIDGET (self->image));
}

static void
notify_config_runtime_id (GbpBuilduiRuntimeRow *self,
                          GParamSpec           *pspec,
                          IdeConfig     *config)
{
  g_assert (GBP_IS_BUILDUI_RUNTIME_ROW (self));
  g_assert (IDE_IS_CONFIG (config));

  gtk_widget_set_visible (GTK_WIDGET (self->image),
                          ide_str_equal0 (self->runtime_id,
                                          ide_config_get_runtime_id (config)));
}

GtkWidget *
gbp_buildui_runtime_row_new (IdeRuntime *runtime,
                             IdeConfig  *config)
{
  GbpBuilduiRuntimeRow *self;
  gboolean sensitive;

  g_return_val_if_fail (IDE_IS_RUNTIME (runtime), NULL);
  g_return_val_if_fail (IDE_IS_CONFIG (config), NULL);

  sensitive = ide_config_supports_runtime (config, runtime);

  self = g_object_new (GBP_TYPE_BUILDUI_RUNTIME_ROW,
                       "sensitive", sensitive,
                       NULL);
  self->runtime_id = g_strdup (ide_runtime_get_id (runtime));
  adw_preferences_row_set_title (ADW_PREFERENCES_ROW (self),
                                 ide_runtime_get_display_name (runtime));

  g_signal_connect_object (config,
                           "notify::runtime-id",
                           G_CALLBACK (notify_config_runtime_id),
                           self,
                           G_CONNECT_SWAPPED);
  gtk_widget_set_visible (GTK_WIDGET (self->image),
                          ide_str_equal0 (self->runtime_id,
                                          ide_config_get_runtime_id (config)));

  return GTK_WIDGET (self);
}

const gchar *
gbp_buildui_runtime_row_get_id (GbpBuilduiRuntimeRow *self)
{
  g_return_val_if_fail (GBP_IS_BUILDUI_RUNTIME_ROW (self), NULL);

  return self->runtime_id;
}
