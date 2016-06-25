/* gbp-build-configuration-row.c
 *
 * Copyright (C) 2016 Christian Hergert <chergert@redhat.com>
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

#include "gbp-build-configuration-row.h"

struct _GbpBuildConfigurationRow
{
  GtkListBoxRow     parent_instance;

  IdeConfiguration *configuration;

  GtkLabel         *label;
  GtkImage         *radio;
  GtkButton        *delete;
  GtkButton        *duplicate;
  GtkStack         *controls;
};

enum {
  PROP_0,
  PROP_ACTIVE,
  PROP_CONFIGURATION,
  PROP_SELECTED,
  LAST_PROP
};

G_DEFINE_TYPE (GbpBuildConfigurationRow, gbp_build_configuration_row, GTK_TYPE_LIST_BOX_ROW)

static GParamSpec *properties [LAST_PROP];

static void
gbp_build_configuration_row_set_configuration (GbpBuildConfigurationRow *self,
                                               IdeConfiguration         *configuration)
{
  g_assert (GBP_IS_BUILD_CONFIGURATION_ROW (self));
  g_assert (IDE_IS_CONFIGURATION (configuration));

  if (g_set_object (&self->configuration, configuration))
    g_object_bind_property (configuration, "display-name",
                            self->label, "label",
                            G_BINDING_SYNC_CREATE);
}

static void
gbp_build_configuration_row_finalize (GObject *object)
{
  GbpBuildConfigurationRow *self = (GbpBuildConfigurationRow *)object;

  g_clear_object (&self->configuration);

  G_OBJECT_CLASS (gbp_build_configuration_row_parent_class)->finalize (object);
}

static void
gbp_build_configuration_row_get_property (GObject    *object,
                                          guint       prop_id,
                                          GValue     *value,
                                          GParamSpec *pspec)
{
  GbpBuildConfigurationRow *self = GBP_BUILD_CONFIGURATION_ROW (object);

  switch (prop_id)
    {
    case PROP_CONFIGURATION:
      g_value_set_object (value, gbp_build_configuration_row_get_configuration (self));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
gbp_build_configuration_row_set_property (GObject      *object,
                                          guint         prop_id,
                                          const GValue *value,
                                          GParamSpec   *pspec)
{
  GbpBuildConfigurationRow *self = GBP_BUILD_CONFIGURATION_ROW (object);

  switch (prop_id)
    {
    case PROP_CONFIGURATION:
      gbp_build_configuration_row_set_configuration (self, g_value_get_object (value));
      break;

    case PROP_ACTIVE:
      if (g_value_get_boolean (value))
        g_object_set (self->radio,
                      "icon-name", "radio-checked-symbolic",
                      NULL);
      else
        g_object_set (self->radio,
                      "icon-name", "radio-symbolic",
                      NULL);
      break;

    case PROP_SELECTED:
      if (g_value_get_boolean (value))
        gtk_stack_set_visible_child_name (self->controls, "controls");
      else
        gtk_stack_set_visible_child_name (self->controls, "empty");
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
gbp_build_configuration_row_class_init (GbpBuildConfigurationRowClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  object_class->finalize = gbp_build_configuration_row_finalize;
  object_class->get_property = gbp_build_configuration_row_get_property;
  object_class->set_property = gbp_build_configuration_row_set_property;

  properties [PROP_CONFIGURATION] =
    g_param_spec_object ("configuration",
                         "Configuration",
                         "The configuration this row represents",
                         IDE_TYPE_CONFIGURATION,
                         (G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS));

  properties [PROP_ACTIVE] =
    g_param_spec_boolean ("active",
                          "Active",
                          "If the row is the active configuration",
                          FALSE,
                          (G_PARAM_WRITABLE | G_PARAM_STATIC_STRINGS));

  properties [PROP_SELECTED] =
    g_param_spec_boolean ("selected",
                          "Selected",
                          "If the row is selected",
                          FALSE,
                          (G_PARAM_WRITABLE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, LAST_PROP, properties);

  gtk_widget_class_set_template_from_resource (widget_class, "/org/gnome/builder/plugins/build-tools-plugin/gbp-build-configuration-row.ui");
  gtk_widget_class_bind_template_child (widget_class, GbpBuildConfigurationRow, label);
  gtk_widget_class_bind_template_child (widget_class, GbpBuildConfigurationRow, duplicate);
  gtk_widget_class_bind_template_child (widget_class, GbpBuildConfigurationRow, delete);
  gtk_widget_class_bind_template_child (widget_class, GbpBuildConfigurationRow, radio);
  gtk_widget_class_bind_template_child (widget_class, GbpBuildConfigurationRow, controls);
}

static void
gbp_build_configuration_row_init (GbpBuildConfigurationRow *self)
{
  gtk_widget_init_template (GTK_WIDGET (self));
}

IdeConfiguration *
gbp_build_configuration_row_get_configuration (GbpBuildConfigurationRow *self)
{
  g_return_val_if_fail (GBP_IS_BUILD_CONFIGURATION_ROW (self), NULL);

  return self->configuration;
}
