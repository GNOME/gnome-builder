/* gbp-buildui-stage-row.c
 *
 * Copyright 2017-2019 Christian Hergert <chergert@redhat.com>
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

#define G_LOG_DOMAIN "gbp-buildui-stage-row"

#include "config.h"

#include <dazzle.h>

#include "gbp-buildui-stage-row.h"

struct _GbpBuilduiStageRow
{
  GtkListBoxRow    parent_instance;

  IdeBuildStage   *stage;

  DzlBoldingLabel *label;
};

enum {
  PROP_0,
  PROP_STAGE,
  N_PROPS
};

G_DEFINE_TYPE (GbpBuilduiStageRow, gbp_buildui_stage_row, GTK_TYPE_LIST_BOX_ROW)

static GParamSpec *properties [N_PROPS];

static void
gbp_buildui_stage_row_notify_completed (GbpBuilduiStageRow *row,
                                      GParamSpec       *pspec,
                                      IdeBuildStage    *stage)
{
  g_assert (GBP_IS_BUILDUI_STAGE_ROW (row));
  g_assert (IDE_IS_BUILD_STAGE (stage));

  if (ide_build_stage_get_completed (stage))
    dzl_gtk_widget_add_style_class (GTK_WIDGET (row->label), "dim-label");
  else
    dzl_gtk_widget_remove_style_class (GTK_WIDGET (row->label), "dim-label");
}

static void
gbp_buildui_stage_row_set_stage (GbpBuilduiStageRow *self,
                               IdeBuildStage    *stage)
{
  const gchar *name;

  g_return_if_fail (GBP_IS_BUILDUI_STAGE_ROW (self));
  g_return_if_fail (IDE_IS_BUILD_STAGE (stage));

  g_set_object (&self->stage, stage);

  name = ide_build_stage_get_name (stage);

  if (name == NULL)
    name = G_OBJECT_TYPE_NAME (stage);

  gtk_label_set_label (GTK_LABEL (self->label), name);

  g_signal_connect_object (stage,
                           "notify::completed",
                           G_CALLBACK (gbp_buildui_stage_row_notify_completed),
                           self,
                           G_CONNECT_SWAPPED);

  g_object_bind_property (stage, "disabled", self, "sensitive", G_BINDING_DEFAULT);
  g_object_bind_property (stage, "active", self->label, "bold", G_BINDING_DEFAULT);

  gbp_buildui_stage_row_notify_completed (self, NULL, stage);
}

static void
gbp_buildui_stage_row_destroy (GtkWidget *widget)
{
  GbpBuilduiStageRow *self = (GbpBuilduiStageRow *)widget;

  g_clear_object (&self->stage);

  GTK_WIDGET_CLASS (gbp_buildui_stage_row_parent_class)->destroy (widget);
}

static void
gbp_buildui_stage_row_get_property (GObject    *object,
                                  guint       prop_id,
                                  GValue     *value,
                                  GParamSpec *pspec)
{
  GbpBuilduiStageRow *self = GBP_BUILDUI_STAGE_ROW (object);

  switch (prop_id)
    {
    case PROP_STAGE:
      g_value_set_object (value, gbp_buildui_stage_row_get_stage (self));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
gbp_buildui_stage_row_set_property (GObject      *object,
                                  guint         prop_id,
                                  const GValue *value,
                                  GParamSpec   *pspec)
{
  GbpBuilduiStageRow *self = GBP_BUILDUI_STAGE_ROW (object);

  switch (prop_id)
    {
    case PROP_STAGE:
      gbp_buildui_stage_row_set_stage (self, g_value_get_object (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
gbp_buildui_stage_row_class_init (GbpBuilduiStageRowClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  object_class->get_property = gbp_buildui_stage_row_get_property;
  object_class->set_property = gbp_buildui_stage_row_set_property;

  widget_class->destroy = gbp_buildui_stage_row_destroy;

  properties [PROP_STAGE] =
    g_param_spec_object ("stage",
                         "Stage",
                         "The stage for the row",
                         IDE_TYPE_BUILD_STAGE,
                         G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS);

  g_object_class_install_properties (object_class, N_PROPS, properties);

  gtk_widget_class_set_template_from_resource (widget_class,
                                               "/plugins/buildui/gbp-buildui-stage-row.ui");
  gtk_widget_class_bind_template_child (widget_class, GbpBuilduiStageRow, label);
}

static void
gbp_buildui_stage_row_init (GbpBuilduiStageRow *self)
{
  gtk_widget_init_template (GTK_WIDGET (self));
}

GtkWidget *
gbp_buildui_stage_row_new (IdeBuildStage *stage)
{
  g_return_val_if_fail (IDE_IS_BUILD_STAGE (stage), NULL);

  return g_object_new (GBP_TYPE_BUILDUI_STAGE_ROW,
                       "stage", stage,
                       "visible", TRUE,
                       NULL);
}

/**
 * gbp_buildui_stage_row_get_stage:
 * @self: a #GbpBuilduiStageRow
 *
 * Gets the stage for the row.
 *
 * Returns: (transfer none): an #IdeBuildStage
 *
 * Since: 3.32
 */
IdeBuildStage *
gbp_buildui_stage_row_get_stage (GbpBuilduiStageRow *self)
{
  g_return_val_if_fail (GBP_IS_BUILDUI_STAGE_ROW (self), NULL);

  return self->stage;
}
