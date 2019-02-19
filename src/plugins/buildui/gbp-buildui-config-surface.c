/* gbp-buildui-config-surface.c
 *
 * Copyright 2018-2019 Christian Hergert <chergert@redhat.com>
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

#define G_LOG_DOMAIN "gbp-buildui-config-surface"

#include "config.h"

#include <dazzle.h>
#include <glib/gi18n.h>
#include <libpeas/peas.h>
#include <libide-gui.h>
#include <libide-foundry.h>

#include "gbp-buildui-config-surface.h"

struct _GbpBuilduiConfigSurface
{
  IdeSurface               parent_instance;

  IdeConfigManager *config_manager;

  GtkListBox              *config_list_box;
  GtkPaned                *paned;
  DzlPreferences          *preferences;

  /* raw pointer, only use for comparison */
  GtkListBoxRow           *last;
};

typedef struct
{
  DzlPreferences   *view;
  IdeConfig *config;
} AddinState;

G_DEFINE_TYPE (GbpBuilduiConfigSurface, gbp_buildui_config_surface, IDE_TYPE_SURFACE)

enum {
  PROP_0,
  PROP_CONFIG_MANAGER,
  N_PROPS
};

static GParamSpec *properties [N_PROPS];

static void
gbp_buildui_config_surface_foreach_cb (PeasExtensionSet *set,
                                       PeasPluginInfo   *plugin_info,
                                       PeasExtension    *exten,
                                       gpointer          user_data)
{
  IdeConfigViewAddin *addin = (IdeConfigViewAddin *)exten;
  AddinState *state = user_data;

  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (PEAS_IS_EXTENSION_SET (set));
  g_assert (plugin_info != NULL);
  g_assert (IDE_IS_CONFIG_VIEW_ADDIN (addin));
  g_assert (state != NULL);

  ide_config_view_addin_load (addin, state->view, state->config);
}

static void
gbp_buildui_config_surface_row_selected_cb (GbpBuilduiConfigSurface *self,
                                            GtkListBoxRow           *row,
                                            GtkListBox              *list_box)
{
  g_autoptr(PeasExtensionSet) set = NULL;
  IdeConfig *config;
  AddinState state = {0};
  GtkWidget *child2;

  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (GBP_IS_BUILDUI_CONFIG_SURFACE (self));
  g_assert (!row || GTK_IS_LIST_BOX_ROW (row));
  g_assert (GTK_IS_LIST_BOX (list_box));

  /* Prevent double applying settings so we don't lose state */
  if (row == self->last)
    return;
  self->last = row;

  /* Clear out any previous view/empty-state */
  child2 = gtk_paned_get_child2 (self->paned);
  if (child2 != NULL)
    gtk_widget_destroy (child2);
  g_assert (self->preferences == NULL);

  /* If no row was selected, add empty-state view */
  if (row == NULL)
    {
      GtkWidget *empty;

      /* Add an empty selection state instead of preferences view */
      empty = g_object_new (DZL_TYPE_EMPTY_STATE,
                            "icon-name", "builder-build-symbolic",
                            "title", _("No build configuration"),
                            "subtitle", _("Select a build configuration from the sidebar to modify."),
                            "visible", TRUE,
                            "hexpand", TRUE,
                            NULL);
      gtk_container_add (GTK_CONTAINER (self->paned), empty);

      return;
    }

  /* We have a configuration to display, so do it */
  self->preferences = g_object_new (DZL_TYPE_PREFERENCES_VIEW,
                                    "use-sidebar", FALSE,
                                    "visible", TRUE,
                                    NULL);
  g_signal_connect (self->preferences,
                    "destroy",
                    G_CALLBACK (gtk_widget_destroyed),
                    &self->preferences);
  gtk_container_add (GTK_CONTAINER (self->paned), GTK_WIDGET (self->preferences));

  config = g_object_get_data (G_OBJECT (row), "CONFIG");
  g_assert (IDE_IS_CONFIG (config));

  set = peas_extension_set_new (peas_engine_get_default (),
                                IDE_TYPE_CONFIG_VIEW_ADDIN,
                                NULL);

  state.view = self->preferences;
  state.config = config;

  peas_extension_set_foreach (set,
                              gbp_buildui_config_surface_foreach_cb,
                              &state);
}

static GtkWidget *
gbp_buildui_config_surface_create_row_cb (gpointer item,
                                          gpointer user_data)
{
  IdeConfig *config = item;
  const gchar *title;
  GtkWidget *row;
  GtkWidget *label;

  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (GBP_IS_BUILDUI_CONFIG_SURFACE (user_data));
  g_assert (IDE_IS_CONFIG (config));

  title = ide_config_get_display_name (config);

  row = g_object_new (GTK_TYPE_LIST_BOX_ROW,
                      "visible", TRUE,
                      NULL);
  label = g_object_new (GTK_TYPE_LABEL,
                        "visible", TRUE,
                        "label", title,
                        "xalign", 0.0f,
                        "margin", 6,
                        NULL);
  gtk_container_add (GTK_CONTAINER (row), GTK_WIDGET (label));

  g_object_set_data_full (G_OBJECT (row),
                          "CONFIG",
                          g_object_ref (config),
                          g_object_unref);

  return row;
}

static void
gbp_buildui_config_surface_set_config_manager (GbpBuilduiConfigSurface *self,
                                               IdeConfigManager *config_manager)
{
  g_assert (GBP_IS_BUILDUI_CONFIG_SURFACE (self));
  g_assert (IDE_IS_CONFIG_MANAGER (config_manager));
  g_assert (self->config_manager == NULL);

  g_set_object (&self->config_manager, config_manager);

  gtk_list_box_bind_model (self->config_list_box,
                           G_LIST_MODEL (config_manager),
                           gbp_buildui_config_surface_create_row_cb,
                           g_object_ref (self),
                           g_object_unref);
}

static void
header_func_cb (GtkListBoxRow *row,
                GtkListBoxRow *before,
                gpointer       user_data)
{
  if (before == NULL)
    {
      PangoAttrList *attrs;
      GtkWidget *header;

      attrs = pango_attr_list_new ();
      pango_attr_list_insert (attrs, pango_attr_weight_new (PANGO_WEIGHT_BOLD));
      pango_attr_list_insert (attrs, pango_attr_foreground_alpha_new (.55 * G_MAXUSHORT));

      header = g_object_new (GTK_TYPE_LABEL,
                             "attributes", attrs,
                             "label", _("Build Configurations"),
                             "xalign", 0.0f,
                             "visible", TRUE,
                             NULL);
      dzl_gtk_widget_add_style_class (header, "header");

      gtk_list_box_row_set_header (row, header);

      pango_attr_list_unref (attrs);
    }
}

static void
gbp_buildui_config_surface_get_property (GObject    *object,
                                         guint       prop_id,
                                         GValue     *value,
                                         GParamSpec *pspec)
{
  GbpBuilduiConfigSurface *self = GBP_BUILDUI_CONFIG_SURFACE (object);

  switch (prop_id)
    {
    case PROP_CONFIG_MANAGER:
      g_value_set_object (value, self->config_manager);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
gbp_buildui_config_surface_set_property (GObject      *object,
                                         guint         prop_id,
                                         const GValue *value,
                                         GParamSpec   *pspec)
{
  GbpBuilduiConfigSurface *self = GBP_BUILDUI_CONFIG_SURFACE (object);

  switch (prop_id)
    {
    case PROP_CONFIG_MANAGER:
      gbp_buildui_config_surface_set_config_manager (self, g_value_get_object (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
gbp_buildui_config_surface_class_init (GbpBuilduiConfigSurfaceClass *klass)
{
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->get_property = gbp_buildui_config_surface_get_property;
  object_class->set_property = gbp_buildui_config_surface_set_property;

  properties [PROP_CONFIG_MANAGER] =
    g_param_spec_object ("config-manager",
                         "Config Manager",
                         "The configuration manager",
                         IDE_TYPE_CONFIG_MANAGER,
                         (G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, N_PROPS, properties);

  gtk_widget_class_set_template_from_resource (widget_class, "/plugins/buildui/gbp-buildui-config-surface.ui");
  gtk_widget_class_bind_template_child (widget_class, GbpBuilduiConfigSurface, config_list_box);
  gtk_widget_class_bind_template_child (widget_class, GbpBuilduiConfigSurface, paned);
}

static void
gbp_buildui_config_surface_init (GbpBuilduiConfigSurface *self)
{
  DzlShortcutController *controller;

  gtk_widget_init_template (GTK_WIDGET (self));

  gtk_list_box_set_header_func (self->config_list_box, header_func_cb, NULL, NULL);

  g_signal_connect_object (self->config_list_box,
                           "row-selected",
                           G_CALLBACK (gbp_buildui_config_surface_row_selected_cb),
                           self,
                           G_CONNECT_SWAPPED);

  controller = dzl_shortcut_controller_find (GTK_WIDGET (self));
  dzl_shortcut_controller_add_command_action (controller,
                                              "org.gnome.builder.buildui.focus",
                                              "<alt>comma",
                                              DZL_SHORTCUT_PHASE_GLOBAL,
                                              "win.surface('buildui')");
}

static void
gbp_buildui_config_surface_set_config_cb (GtkWidget *widget,
                                          gpointer   user_data)
{
  IdeConfig *config = user_data;

  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (GTK_IS_LIST_BOX_ROW (widget));
  g_assert (IDE_IS_CONFIG (config));

  if (g_object_get_data (G_OBJECT (widget), "CONFIG") == (gpointer)config)
    {
      GtkListBox *list_box = GTK_LIST_BOX (gtk_widget_get_parent (widget));

      gtk_list_box_select_row (list_box, GTK_LIST_BOX_ROW (widget));
    }
}

void
gbp_buildui_config_surface_set_config (GbpBuilduiConfigSurface *self,
                                       IdeConfig        *config)
{
  g_return_if_fail (IDE_IS_MAIN_THREAD ());
  g_return_if_fail (GBP_IS_BUILDUI_CONFIG_SURFACE (self));
  g_return_if_fail (IDE_IS_CONFIG (config));

  gtk_container_foreach (GTK_CONTAINER (self->config_list_box),
                         gbp_buildui_config_surface_set_config_cb,
                         config);
}
