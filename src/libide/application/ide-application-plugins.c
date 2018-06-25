/* ide-application-plugins.c
 *
 * Copyright 2015 Christian Hergert <christian@hergert.me>
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

#include "config.h"

#include <libpeas/peas.h>
#include <girepository.h>

#include "application/ide-application.h"
#include "application/ide-application-addin.h"
#include "application/ide-application-private.h"
#include "util/ide-flatpak.h"

static GSettings *_ide_application_plugin_get_settings (IdeApplication *self,
                                                        const gchar    *module_name);

static const gchar *blacklisted_plugins[] = {
  "build-tools-plugin", /* Renamed to buildui */
};

static gboolean
ide_application_can_load_plugin (IdeApplication *self,
                                 PeasPluginInfo *plugin_info)
{
  const gchar *module_name;
  const gchar **deps;

  g_assert (IDE_IS_APPLICATION (self));
  g_assert (plugin_info != NULL);

  module_name = peas_plugin_info_get_module_name (plugin_info);

  for (guint i = 0; i < G_N_ELEMENTS (blacklisted_plugins); i++)
    {
      if (g_strcmp0 (module_name, blacklisted_plugins[i]) == 0)
        return FALSE;
    }

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
   *
   * Right now, we don't have any way to check that the plugin is implementing
   * the same version of the API/ABI that the application exports. There are a
   * couple ways we could go about doing this.
   *
   * One approach might be to generate UUIDs for each plugin structure,
   * and update it every time the structure changes. However, plenty of changes
   * can be safe for existing modules. So perhaps we need something that has
   * a revision since last break. Then plugins would specify which version
   * and revision of an interface they require.
   *
   * Imagine the scenario that FooIface added the method frobnicate(). Previous
   * extensions for FooIface are perfectly happy to keep on working, but a new
   * addin that requires FooIface may require frobnicate()'s existance. So while
   * the ABI hasn't broken, some plugins will require a newer revision.
   *
   * This is not entirely different from libtool's interface age. Presumably,
   * Gedit's IAge is similar here, but we would need it per-structure.
   */

  /*
   * If this plugin has dependencies, we need to check that the dependencies
   * can also be loaded.
   */
  if ((deps = peas_plugin_info_get_dependencies (plugin_info)))
    {
      for (guint i = 0; deps[i]; i++)
        {
          GSettings *settings = _ide_application_plugin_get_settings (self, deps[i]);

          if (!g_settings_get_boolean (settings, "enabled"))
            return FALSE;
        }
    }

  return TRUE;
}

void
ide_application_discover_plugins (IdeApplication *self)
{
  PeasEngine *engine = peas_engine_get_default ();
  g_autofree gchar *path = NULL;
  g_autoptr(GError) error = NULL;
  const GList *list;

  g_return_if_fail (IDE_IS_APPLICATION (self));

  /*
   * We have access to ~/.local/share/gnome-builder/ for plugins even when we are
   * bundled with flatpak, so might as well use it.
   */
  if (ide_is_flatpak ())
    {
      g_autofree gchar *plugins_dir = g_build_filename (g_get_home_dir (),
                                                        ".local",
                                                        "share",
                                                        "gnome-builder",
                                                        "plugins",
                                                        NULL);
      g_irepository_prepend_search_path (plugins_dir);
      peas_engine_prepend_search_path (engine, plugins_dir, plugins_dir);
    }

  path = g_build_filename (g_get_user_data_dir (), "gnome-builder", "plugins", NULL);
  peas_engine_prepend_search_path (engine, path, NULL);

  g_irepository_prepend_search_path (PACKAGE_LIBDIR"/gnome-builder/girepository-1.0");
  peas_engine_prepend_search_path (engine,
                                   PACKAGE_LIBDIR"/gnome-builder/plugins",
                                   PACKAGE_DATADIR"/gnome-builder/plugins");

  peas_engine_prepend_search_path (engine, "resource:///org/gnome/builder/plugins/", NULL);

  if (!g_irepository_require (NULL, "GtkSource", "4", 0, &error) ||
      !g_irepository_require (NULL, "Gtk", "3.0", 0, &error) ||
      !g_irepository_require (NULL, "Dazzle", "1.0", 0, &error) ||
      !g_irepository_require (NULL, "Ide", "1.0", 0, &error))
    g_message ("Cannot enable Python 3 plugins: %s", error->message);
  else
    {
      /* Avoid spamming stderr with Ide import tracebacks */
      peas_engine_enable_loader (engine, "python3");
    }

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
  g_assert (dzl_str_equal0 (key, "enabled"));
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

static GSettings *
_ide_application_plugin_get_settings (IdeApplication *self,
                                      const gchar    *module_name)
{
  GSettings *settings;

  if (G_UNLIKELY(self->plugin_settings == NULL))
    {
      self->plugin_settings = g_hash_table_new_full (g_str_hash, g_str_equal,
                                                     g_free, g_object_unref);
    }

  if (!(settings = g_hash_table_lookup (self->plugin_settings, module_name)))
    {
      g_autofree gchar *path = NULL;

      path = g_strdup_printf ("/org/gnome/builder/plugins/%s/", module_name);
      settings = g_settings_new_with_path ("org.gnome.builder.plugin", path);
      g_hash_table_insert (self->plugin_settings, g_strdup (module_name), settings);
    }

  return settings;
}

static void
ide_application_plugins_load_plugin_gresources (IdeApplication *self,
                                                PeasPluginInfo *plugin_info,
                                                PeasEngine     *engine)
{
  g_autofree gchar *gresources_path = NULL;
  g_autofree gchar *gresources_basename = NULL;
  const gchar *module_dir;
  const gchar *module_name;

  g_assert (IDE_IS_APPLICATION (self));
  g_assert (plugin_info != NULL);
  g_assert (PEAS_IS_ENGINE (engine));

  module_dir = peas_plugin_info_get_module_dir (plugin_info);
  module_name = peas_plugin_info_get_module_name (plugin_info);
  gresources_basename = g_strdup_printf ("%s.gresource", module_name);
  gresources_path = g_build_filename (module_dir, gresources_basename, NULL);

  if (g_file_test (gresources_path, G_FILE_TEST_IS_REGULAR))
    {
      g_autoptr(GError) error = NULL;
      GResource *resource;

      resource = g_resource_load (gresources_path, &error);

      if (resource == NULL)
        {
          g_warning ("Failed to load gresources: %s", error->message);
          return;
        }

      g_hash_table_insert (self->plugin_gresources, g_strdup (module_name), resource);
      g_resources_register (resource);
    }
}

static void
ide_application_plugins_unload_plugin_gresources (IdeApplication *self,
                                                  PeasPluginInfo *plugin_info,
                                                  PeasEngine     *engine)
{
  const gchar *module_name;
  GResource *resources;

  g_assert (IDE_IS_APPLICATION (self));
  g_assert (plugin_info != NULL);
  g_assert (PEAS_IS_ENGINE (engine));

  module_name = peas_plugin_info_get_module_name (plugin_info);
  resources = g_hash_table_lookup (self->plugin_gresources, module_name);

  if (resources != NULL)
    {
      g_resources_unregister (resources);
      g_hash_table_remove (self->plugin_gresources, module_name);
    }
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
      const gchar *module_name;

      module_name = peas_plugin_info_get_module_name (plugin_info);
      settings = _ide_application_plugin_get_settings (self, module_name);

      g_object_set_data (G_OBJECT (settings), "PEAS_PLUGIN_INFO", plugin_info);

      g_signal_connect_object (settings,
                               "changed::enabled",
                               G_CALLBACK (ide_application_plugins_enabled_changed),
                               self,
                               G_CONNECT_SWAPPED);

      if (!g_settings_get_boolean (settings, "enabled"))
        continue;

      /*
       * If we are running the unit tests, we don't want to load plugins here,
       * but defer until the test is loading to perform the loading.  However,
       * we do want all of the other machinery above to be setup.
       */
      if (self->mode == IDE_APPLICATION_MODE_TESTS)
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
ide_application_load_plugin_resources (IdeApplication *self,
                                       PeasPluginInfo *plugin_info,
                                       PeasEngine     *engine)
{
  g_autofree gchar *path = NULL;
  const gchar *data_dir;
  const gchar *module_name;

  g_assert (IDE_IS_APPLICATION (self));
  g_assert (plugin_info != NULL);
  g_assert (PEAS_IS_ENGINE (engine));

  ide_application_plugins_load_plugin_gresources (self, plugin_info, engine);

  module_name = peas_plugin_info_get_module_name (plugin_info);
  data_dir = peas_plugin_info_get_data_dir (plugin_info);

  /* Add embedded resources path */
  path = g_strdup_printf ("resource:///org/gnome/builder/plugins/%s/", module_name);
  dzl_application_add_resources (DZL_APPLICATION (self), path);

  /* If the data dir is not also a resource, add it */
  if (!g_str_has_prefix (data_dir, "resource://"))
    dzl_application_add_resources (DZL_APPLICATION (self), data_dir);
}

static void
ide_application_unload_plugin_resources (IdeApplication *self,
                                         PeasPluginInfo *plugin_info,
                                         PeasEngine     *engine)
{
  g_autofree gchar *path = NULL;
  const gchar *data_dir;
  const gchar *module_name;

  g_assert (IDE_IS_APPLICATION (self));
  g_assert (plugin_info != NULL);
  g_assert (PEAS_IS_ENGINE (engine));

  module_name = peas_plugin_info_get_module_name (plugin_info);
  data_dir = peas_plugin_info_get_data_dir (plugin_info);

  /* Remove embedded gresources */
  path = g_strdup_printf ("resource:///org/gnome/builder/plugins/%s/", module_name);
  dzl_application_remove_resources (DZL_APPLICATION (self), path);

  /* Remove on disk resources */
  if (!g_str_has_prefix (data_dir, "resource://"))
    dzl_application_remove_resources (DZL_APPLICATION (self), data_dir);

  ide_application_plugins_unload_plugin_gresources (self, plugin_info, engine);
}

void
ide_application_init_plugin_accessories (IdeApplication *self)
{
  const GList *list;
  PeasEngine *engine;

  g_assert (IDE_IS_APPLICATION (self));

  self->plugin_gresources = g_hash_table_new_full (g_str_hash, g_str_equal, g_free,
                                                   (GDestroyNotify)g_resource_unref);

  engine = peas_engine_get_default ();

  g_signal_connect_object (engine,
                           "load-plugin",
                           G_CALLBACK (ide_application_load_plugin_resources),
                           self,
                           G_CONNECT_AFTER | G_CONNECT_SWAPPED);

  g_signal_connect_object (engine,
                           "unload-plugin",
                           G_CALLBACK (ide_application_unload_plugin_resources),
                           self,
                           G_CONNECT_SWAPPED);

  list = peas_engine_get_plugin_list (engine);

  for (; list != NULL; list = list->next)
    {
      PeasPluginInfo *plugin_info = list->data;
      const gchar *module_name;
      GSettings *settings;

      module_name = peas_plugin_info_get_module_name (plugin_info);
      settings = _ide_application_plugin_get_settings (self, module_name);
      if (!g_settings_get_boolean (settings, "enabled"))
        continue;

      ide_application_load_plugin_resources (self, plugin_info, engine);
    }
}
