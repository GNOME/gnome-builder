/* ide-application-plugins.c
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

#define G_LOG_DOMAIN "ide-application"

#include <libpeas/peas.h>
#include <girepository.h>

#include "ide-application.h"
#include "ide-application-addin.h"
#include "ide-application-private.h"
#include "ide-css-provider.h"
#include "ide-macros.h"

static gboolean
ide_application_can_load_plugin (IdeApplication *self,
                                 PeasPluginInfo *plugin_info)
{
  g_assert (IDE_IS_APPLICATION (self));
  g_assert (plugin_info != NULL);

  if (self->mode == IDE_APPLICATION_MODE_WORKER)
    {
      if (self->worker != plugin_info)
        return FALSE;
    }

  if (self->mode == IDE_APPLICATION_MODE_TOOL)
    {
      /*
       * Plugins might provide critical features needed
       * to load a project (build system, vcs, etc).
       */
      return TRUE;
    }

  /*
   * TODO: Do ABI check on external data.
   */

  return TRUE;
}

void
ide_application_discover_plugins (IdeApplication *self)
{
  PeasEngine *engine = peas_engine_get_default ();
  const GList *list;
  gchar *path;

  g_return_if_fail (IDE_IS_APPLICATION (self));

  peas_engine_enable_loader (engine, "python3");

  if (g_getenv ("GB_IN_TREE_PLUGINS") != NULL)
    {
      GDir *dir;

      g_irepository_require_private (g_irepository_get_default (),
                                     BUILDDIR"/libide",
                                     "Ide", "1.0", 0, NULL);

      if ((dir = g_dir_open (BUILDDIR"/plugins", 0, NULL)))
        {
          const gchar *name;

          while ((name = g_dir_read_name (dir)))
            {
              path = g_build_filename (BUILDDIR, "plugins", name, NULL);
              peas_engine_prepend_search_path (engine, path, path);
              g_free (path);
            }

          g_dir_close (dir);
        }
    }
  else
    {
      g_irepository_prepend_search_path (PACKAGE_LIBDIR"/gnome-builder/girepository-1.0");

      peas_engine_prepend_search_path (engine,
                                       PACKAGE_LIBDIR"/gnome-builder/plugins",
                                       PACKAGE_DATADIR"/gnome-builder/plugins");
    }

  peas_engine_prepend_search_path (engine,
                                   "resource:///org/gnome/builder/plugins",
                                   "resource:///org/gnome/builder/plugins");

  path = g_build_filename (g_get_user_data_dir (), "gnome-builder", "plugins", NULL);
  peas_engine_prepend_search_path (engine, path, path);
  g_free (path);

  peas_engine_rescan_plugins (engine);

  list = peas_engine_get_plugin_list (engine);

  for (; list; list = list->next)
    {
      PeasPluginInfo *plugin_info = list->data;

      g_debug ("Discovered plugin \"%s\"",
               peas_plugin_info_get_module_name (plugin_info));
    }
}

static void
ide_application_plugins_enabled_changed (IdeApplication *self,
                                         const gchar    *key,
                                         GSettings      *settings)
{
  PeasPluginInfo *plugin_info;
  PeasEngine *engine;
  gboolean enabled;

  g_assert (IDE_IS_APPLICATION (self));
  g_assert (ide_str_equal0 (key, "enabled"));
  g_assert (G_IS_SETTINGS (settings));

  enabled = g_settings_get_boolean (settings, key);

  engine = peas_engine_get_default ();

  plugin_info = g_object_get_data (G_OBJECT (settings), "PEAS_PLUGIN_INFO");
  g_assert (plugin_info != NULL);

  if (enabled &&
      ide_application_can_load_plugin (self, plugin_info) &&
      !peas_plugin_info_is_loaded (plugin_info))
    peas_engine_load_plugin (engine, plugin_info);
  else if (!enabled && peas_plugin_info_is_loaded (plugin_info))
    peas_engine_unload_plugin (engine, plugin_info);
}

void
ide_application_load_plugins (IdeApplication *self)
{
  PeasEngine *engine;
  const GList *list;

  g_return_if_fail (IDE_IS_APPLICATION (self));

  engine = peas_engine_get_default ();
  list = peas_engine_get_plugin_list (engine);

  for (; list; list = list->next)
    {
      PeasPluginInfo *plugin_info = list->data;
      GSettings *settings;
      g_autofree gchar *path = NULL;
      const gchar *module_name;

      module_name = peas_plugin_info_get_module_name (plugin_info);
      path = g_strdup_printf ("/org/gnome/builder/plugins/%s/", module_name);
      settings = g_settings_new_with_path ("org.gnome.builder.plugin", path);

      g_object_set_data (G_OBJECT (settings), "PEAS_PLUGIN_INFO", plugin_info);

      g_signal_connect_object (settings,
                               "changed::enabled",
                               G_CALLBACK (ide_application_plugins_enabled_changed),
                               self,
                               G_CONNECT_SWAPPED);

      if (!g_settings_get_boolean (settings, "enabled"))
        continue;

      if (ide_application_can_load_plugin (self, plugin_info))
        {
          g_debug ("Loading plugin \"%s\"",
                   peas_plugin_info_get_module_name (plugin_info));
          peas_engine_load_plugin (engine, plugin_info);
        }
    }
}

static void
ide_application_addin_added (PeasExtensionSet *set,
                             PeasPluginInfo   *plugin_info,
                             PeasExtension    *extension,
                             gpointer          user_data)
{
  IdeApplication *self = user_data;

  g_assert (PEAS_IS_EXTENSION_SET (set));
  g_assert (plugin_info != NULL);
  g_assert (IDE_IS_APPLICATION_ADDIN (extension));

  ide_application_addin_load (IDE_APPLICATION_ADDIN (extension), self);
}

static void
ide_application_addin_removed (PeasExtensionSet *set,
                               PeasPluginInfo   *plugin_info,
                               PeasExtension    *extension,
                               gpointer          user_data)
{
  IdeApplication *self = user_data;

  g_assert (PEAS_IS_EXTENSION_SET (set));
  g_assert (plugin_info != NULL);
  g_assert (IDE_IS_APPLICATION_ADDIN (extension));

  ide_application_addin_unload (IDE_APPLICATION_ADDIN (extension), self);
}

void
ide_application_load_addins (IdeApplication *self)
{
  g_return_if_fail (IDE_IS_APPLICATION (self));

  self->addins = peas_extension_set_new (peas_engine_get_default (),
                                         IDE_TYPE_APPLICATION_ADDIN,
                                         NULL);

  g_signal_connect_object (self->addins,
                           "extension-added",
                           G_CALLBACK (ide_application_addin_added),
                           self,
                           0);

  g_signal_connect_object (self->addins,
                           "extension-removed",
                           G_CALLBACK (ide_application_addin_removed),
                           self,
                           0);

  peas_extension_set_foreach (self->addins,
                              ide_application_addin_added,
                              self);
}

static void
ide_application_load_plugin_menus (IdeApplication *self,
                                   PeasPluginInfo *plugin_info,
                                   PeasEngine     *engine)
{
  const gchar *module_name;
  gchar *path;
  guint merge_id;

  g_assert (IDE_IS_APPLICATION (self));
  g_assert (plugin_info != NULL);
  g_assert (PEAS_IS_ENGINE (engine));

  /*
   * First check embedded resource for menus.ui.
   */
  module_name = peas_plugin_info_get_module_name (plugin_info);
  path = g_strdup_printf ("/org/gnome/builder/plugins/%s/gtk/menus.ui", module_name);
  merge_id = egg_menu_manager_add_resource (self->menu_manager, path, NULL);
  if (merge_id != 0)
    g_hash_table_insert (self->merge_ids, g_strdup (module_name), GINT_TO_POINTER (merge_id));
  g_free (path);

  /*
   * Maybe this is python and embedded resources are annoying to build.
   * Could be a file on disk.
   */
  if (merge_id == 0)
    {
      path = g_strdup_printf ("%s/gtk/menus.ui", peas_plugin_info_get_data_dir (plugin_info));
      if (g_file_test (path, G_FILE_TEST_IS_REGULAR))
        {
          merge_id = egg_menu_manager_add_filename (self->menu_manager, path, NULL);
          if (merge_id != 0)
            g_hash_table_insert (self->merge_ids, g_strdup (module_name), GINT_TO_POINTER (merge_id));
        }
      g_free (path);
    }
}

static void
ide_application_unload_plugin_menus (IdeApplication *self,
                                     PeasPluginInfo *plugin_info,
                                     PeasEngine     *engine)
{
  const gchar *module_name;
  guint merge_id;

  g_assert (IDE_IS_APPLICATION (self));
  g_assert (plugin_info != NULL);
  g_assert (PEAS_IS_ENGINE (engine));

  module_name = peas_plugin_info_get_module_name (plugin_info);
  merge_id = GPOINTER_TO_INT (g_hash_table_lookup (self->merge_ids, module_name));
  if (merge_id != 0)
    egg_menu_manager_remove (self->menu_manager, merge_id);
  g_hash_table_remove (self->merge_ids, module_name);
}

static void
ide_application_load_plugin_css (IdeApplication *self,
                                 PeasPluginInfo *plugin_info,
                                 PeasEngine     *engine)
{
  g_autofree gchar *base_path = NULL;
  GtkCssProvider *provider;
  const gchar *module_name;
  GdkScreen *screen;

  g_assert (IDE_IS_APPLICATION (self));
  g_assert (plugin_info != NULL);
  g_assert (PEAS_IS_ENGINE (engine));

  if (self->plugin_css == NULL)
    self->plugin_css = g_hash_table_new_full (NULL, NULL, NULL, g_object_unref);

  module_name = peas_plugin_info_get_module_name (plugin_info);
  base_path = g_strdup_printf ("/org/gnome/builder/plugins/%s", module_name);
  provider = ide_css_provider_new (base_path);

  screen = gdk_screen_get_default ();
  gtk_style_context_add_provider_for_screen (screen,
                                             GTK_STYLE_PROVIDER (provider),
                                             GTK_STYLE_PROVIDER_PRIORITY_APPLICATION + 1);

  g_hash_table_insert (self->plugin_css, plugin_info, provider);
}

static void
ide_application_unload_plugin_css (IdeApplication *self,
                                   PeasPluginInfo *plugin_info,
                                   PeasEngine     *engine)
{
  GtkStyleProvider *provider;

  g_assert (IDE_IS_APPLICATION (self));
  g_assert (plugin_info != NULL);
  g_assert (PEAS_IS_ENGINE (engine));

  provider = g_hash_table_lookup (self->plugin_css, plugin_info);

  if (provider != NULL)
    {
      GdkScreen *screen = gdk_screen_get_default ();

      gtk_style_context_remove_provider_for_screen (screen, provider);
      g_hash_table_remove (self->plugin_css, plugin_info);
    }
}

void
ide_application_init_plugin_menus (IdeApplication *self)
{
  const GList *list;
  PeasEngine *engine;

  g_assert (IDE_IS_APPLICATION (self));

  self->merge_ids = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, NULL);

  engine = peas_engine_get_default ();

  g_signal_connect_object (engine,
                           "load-plugin",
                           G_CALLBACK (ide_application_load_plugin_menus),
                           self,
                           G_CONNECT_AFTER | G_CONNECT_SWAPPED);
  g_signal_connect_object (engine,
                           "load-plugin",
                           G_CALLBACK (ide_application_load_plugin_css),
                           self,
                           G_CONNECT_AFTER | G_CONNECT_SWAPPED);

  g_signal_connect_object (engine,
                           "unload-plugin",
                           G_CALLBACK (ide_application_unload_plugin_menus),
                           self,
                           G_CONNECT_SWAPPED);
  g_signal_connect_object (engine,
                           "unload-plugin",
                           G_CALLBACK (ide_application_unload_plugin_css),
                           self,
                           G_CONNECT_SWAPPED);

  list = peas_engine_get_plugin_list (engine);

  for (; list != NULL; list = list->next)
    ide_application_load_plugin_menus (self, list->data, engine);
}
