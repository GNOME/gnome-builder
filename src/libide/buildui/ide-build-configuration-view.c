/* ide-build-configuration-view.c
 *
 * Copyright 2016 Christian Hergert <chergert@redhat.com>
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

#define G_LOG_DOMAIN "ide-build-configuration-view"

#include "config.h"

#include <ide.h>
#include <string.h>

#include "buildui/ide-build-configuration-view.h"
#include "buildui/ide-environment-editor.h"

struct _IdeBuildConfigurationView
{
  DzlColumnLayout       parent_instance;

  IdeConfiguration     *configuration;

  GBinding             *configure_binding;
  GBinding             *display_name_binding;
  GBinding             *prefix_binding;

  GtkEntry             *build_system_entry;
  GtkEntry             *configure_entry;
  GtkEntry             *display_name_entry;
  IdeEnvironmentEditor *environment_editor;
  GtkEntry             *prefix_entry;
  GtkListBox           *runtime_list_box;
  GtkListBox           *toolchain_list_box;
  GtkEntry             *workdir_entry;
};

enum {
  PROP_0,
  PROP_CONFIGURATION,
  LAST_PROP
};

G_DEFINE_TYPE (IdeBuildConfigurationView, ide_build_configuration_view, DZL_TYPE_COLUMN_LAYOUT)

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
  gboolean sensitive;

  g_assert (IDE_IS_RUNTIME (runtime));
  g_assert (IDE_IS_CONFIGURATION (configuration));

  sensitive = ide_configuration_supports_runtime (configuration, runtime);

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
                      "sensitive", sensitive,
                      "visible", TRUE,
                      NULL);

  g_object_set_data (G_OBJECT (row), "IDE_RUNTIME", runtime);

  return row;
}

static GtkWidget *
create_toolchain_row (gpointer item,
                      gpointer user_data)
{
  IdeToolchain *toolchain = item;
  IdeConfiguration *configuration = user_data;
  IdeRuntime *runtime;
  GtkWidget *box;
  GtkWidget *image;
  GtkWidget *label;
  GtkWidget *row;
  gboolean sensitive;

  g_assert (IDE_IS_TOOLCHAIN (toolchain));
  g_assert (IDE_IS_CONFIGURATION (configuration));

  runtime = ide_configuration_get_runtime (configuration);
  sensitive = runtime && ide_runtime_supports_toolchain (runtime, toolchain);

  box = g_object_new (GTK_TYPE_BOX,
                      "spacing", 12,
                      "visible", TRUE,
                      NULL);

  label = g_object_new (GTK_TYPE_LABEL,
                        "use-markup", TRUE,
                        "visible", TRUE,
                        "xalign", 0.0f,
                        NULL);
  g_object_bind_property (toolchain, "display-name", label, "label", G_BINDING_SYNC_CREATE);
  gtk_container_add (GTK_CONTAINER (box), label);

  image = g_object_new (GTK_TYPE_IMAGE,
                        "icon-name", "object-select-symbolic",
                        "visible", TRUE,
                        NULL);
  g_object_bind_property_full (configuration, "toolchain",
                               image, "visible",
                               G_BINDING_SYNC_CREATE,
                               map_pointer_to,
                               NULL,
                               g_object_ref (toolchain),
                               g_object_unref);
  gtk_container_add (GTK_CONTAINER (box), image);

  label = g_object_new (GTK_TYPE_LABEL,
                        "hexpand", TRUE,
                        "visible", TRUE,
                        NULL);
  gtk_container_add (GTK_CONTAINER (box), label);

  row = g_object_new (GTK_TYPE_LIST_BOX_ROW,
                      "child", box,
                      "sensitive", sensitive,
                      "visible", TRUE,
                      NULL);

  g_object_set_data (G_OBJECT (row), "IDE_TOOLCHAIN", toolchain);

  return row;
}

static void
runtime_row_activated (IdeBuildConfigurationView *self,
                       GtkListBoxRow             *row,
                       GtkListBox                *list_box)
{
  IdeRuntime *runtime;

  g_assert (IDE_IS_BUILD_CONFIGURATION_VIEW (self));
  g_assert (GTK_IS_LIST_BOX_ROW (row));
  g_assert (GTK_IS_LIST_BOX (list_box));

  runtime = g_object_get_data (G_OBJECT (row), "IDE_RUNTIME");

  if (self->configuration != NULL)
    ide_configuration_set_runtime (self->configuration, runtime);
}

static void
runtime_changed (IdeBuildConfigurationView *self,
                 GParamSpec                *pspec,
                 IdeConfiguration          *configuration)
{
  GList *children;
  IdeRuntime *runtime;

  g_assert (IDE_IS_BUILD_CONFIGURATION_VIEW (self));
  g_assert (IDE_IS_CONFIGURATION (configuration));

  runtime = ide_configuration_get_runtime (configuration);
  children = gtk_container_get_children (GTK_CONTAINER (self->toolchain_list_box));
  for (const GList *iter = children; iter != NULL; iter = iter->next)
    {
      gboolean sensitive;
      IdeToolchain *toolchain;
      GtkWidget *widget = iter->data;

      toolchain = g_object_get_data (G_OBJECT (widget), "IDE_TOOLCHAIN");
      sensitive = ide_runtime_supports_toolchain (runtime, toolchain);
      gtk_widget_set_sensitive (widget, sensitive);
    }

  g_list_free (children);
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
toolchain_row_activated (IdeBuildConfigurationView *self,
                         GtkListBoxRow             *row,
                         GtkListBox                *list_box)
{
  IdeToolchain *toolchain;

  g_assert (IDE_IS_BUILD_CONFIGURATION_VIEW (self));
  g_assert (GTK_IS_LIST_BOX_ROW (row));
  g_assert (GTK_IS_LIST_BOX (list_box));

  toolchain = g_object_get_data (G_OBJECT (row), "IDE_TOOLCHAIN");

  if (self->configuration != NULL)
    ide_configuration_set_toolchain (self->configuration, toolchain);
}

static void
ide_build_configuration_view_connect (IdeBuildConfigurationView *self,
                                      IdeConfiguration          *configuration)
{
  IdeRuntimeManager *runtime_manager;
  IdeToolchainManager *toolchain_manager;
  IdeContext *context;
  IdeEnvironment *environment;

  g_assert (IDE_IS_BUILD_CONFIGURATION_VIEW (self));
  g_assert (IDE_IS_CONFIGURATION (configuration));

  context = ide_object_get_context (IDE_OBJECT (configuration));
  runtime_manager = ide_context_get_runtime_manager (context);
  toolchain_manager = ide_context_get_toolchain_manager (context);

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

  gtk_list_box_bind_model (self->toolchain_list_box,
                           G_LIST_MODEL (toolchain_manager),
                           create_toolchain_row,
                           g_object_ref (configuration),
                           g_object_unref);

  g_signal_connect_object (configuration,
                           "notify::runtime",
                           G_CALLBACK (runtime_changed),
                           self,
                           G_CONNECT_SWAPPED);

  environment = ide_configuration_get_environment (configuration);
  ide_environment_editor_set_environment (self->environment_editor, environment);
}

static void
ide_build_configuration_view_disconnect (IdeBuildConfigurationView *self,
                                         IdeConfiguration          *configuration)
{
  g_assert (IDE_IS_BUILD_CONFIGURATION_VIEW (self));
  g_assert (IDE_IS_CONFIGURATION (configuration));

  gtk_list_box_bind_model (self->runtime_list_box, NULL, NULL, NULL, NULL);

  dzl_clear_pointer (&self->configure_binding, g_binding_unbind);
  dzl_clear_pointer (&self->display_name_binding, g_binding_unbind);
  dzl_clear_pointer (&self->prefix_binding, g_binding_unbind);
}

static void
ide_build_configuration_view_destroy (GtkWidget *widget)
{
  IdeBuildConfigurationView *self = (IdeBuildConfigurationView *)widget;

  if (self->configuration != NULL)
    {
      ide_build_configuration_view_disconnect (self, self->configuration);
      g_clear_object (&self->configuration);
    }

  GTK_WIDGET_CLASS (ide_build_configuration_view_parent_class)->destroy (widget);
}

static void
ide_build_configuration_view_get_property (GObject    *object,
                                           guint       prop_id,
                                           GValue     *value,
                                           GParamSpec *pspec)
{
  IdeBuildConfigurationView *self = IDE_BUILD_CONFIGURATION_VIEW (object);

  switch (prop_id)
    {
    case PROP_CONFIGURATION:
      g_value_set_object (value, ide_build_configuration_view_get_configuration (self));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
ide_build_configuration_view_set_property (GObject      *object,
                                           guint         prop_id,
                                           const GValue *value,
                                           GParamSpec   *pspec)
{
  IdeBuildConfigurationView *self = IDE_BUILD_CONFIGURATION_VIEW (object);

  switch (prop_id)
    {
    case PROP_CONFIGURATION:
      ide_build_configuration_view_set_configuration (self, g_value_get_object (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
ide_build_configuration_view_class_init (IdeBuildConfigurationViewClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  object_class->get_property = ide_build_configuration_view_get_property;
  object_class->set_property = ide_build_configuration_view_set_property;

  properties [PROP_CONFIGURATION] =
    g_param_spec_object ("configuration",
                         "Configuration",
                         "Configuration",
                         IDE_TYPE_CONFIGURATION,
                         (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, LAST_PROP, properties);

  widget_class->destroy = ide_build_configuration_view_destroy;

  gtk_widget_class_set_template_from_resource (widget_class, "/org/gnome/builder/plugins/buildui/ide-build-configuration-view.ui");
  gtk_widget_class_set_css_name (widget_class, "configurationview");
  gtk_widget_class_bind_template_child (widget_class, IdeBuildConfigurationView, build_system_entry);
  gtk_widget_class_bind_template_child (widget_class, IdeBuildConfigurationView, configure_entry);
  gtk_widget_class_bind_template_child (widget_class, IdeBuildConfigurationView, display_name_entry);
  gtk_widget_class_bind_template_child (widget_class, IdeBuildConfigurationView, environment_editor);
  gtk_widget_class_bind_template_child (widget_class, IdeBuildConfigurationView, prefix_entry);
  gtk_widget_class_bind_template_child (widget_class, IdeBuildConfigurationView, runtime_list_box);
  gtk_widget_class_bind_template_child (widget_class, IdeBuildConfigurationView, toolchain_list_box);
  gtk_widget_class_bind_template_child (widget_class, IdeBuildConfigurationView, workdir_entry);

  g_type_ensure (IDE_TYPE_ENVIRONMENT_EDITOR);
}

static void
ide_build_configuration_view_init (IdeBuildConfigurationView *self)
{
  gtk_widget_init_template (GTK_WIDGET (self));

  g_signal_connect_object (self->runtime_list_box,
                           "row-activated",
                           G_CALLBACK (runtime_row_activated),
                           self,
                           G_CONNECT_SWAPPED);

  g_signal_connect_object (self->toolchain_list_box,
                           "row-activated",
                           G_CALLBACK (toolchain_row_activated),
                           self,
                           G_CONNECT_SWAPPED);
}

IdeConfiguration *
ide_build_configuration_view_get_configuration (IdeBuildConfigurationView *self)
{
  g_return_val_if_fail (IDE_IS_BUILD_CONFIGURATION_VIEW (self), NULL);

  return self->configuration;
}

void
ide_build_configuration_view_set_configuration (IdeBuildConfigurationView *self,
                                                IdeConfiguration          *configuration)
{
  g_return_if_fail (IDE_IS_BUILD_CONFIGURATION_VIEW (self));
  g_return_if_fail (!configuration || IDE_IS_CONFIGURATION (configuration));

  if (configuration != NULL)
    {
      IdeContext *context = ide_object_get_context (IDE_OBJECT (configuration));
      IdeBuildSystem *build_system = ide_context_get_build_system (context);
      g_autofree gchar *name = ide_build_system_get_display_name (build_system);
      IdeVcs *vcs = ide_context_get_vcs (context);
      GFile *workdir = ide_vcs_get_working_directory (vcs);
      g_autofree gchar *path = g_file_get_path (workdir);

      gtk_entry_set_text (self->build_system_entry, name);
      gtk_entry_set_text (self->workdir_entry, path);
    }

  if (self->configuration != configuration)
    {
      if (self->configuration != NULL)
        {
          ide_build_configuration_view_disconnect (self, self->configuration);
          g_clear_object (&self->configuration);
        }

      if (configuration != NULL)
        {
          self->configuration = g_object_ref (configuration);
          ide_build_configuration_view_connect (self, configuration);
        }

      g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_CONFIGURATION]);
    }
}
