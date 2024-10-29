/* ide-application-tweaks.c
 *
 * Copyright 2022 Christian Hergert <chergert@redhat.com>
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

#define G_LOG_DOMAIN "ide-application-tweaks"

#include "config.h"

#include <libide-plugins.h>
#include <libide-tweaks.h>

#include "ide-application-private.h"
#include "ide-application-tweaks.h"
#include "ide-plugin-view.h"
#include "ide-primary-workspace.h"

#include "ide-plugin-section-private.h"

static const char *tweaks_resources[] = {
  "resource:///org/gnome/libide-gui/tweaks.ui",
};

static GtkWidget *
create_plugin_toggle (IdeTweaksWidget *instance,
                      IdeTweaksWidget *widget,
                      IdePlugin       *plugin)
{
  g_autofree char *schema_path = NULL;
  g_autoptr(GSettings) settings = NULL;
  AdwExpanderRow *row;
  GtkWidget *view;
  GtkSwitch *toggle;
  const char *id;

  g_assert (IDE_IS_TWEAKS_WIDGET (instance));
  g_assert (IDE_IS_TWEAKS_WIDGET (widget));
  g_assert (IDE_IS_PLUGIN (plugin));

  id = ide_plugin_get_id (plugin);

  toggle = g_object_new (GTK_TYPE_SWITCH,
                         "valign", GTK_ALIGN_CENTER,
                         NULL);
  row = g_object_new (ADW_TYPE_EXPANDER_ROW,
                      "title", ide_plugin_get_name (plugin),
                      "subtitle", ide_plugin_get_description (plugin),
                      "show-enable-switch", FALSE,
                      NULL);
  adw_expander_row_add_suffix (row, GTK_WIDGET (toggle));

  view = g_object_new (IDE_TYPE_PLUGIN_VIEW,
                       "plugin", plugin,
                       NULL);
  adw_expander_row_add_row (row, GTK_WIDGET (view));

  schema_path = g_strdup_printf ("/org/gnome/builder/plugins/%s/", id);
  settings = g_settings_new_with_path ("org.gnome.builder.plugin", schema_path);
  g_object_set_data_full (G_OBJECT (row),
                          "SETTINGS",
                          g_object_ref (settings),
                          g_object_unref);

  g_settings_bind (settings, "enabled", toggle, "active", G_SETTINGS_BIND_DEFAULT);

  return GTK_WIDGET (row);
}

static void
add_plugin_tweaks (IdeTweaksPage *page)
{
  g_autoptr(GHashTable) categories = g_hash_table_new_full (g_str_hash, g_str_equal, NULL, g_object_unref);
  IdeTweaksPage *category_page;
  GListModel *sections;
  guint n_sections;

  g_return_if_fail (IDE_IS_TWEAKS_PAGE (page));

  sections = _ide_plugin_section_get_all ();
  n_sections = g_list_model_get_n_items (sections);

  for (guint i = 0; i < n_sections; i++)
    {
      g_autoptr(IdeTweaksSection) t_section = ide_tweaks_section_new ();
      g_autoptr(IdePluginSection) section = g_list_model_get_item (sections, i);
      GListModel *plugins = ide_plugin_section_get_plugins (section);
      guint n_plugins;

      ide_tweaks_section_set_title (t_section,
                                    ide_plugin_section_get_id (section));
      ide_tweaks_item_insert_after (IDE_TWEAKS_ITEM (t_section),
                                    IDE_TWEAKS_ITEM (page),
                                    NULL);

      n_plugins = g_list_model_get_n_items (plugins);

      for (guint j = 0; j < n_plugins; j++)
        {
          g_autoptr(IdePlugin) plugin = g_list_model_get_item (plugins, j);
          const char *category_id = ide_plugin_get_category_id (plugin);
          const char *category = ide_plugin_get_category (plugin);
          g_autoptr(IdeTweaksWidget) widget = NULL;
          IdeTweaksGroup *group;

          if (!(category_page = g_hash_table_lookup (categories, category)))
            {
              g_autofree char *page_id = g_strdup_printf ("plugin_%s_page", category_id);
              g_autoptr(IdeTweaksGroup) first_group = ide_tweaks_group_new ();

              category_page = ide_tweaks_page_new ();
              GTK_BUILDABLE_GET_IFACE (category_page)->set_id (GTK_BUILDABLE (category_page), page_id);
              ide_tweaks_page_set_title (category_page, category);
              ide_tweaks_page_set_show_icon (category_page, FALSE);
              ide_tweaks_item_insert_after (IDE_TWEAKS_ITEM (category_page),
                                            IDE_TWEAKS_ITEM (t_section),
                                            NULL);
              ide_tweaks_item_insert_after (IDE_TWEAKS_ITEM (first_group),
                                            IDE_TWEAKS_ITEM (category_page),
                                            NULL);
              g_hash_table_insert (categories, (char *)category, category_page);
            }

          group = IDE_TWEAKS_GROUP (ide_tweaks_item_get_first_child (IDE_TWEAKS_ITEM (category_page)));
          widget = ide_tweaks_widget_new ();
          g_signal_connect_object (widget,
                                   "create-for-item",
                                   G_CALLBACK (create_plugin_toggle),
                                   plugin,
                                   0);

          ide_tweaks_item_insert_after (IDE_TWEAKS_ITEM (widget),
                                        IDE_TWEAKS_ITEM (group),
                                        NULL);
        }
    }
}

static gboolean
try_reuse_window (IdeTweaksWindow *window,
                  IdeContext      *context,
                  const char      *page)
{
  g_autofree char *context_project_id = NULL;
  const char *project_id;
  IdeTweaks *tweaks;
  GObject *object = NULL;

  g_assert (IDE_IS_TWEAKS_WINDOW (window));
  g_assert (!context || IDE_IS_CONTEXT (context));

  if (!(tweaks = ide_tweaks_window_get_tweaks (window)))
    return FALSE;

  project_id = ide_tweaks_get_project_id (tweaks);
  context_project_id = context ? ide_context_dup_project_id (context) : NULL;
  if (g_strcmp0 (project_id, context_project_id) != 0)
    return FALSE;

  if (page != NULL)
    {
      if (!(object = ide_tweaks_get_object (tweaks, page)) ||
          !IDE_IS_TWEAKS_ITEM (object))
        return FALSE;
    }

  ide_tweaks_window_navigate_initial (window);

  if (object != NULL)
    ide_tweaks_window_navigate_to (window, IDE_TWEAKS_ITEM (object));

  gtk_window_present (GTK_WINDOW (window));

  return TRUE;
}

void
ide_show_tweaks (IdeContext *context,
                 const char *page)
{
  IdeApplication *app = IDE_APPLICATION_DEFAULT;
  g_autoptr(IdeTweaks) tweaks = NULL;
  IdeTweaksWindow *window;
  IdeWorkbench *workbench;
  GtkWindow *toplevel = NULL;

  g_return_if_fail (IDE_IS_MAIN_THREAD ());
  g_return_if_fail (IDE_IS_APPLICATION (app));
  g_return_if_fail (!context || IDE_IS_CONTEXT (context));

  if ((workbench = ide_application_get_active_workbench (app)))
    {
      GList *windows = gtk_window_group_list_windows (GTK_WINDOW_GROUP (workbench));
      gboolean found = FALSE;

      for (const GList *iter = windows; !found && iter; iter = iter->next)
        {
          GtkWindow *win = iter->data;

          if (IDE_IS_TWEAKS_WINDOW (win) &&
              try_reuse_window (IDE_TWEAKS_WINDOW (win), context, page))
            found = TRUE;
          else if (IDE_IS_PRIMARY_WORKSPACE (win))
            toplevel = win;
        }

      if (!found && windows != NULL)
        {
          for (const GList *iter = windows; iter; iter = iter->next)
            {
              GtkWindow *win = iter->data;

              if (IDE_IS_WORKSPACE (win))
                {
                  toplevel = win;
                  break;
                }
            }
        }

      g_list_free (windows);

      if (found)
        return;
    }

  tweaks = ide_tweaks_new_for_context (context);

  /* Load our base tweaks scaffolding */
  for (guint i = 0; i < G_N_ELEMENTS (tweaks_resources); i++)
    {
      g_autoptr(GFile) tweaks_file = g_file_new_for_uri (tweaks_resources[i]);
      g_autoptr(GError) error = NULL;

      ide_tweaks_load_from_file (tweaks, tweaks_file, NULL, &error);

      if (error != NULL)
        g_critical ("Failed to load tweaks: %s", error->message);
    }

  /* Expose plugin toggles when in application mode */
  if (context == NULL)
    add_plugin_tweaks (IDE_TWEAKS_PAGE (ide_tweaks_get_object (tweaks, "plugins_page")));

  /* Prepare window and setup :application */
  window = g_object_new (IDE_TYPE_TWEAKS_WINDOW,
                         "tweaks", tweaks,
                         "transient-for", toplevel,
                         NULL);

  if (workbench != NULL)
    gtk_window_group_add_window (GTK_WINDOW_GROUP (workbench), GTK_WINDOW (window));
  else
    gtk_application_add_window (GTK_APPLICATION (app), GTK_WINDOW (window));

  /* Switch pages before we display if necessary */
  if (page != NULL)
    {
      GObject *object;

      if ((object = ide_tweaks_get_object (tweaks, page)) && IDE_IS_TWEAKS_ITEM (object))
        ide_tweaks_window_navigate_to (window, IDE_TWEAKS_ITEM (object));
    }

  /* Now we can display */
  gtk_window_present (GTK_WINDOW (window));
}
