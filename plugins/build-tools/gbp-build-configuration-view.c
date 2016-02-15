/* gbp-build-configuration-view.c
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

#include <ide.h>
#include <string.h>

#include "gbp-build-configuration-view.h"

#include "ide-internal.h"

struct _GbpBuildConfigurationView
{
  EggColumnLayout       parent_instance;

  IdeConfiguration     *configuration;

  GBinding             *configure_binding;
  GBinding             *display_name_binding;
  GBinding             *prefix_binding;

  GtkEntry             *configure_entry;
  GtkListBox           *device_list_box;
  GtkEntry             *display_name_entry;
  IdeEnvironmentEditor *environment_editor;
  GtkEntry             *prefix_entry;
  GtkListBox           *runtime_list_box;
};

enum {
  PROP_0,
  PROP_CONFIGURATION,
  LAST_PROP
};

G_DEFINE_TYPE (GbpBuildConfigurationView, gbp_build_configuration_view, EGG_TYPE_COLUMN_LAYOUT)

static GParamSpec *properties [LAST_PROP];

static gboolean
map_pointer_to (GBinding     *binding,
                const GValue *from_value,
                GValue       *to_value,
                gpointer      user_data)
{
  g_value_set_boolean (to_value, (user_data == g_value_get_object (from_value)));
  return TRUE;
}

static GtkWidget *
create_runtime_row (gpointer item,
                    gpointer user_data)
{
  IdeRuntime *runtime = item;
  IdeConfiguration *configuration = user_data;
  GtkWidget *box;
  GtkWidget *image;
  GtkWidget *label;
  GtkWidget *row;

  g_assert (IDE_IS_RUNTIME (runtime));
  g_assert (IDE_IS_CONFIGURATION (configuration));

  box = g_object_new (GTK_TYPE_BOX,
                      "spacing", 12,
                      "visible", TRUE,
                      NULL);

  label = g_object_new (GTK_TYPE_LABEL,
                        "use-markup", TRUE,
                        "visible", TRUE,
                        "xalign", 0.0f,
                        NULL);
  g_object_bind_property (runtime, "display-name", label, "label", G_BINDING_SYNC_CREATE);
  gtk_container_add (GTK_CONTAINER (box), label);

  image = g_object_new (GTK_TYPE_IMAGE,
                        "icon-name", "object-select-symbolic",
                        "visible", TRUE,
                        NULL);
  g_object_bind_property_full (configuration, "runtime",
                               image, "visible",
                               G_BINDING_SYNC_CREATE,
                               map_pointer_to,
                               NULL,
                               g_object_ref (runtime),
                               g_object_unref);
  gtk_container_add (GTK_CONTAINER (box), image);

  label = g_object_new (GTK_TYPE_LABEL,
                        "hexpand", TRUE,
                        "visible", TRUE,
                        NULL);
  gtk_container_add (GTK_CONTAINER (box), label);

  row = g_object_new (GTK_TYPE_LIST_BOX_ROW,
                      "child", box,
                      "visible", TRUE,
                      NULL);

  g_object_set_data (G_OBJECT (row), "IDE_RUNTIME", runtime);

  return row;
}

static GtkWidget *
create_device_row (gpointer item,
                   gpointer user_data)
{
  IdeDevice *device = item;
  IdeConfiguration *configuration = user_data;
  GtkWidget *box;
  GtkWidget *image;
  GtkWidget *label;
  GtkWidget *row;

  g_assert (IDE_IS_DEVICE (device));
  g_assert (IDE_IS_CONFIGURATION (configuration));

  box = g_object_new (GTK_TYPE_BOX,
                      "spacing", 12,
                      "visible", TRUE,
                      NULL);

  label = g_object_new (GTK_TYPE_LABEL,
                        "use-markup", TRUE,
                        "visible", TRUE,
                        "xalign", 0.0f,
                        NULL);
  g_object_bind_property (device, "display-name", label, "label", G_BINDING_SYNC_CREATE);
  gtk_container_add (GTK_CONTAINER (box), label);

  image = g_object_new (GTK_TYPE_IMAGE,
                        "icon-name", "object-select-symbolic",
                        "visible", TRUE,
                        NULL);
  g_object_bind_property_full (configuration, "device",
                               image, "visible",
                               G_BINDING_SYNC_CREATE,
                               map_pointer_to,
                               NULL,
                               g_object_ref (device),
                               g_object_unref);
  gtk_container_add (GTK_CONTAINER (box), image);

  label = g_object_new (GTK_TYPE_LABEL,
                        "hexpand", TRUE,
                        "visible", TRUE,
                        NULL);
  gtk_container_add (GTK_CONTAINER (box), label);

  row = g_object_new (GTK_TYPE_LIST_BOX_ROW,
                      "child", box,
                      "visible", TRUE,
                      NULL);

  g_object_set_data (G_OBJECT (row), "IDE_DEVICE", device);

  return row;
}

static void
device_row_activated (GbpBuildConfigurationView *self,
                      GtkListBoxRow             *row,
                      GtkListBox                *list_box)
{
  IdeDevice *device;

  g_assert (GBP_IS_BUILD_CONFIGURATION_VIEW (self));
  g_assert (GTK_IS_LIST_BOX_ROW (row));
  g_assert (GTK_IS_LIST_BOX (list_box));

  device = g_object_get_data (G_OBJECT (row), "IDE_DEVICE");

  if (self->configuration != NULL)
    ide_configuration_set_device (self->configuration, device);
}

static void
runtime_row_activated (GbpBuildConfigurationView *self,
                       GtkListBoxRow             *row,
                       GtkListBox                *list_box)
{
  IdeRuntime *runtime;

  g_assert (GBP_IS_BUILD_CONFIGURATION_VIEW (self));
  g_assert (GTK_IS_LIST_BOX_ROW (row));
  g_assert (GTK_IS_LIST_BOX (list_box));

  runtime = g_object_get_data (G_OBJECT (row), "IDE_RUNTIME");

  if (self->configuration != NULL)
    ide_configuration_set_runtime (self->configuration, runtime);
}

static gboolean
treat_null_as_empty (GBinding     *binding,
                     const GValue *from_value,
                     GValue       *to_value,
                     gpointer      user_data)
{
  const gchar *str = g_value_get_string (from_value);
  g_value_set_string (to_value, str ?: "");
  return TRUE;
}

static void
gbp_build_configuration_view_connect (GbpBuildConfigurationView *self,
                                      IdeConfiguration          *configuration)
{
  IdeRuntimeManager *runtime_manager;
  IdeDeviceManager *device_manager;
  IdeContext *context;
  IdeEnvironment *environment;

  g_assert (GBP_IS_BUILD_CONFIGURATION_VIEW (self));
  g_assert (IDE_IS_CONFIGURATION (configuration));

  context = ide_object_get_context (IDE_OBJECT (configuration));
  runtime_manager = ide_context_get_runtime_manager (context);
  device_manager = ide_context_get_device_manager (context);

  self->display_name_binding =
    g_object_bind_property_full (configuration, "display-name",
                                 self->display_name_entry, "text",
                                 G_BINDING_SYNC_CREATE | G_BINDING_BIDIRECTIONAL,
                                 treat_null_as_empty, NULL, NULL, NULL);

  self->configure_binding =
    g_object_bind_property_full (configuration, "config-opts",
                                 self->configure_entry, "text",
                                 G_BINDING_SYNC_CREATE | G_BINDING_BIDIRECTIONAL,
                                 treat_null_as_empty, NULL, NULL, NULL);

  self->prefix_binding =
    g_object_bind_property_full (configuration, "prefix",
                                 self->prefix_entry, "text",
                                 G_BINDING_SYNC_CREATE | G_BINDING_BIDIRECTIONAL,
                                 treat_null_as_empty, NULL, NULL, NULL);

  gtk_list_box_bind_model (self->runtime_list_box,
                           G_LIST_MODEL (runtime_manager),
                           create_runtime_row,
                           g_object_ref (configuration),
                           g_object_unref);

  gtk_list_box_bind_model (self->device_list_box,
                           G_LIST_MODEL (device_manager),
                           create_device_row,
                           g_object_ref (configuration),
                           g_object_unref);

  environment = ide_configuration_get_environment (configuration);
  ide_environment_editor_set_environment (self->environment_editor, environment);
}

static void
gbp_build_configuration_view_disconnect (GbpBuildConfigurationView *self,
                                         IdeConfiguration          *configuration)
{
  g_assert (GBP_IS_BUILD_CONFIGURATION_VIEW (self));
  g_assert (IDE_IS_CONFIGURATION (configuration));

  gtk_list_box_bind_model (self->device_list_box, NULL, NULL, NULL, NULL);
  gtk_list_box_bind_model (self->runtime_list_box, NULL, NULL, NULL, NULL);

  g_clear_pointer (&self->configure_binding, g_binding_unbind);
  g_clear_pointer (&self->display_name_binding, g_binding_unbind);
  g_clear_pointer (&self->prefix_binding, g_binding_unbind);
}

static void
gbp_build_configuration_view_destroy (GtkWidget *widget)
{
  GbpBuildConfigurationView *self = (GbpBuildConfigurationView *)widget;

  if (self->configuration != NULL)
    {
      gbp_build_configuration_view_disconnect (self, self->configuration);
      g_clear_object (&self->configuration);
    }

  GTK_WIDGET_CLASS (gbp_build_configuration_view_parent_class)->destroy (widget);
}

static void
gbp_build_configuration_view_get_property (GObject    *object,
                                           guint       prop_id,
                                           GValue     *value,
                                           GParamSpec *pspec)
{
  GbpBuildConfigurationView *self = GBP_BUILD_CONFIGURATION_VIEW (object);

  switch (prop_id)
    {
    case PROP_CONFIGURATION:
      g_value_set_object (value, gbp_build_configuration_view_get_configuration (self));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
gbp_build_configuration_view_set_property (GObject      *object,
                                           guint         prop_id,
                                           const GValue *value,
                                           GParamSpec   *pspec)
{
  GbpBuildConfigurationView *self = GBP_BUILD_CONFIGURATION_VIEW (object);

  switch (prop_id)
    {
    case PROP_CONFIGURATION:
      gbp_build_configuration_view_set_configuration (self, g_value_get_object (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
gbp_build_configuration_view_class_init (GbpBuildConfigurationViewClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  object_class->get_property = gbp_build_configuration_view_get_property;
  object_class->set_property = gbp_build_configuration_view_set_property;

  properties [PROP_CONFIGURATION] =
    g_param_spec_object ("configuration",
                         "Configuration",
                         "Configuration",
                         IDE_TYPE_CONFIGURATION,
                         (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, LAST_PROP, properties);

  widget_class->destroy = gbp_build_configuration_view_destroy;

  gtk_widget_class_set_template_from_resource (widget_class, "/org/gnome/builder/plugins/build-tools-plugin/gbp-build-configuration-view.ui");
  gtk_widget_class_set_css_name (widget_class, "configurationview");
  gtk_widget_class_bind_template_child (widget_class, GbpBuildConfigurationView, configure_entry);
  gtk_widget_class_bind_template_child (widget_class, GbpBuildConfigurationView, device_list_box);
  gtk_widget_class_bind_template_child (widget_class, GbpBuildConfigurationView, display_name_entry);
  gtk_widget_class_bind_template_child (widget_class, GbpBuildConfigurationView, environment_editor);
  gtk_widget_class_bind_template_child (widget_class, GbpBuildConfigurationView, prefix_entry);
  gtk_widget_class_bind_template_child (widget_class, GbpBuildConfigurationView, runtime_list_box);
}

static void
gbp_build_configuration_view_init (GbpBuildConfigurationView *self)
{
  gtk_widget_init_template (GTK_WIDGET (self));

  g_signal_connect_object (self->device_list_box,
                           "row-activated",
                           G_CALLBACK (device_row_activated),
                           self,
                           G_CONNECT_SWAPPED);

  g_signal_connect_object (self->runtime_list_box,
                           "row-activated",
                           G_CALLBACK (runtime_row_activated),
                           self,
                           G_CONNECT_SWAPPED);
}

IdeConfiguration *
gbp_build_configuration_view_get_configuration (GbpBuildConfigurationView *self)
{
  g_return_val_if_fail (GBP_IS_BUILD_CONFIGURATION_VIEW (self), NULL);

  return self->configuration;
}

void
gbp_build_configuration_view_set_configuration (GbpBuildConfigurationView *self,
                                                IdeConfiguration          *configuration)
{
  g_return_if_fail (GBP_IS_BUILD_CONFIGURATION_VIEW (self));
  g_return_if_fail (!configuration || IDE_IS_CONFIGURATION (configuration));

  if (self->configuration != configuration)
    {
      if (self->configuration != NULL)
        {
          gbp_build_configuration_view_disconnect (self, self->configuration);
          g_clear_object (&self->configuration);
        }

      if (configuration != NULL)
        {
          self->configuration = g_object_ref (configuration);
          gbp_build_configuration_view_connect (self, configuration);
        }

      g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_CONFIGURATION]);
    }
}
