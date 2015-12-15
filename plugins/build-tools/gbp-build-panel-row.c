/* gbp-build-panel-row.c
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

#include <ide.h>

#include "gbp-build-panel-row.h"

struct _GbpBuildPanelRow
{
  GtkListBoxRow  parent_instance;

  GtkLabel      *file_label;
  GtkLabel      *message_label;

  IdeDiagnostic *diagnostic;
};

G_DEFINE_TYPE (GbpBuildPanelRow, gbp_build_panel_row, GTK_TYPE_LIST_BOX_ROW)

enum {
  PROP_0,
  PROP_DIAGNOSTIC,
  LAST_PROP
};

static GParamSpec *properties [LAST_PROP];

static void
gbp_build_panel_row_set_diagnostic (GbpBuildPanelRow *self,
                                    IdeDiagnostic    *diagnostic)
{
  gchar *text;

  g_return_if_fail (GBP_IS_BUILD_PANEL_ROW (self));
  g_return_if_fail (diagnostic != NULL);

  text = ide_diagnostic_get_text_for_display (diagnostic);
  gtk_label_set_label (self->message_label, text);
  g_free (text);
}

static void
gbp_build_panel_row_get_property (GObject    *object,
                                  guint       prop_id,
                                  GValue     *value,
                                  GParamSpec *pspec)
{
  GbpBuildPanelRow *self = GBP_BUILD_PANEL_ROW(object);

  switch (prop_id)
    {
    case PROP_DIAGNOSTIC:
      g_value_set_boxed (value, self->diagnostic);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
    }
}

static void
gbp_build_panel_row_set_property (GObject      *object,
                                  guint         prop_id,
                                  const GValue *value,
                                  GParamSpec   *pspec)
{
  GbpBuildPanelRow *self = GBP_BUILD_PANEL_ROW(object);

  switch (prop_id)
    {
    case PROP_DIAGNOSTIC:
      gbp_build_panel_row_set_diagnostic (self, g_value_get_boxed (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
    }
}

static void
gbp_build_panel_row_class_init (GbpBuildPanelRowClass *klass)
{
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->get_property = gbp_build_panel_row_get_property;
  object_class->set_property = gbp_build_panel_row_set_property;

  properties [PROP_DIAGNOSTIC] =
    g_param_spec_boxed ("diagnostic",
                        "Diagnostic",
                        "Diagnostic",
                        IDE_TYPE_DIAGNOSTIC,
                        (G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, LAST_PROP, properties);

  gtk_widget_class_set_template_from_resource (widget_class, "/org/gnome/builder/plugins/build-tools-plugin/gbp-build-panel-row.ui");
  gtk_widget_class_set_css_name (widget_class, "buildpanelrow");
  gtk_widget_class_bind_template_child (widget_class, GbpBuildPanelRow, file_label);
  gtk_widget_class_bind_template_child (widget_class, GbpBuildPanelRow, message_label);
}

static void
gbp_build_panel_row_init (GbpBuildPanelRow *self)
{
  gtk_widget_init_template (GTK_WIDGET (self));
}
