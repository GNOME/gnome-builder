/* gb-preferences-page-plugins.c
 *
 * Copyright (C) 2015 Christian Hergert <christian@hergert.me>
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
#include <libpeas/peas.h>

#include "gb-preferences-page-plugins.h"

struct _GbPreferencesPagePlugins
{
  GbPreferencesPage parent_instance;

  GtkListBox *list_box;
};

G_DEFINE_TYPE (GbPreferencesPagePlugins, gb_preferences_page_plugins, GB_TYPE_PREFERENCES_PAGE)

static void
gb_preferences_page_plugins_add_plugin (GbPreferencesPagePlugins *self,
                                        PeasPluginInfo           *plugin_info)
{
  const gchar *description;
  const gchar *name;
  GtkListBoxRow *row;
  GtkLabel *label;
  GtkBox *box;

  g_assert (GB_IS_PREFERENCES_PAGE_PLUGINS (self));
  g_assert (plugin_info != NULL);

  name = peas_plugin_info_get_name (plugin_info);
  description = peas_plugin_info_get_description (plugin_info);

  if (ide_str_equal0 (name, "Fallback"))
    return;

  row = g_object_new (GTK_TYPE_LIST_BOX_ROW,
                      "visible", TRUE,
                      NULL);
  g_object_set_data (G_OBJECT (row), "PEAS_PLUGIN_INFO", plugin_info);
  gtk_container_add (GTK_CONTAINER (self->list_box), GTK_WIDGET (row));

  box = g_object_new (GTK_TYPE_BOX,
                      "margin", 6,
                      "orientation", GTK_ORIENTATION_VERTICAL,
                      "visible", TRUE,
                      NULL);
  gtk_container_add (GTK_CONTAINER (row), GTK_WIDGET (box));

  label = g_object_new (GTK_TYPE_LABEL,
                        "label", name,
                        "visible", TRUE,
                        "xalign", 0.0f,
                        NULL);
  gtk_container_add (GTK_CONTAINER (box), GTK_WIDGET (label));

  label = g_object_new (GTK_TYPE_LABEL,
                        "label", description,
                        "visible", TRUE,
                        "xalign", 0.0f,
                        "wrap", TRUE,
                        NULL);
  gtk_style_context_add_class (gtk_widget_get_style_context (GTK_WIDGET (label)), "dim-label");
  gtk_container_add (GTK_CONTAINER (box), GTK_WIDGET (label));
}

static void
gb_preferences_page_plugins_reload (GbPreferencesPagePlugins *self)
{
  PeasEngine *engine;
  const GList *plugins;
  const GList *iter;
  GList *children;

  g_assert (GB_IS_PREFERENCES_PAGE_PLUGINS (self));

  engine = peas_engine_get_default ();

  children = gtk_container_get_children (GTK_CONTAINER (self->list_box));
  for (iter = children; iter; iter = iter->next)
    gtk_container_remove (GTK_CONTAINER (self->list_box), iter->data);
  g_list_free (children);

  plugins = peas_engine_get_plugin_list (engine);
  for (iter = plugins; iter; iter = iter->next)
    gb_preferences_page_plugins_add_plugin (self, iter->data);
}

static gint
sort_rows_func (GtkListBoxRow *row1,
                GtkListBoxRow *row2,
                gpointer       user_data)
{
  PeasPluginInfo *pi1 = g_object_get_data (G_OBJECT (row1), "PEAS_PLUGIN_INFO");
  PeasPluginInfo *pi2 = g_object_get_data (G_OBJECT (row2), "PEAS_PLUGIN_INFO");
  const gchar *name1 = peas_plugin_info_get_name (pi1);
  const gchar *name2 = peas_plugin_info_get_name (pi2);

  return g_utf8_collate (name1, name2);
}

static void
gb_preferences_page_plugins_class_init (GbPreferencesPagePluginsClass *klass)
{
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  gtk_widget_class_set_template_from_resource (widget_class, "/org/gnome/builder/ui/gb-preferences-page-plugins.ui");

  gtk_widget_class_bind_template_child (widget_class, GbPreferencesPagePlugins, list_box);
}

static void
gb_preferences_page_plugins_init (GbPreferencesPagePlugins *self)
{
  gtk_widget_init_template (GTK_WIDGET (self));

  g_signal_connect_object (peas_engine_get_default (),
                           "notify::plugin-list",
                           G_CALLBACK (gb_preferences_page_plugins_reload),
                           self,
                           G_CONNECT_SWAPPED);

  gtk_list_box_set_sort_func (self->list_box, sort_rows_func, NULL, NULL);

  gb_preferences_page_plugins_reload (self);
}
