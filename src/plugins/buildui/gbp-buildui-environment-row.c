/* gbp-buildui-environment-row.c
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

#define G_LOG_DOMAIN "gbp-buildui-environment-row"

#include "config.h"

#include "gbp-buildui-environment-row.h"

struct _GbpBuilduiEnvironmentRow
{
  GtkListBoxRow parent_instance;
  GtkLabel *variable;
};

enum {
  PROP_0,
  PROP_VARIABLE,
  N_PROPS
};

enum {
  REMOVE,
  N_SIGNALS
};

G_DEFINE_FINAL_TYPE (GbpBuilduiEnvironmentRow, gbp_buildui_environment_row, GTK_TYPE_LIST_BOX_ROW)

static GParamSpec *properties [N_PROPS];
static guint signals [N_SIGNALS];

static void
variable_copy_action (GtkWidget  *widget,
                      const char *action_name,
                      GVariant   *param)
{
  GbpBuilduiEnvironmentRow *self = (GbpBuilduiEnvironmentRow *)widget;

  g_assert (GBP_IS_BUILDUI_ENVIRONMENT_ROW (self));

  gdk_clipboard_set_text (gtk_widget_get_clipboard (widget),
                          gbp_buildui_environment_row_get_variable (self));
}

static void
variable_remove_action (GtkWidget  *widget,
                        const char *action_name,
                        GVariant   *param)
{
  GbpBuilduiEnvironmentRow *self = (GbpBuilduiEnvironmentRow *)widget;

  g_assert (GBP_IS_BUILDUI_ENVIRONMENT_ROW (self));

  g_signal_emit (self, signals [REMOVE], 0);
}

static void
gbp_buildui_environment_row_get_property (GObject    *object,
                                          guint       prop_id,
                                          GValue     *value,
                                          GParamSpec *pspec)
{
  GbpBuilduiEnvironmentRow *self = GBP_BUILDUI_ENVIRONMENT_ROW (object);

  switch (prop_id)
    {
    case PROP_VARIABLE:
      g_value_set_string (value, gbp_buildui_environment_row_get_variable (self));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
gbp_buildui_environment_row_set_property (GObject      *object,
                                          guint         prop_id,
                                          const GValue *value,
                                          GParamSpec   *pspec)
{
  GbpBuilduiEnvironmentRow *self = GBP_BUILDUI_ENVIRONMENT_ROW (object);

  switch (prop_id)
    {
    case PROP_VARIABLE:
      gtk_label_set_label (self->variable, g_value_get_string (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
gbp_buildui_environment_row_class_init (GbpBuilduiEnvironmentRowClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  object_class->get_property = gbp_buildui_environment_row_get_property;
  object_class->set_property = gbp_buildui_environment_row_set_property;

  properties[PROP_VARIABLE] =
    g_param_spec_string ("variable", NULL, NULL,
                         NULL,
                         (G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, N_PROPS, properties);

  signals [REMOVE] =
    g_signal_new ("remove",
                  G_TYPE_FROM_CLASS (klass),
                  G_SIGNAL_RUN_LAST,
                  0,
                  NULL, NULL,
                  NULL,
                  G_TYPE_NONE, 0);

  gtk_widget_class_set_template_from_resource (widget_class, "/plugins/buildui/gbp-buildui-environment-row.ui");
  gtk_widget_class_bind_template_child (widget_class, GbpBuilduiEnvironmentRow, variable);

  gtk_widget_class_install_action (widget_class, "clipboard.copy", NULL, variable_copy_action);
  gtk_widget_class_install_action (widget_class, "variable.remove", NULL, variable_remove_action);
}

static void
gbp_buildui_environment_row_init (GbpBuilduiEnvironmentRow *self)
{
  gtk_widget_init_template (GTK_WIDGET (self));
}

GtkWidget *
gbp_buildui_environment_row_new (const char *variable)
{
  return g_object_new (GBP_TYPE_BUILDUI_ENVIRONMENT_ROW,
                       "variable", variable,
                       NULL);
}

const char *
gbp_buildui_environment_row_get_variable (GbpBuilduiEnvironmentRow *self)
{
  g_return_val_if_fail (GBP_IS_BUILDUI_ENVIRONMENT_ROW (self), NULL);

  return gtk_label_get_label (self->variable);
}
