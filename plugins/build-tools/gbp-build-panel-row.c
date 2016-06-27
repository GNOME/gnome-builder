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

#include <glib/gi18n.h>
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

gint
gbp_build_panel_row_compare (gconstpointer a,
                             gconstpointer b)
{
  const GbpBuildPanelRow *rowa = a;
  const GbpBuildPanelRow *rowb = b;

  return ide_diagnostic_compare (rowa->diagnostic, rowb->diagnostic);
}

static void
gbp_build_panel_row_set_diagnostic (GbpBuildPanelRow *self,
                                    IdeDiagnostic    *diagnostic)
{
  g_return_if_fail (GBP_IS_BUILD_PANEL_ROW (self));

  if ((diagnostic != NULL) && (self->diagnostic != diagnostic))
    {
      IdeSourceLocation *location;
      const gchar *path = NULL;
      IdeFile *file;
      const gchar *text;

      self->diagnostic = ide_diagnostic_ref (diagnostic);

      if ((location = ide_diagnostic_get_location (diagnostic)) &&
          (file = ide_source_location_get_file (location)))
        path = ide_file_get_path (file);

      if (path)
        gtk_label_set_label (self->file_label, path);
      else
        gtk_label_set_label (self->file_label, _("Unknown file"));

      text = ide_diagnostic_get_text (diagnostic);
      gtk_label_set_label (self->message_label, text);
    }
}

static void
gbp_build_panel_row_finalize (GObject *object)
{
  GbpBuildPanelRow *self = (GbpBuildPanelRow *)object;

  g_clear_pointer (&self->diagnostic, ide_diagnostic_unref);

  G_OBJECT_CLASS (gbp_build_panel_row_parent_class)->finalize (object);
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

  object_class->finalize = gbp_build_panel_row_finalize;
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
  gtk_widget_class_bind_template_child (widget_class, GbpBuildPanelRow, file_label);
  gtk_widget_class_bind_template_child (widget_class, GbpBuildPanelRow, message_label);
}

static void
gbp_build_panel_row_init (GbpBuildPanelRow *self)
{
  gtk_widget_init_template (GTK_WIDGET (self));
}

IdeDiagnostic *
gbp_build_panel_row_get_diagnostic (GbpBuildPanelRow *self)
{
  g_return_val_if_fail (GBP_IS_BUILD_PANEL_ROW (self), NULL);

  return self->diagnostic;
}
