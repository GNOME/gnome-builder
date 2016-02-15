/* gbp-build-panel.c
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

#include "egg-binding-group.h"
#include "egg-signal-group.h"

#include "gbp-build-configuration-row.h"
#include "gbp-build-panel.h"
#include "gbp-build-panel-row.h"

struct _GbpBuildPanel
{
  GtkBin            parent_instance;

  IdeBuildResult   *result;
  EggSignalGroup   *signals;
  EggBindingGroup  *bindings;

  GtkListBox       *configurations;
  GtkLabel         *configuration_label;
  GtkPopover       *configuration_popover;
  GtkListBox       *diagnostics;
  GtkLabel         *errors_label;
  GtkLabel         *running_time_label;
  GtkRevealer      *status_revealer;
  GtkLabel         *status_label;
  GtkLabel         *warnings_label;

  guint             running_time_source;

  guint             error_count;
  guint             warning_count;
};

G_DEFINE_TYPE (GbpBuildPanel, gbp_build_panel, GTK_TYPE_BIN)

enum {
  PROP_0,
  PROP_CONFIGURATION_MANAGER,
  PROP_RESULT,
  LAST_PROP
};

static GParamSpec *properties [LAST_PROP];

static gboolean
map_current_to_bool (GBinding     *binding,
                     const GValue *from_value,
                     GValue       *to_value,
                     gpointer      user_data)
{
  IdeConfiguration *configuration = user_data;
  IdeConfiguration *current;

  g_assert (IDE_IS_CONFIGURATION (configuration));

  current = g_value_get_object (from_value);
  g_value_set_boolean (to_value, (configuration == current));

  return TRUE;
}

static GtkWidget *
create_configuration_row (gpointer item,
                          gpointer user_data)
{
  IdeConfiguration *configuration = item;
  IdeConfigurationManager *manager = user_data;
  GtkWidget *ret;

  g_assert (IDE_IS_CONFIGURATION (configuration));
  g_assert (IDE_IS_CONFIGURATION_MANAGER (manager));

  ret = g_object_new (GBP_TYPE_BUILD_CONFIGURATION_ROW,
                      "configuration", configuration,
                      "visible", TRUE,
                      NULL);

  g_object_bind_property_full (manager, "current", ret, "selected",
                               G_BINDING_SYNC_CREATE,
                               map_current_to_bool, NULL,
                               g_object_ref (configuration), g_object_unref);

  return ret;
}

static void
gbp_build_panel_set_configuration_manager (GbpBuildPanel           *self,
                                           IdeConfigurationManager *configuration_manager)
{
  g_assert (GBP_IS_BUILD_PANEL (self));
  g_assert (IDE_IS_CONFIGURATION_MANAGER (configuration_manager));

  gtk_list_box_bind_model (self->configurations,
                           G_LIST_MODEL (configuration_manager),
                           create_configuration_row,
                           g_object_ref (configuration_manager),
                           g_object_unref);

  g_object_bind_property (configuration_manager, "current-display-name",
                          self->configuration_label, "label",
                          G_BINDING_SYNC_CREATE);
}

void
gbp_build_panel_add_error (GbpBuildPanel *self,
                           const gchar   *message)
{
  g_return_if_fail (GBP_IS_BUILD_PANEL (self));
  g_return_if_fail (message != NULL);

}

static void
gbp_build_panel_diagnostic (GbpBuildPanel  *self,
                            IdeDiagnostic  *diagnostic,
                            IdeBuildResult *result)
{
  IdeDiagnosticSeverity severity;
  GtkWidget *row;
  gchar *str;

  g_assert (GBP_IS_BUILD_PANEL (self));
  g_assert (diagnostic != NULL);
  g_assert (IDE_IS_BUILD_RESULT (result));

  severity = ide_diagnostic_get_severity (diagnostic);

  if (severity == IDE_DIAGNOSTIC_WARNING)
    {
      self->warning_count++;
      str = g_strdup_printf (ngettext ("%d warning", "%d warnings", self->warning_count), self->warning_count);
      gtk_label_set_label (self->warnings_label, str);
      g_free (str);
    }
  else if (severity == IDE_DIAGNOSTIC_ERROR)
    {
      self->error_count++;
      str = g_strdup_printf (ngettext ("%d error", "%d errors", self->error_count), self->error_count);
      gtk_label_set_label (self->errors_label, str);
      g_free (str);
    }

  row = g_object_new (GBP_TYPE_BUILD_PANEL_ROW,
                      "diagnostic", diagnostic,
                      "visible", TRUE,
                      NULL);
  gtk_container_add (GTK_CONTAINER (self->diagnostics), row);
}

static gboolean
gbp_build_panel_update_running_time (GbpBuildPanel *self)
{
  g_assert (GBP_IS_BUILD_PANEL (self));

  if (self->result != NULL)
    {
      GTimeSpan span;
      guint hours;
      guint minutes;
      guint seconds;
      gchar *text;

      span = ide_build_result_get_running_time (self->result);

      hours = span / G_TIME_SPAN_HOUR;
      minutes = (span % G_TIME_SPAN_HOUR) / G_TIME_SPAN_MINUTE;
      seconds = (span % G_TIME_SPAN_MINUTE) / G_TIME_SPAN_SECOND;

      text = g_strdup_printf ("%02u:%02u:%02u", hours, minutes, seconds);
      gtk_label_set_label (self->running_time_label, text);
      g_free (text);
    }

  return G_SOURCE_CONTINUE;
}

static void
gbp_build_panel_connect (GbpBuildPanel  *self,
                         IdeBuildResult *result)
{
  g_return_if_fail (GBP_IS_BUILD_PANEL (self));
  g_return_if_fail (IDE_IS_BUILD_RESULT (result));
  g_return_if_fail (self->result == NULL);

  self->result = g_object_ref (result);
  self->error_count = 0;
  self->warning_count = 0;

  gtk_label_set_label (self->warnings_label, "—");
  gtk_label_set_label (self->errors_label, "—");

  egg_signal_group_set_target (self->signals, result);
  egg_binding_group_set_source (self->bindings, result);

  if (ide_build_result_get_running (result))
    {
      gtk_label_set_label (self->running_time_label, NULL);
      self->running_time_source =
        g_timeout_add_seconds (1, (GSourceFunc)gbp_build_panel_update_running_time, self);
    }

  gtk_revealer_set_reveal_child (self->status_revealer, TRUE);
}

static void
gbp_build_panel_disconnect (GbpBuildPanel *self)
{
  g_return_if_fail (GBP_IS_BUILD_PANEL (self));

  gtk_revealer_set_reveal_child (self->status_revealer, FALSE);

  egg_signal_group_set_target (self->signals, NULL);
  egg_binding_group_set_source (self->bindings, NULL);
  ide_clear_source (&self->running_time_source);
  g_clear_object (&self->result);
}

void
gbp_build_panel_set_result (GbpBuildPanel  *self,
                            IdeBuildResult *result)
{
  g_return_if_fail (GBP_IS_BUILD_PANEL (self));
  g_return_if_fail (!result || IDE_IS_BUILD_RESULT (result));

  if (result != self->result)
    {
      if (self->result)
        gbp_build_panel_disconnect (self);

      if (result)
        gbp_build_panel_connect (self, result);

      gtk_container_foreach (GTK_CONTAINER (self->diagnostics),
                             (GtkCallback)gtk_widget_destroy,
                             NULL);
    }
}

static void
gbp_build_panel_notify_running (GbpBuildPanel  *self,
                                GParamSpec     *pspec,
                                IdeBuildResult *result)
{
  g_assert (GBP_IS_BUILD_PANEL (self));
  g_assert (IDE_IS_BUILD_RESULT (result));

  if (!ide_build_result_get_running (result))
    ide_clear_source (&self->running_time_source);

  gbp_build_panel_update_running_time (self);
}

static void
gbp_build_panel_configuration_activated (GbpBuildPanel *self,
                                         GtkListBoxRow *row,
                                         GtkListBox    *list_box)
{
  IdeConfigurationManager *manager;
  IdeConfiguration *config;
  IdeWorkbench *workbench;
  IdeContext *context;

  g_assert (GBP_IS_BUILD_PANEL (self));
  g_assert (GTK_IS_LIST_BOX_ROW (row));
  g_assert (GTK_IS_LIST_BOX (list_box));

  workbench = ide_widget_get_workbench (GTK_WIDGET (self));
  context = ide_workbench_get_context (workbench);
  manager = ide_context_get_configuration_manager (context);
  config = gbp_build_configuration_row_get_configuration (GBP_BUILD_CONFIGURATION_ROW (row));
  ide_configuration_manager_set_current (manager, config);

  gtk_widget_hide (GTK_WIDGET (self->configuration_popover));
}

static void
gbp_build_panel_diagnostic_activated (GbpBuildPanel *self,
                                      GtkListBoxRow *row,
                                      GtkListBox    *list_box)
{
  g_autoptr(IdeUri) uri = NULL;
  IdeDiagnostic *diagnostic;
  IdeSourceLocation *loc;
  IdeWorkbench *workbench;

  g_assert (GBP_IS_BUILD_PANEL (self));
  g_assert (GTK_IS_LIST_BOX_ROW (row));
  g_assert (GTK_IS_LIST_BOX (list_box));

  diagnostic = gbp_build_panel_row_get_diagnostic (GBP_BUILD_PANEL_ROW (row));
  if (diagnostic == NULL)
    return;

  loc = ide_diagnostic_get_location (diagnostic);
  if (loc == NULL)
    return;

  uri = ide_source_location_get_uri (loc);
  if (uri == NULL)
    return;

  workbench = ide_widget_get_workbench (GTK_WIDGET (self));
  ide_workbench_open_uri_async (workbench, uri, "editor", NULL, NULL, NULL);
}

static void
gbp_build_panel_destroy (GtkWidget *widget)
{
  GbpBuildPanel *self = (GbpBuildPanel *)widget;

  if (self->result)
    gbp_build_panel_disconnect (self);

  g_clear_object (&self->bindings);
  g_clear_object (&self->signals);

  GTK_WIDGET_CLASS (gbp_build_panel_parent_class)->destroy (widget);
}

static void
gbp_build_panel_get_property (GObject    *object,
                              guint       prop_id,
                              GValue     *value,
                              GParamSpec *pspec)
{
  GbpBuildPanel *self = GBP_BUILD_PANEL(object);

  switch (prop_id)
    {
    case PROP_RESULT:
      g_value_set_object (value, self->result);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
    }
}

static void
gbp_build_panel_set_property (GObject      *object,
                              guint         prop_id,
                              const GValue *value,
                              GParamSpec   *pspec)
{
  GbpBuildPanel *self = GBP_BUILD_PANEL(object);

  switch (prop_id)
    {
    case PROP_CONFIGURATION_MANAGER:
      gbp_build_panel_set_configuration_manager (self, g_value_get_object (value));
      break;

    case PROP_RESULT:
      gbp_build_panel_set_result (self, g_value_get_object (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
    }
}

static void
gbp_build_panel_class_init (GbpBuildPanelClass *klass)
{
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->get_property = gbp_build_panel_get_property;
  object_class->set_property = gbp_build_panel_set_property;

  widget_class->destroy = gbp_build_panel_destroy;

  properties [PROP_CONFIGURATION_MANAGER] =
    g_param_spec_object ("configuration-manager",
                         "Configuration Manager",
                         "Configuration Manager",
                         IDE_TYPE_CONFIGURATION_MANAGER,
                         (G_PARAM_WRITABLE | G_PARAM_STATIC_STRINGS));

  properties [PROP_RESULT] =
    g_param_spec_object ("result",
                         "Result",
                         "Result",
                         IDE_TYPE_BUILD_RESULT,
                         (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, LAST_PROP, properties);

  gtk_widget_class_set_template_from_resource (widget_class, "/org/gnome/builder/plugins/build-tools-plugin/gbp-build-panel.ui");
  gtk_widget_class_set_css_name (widget_class, "buildpanel");
  gtk_widget_class_bind_template_child (widget_class, GbpBuildPanel, configurations);
  gtk_widget_class_bind_template_child (widget_class, GbpBuildPanel, configuration_label);
  gtk_widget_class_bind_template_child (widget_class, GbpBuildPanel, configuration_popover);
  gtk_widget_class_bind_template_child (widget_class, GbpBuildPanel, diagnostics);
  gtk_widget_class_bind_template_child (widget_class, GbpBuildPanel, errors_label);
  gtk_widget_class_bind_template_child (widget_class, GbpBuildPanel, running_time_label);
  gtk_widget_class_bind_template_child (widget_class, GbpBuildPanel, status_label);
  gtk_widget_class_bind_template_child (widget_class, GbpBuildPanel, status_revealer);
  gtk_widget_class_bind_template_child (widget_class, GbpBuildPanel, warnings_label);
}

static void
gbp_build_panel_init (GbpBuildPanel *self)
{
  gtk_widget_init_template (GTK_WIDGET (self));

  self->signals = egg_signal_group_new (IDE_TYPE_BUILD_RESULT);

  egg_signal_group_connect_object (self->signals,
                                   "diagnostic",
                                   G_CALLBACK (gbp_build_panel_diagnostic),
                                   self,
                                   G_CONNECT_SWAPPED);

  egg_signal_group_connect_object (self->signals,
                                   "notify::running",
                                   G_CALLBACK (gbp_build_panel_notify_running),
                                   self,
                                   G_CONNECT_SWAPPED);

  g_signal_connect_object (self->configurations,
                           "row-activated",
                           G_CALLBACK (gbp_build_panel_configuration_activated),
                           self,
                           G_CONNECT_SWAPPED);

  g_signal_connect_object (self->diagnostics,
                           "row-activated",
                           G_CALLBACK (gbp_build_panel_diagnostic_activated),
                           self,
                           G_CONNECT_SWAPPED);

  self->bindings = egg_binding_group_new ();

  egg_binding_group_bind (self->bindings, "mode", self->status_label, "label",
                          G_BINDING_SYNC_CREATE);
}
