/* gbp-buildui-config-view-addin.c
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

#define G_LOG_DOMAIN "gbp-buildui-config-view-addin"

#include "config.h"

#include <glib/gi18n.h>
#include <libide-foundry.h>
#include <libide-gui.h>

#include "gbp-buildui-config-view-addin.h"
#include "gbp-buildui-runtime-categories.h"
#include "gbp-buildui-runtime-row.h"

struct _GbpBuilduiConfigViewAddin
{
  GObject parent_instance;
};

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
add_description_row (DzlPreferences *preferences,
                     const gchar    *page,
                     const gchar    *group,
                     const gchar    *title,
                     const gchar    *value,
                     GtkWidget      *value_widget)
{
  GtkWidget *widget;

  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (DZL_IS_PREFERENCES (preferences));

  widget = g_object_new (GTK_TYPE_LABEL,
                         "xalign", 0.0f,
                         "label", title,
                         "visible", TRUE,
                         "margin-right", 12,
                         NULL);
  dzl_gtk_widget_add_style_class (widget, "dim-label");

  if (value_widget == NULL)
    value_widget = g_object_new (GTK_TYPE_LABEL,
                                 "hexpand", TRUE,
                                 "label", value,
                                 "xalign", 0.0f,
                                 "visible", TRUE,
                                 NULL);

  dzl_preferences_add_table_row (preferences, page, group, widget, value_widget, NULL);
}

static GtkWidget *
create_stack_list_row (gpointer item,
                       gpointer user_data)
{
  IdeConfig *config = user_data;
  GtkWidget *row;

  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (IDE_IS_CONFIG (config));

  if (IDE_IS_RUNTIME (item))
    return gbp_buildui_runtime_row_new (item, config);

  row = g_object_new (GTK_TYPE_LIST_BOX_ROW,
                      "visible", TRUE,
                      NULL);
  g_object_set_data_full (G_OBJECT (row),
                          "ITEM",
                          g_object_ref (item),
                          g_object_unref);

  if (GBP_IS_BUILDUI_RUNTIME_CATEGORIES (item))
    {
      const gchar *category = gbp_buildui_runtime_categories_get_name (item);
      GtkWidget *label;

      label = g_object_new (GTK_TYPE_LABEL,
                            "label", category,
                            "margin", 10,
                            "use-markup", TRUE,
                            "visible", TRUE,
                            "xalign", 0.0f,
                            NULL);
      gtk_container_add (GTK_CONTAINER (row), label);
    }
  else if (DZL_IS_LIST_MODEL_FILTER (item))
    {
      const gchar *category = g_object_get_data (item, "CATEGORY");
      GtkWidget *label;

      label = g_object_new (GTK_TYPE_LABEL,
                            "label", category,
                            "margin", 10,
                            "use-markup", TRUE,
                            "visible", TRUE,
                            "xalign", 0.0f,
                            NULL);
      gtk_container_add (GTK_CONTAINER (row), label);
    }

  return row;
}

static void
on_runtime_row_activated_cb (DzlStackList  *stack_list,
                             GtkListBoxRow *row,
                             gpointer       user_data)
{
  IdeConfig *config = user_data;
  gpointer item;

  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (DZL_IS_STACK_LIST (stack_list));
  g_assert (GTK_IS_LIST_BOX_ROW (row));
  g_assert (IDE_IS_CONFIG (config));

  if (GBP_IS_BUILDUI_RUNTIME_ROW (row))
    {
      const gchar *id;

      id = gbp_buildui_runtime_row_get_id (GBP_BUILDUI_RUNTIME_ROW (row));
      ide_config_set_runtime_id (config, id);

      {
        GtkWidget *box;

        if ((box = gtk_widget_get_ancestor (GTK_WIDGET (row), GTK_TYPE_LIST_BOX)))
          gtk_list_box_unselect_all (GTK_LIST_BOX (box));
      }

      return;
    }

  item = g_object_get_data (G_OBJECT (row), "ITEM");
  g_assert (G_IS_LIST_MODEL (item) || IDE_IS_RUNTIME (item));

  if (GBP_IS_BUILDUI_RUNTIME_CATEGORIES (item))
    {
      dzl_stack_list_push (stack_list,
                           create_stack_list_row (item, config),
                           G_LIST_MODEL (item),
                           create_stack_list_row,
                           g_object_ref (config),
                           g_object_unref);
    }
  else if (G_IS_LIST_MODEL (item))
    {
      dzl_stack_list_push (stack_list,
                           create_stack_list_row (item, config),
                           G_LIST_MODEL (item),
                           create_stack_list_row,
                           g_object_ref (config),
                           g_object_unref);
    }
}

static GtkWidget *
create_runtime_box (IdeConfig  *config,
                    IdeRuntimeManager *runtime_manager)
{
  g_autoptr(GbpBuilduiRuntimeCategories) filter = NULL;
  DzlStackList *stack;
  const gchar *category;
  IdeRuntime *runtime;
  GtkWidget *header;
  GtkWidget *frame;

  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (IDE_IS_CONFIG (config));
  g_assert (IDE_IS_RUNTIME_MANAGER (runtime_manager));

  filter = gbp_buildui_runtime_categories_new (runtime_manager, NULL);

  frame = g_object_new (GTK_TYPE_FRAME,
                        "visible", TRUE,
                        NULL);

  header = g_object_new (GTK_TYPE_LABEL,
                         "label", _("All Runtimes"),
                         "margin", 10,
                         "visible", TRUE,
                         "xalign", 0.0f,
                         NULL);

  stack = g_object_new (DZL_TYPE_STACK_LIST,
                        "visible", TRUE,
                        NULL);
  dzl_stack_list_push (stack,
                       header,
                       G_LIST_MODEL (filter),
                       create_stack_list_row,
                       g_object_ref (config),
                       g_object_unref);
  gtk_container_add (GTK_CONTAINER (frame), GTK_WIDGET (stack));

  g_signal_connect_object (stack,
                           "row-activated",
                           G_CALLBACK (on_runtime_row_activated_cb),
                           config,
                           0);

  if ((runtime = ide_config_get_runtime (config)) &&
      (category = ide_runtime_get_category (runtime)))
    {
      g_autoptr(GString) prefix = g_string_new (NULL);
      g_auto(GStrv) parts = g_strsplit (category, "/", 0);

      for (guint i = 0; parts[i]; i++)
        {
          g_autoptr(GListModel) model = NULL;

          g_string_append (prefix, parts[i]);
          if (parts[i+1])
            g_string_append_c (prefix, '/');

          model = gbp_buildui_runtime_categories_create_child_model (filter, prefix->str);

          dzl_stack_list_push (stack,
                               create_stack_list_row (model, config),
                               model,
                               create_stack_list_row,
                               g_object_ref (config),
                               g_object_unref);
        }
    }

  return GTK_WIDGET (frame);
}

static void
notify_toolchain_id (IdeConfig *config,
                     GParamSpec       *pspec,
                     GtkImage         *image)
{
  const gchar *toolchain_id;
  const gchar *current;

  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (IDE_IS_CONFIG (config));
  g_assert (GTK_IS_IMAGE (image));

  toolchain_id = ide_config_get_toolchain_id (config);
  current = g_object_get_data (G_OBJECT (image), "TOOLCHAIN_ID");

  gtk_widget_set_visible (GTK_WIDGET (image), ide_str_equal0 (toolchain_id, current));
}

static GtkWidget *
create_toolchain_row (gpointer item,
                      gpointer user_data)
{
  IdeToolchain *toolchain = item;
  IdeConfig *config = user_data;
  const gchar *toolchain_id;
  GtkWidget *label;
  GtkWidget *row;
  GtkWidget *box;
  GtkImage *image;

  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (IDE_IS_TOOLCHAIN (toolchain));
  g_assert (IDE_IS_CONFIG (config));

  toolchain_id = ide_toolchain_get_id (toolchain);

  row = g_object_new (GTK_TYPE_LIST_BOX_ROW,
                      "visible", TRUE,
                      NULL);
  g_object_set_data_full (G_OBJECT (row), "TOOLCHAIN_ID", g_strdup (toolchain_id), g_free);

  box = g_object_new (GTK_TYPE_BOX,
                      "spacing", 6,
                      "visible", TRUE,
                      NULL);
  gtk_container_add (GTK_CONTAINER (row), box);

  label = g_object_new (GTK_TYPE_LABEL,
                        "label", ide_toolchain_get_display_name (toolchain),
                        "visible", TRUE,
                        "xalign", 0.0f,
                        NULL);
  gtk_container_add (GTK_CONTAINER (box), label);

  image = g_object_new (GTK_TYPE_IMAGE,
                        "icon-name", "object-select-symbolic",
                        "halign", GTK_ALIGN_START,
                        "hexpand", TRUE,
                        NULL);
  g_object_set_data_full (G_OBJECT (image), "TOOLCHAIN_ID", g_strdup (toolchain_id), g_free);
  gtk_container_add (GTK_CONTAINER (box), GTK_WIDGET (image));

  g_signal_connect_object (config,
                           "notify::toolchain-id",
                           G_CALLBACK (notify_toolchain_id),
                           image,
                           0);
  notify_toolchain_id (config, NULL, image);

  return row;
}

static void
on_toolchain_row_activated_cb (GtkListBox       *list_box,
                               GtkListBoxRow    *row,
                               IdeConfig *config)
{
  const gchar *toolchain_id;

  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (GTK_IS_LIST_BOX (list_box));
  g_assert (GTK_IS_LIST_BOX_ROW (row));
  g_assert (IDE_IS_CONFIG (config));

  if ((toolchain_id = g_object_get_data (G_OBJECT (row), "TOOLCHAIN_ID")))
    ide_config_set_toolchain_id (config, toolchain_id);

  gtk_list_box_unselect_all (list_box);
}

static GtkWidget *
create_toolchain_box (IdeConfig    *config,
                      IdeToolchainManager *toolchain_manager)
{
  GtkScrolledWindow *scroller;
  GtkListBox *list_box;

  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (IDE_IS_CONFIG (config));
  g_assert (IDE_IS_TOOLCHAIN_MANAGER (toolchain_manager));

  scroller = g_object_new (GTK_TYPE_SCROLLED_WINDOW,
                           "propagate-natural-height", TRUE,
                           "shadow-type", GTK_SHADOW_IN,
                           "visible", TRUE,
                           NULL);

  list_box = g_object_new (GTK_TYPE_LIST_BOX,
                           "visible", TRUE,
                           NULL);
  g_signal_connect_object (list_box,
                           "row-activated",
                           G_CALLBACK (on_toolchain_row_activated_cb),
                           config,
                           0);
  gtk_container_add (GTK_CONTAINER (scroller), GTK_WIDGET (list_box));

  gtk_list_box_bind_model (list_box,
                           G_LIST_MODEL (toolchain_manager),
                           create_toolchain_row,
                           g_object_ref (config),
                           g_object_unref);

  return GTK_WIDGET (scroller);
}

static void
gbp_buildui_config_view_addin_load (IdeConfigViewAddin *addin,
                                    DzlPreferences     *preferences,
                                    IdeConfig   *config)
{
  IdeToolchainManager *toolchain_manager;
  IdeRuntimeManager *runtime_manager;
  g_autoptr(GFile) workdir = NULL;
  IdeBuildSystem *build_system;
  IdeEnvironment *environ_;
  IdeContext *context;
  GtkWidget *box;
  GtkWidget *entry;
  static const struct {
    const gchar *label;
    const gchar *action;
    const gchar *tooltip;
    const gchar *style_class;
  } actions[] = {
    { N_("Make _Active"), "config-manager.current", N_("Select this configuration as the active configuration.") },
    { N_("_Duplicate"), "config-manager.duplicate", N_("Duplicating the configuration allows making changes without modifying this configuration.") },
    { N_("_Remove"), "config-manager.delete", N_("Removes the configuration and cannot be undone."), "destructive-action" },
  };

  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (GBP_IS_BUILDUI_CONFIG_VIEW_ADDIN (addin));
  g_assert (DZL_IS_PREFERENCES (preferences));
  g_assert (IDE_IS_CONFIG (config));

  /* Get manager objects */
  context = ide_object_get_context (IDE_OBJECT (config));
  runtime_manager = ide_runtime_manager_from_context (context);
  toolchain_manager = ide_toolchain_manager_from_context (context);
  build_system = ide_build_system_from_context (context);
  workdir = ide_context_ref_workdir (context);

  /* Add our pages */
  dzl_preferences_add_page (preferences, "general", _("General"), 0);
  dzl_preferences_add_page (preferences, "environ", _("Environment"), 10);

  /* Add groups to pages */
  dzl_preferences_add_list_group (preferences, "general", "general", _("Overview"), GTK_SELECTION_NONE, 0);
  dzl_preferences_add_group (preferences, "general", "buttons", NULL, 0);
  dzl_preferences_add_group (preferences, "environ", "build", _("Build Environment"), 0);

  /* actions button box */
  box = g_object_new (GTK_TYPE_BOX,
                      "homogeneous", TRUE,
                      "spacing", 12,
                      "visible", TRUE,
                      NULL);
  for (guint i = 0; i < G_N_ELEMENTS (actions); i++)
    {
      GtkWidget *button;

      button = g_object_new (GTK_TYPE_BUTTON,
                             "visible", TRUE,
                             "action-name", actions[i].action,
                             "action-target", g_variant_new_string (ide_config_get_id (config)),
                             "label", g_dgettext (GETTEXT_PACKAGE, actions[i].label),
                             "tooltip-text", g_dgettext (GETTEXT_PACKAGE, actions[i].tooltip),
                             "use-underline", TRUE,
                             NULL);
      if (actions[i].style_class)
        dzl_gtk_widget_add_style_class (button, actions[i].style_class);
      gtk_container_add (GTK_CONTAINER (box), button);
    }

  /* Add description info */
  add_description_row (preferences, "general", "general", _("Name"), ide_config_get_display_name (config), NULL);
  add_description_row (preferences, "general", "general", _("Source Directory"), g_file_peek_path (workdir), NULL);
  add_description_row (preferences, "general", "general", _("Build System"), ide_build_system_get_display_name (build_system), NULL);

  entry = g_object_new (GTK_TYPE_ENTRY,
                        "visible", TRUE,
                        "hexpand", TRUE,
                        NULL);
  g_object_bind_property_full (config, "prefix", entry, "text",
                               G_BINDING_SYNC_CREATE | G_BINDING_BIDIRECTIONAL,
                               treat_null_as_empty, NULL, NULL, NULL);
  add_description_row (preferences, "general", "general", _("Install Prefix"), NULL, entry);

  entry = g_object_new (GTK_TYPE_ENTRY,
                        "visible", TRUE,
                        "hexpand", TRUE,
                        NULL);
  g_object_bind_property_full (config, "config-opts", entry, "text",
                               G_BINDING_SYNC_CREATE | G_BINDING_BIDIRECTIONAL,
                               treat_null_as_empty, NULL, NULL, NULL);
  add_description_row (preferences, "general", "general", _("Configure Options"), NULL, entry);

  dzl_preferences_add_custom (preferences, "general", "buttons", box, NULL, 5);

  /* Setup runtime selection */
  dzl_preferences_add_group (preferences, "general", "runtime", _("Application Runtime"), 10);
  dzl_preferences_add_custom (preferences, "general", "runtime", create_runtime_box (config, runtime_manager), NULL, 10);

  /* Setup toolchain selection */
  dzl_preferences_add_group (preferences, "general", "toolchain", _("Build Toolchain"), 20);
  dzl_preferences_add_custom (preferences, "general", "toolchain", create_toolchain_box (config, toolchain_manager), NULL, 10);

  /* Add environment selector */
  environ_ = ide_config_get_environment (config);
  dzl_preferences_add_custom (preferences, "environ", "build",
                              g_object_new (GTK_TYPE_FRAME,
                                            "visible", TRUE,
                                            "child", g_object_new (IDE_TYPE_ENVIRONMENT_EDITOR,
                                                                   "environment", environ_,
                                                                   "visible", TRUE,
                                                                   NULL),
                                            NULL),
                              NULL, 0);
}

static void
config_view_addin_iface_init (IdeConfigViewAddinInterface *iface)
{
  iface->load = gbp_buildui_config_view_addin_load;
}

G_DEFINE_TYPE_WITH_CODE (GbpBuilduiConfigViewAddin, gbp_buildui_config_view_addin, G_TYPE_OBJECT,
                         G_IMPLEMENT_INTERFACE (IDE_TYPE_CONFIG_VIEW_ADDIN, config_view_addin_iface_init))

static void
gbp_buildui_config_view_addin_class_init (GbpBuilduiConfigViewAddinClass *klass)
{
}

static void
gbp_buildui_config_view_addin_init (GbpBuilduiConfigViewAddin *self)
{
}
