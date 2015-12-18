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

#include "gbp-build-panel.h"
#include "gbp-build-panel-row.h"

struct _GbpBuildPanel
{
  GtkBin            parent_instance;

  IdeBuildResult   *result;
  EggSignalGroup   *signals;
  EggBindingGroup  *bindings;

  IdeDevice        *device;

  GtkListBox       *diagnostics;
  GtkRevealer      *status_revealer;
  GtkLabel         *status_label;
  GtkLabel         *running_time_label;
  GtkMenuButton    *device_button;
  GtkLabel         *device_label;
  GtkListBox       *devices;
  GtkPopover       *device_popover;

  guint             running_time_source;
};

G_DEFINE_TYPE (GbpBuildPanel, gbp_build_panel, GTK_TYPE_BIN)

enum {
  PROP_0,
  PROP_DEVICE,
  PROP_DEVICE_MANAGER,
  PROP_RESULT,
  LAST_PROP
};

static GParamSpec *properties [LAST_PROP];

static GtkWidget *
create_device_row (gpointer item,
                   gpointer user_data)
{
  GtkListBoxRow *row;
  IdeDevice *device = item;
  const gchar *type;
  const gchar *name;
  GtkLabel *label;
  gchar *str;

  g_assert (IDE_IS_DEVICE (device));

  row = g_object_new (GTK_TYPE_LIST_BOX_ROW,
                      "visible", TRUE,
                      NULL);

  g_object_set_data_full (G_OBJECT (row),
                          "IDE_DEVICE_ID",
                          g_strdup (ide_device_get_id (device)),
                          g_free);

  name = ide_device_get_display_name (device);
  type = ide_device_get_system_type (device);
  str = g_strdup_printf ("%s (%s)", name, type);

  label = g_object_new (GTK_TYPE_LABEL,
                        "label", str,
                        "xalign", 0.0f,
                        "visible", TRUE,
                        NULL);
  gtk_container_add (GTK_CONTAINER (row), GTK_WIDGET (label));

  g_free (str);

  return GTK_WIDGET (row);
}

static void
gbp_build_panel_set_device (GbpBuildPanel *self,
                            IdeDevice     *device)
{
  g_return_if_fail (GBP_IS_BUILD_PANEL (self));
  g_return_if_fail (!device || IDE_IS_DEVICE (device));

  if (g_set_object (&self->device, device))
    {
      const gchar *name = NULL;

      if (device != NULL)
        name = ide_device_get_display_name (device);
      gtk_label_set_label (self->device_label, name);
    }
}

static void
gbp_build_panel_set_device_manager (GbpBuildPanel    *self,
                                    IdeDeviceManager *device_manager)
{
  g_return_if_fail (GBP_IS_BUILD_PANEL (self));
  g_return_if_fail (!device_manager || IDE_IS_DEVICE_MANAGER (device_manager));

  gtk_list_box_bind_model (self->devices,
                           G_LIST_MODEL (device_manager),
                           create_device_row, NULL, NULL);
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
  GtkWidget *row;

  g_assert (GBP_IS_BUILD_PANEL (self));
  g_assert (diagnostic != NULL);
  g_assert (IDE_IS_BUILD_RESULT (result));

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
gbp_build_panel_device_activated (GbpBuildPanel *self,
                                  GtkListBoxRow *row,
                                  GtkListBox    *list_box)
{
  const gchar *id;

  g_assert (GBP_IS_BUILD_PANEL (self));
  g_assert (GTK_IS_LIST_BOX_ROW (row));
  g_assert (GTK_IS_LIST_BOX (list_box));

  if ((id = g_object_get_data (G_OBJECT (row), "IDE_DEVICE_ID")))
    ide_widget_action (GTK_WIDGET (self),
                       "build-tools", "device",
                       g_variant_new_string (id));

  gtk_widget_hide (GTK_WIDGET (self->device_popover));
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
  g_clear_object (&self->device);

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
    case PROP_DEVICE:
      g_value_set_object (value, self->device);
      break;

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
    case PROP_DEVICE:
      gbp_build_panel_set_device (self, g_value_get_object (value));
      break;

    case PROP_DEVICE_MANAGER:
      gbp_build_panel_set_device_manager (self, g_value_get_object (value));
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

  properties [PROP_DEVICE] =
    g_param_spec_object ("device",
                         "Device",
                         "Device",
                         IDE_TYPE_DEVICE,
                         (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  properties [PROP_DEVICE_MANAGER] =
    g_param_spec_object ("device-manager",
                         "Device Manager",
                         "Device Manager",
                         IDE_TYPE_DEVICE_MANAGER,
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
  gtk_widget_class_bind_template_child (widget_class, GbpBuildPanel, device_button);
  gtk_widget_class_bind_template_child (widget_class, GbpBuildPanel, device_label);
  gtk_widget_class_bind_template_child (widget_class, GbpBuildPanel, device_popover);
  gtk_widget_class_bind_template_child (widget_class, GbpBuildPanel, devices);
  gtk_widget_class_bind_template_child (widget_class, GbpBuildPanel, diagnostics);
  gtk_widget_class_bind_template_child (widget_class, GbpBuildPanel, running_time_label);
  gtk_widget_class_bind_template_child (widget_class, GbpBuildPanel, status_label);
  gtk_widget_class_bind_template_child (widget_class, GbpBuildPanel, status_revealer);
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

  g_signal_connect_object (self->devices,
                           "row-activated",
                           G_CALLBACK (gbp_build_panel_device_activated),
                           self,
                           G_CONNECT_SWAPPED);

  g_signal_connect_object (self->diagnostics,
                           "row-activated",
                           G_CALLBACK (gbp_build_panel_diagnostic_activated),
                           self,
                           G_CONNECT_SWAPPED);

  self->bindings = egg_binding_group_new ();

  egg_binding_group_bind (self->bindings, "mode",
                          self->status_label, "label",
                          G_BINDING_SYNC_CREATE);
}
