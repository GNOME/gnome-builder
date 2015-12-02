/* ide-menu-merger.c
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
#include <libpeas/peas.h>

#include "ide-application.h"
#include "ide-debug.h"
#include "ide-macros.h"
#include "ide-menu-extension.h"
#include "ide-menu-merger.h"

struct _IdeMenuMerger
{
  GObject     parent_instance;

  /*
   * A GHashTable containing GPtrArray of IdeMenuExtension.
   * The resource path is the key to the array of menu extensions.
   */
  GHashTable *by_resource_path;
};

G_DEFINE_TYPE (IdeMenuMerger, ide_menu_merger, G_TYPE_OBJECT)

const gchar *
get_object_id (GObject *object)
{
  if (GTK_IS_BUILDABLE (object))
    return gtk_buildable_get_name (GTK_BUILDABLE (object));
  else
    return g_object_get_data (object, "gtk-builder-name");
}

static gint
find_position (GMenu       *menu,
               const gchar *after)
{
  if (after != NULL)
    {
      gint n_items = g_menu_model_get_n_items (G_MENU_MODEL (menu));
      gint i;

      for (i = 0; i < n_items; i++)
        {
          g_autoptr(GMenuLinkIter) iter = NULL;
          g_autoptr(GMenuModel) value = NULL;
          const gchar *name;

          iter = g_menu_model_iterate_item_links (G_MENU_MODEL (menu), i);

          while (g_menu_link_iter_get_next (iter, &name, &value))
            {
              const gchar *id = get_object_id (G_OBJECT (value));

              if (ide_str_equal0 (id, after))
                return i + 1;
            }
        }
    }

  return -1;
}

static void
save_extension (IdeMenuMerger    *self,
                const gchar      *resource_path,
                IdeMenuExtension *extension)
{
  GPtrArray *ar;

  g_assert (IDE_IS_MENU_MERGER (self));
  g_assert (resource_path != NULL);
  g_assert (IDE_IS_MENU_EXTENSION (extension));

  ar = g_hash_table_lookup (self->by_resource_path, resource_path);

  if (ar == NULL)
    {
      ar = g_ptr_array_new_with_free_func (g_object_unref);
      g_hash_table_insert (self->by_resource_path, g_strdup (resource_path), ar);
    }

  g_ptr_array_add (ar, g_object_ref (extension));
}

static void
ide_menu_merger_merge (IdeMenuMerger *self,
                       const gchar   *resource_path,
                       GMenu         *app_menu,
                       GMenu         *menu)
{
  gint n_items;
  gint i;

  g_assert (IDE_IS_MENU_MERGER (self));
  g_assert (resource_path != NULL);
  g_assert (G_IS_MENU (app_menu));
  g_assert (G_IS_MENU (menu));

  n_items = g_menu_model_get_n_items (G_MENU_MODEL (menu));

  for (i = 0; i < n_items; i++)
    {
      g_autoptr(GMenuLinkIter) iter = NULL;
      g_autoptr(GMenuModel) value = NULL;
      const gchar *name;

      iter = g_menu_model_iterate_item_links (G_MENU_MODEL (menu), i);

      while (g_menu_link_iter_get_next (iter, &name, &value))
        {
          g_autofree gchar *after = NULL;
          g_autoptr(GMenuItem) item = NULL;
          g_autoptr(IdeMenuExtension) extension = NULL;
          gint position;

          if (!ide_str_equal0 (name, "section"))
            continue;

          if (!g_menu_model_get_item_attribute (G_MENU_MODEL (menu), i, "after", "s", &after))
            after = NULL;

          extension = ide_menu_extension_new (app_menu);

          position = find_position (app_menu, after);
          item = g_menu_item_new_section (NULL, value);
          ide_menu_extension_insert_menu_item (extension, position, item);

          save_extension (self, resource_path, extension);
        }
    }
}

static void
ide_menu_merger_load_resource (IdeMenuMerger *self,
                               const gchar   *resource_path)
{
  GtkApplication *app = GTK_APPLICATION (IDE_APPLICATION_DEFAULT);
  GtkBuilder *builder;
  const GSList *iter;
  GSList *list;

  g_assert (IDE_IS_MENU_MERGER (self));
  g_assert (resource_path != NULL);

  if (!g_resources_get_info (resource_path, 0, NULL, NULL, NULL))
    return;

  builder = gtk_builder_new_from_resource (resource_path);
  list = gtk_builder_get_objects (builder);

  for (iter = list; iter; iter = iter->next)
    {
      GObject *object = iter->data;
      const gchar *id = get_object_id (object);
      GMenu *app_menu;

      if (id == NULL || !G_IS_MENU (object))
        continue;

      app_menu = gtk_application_get_menu_by_id (app, id);
      if (app_menu == NULL)
        continue;

      ide_menu_merger_merge (self, resource_path, app_menu, G_MENU (object));
    }

  g_slist_free (list);
  g_object_unref (builder);
}

static void
ide_menu_merger_load_plugin (IdeMenuMerger  *self,
                             PeasPluginInfo *plugin_info,
                             PeasEngine     *engine)
{
  const gchar *module_name;
  gchar *path;

  g_assert (IDE_IS_MENU_MERGER (self));
  g_assert (plugin_info != NULL);
  g_assert (PEAS_IS_ENGINE (engine));

  module_name = peas_plugin_info_get_module_name (plugin_info);

  path = g_strdup_printf ("/org/gnome/builder/plugins/%s/gtk/menus.ui", module_name);
  ide_menu_merger_load_resource (self, path);
  g_free (path);
}

static void
ide_menu_merger_unload_plugin (IdeMenuMerger  *self,
                               PeasPluginInfo *plugin_info,
                               PeasEngine     *engine)
{
  const gchar *module_name;
  gchar *path;

  g_assert (IDE_IS_MENU_MERGER (self));
  g_assert (plugin_info != NULL);
  g_assert (PEAS_IS_ENGINE (engine));

  module_name = peas_plugin_info_get_module_name (plugin_info);

  path = g_strdup_printf ("/org/gnome/builder/plugins/%s/gtk/menus.ui", module_name);
  g_hash_table_remove (self->by_resource_path, path);
  g_free (path);
}

static void
ide_menu_merger_finalize (GObject *object)
{
  G_OBJECT_CLASS (ide_menu_merger_parent_class)->finalize (object);
}

static void
ide_menu_merger_class_init (IdeMenuMergerClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = ide_menu_merger_finalize;
}

static void
ide_menu_merger_init (IdeMenuMerger *self)
{
  PeasEngine *engine = peas_engine_get_default ();
  const GList *list;

  self->by_resource_path = g_hash_table_new_full (g_str_hash, g_str_equal,
                                                  g_free, (GDestroyNotify)g_ptr_array_unref);

  g_signal_connect_object (engine,
                           "load-plugin",
                           G_CALLBACK (ide_menu_merger_load_plugin),
                           self,
                           G_CONNECT_AFTER | G_CONNECT_SWAPPED);

  g_signal_connect_object (engine,
                           "unload-plugin",
                           G_CALLBACK (ide_menu_merger_unload_plugin),
                           self,
                           G_CONNECT_SWAPPED);

  list = peas_engine_get_plugin_list (engine);

  for (; list != NULL; list = list->next)
    {
      PeasPluginInfo *plugin_info = list->data;

      ide_menu_merger_load_plugin (self, plugin_info, engine);
    }
}

IdeMenuMerger *
ide_menu_merger_new (void)
{
  return g_object_new (IDE_TYPE_MENU_MERGER, NULL);
}
