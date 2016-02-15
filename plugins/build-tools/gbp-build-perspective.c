/* gbp-build-perspective.c
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

#include <glib/gi18n.h>

#include "gbp-build-configuration-view.h"
#include "gbp-build-perspective.h"

struct _GbpBuildPerspective
{
  GtkBin                     parent_instance;

  GActionGroup              *actions;
  IdeConfiguration          *configuration;
  IdeConfigurationManager   *configuration_manager;

  GtkListBox                *list_box;
  GbpBuildConfigurationView *view;
};

enum {
  PROP_0,
  PROP_CONFIGURATION,
  PROP_CONFIGURATION_MANAGER,
  LAST_PROP
};

static void perspective_iface_init (IdePerspectiveInterface *iface);

G_DEFINE_TYPE_EXTENDED (GbpBuildPerspective, gbp_build_perspective, GTK_TYPE_BIN, 0,
                        G_IMPLEMENT_INTERFACE (IDE_TYPE_PERSPECTIVE, perspective_iface_init))

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
create_configuration_row (gpointer item,
                          gpointer user_data)
{
  IdeConfigurationManager *manager = user_data;
  IdeConfiguration *configuration = item;
  GtkWidget *row;
  GtkWidget *box;
  GtkWidget *label;
  GtkWidget *image;

  g_assert (IDE_IS_CONFIGURATION (configuration));
  g_assert (IDE_IS_CONFIGURATION_MANAGER (manager));

  row = g_object_new (GTK_TYPE_LIST_BOX_ROW,
                      "visible", TRUE,
                      NULL);
  g_object_set_data_full (G_OBJECT (row), "IDE_CONFIGURATION",
                          g_object_ref (configuration), g_object_unref);

  box = g_object_new (GTK_TYPE_BOX,
                      "orientation", GTK_ORIENTATION_HORIZONTAL,
                      "visible", TRUE,
                      NULL);
  gtk_container_add (GTK_CONTAINER (row), box);

  label = g_object_new (GTK_TYPE_LABEL,
                        "ellipsize", PANGO_ELLIPSIZE_MIDDLE,
                        "visible", TRUE,
                        "xalign", 0.0f,
                        NULL);
  g_object_bind_property (configuration, "display-name",
                          label, "label",
                          G_BINDING_SYNC_CREATE);
  gtk_container_add (GTK_CONTAINER (box), label);

  image = g_object_new (GTK_TYPE_IMAGE,
                        "icon-name", "object-select-symbolic",
                        "xpad", 6,
                        NULL);
  g_object_bind_property_full (manager, "current", image, "visible",
                               G_BINDING_SYNC_CREATE,
                               map_pointer_to, NULL, configuration, NULL);
  gtk_container_add (GTK_CONTAINER (box), image);

  label = g_object_new (GTK_TYPE_LABEL,
                        "hexpand", TRUE,
                        "visible", TRUE,
                        NULL);
  gtk_container_add (GTK_CONTAINER (box), label);

  return row;
}

static void
gbp_build_perspective_set_configuration_manager (GbpBuildPerspective     *self,
                                                 IdeConfigurationManager *manager)
{
  g_assert (GBP_IS_BUILD_PERSPECTIVE (self));
  g_assert (IDE_IS_CONFIGURATION_MANAGER (manager));

  g_set_object (&self->configuration_manager, manager);
  gtk_list_box_bind_model (self->list_box,
                           G_LIST_MODEL (manager),
                           create_configuration_row,
                           g_object_ref (manager),
                           g_object_unref);
}

static void
gbp_build_perspective_row_selected (GbpBuildPerspective *self,
                                    GtkListBoxRow       *row,
                                    GtkListBox          *list_box)
{
  g_assert (GBP_IS_BUILD_PERSPECTIVE (self));
  g_assert (!row || GTK_IS_LIST_BOX_ROW (row));
  g_assert (GTK_IS_LIST_BOX (list_box));

  if (row != NULL)
    {
      IdeConfiguration *configuration;

      configuration = g_object_get_data (G_OBJECT (row), "IDE_CONFIGURATION");
      g_set_object (&self->configuration, configuration);
      gbp_build_configuration_view_set_configuration (self->view, configuration);
    }
}

static void
gbp_build_perspective_row_activated (GbpBuildPerspective *self,
                                     GtkListBoxRow       *row,
                                     GtkListBox          *list_box)
{
  IdeConfiguration *configuration;

  g_assert (GBP_IS_BUILD_PERSPECTIVE (self));
  g_assert (GTK_IS_LIST_BOX_ROW (row));
  g_assert (GTK_IS_LIST_BOX (list_box));


  configuration = g_object_get_data (G_OBJECT (row), "IDE_CONFIGURATION");
  ide_configuration_manager_set_current (self->configuration_manager, configuration);
}

static void
duplicate_configuration (GSimpleAction *action,
                         GVariant      *variant,
                         gpointer       user_data)
{
  GbpBuildPerspective *self = user_data;

  g_assert (GBP_IS_BUILD_PERSPECTIVE (self));

  if (self->configuration != NULL)
    {
      g_autoptr(IdeConfiguration) copy = NULL;

      copy = ide_configuration_duplicate (self->configuration);
      ide_configuration_manager_add (self->configuration_manager, copy);
    }
}

static void
delete_configuration (GSimpleAction *action,
                      GVariant      *variant,
                      gpointer       user_data)
{
  GbpBuildPerspective *self = user_data;

  g_assert (GBP_IS_BUILD_PERSPECTIVE (self));

  if (self->configuration != NULL)
    {
      g_autoptr(IdeConfiguration) config = NULL;

      /*
       * Make sure we hold onto a reference during the call, as it is likely
       * self->configuration will change during this call.
       */
      config = g_object_ref (self->configuration);
      ide_configuration_manager_remove (self->configuration_manager, config);

      /*
       * Switch to the first configuration in the list. The configuration
       * manager should have added a new "default" configuration if we
       * deleted the last configuration, so we should just get the 0th
       * index.
       */
      if (g_list_model_get_n_items (G_LIST_MODEL (self->configuration_manager)) > 0)
        {
          g_autoptr(IdeConfiguration) first = NULL;

          first = g_list_model_get_item (G_LIST_MODEL (self->configuration_manager), 0);
          gbp_build_perspective_set_configuration (self, first);
        }
    }
}

static void
gbp_build_perspective_finalize (GObject *object)
{
  GbpBuildPerspective *self = (GbpBuildPerspective *)object;

  g_clear_object (&self->actions);
  g_clear_object (&self->configuration);

  G_OBJECT_CLASS (gbp_build_perspective_parent_class)->finalize (object);
}

static void
gbp_build_perspective_get_property (GObject    *object,
                                    guint       prop_id,
                                    GValue     *value,
                                    GParamSpec *pspec)
{
  GbpBuildPerspective *self = GBP_BUILD_PERSPECTIVE (object);

  switch (prop_id)
    {
    case PROP_CONFIGURATION:
      g_value_set_object (value, gbp_build_perspective_get_configuration (self));
      break;

    case PROP_CONFIGURATION_MANAGER:
      g_value_set_object (value, self->configuration_manager);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
gbp_build_perspective_set_property (GObject      *object,
                                    guint         prop_id,
                                    const GValue *value,
                                    GParamSpec   *pspec)
{
  GbpBuildPerspective *self = GBP_BUILD_PERSPECTIVE (object);

  switch (prop_id)
    {
    case PROP_CONFIGURATION:
      gbp_build_perspective_set_configuration (self, g_value_get_object (value));
      break;

    case PROP_CONFIGURATION_MANAGER:
      gbp_build_perspective_set_configuration_manager (self, g_value_get_object (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
gbp_build_perspective_class_init (GbpBuildPerspectiveClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  object_class->finalize = gbp_build_perspective_finalize;
  object_class->get_property = gbp_build_perspective_get_property;
  object_class->set_property = gbp_build_perspective_set_property;

  properties [PROP_CONFIGURATION_MANAGER] =
    g_param_spec_object ("configuration-manager",
                         "Configuration Manager",
                         "Configuration Manager",
                         IDE_TYPE_CONFIGURATION_MANAGER,
                         (G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS));

  properties [PROP_CONFIGURATION] =
    g_param_spec_object ("configuration",
                         "Configuration",
                         "The configuration to edit",
                         IDE_TYPE_CONFIGURATION,
                         (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, LAST_PROP, properties);

  gtk_widget_class_set_template_from_resource (widget_class, "/org/gnome/builder/plugins/build-tools-plugin/gbp-build-perspective.ui");
  gtk_widget_class_set_css_name (widget_class, "buildperspective");
  gtk_widget_class_bind_template_child (widget_class, GbpBuildPerspective, list_box);
  gtk_widget_class_bind_template_child (widget_class, GbpBuildPerspective, view);

  g_type_ensure (GBP_TYPE_BUILD_CONFIGURATION_VIEW);
}

static void
gbp_build_perspective_init (GbpBuildPerspective *self)
{
  static GActionEntry actions[] = {
    { "delete-configuration", delete_configuration },
    { "duplicate-configuration", duplicate_configuration },
  };

  gtk_widget_init_template (GTK_WIDGET (self));

  g_signal_connect_object (self->list_box,
                           "row-selected",
                           G_CALLBACK (gbp_build_perspective_row_selected),
                           self,
                           G_CONNECT_SWAPPED);

  g_signal_connect_object (self->list_box,
                           "row-activated",
                           G_CALLBACK (gbp_build_perspective_row_activated),
                           self,
                           G_CONNECT_SWAPPED);

  self->actions = G_ACTION_GROUP (g_simple_action_group_new ());
  g_action_map_add_action_entries (G_ACTION_MAP (self->actions), actions,
                                   G_N_ELEMENTS (actions), self);
}

GtkWidget *
gbp_build_perspective_new (void)
{
  return g_object_new (GBP_TYPE_BUILD_PERSPECTIVE, NULL);
}

IdeConfiguration *
gbp_build_perspective_get_configuration (GbpBuildPerspective *self)
{
  g_return_val_if_fail (GBP_IS_BUILD_PERSPECTIVE (self), NULL);

  return self->configuration;
}

static void
find_configuration_row (GtkWidget *widget,
                        gpointer   data)
{
  struct {
    IdeConfiguration *config;
    GtkWidget        *row;
  } *lookup = data;

  if (lookup->row != NULL)
    return;

  if (lookup->config == g_object_get_data (G_OBJECT (widget), "IDE_CONFIGURATION"))
    lookup->row = widget;
}

void
gbp_build_perspective_set_configuration (GbpBuildPerspective *self,
                                         IdeConfiguration    *configuration)
{
  struct {
    IdeConfiguration *config;
    GtkWidget        *row;
  } lookup = { configuration, NULL };

  g_return_if_fail (GBP_IS_BUILD_PERSPECTIVE (self));
  g_return_if_fail (!configuration || IDE_IS_CONFIGURATION (configuration));

  gtk_container_foreach (GTK_CONTAINER (self->list_box),
                         find_configuration_row,
                         &lookup);

  if (GTK_IS_LIST_BOX_ROW (lookup.row))
    gtk_list_box_select_row (self->list_box, GTK_LIST_BOX_ROW (lookup.row));
}

static gchar *
gbp_build_perspective_get_icon_name (IdePerspective *perspective)
{
  return g_strdup ("builder-build-configure-symbolic");
}

static gchar *
gbp_build_perspective_get_title (IdePerspective *perspective)
{
  return g_strdup ("Build Configuration");
}

static gchar *
gbp_build_perspective_get_id (IdePerspective *perspective)
{
  return g_strdup ("buildperspective");
}

static gint
gbp_build_perspective_get_priority (IdePerspective *perspective)
{
  return 80000;
}

static GActionGroup *
gbp_build_perspective_get_actions (IdePerspective *perspective)
{
  GbpBuildPerspective *self = (GbpBuildPerspective *)perspective;

  g_assert (GBP_IS_BUILD_PERSPECTIVE (self));

  return g_object_ref (self->actions);
}

static void
perspective_iface_init (IdePerspectiveInterface *iface)
{
  iface->get_actions = gbp_build_perspective_get_actions;
  iface->get_icon_name = gbp_build_perspective_get_icon_name;
  iface->get_title = gbp_build_perspective_get_title;
  iface->get_id = gbp_build_perspective_get_id;
  iface->get_priority = gbp_build_perspective_get_priority;
}
