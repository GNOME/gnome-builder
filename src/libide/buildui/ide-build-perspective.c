/* ide-build-perspective.c
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

#define G_LOG_DOMAIN "ide-build-perspective"

#include "config.h"

#include <glib/gi18n.h>

#include "buildui/ide-build-configuration-row.h"
#include "buildui/ide-build-configuration-view.h"
#include "buildui/ide-build-perspective.h"

struct _IdeBuildPerspective
{
  GtkBin                     parent_instance;

  IdeConfiguration          *configuration;
  IdeConfigurationManager   *configuration_manager;

  GtkListBox                *list_box;
  IdeBuildConfigurationView *view;
};

enum {
  PROP_0,
  PROP_CONFIGURATION,
  PROP_CONFIGURATION_MANAGER,
  LAST_PROP
};

static void perspective_iface_init (IdePerspectiveInterface *iface);

G_DEFINE_TYPE_EXTENDED (IdeBuildPerspective, ide_build_perspective, GTK_TYPE_BIN, 0,
                        G_IMPLEMENT_INTERFACE (IDE_TYPE_PERSPECTIVE, perspective_iface_init))

static GParamSpec *properties [LAST_PROP];

static gboolean
map_pointer_to (GBinding     *binding,
                const GValue *from_value,
                GValue       *to_value,
                gpointer      user_data)
{
  g_value_set_boolean (to_value, user_data == g_value_get_object (from_value));
  return TRUE;
}

static void
select_first_row (GtkWidget *widget,
                  gpointer   user_data)
{
  gboolean *selected = user_data;

  g_assert (IDE_IS_BUILD_CONFIGURATION_ROW (widget));
  g_assert (selected != NULL);

  if (*selected == FALSE)
    {
      *selected = TRUE;
      gtk_list_box_select_row (GTK_LIST_BOX (gtk_widget_get_parent (widget)),
                               GTK_LIST_BOX_ROW (widget));
    }
}

static gboolean
update_selection_in_main (gpointer data)
{
  g_autoptr(GtkListBox) list_box = data;
  gboolean selected = FALSE;

  g_assert (GTK_IS_LIST_BOX (list_box));

  if (!gtk_widget_in_destruction (GTK_WIDGET (list_box)))
    {
      if (NULL == gtk_list_box_get_selected_row (list_box))
        {
          gtk_container_foreach (GTK_CONTAINER (list_box),
                                 select_first_row,
                                 &selected);
        }
    }

  return G_SOURCE_REMOVE;
}

static GtkWidget *
create_configuration_row (gpointer item,
                          gpointer user_data)
{
  IdeConfigurationManager *manager = user_data;
  IdeConfiguration *configuration = item;
  GtkWidget *ret;

  g_assert (IDE_IS_CONFIGURATION (configuration));
  g_assert (IDE_IS_CONFIGURATION_MANAGER (manager));

  ret = g_object_new (IDE_TYPE_BUILD_CONFIGURATION_ROW,
                      "configuration", configuration,
                      "visible", TRUE,
                      NULL);

  g_object_bind_property_full (manager, "current", ret, "active",
                               G_BINDING_SYNC_CREATE,
                               map_pointer_to, NULL, configuration, NULL);

  return ret;
}

static void
ide_build_perspective_set_configuration_manager (IdeBuildPerspective     *self,
                                                 IdeConfigurationManager *manager)
{
  g_assert (IDE_IS_BUILD_PERSPECTIVE (self));
  g_assert (IDE_IS_CONFIGURATION_MANAGER (manager));

  g_set_object (&self->configuration_manager, manager);
  gtk_list_box_bind_model (self->list_box,
                           G_LIST_MODEL (manager),
                           create_configuration_row,
                           g_object_ref (manager),
                           g_object_unref);

  update_selection_in_main (g_object_ref (self->list_box));
}

static void
update_selected_state (GtkWidget *widget,
                       gpointer   user_data)
{
  IdeBuildConfigurationRow *row = (IdeBuildConfigurationRow *)widget;
  IdeConfiguration *selected = user_data;
  IdeConfiguration *config;

  g_assert (IDE_IS_BUILD_CONFIGURATION_ROW (row));
  g_assert (IDE_IS_CONFIGURATION (selected));

  config = ide_build_configuration_row_get_configuration (row);

  g_object_set (row,
                "selected", (config == selected),
                NULL);
}

static void
ide_build_perspective_row_selected (IdeBuildPerspective      *self,
                                    IdeBuildConfigurationRow *row,
                                    GtkListBox               *list_box)
{
  g_assert (IDE_IS_BUILD_PERSPECTIVE (self));
  g_assert (!row || IDE_IS_BUILD_CONFIGURATION_ROW (row));
  g_assert (GTK_IS_LIST_BOX (list_box));

  if (row != NULL)
    {
      IdeConfiguration *configuration;

      configuration = ide_build_configuration_row_get_configuration (row);
      g_set_object (&self->configuration, configuration);
      ide_build_configuration_view_set_configuration (self->view, configuration);

      gtk_container_foreach (GTK_CONTAINER (list_box),
                             update_selected_state,
                             configuration);
    }
  else
    {
      /* Possibly wait for a new row to be added (the new default config)
       * and select it in the main loop.
       */
      g_timeout_add (0, update_selection_in_main, g_object_ref (list_box));
    }
}

static void
ide_build_perspective_row_activated (IdeBuildPerspective      *self,
                                     IdeBuildConfigurationRow *row,
                                     GtkListBox               *list_box)
{
  IdeConfiguration *configuration;

  g_assert (IDE_IS_BUILD_PERSPECTIVE (self));
  g_assert (IDE_IS_BUILD_CONFIGURATION_ROW (row));
  g_assert (GTK_IS_LIST_BOX (list_box));

  configuration = ide_build_configuration_row_get_configuration (row);
  ide_configuration_manager_set_current (self->configuration_manager, configuration);
}

static void
duplicate_configuration (GSimpleAction *action,
                         GVariant      *variant,
                         gpointer       user_data)
{
  IdeBuildPerspective *self = user_data;

  g_assert (IDE_IS_BUILD_PERSPECTIVE (self));

  if (self->configuration != NULL)
    {
      IdeContext *context;
      IdeConfigurationManager *config_manager;

      context = ide_widget_get_context (GTK_WIDGET (self));
      config_manager = ide_context_get_configuration_manager (context);
      ide_configuration_manager_duplicate (config_manager, self->configuration);
    }
}

static void
delete_configuration (GSimpleAction *action,
                      GVariant      *variant,
                      gpointer       user_data)
{
  IdeBuildPerspective *self = user_data;

  g_assert (IDE_IS_BUILD_PERSPECTIVE (self));

  if (self->configuration != NULL)
    {
      g_autoptr(IdeConfiguration) config = NULL;

      /*
       * Make sure we hold onto a reference during the call, as it is likely
       * self->configuration will change during this call.
       */
      config = g_object_ref (self->configuration);
      ide_configuration_manager_delete (self->configuration_manager, config);

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
          ide_build_perspective_set_configuration (self, first);
        }
    }
}

static void
ide_build_perspective_grab_focus (GtkWidget *widget)
{
  IdeBuildPerspective *self = (IdeBuildPerspective *)widget;

  g_assert (IDE_IS_BUILD_PERSPECTIVE (self));

  gtk_widget_child_focus (GTK_WIDGET (self->list_box), GTK_DIR_TAB_FORWARD);
}

static void
ide_build_perspective_finalize (GObject *object)
{
  IdeBuildPerspective *self = (IdeBuildPerspective *)object;

  g_clear_object (&self->configuration);

  G_OBJECT_CLASS (ide_build_perspective_parent_class)->finalize (object);
}

static void
ide_build_perspective_get_property (GObject    *object,
                                    guint       prop_id,
                                    GValue     *value,
                                    GParamSpec *pspec)
{
  IdeBuildPerspective *self = IDE_BUILD_PERSPECTIVE (object);

  switch (prop_id)
    {
    case PROP_CONFIGURATION:
      g_value_set_object (value, ide_build_perspective_get_configuration (self));
      break;

    case PROP_CONFIGURATION_MANAGER:
      g_value_set_object (value, self->configuration_manager);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
ide_build_perspective_set_property (GObject      *object,
                                    guint         prop_id,
                                    const GValue *value,
                                    GParamSpec   *pspec)
{
  IdeBuildPerspective *self = IDE_BUILD_PERSPECTIVE (object);

  switch (prop_id)
    {
    case PROP_CONFIGURATION:
      ide_build_perspective_set_configuration (self, g_value_get_object (value));
      break;

    case PROP_CONFIGURATION_MANAGER:
      ide_build_perspective_set_configuration_manager (self, g_value_get_object (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
ide_build_perspective_class_init (IdeBuildPerspectiveClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  object_class->finalize = ide_build_perspective_finalize;
  object_class->get_property = ide_build_perspective_get_property;
  object_class->set_property = ide_build_perspective_set_property;

  widget_class->grab_focus = ide_build_perspective_grab_focus;

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

  gtk_widget_class_set_template_from_resource (widget_class, "/org/gnome/builder/plugins/buildui/ide-build-perspective.ui");
  gtk_widget_class_set_css_name (widget_class, "buildperspective");
  gtk_widget_class_bind_template_child (widget_class, IdeBuildPerspective, list_box);
  gtk_widget_class_bind_template_child (widget_class, IdeBuildPerspective, view);

  g_type_ensure (IDE_TYPE_BUILD_CONFIGURATION_VIEW);
}

static const GActionEntry actions[] = {
  { "delete-configuration", delete_configuration },
  { "duplicate-configuration", duplicate_configuration },
};

static void
ide_build_perspective_init (IdeBuildPerspective *self)
{
  g_autoptr(GSimpleActionGroup) group = NULL;

  gtk_widget_init_template (GTK_WIDGET (self));

  g_signal_connect_object (self->list_box,
                           "row-selected",
                           G_CALLBACK (ide_build_perspective_row_selected),
                           self,
                           G_CONNECT_SWAPPED);

  g_signal_connect_object (self->list_box,
                           "row-activated",
                           G_CALLBACK (ide_build_perspective_row_activated),
                           self,
                           G_CONNECT_SWAPPED);

  group = g_simple_action_group_new ();
  g_action_map_add_action_entries (G_ACTION_MAP (group), actions, G_N_ELEMENTS (actions), self);
  gtk_widget_insert_action_group (GTK_WIDGET (self), "build-preferences", G_ACTION_GROUP (group));
}

GtkWidget *
ide_build_perspective_new (void)
{
  return g_object_new (IDE_TYPE_BUILD_PERSPECTIVE, NULL);
}

IdeConfiguration *
ide_build_perspective_get_configuration (IdeBuildPerspective *self)
{
  g_return_val_if_fail (IDE_IS_BUILD_PERSPECTIVE (self), NULL);

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
  IdeBuildConfigurationRow *row = (IdeBuildConfigurationRow *)widget;

  g_assert (IDE_IS_BUILD_CONFIGURATION_ROW (row));

  if (lookup->row != NULL)
    return;

  if (lookup->config == ide_build_configuration_row_get_configuration (row))
    lookup->row = widget;
}

void
ide_build_perspective_set_configuration (IdeBuildPerspective *self,
                                         IdeConfiguration    *configuration)
{
  struct {
    IdeConfiguration *config;
    GtkWidget        *row;
  } lookup = { configuration, NULL };

  g_return_if_fail (IDE_IS_BUILD_PERSPECTIVE (self));
  g_return_if_fail (!configuration || IDE_IS_CONFIGURATION (configuration));

  gtk_container_foreach (GTK_CONTAINER (self->list_box),
                         find_configuration_row,
                         &lookup);

  if (GTK_IS_LIST_BOX_ROW (lookup.row))
    gtk_list_box_select_row (self->list_box, GTK_LIST_BOX_ROW (lookup.row));
}

static gchar *
ide_build_perspective_get_icon_name (IdePerspective *perspective)
{
  return g_strdup ("builder-build-configure-symbolic");
}

static gchar *
ide_build_perspective_get_title (IdePerspective *perspective)
{
  return g_strdup (_("Build Preferences"));
}

static gchar *
ide_build_perspective_get_id (IdePerspective *perspective)
{
  return g_strdup ("buildperspective");
}

static gchar *
ide_build_perspective_get_accelerator (IdePerspective *perspective)
{
  return g_strdup ("<alt>comma");
}

static void
perspective_iface_init (IdePerspectiveInterface *iface)
{
  iface->get_icon_name = ide_build_perspective_get_icon_name;
  iface->get_title = ide_build_perspective_get_title;
  iface->get_id = ide_build_perspective_get_id;
  iface->get_accelerator = ide_build_perspective_get_accelerator;
}
