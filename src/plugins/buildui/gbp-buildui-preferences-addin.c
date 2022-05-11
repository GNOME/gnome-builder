/* gbp-buildui-preferences-addin.h
 *
 * Copyright 2022 Georges Basile Stavracas Neto <georges.stavracas@gmail.com>
 *           2018-2019 Christian Hergert <chergert@redhat.com>
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

#define G_LOG_DOMAIN "gbp-buildui-preferences-addin"

#include "config.h"

#include <glib/gi18n.h>

#include <libide-sourceview.h>

#include "gbp-buildui-preferences-addin.h"
#include "gbp-buildui-runtime-categories.h"
#include "gbp-buildui-runtime-row.h"

struct _GbpBuilduiPreferencesAddin
{
  GObject parent_instance;
};

static GtkWidget *
create_overview_row (const char *title,
                     const char *value)
{
  GtkWidget *row;

  row = g_object_new (ADW_TYPE_ENTRY_ROW,
                      "title", title,
                      "text", value,
                      "editable", FALSE,
                      "show-apply-button", FALSE,
                      NULL);

  return row;
}

static void
overview_func (const char                   *page_name,
               const IdePreferenceItemEntry *entry,
               AdwPreferencesGroup          *group,
               gpointer                      user_data)
{
  IdeContext *context = user_data;

  g_assert (IDE_IS_CONTEXT (context));
  g_assert (ADW_IS_PREFERENCES_GROUP (group));

  if (FALSE) {}
  else if (g_strcmp0 (entry->name, "kind") == 0)
    {
      IdeBuildSystem *build_system = ide_build_system_from_context (context);
      g_autofree char *name = ide_build_system_get_display_name (build_system);

      adw_preferences_group_add (group, create_overview_row (entry->title, name));
    }
  else if (g_strcmp0 (entry->name, "srcdir") == 0)
    {
      g_autoptr(GFile) workdir = ide_context_ref_workdir (context);
      g_autofree char *text = NULL;

      if (g_file_is_native (workdir))
        text = ide_path_collapse (g_file_peek_path (workdir));
      else
        text = g_file_get_uri (workdir);

      adw_preferences_group_add (group, create_overview_row (entry->title, text));
    }
  else if (g_strcmp0 (entry->name, "vcsuri") == 0)
    {
      IdeVcs *vcs = ide_vcs_from_context (context);
      g_autofree char *name = NULL;

      if (vcs != NULL)
        name = ide_vcs_get_display_name (vcs);
      else
        name = g_strdup (_("No Version Control"));

      adw_preferences_group_add (group, create_overview_row (entry->title, name));
    }
}

static const IdePreferenceGroupEntry overview_groups[] = {
  { "overview", "project",        0, N_("Project") },
  { "overview", "runtime",      100, N_("Runtime") },
};

static const IdePreferenceItemEntry overview_items[] = {
  { "overview", "project", "kind", 0, overview_func, N_("Build System") },
  { "overview", "project", "srcdir", 0, overview_func, N_("Source Directory") },
  { "overview", "project", "vcsuri", 0, overview_func, N_("Version Control") },
};

static gboolean
treat_null_as_empty (GBinding     *binding,
                     const GValue *from_value,
                     GValue       *to_value,
                     gpointer      user_data)
{
  const char *str = g_value_get_string (from_value);
  g_value_set_string (to_value, str ?: "");
  return TRUE;
}

static void
add_description_row (AdwPreferencesGroup *group,
                     const char          *title,
                     const char          *value)
{
  GtkWidget *label;
  GtkWidget *row;

  label = g_object_new (GTK_TYPE_LABEL,
                        "label", value,
                        "tooltip-text", value,
                        "selectable", TRUE,
                        "max-width-chars", 30,
                        NULL);
  gtk_widget_add_css_class (label, "dim-label");

  row = g_object_new (ADW_TYPE_ACTION_ROW,
                      "title", title,
                      NULL);
  adw_action_row_add_suffix (ADW_ACTION_ROW (row), label);
  adw_preferences_group_add (group, row);
}

static void
add_entry_row (AdwPreferencesGroup *group,
               const char          *title,
               gpointer             source_object,
               const char          *bind_property)
{
  GtkWidget *row;

  row = g_object_new (ADW_TYPE_ENTRY_ROW,
                      "title", title,
                      NULL);

  g_object_bind_property_full (source_object, bind_property,
                               row, "text",
                               G_BINDING_SYNC_CREATE | G_BINDING_BIDIRECTIONAL,
                               treat_null_as_empty, NULL, NULL, NULL);

  adw_preferences_group_add (group, row);
}

static void
create_general_widgetry (const char                   *page_name,
                         const IdePreferenceItemEntry *entry,
                         AdwPreferencesGroup          *group,
                         gpointer                      user_data)
{
  g_autoptr(GFile) workdir = NULL;
  IdeConfig *config = user_data;
  IdeConfigManager *config_manager;
  IdeBuildSystem *build_system;
  IdeContext *context;
  GtkWidget *box;
  static const struct {
    const gchar *label;
    const gchar *action;
    const gchar *tooltip;
    const gchar *style_class;
  } actions[] = {
    {
      N_("Make _Active"), "config-manager.current",
      N_("Select this configuration as the active configuration."),
    },
    {
      N_("_Duplicate"), "config-manager.duplicate",
      N_("Duplicating the configuration allows making changes without modifying this configuration."),
    },
    {
      N_("_Remove"), "config-manager.delete",
      N_("Removes the configuration and cannot be undone."),
      "destructive-action",
    },
  };

  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (ADW_IS_PREFERENCES_GROUP (group));
  g_assert (IDE_IS_CONFIG (config));
  g_assert (page_name != NULL && g_str_equal (page_name, ide_config_get_id (config)));

  context = ide_object_get_context (IDE_OBJECT (config));
  build_system = ide_build_system_from_context (context);
  workdir = ide_context_ref_workdir (context);

  add_description_row (group, _("Name"), ide_config_get_display_name (config));
  add_description_row (group, _("Source Directory"), g_file_peek_path (workdir));
  add_description_row (group, _("Build System"), ide_build_system_get_display_name (build_system));
  /* Translators: "Install" is a noun here */
  add_entry_row (group, _("Install Prefix"), config, "prefix");
  /* Translators: "Configure" is a noun here */
  add_entry_row (group, _("Configure Options"), config, "config-opts");
  /* Translators: "Run" is a noun here, this string is analogous to "Execution Options" */
  add_entry_row (group, _("Run Options"), config, "run-opts");

  config_manager = ide_config_manager_from_context (context);
  gtk_widget_insert_action_group (GTK_WIDGET (group),
                                  "config-manager",
                                  G_ACTION_GROUP (config_manager));

  /* actions button box */
  box = g_object_new (GTK_TYPE_BOX,
                      "homogeneous", TRUE,
                      "margin-top", 12,
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
        gtk_widget_add_css_class (button, actions[i].style_class);
      gtk_box_append (GTK_BOX (box), button);
    }

  adw_preferences_group_add (group, box);
}

static void
notify_toolchain_id (IdeConfig  *config,
                     GParamSpec *pspec,
                     GtkImage   *image)
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
  GtkWidget *image;
  GtkWidget *row;

  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (IDE_IS_TOOLCHAIN (toolchain));
  g_assert (IDE_IS_CONFIG (config));

  toolchain_id = ide_toolchain_get_id (toolchain);

  row = g_object_new (ADW_TYPE_ACTION_ROW,
                      "title", ide_toolchain_get_display_name (toolchain),
                      NULL);

  g_object_set_data_full (G_OBJECT (row), "TOOLCHAIN_ID", g_strdup (toolchain_id), g_free);

  image = g_object_new (GTK_TYPE_IMAGE,
                        "icon-name", "object-select-symbolic",
                        "valign", GTK_ALIGN_CENTER,
                        NULL);
  g_object_set_data_full (G_OBJECT (image), "TOOLCHAIN_ID", g_strdup (toolchain_id), g_free);
  adw_action_row_add_suffix (ADW_ACTION_ROW (row), image);

  g_signal_connect_object (config,
                           "notify::toolchain-id",
                           G_CALLBACK (notify_toolchain_id),
                           image,
                           0);

  notify_toolchain_id (config, NULL, GTK_IMAGE (image));

  return row;
}

static void
on_toolchain_row_activated_cb (GtkListBox    *list_box,
                               GtkListBoxRow *row,
                               IdeConfig     *config)
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

static void
create_toolchain_widgetry (const char                   *page_name,
                           const IdePreferenceItemEntry *entry,
                           AdwPreferencesGroup          *group,
                           gpointer                      user_data)
{
  IdeToolchainManager *toolchain_manager;
  IdeConfig *config = user_data;
  GtkListBox *listbox;
  IdeContext *context;

  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (ADW_IS_PREFERENCES_GROUP (group));
  g_assert (IDE_IS_CONFIG (config));
  g_assert (page_name != NULL && g_str_equal (page_name, ide_config_get_id (config)));

  context = ide_object_get_context (IDE_OBJECT (config));
  toolchain_manager = ide_toolchain_manager_from_context (context);

  listbox = GTK_LIST_BOX (gtk_list_box_new ());
  gtk_widget_add_css_class (GTK_WIDGET (listbox), "boxed-list");
  g_signal_connect_object (listbox,
                           "row-activated",
                           G_CALLBACK (on_toolchain_row_activated_cb),
                           config,
                           0);

  gtk_list_box_bind_model (listbox,
                           G_LIST_MODEL (toolchain_manager),
                           create_toolchain_row,
                           g_object_ref (config),
                           g_object_unref);

  adw_preferences_group_add (group, GTK_WIDGET (listbox));
}

static void
create_runtime_widgetry (const char                   *page_name,
                         const IdePreferenceItemEntry *entry,
                         AdwPreferencesGroup          *group,
                         gpointer                      user_data)
{
  g_autoptr(GbpBuilduiRuntimeCategories) filter = NULL;
  IdeRuntimeManager *runtime_manager;
  IdeContext *context;
  IdeConfig *config = user_data;

  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (ADW_IS_PREFERENCES_GROUP (group));
  g_assert (IDE_IS_CONFIG (config));
  g_assert (page_name != NULL && g_str_equal (page_name, ide_config_get_id (config)));

  context = ide_object_get_context (IDE_OBJECT (config));
  runtime_manager = ide_runtime_manager_from_context (context);

  filter = gbp_buildui_runtime_categories_new (runtime_manager, NULL);

  // TODO
}

static void
gbp_buildui_preferences_addin_load (IdePreferencesAddin  *addin,
                                    IdePreferencesWindow *window,
                                    IdeContext           *context)
{
  GbpBuilduiPreferencesAddin *self = (GbpBuilduiPreferencesAddin *)addin;
  IdePreferencePageEntry *pages;
  IdeConfigManager *config_manager;
  guint n_configs;

  IDE_ENTRY;

  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (GBP_IS_BUILDUI_PREFERENCES_ADDIN (self));
  g_assert (IDE_IS_PREFERENCES_WINDOW (window));
  g_assert (!context || IDE_IS_CONTEXT (context));
  g_assert (ide_preferences_window_get_mode (window) == IDE_PREFERENCES_MODE_PROJECT);

  ide_preferences_window_add_groups (window,
                                     overview_groups,
                                     G_N_ELEMENTS (overview_groups),
                                     NULL);
  ide_preferences_window_add_items (window,
                                    overview_items,
                                    G_N_ELEMENTS (overview_items),
                                    g_object_ref (context),
                                    g_object_unref);

  config_manager = ide_config_manager_from_context (context);
  n_configs = g_list_model_get_n_items (G_LIST_MODEL (config_manager));
  pages = g_new0 (IdePreferencePageEntry, n_configs);
  for (guint i = 0; i < n_configs; i++)
    {
      IdePreferencePageEntry *page;
      g_autoptr(IdeConfig) config = NULL;

      page = &pages[i];
      config = g_list_model_get_item (G_LIST_MODEL (config_manager), i);

      page->parent = "build";
      page->section = NULL;
      page->name = ide_config_get_id (config);
      page->icon_name = NULL;
      page->title = ide_config_get_display_name (config);
    }

  ide_preferences_window_add_pages (window, pages, n_configs, NULL);

  for (guint i = 0; i < n_configs; i++)
    {
      g_autoptr(IdeConfig) config = g_list_model_get_item (G_LIST_MODEL (config_manager), i);
      const char *page = ide_config_get_id (config);

      const IdePreferenceGroupEntry groups[] = {
        { page, "general",     0, N_("General") },
        { page, "toolchain", 100, N_("Build Toolchain") },
        { page, "runtime",   200, N_("Application Runtime") },
      };

      const IdePreferenceItemEntry items[] = {
        { page, "general",   "general",     0, create_general_widgetry },
        { page, "toolchain", "toolchain", 100, create_toolchain_widgetry },
        { page, "runtime",   "runtime",   200, create_runtime_widgetry },
      };

      ide_preferences_window_add_groups (window, groups, G_N_ELEMENTS (groups), NULL);
      ide_preferences_window_add_items (window, items, G_N_ELEMENTS (items), config, NULL);
    }

  IDE_EXIT;
}

static void
preferences_addin_init (IdePreferencesAddinInterface *iface)
{
  iface->load = gbp_buildui_preferences_addin_load;
}

G_DEFINE_TYPE_WITH_CODE (GbpBuilduiPreferencesAddin, gbp_buildui_preferences_addin, G_TYPE_OBJECT,
                         G_IMPLEMENT_INTERFACE (IDE_TYPE_PREFERENCES_ADDIN, preferences_addin_init))

static void
gbp_buildui_preferences_addin_class_init (GbpBuilduiPreferencesAddinClass *klass)
{
}

static void
gbp_buildui_preferences_addin_init (GbpBuilduiPreferencesAddin *self)
{
}
