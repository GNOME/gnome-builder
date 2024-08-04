/* ide-application-plugins.c
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

#define G_LOG_DOMAIN "ide-application-plugins"

#include "config.h"

#include <libide-plugins.h>

#include "ide-application.h"
#include "ide-application-addin.h"
#include "ide-application-private.h"

static void
ide_application_changed_plugin_cb (GSettings      *settings,
                                   const gchar    *key,
                                   PeasPluginInfo *plugin_info)
{
  PeasEngine *engine;

  IDE_ENTRY;

  g_assert (G_IS_SETTINGS (settings));
  g_assert (key != NULL);
  g_assert (plugin_info != NULL);

  engine = peas_engine_get_default ();

  if (!g_settings_get_boolean (settings, key))
    peas_engine_unload_plugin (engine, plugin_info);
  else
    peas_engine_load_plugin (engine, plugin_info);

  IDE_EXIT;
}

static GSettings *
_ide_application_plugin_get_settings (IdeApplication *self,
                                      PeasPluginInfo *plugin_info)
{
  GSettings *settings;
  const gchar *module_name;

  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (IDE_IS_APPLICATION (self));
  g_assert (plugin_info != NULL);

  module_name = peas_plugin_info_get_module_name (plugin_info);

  if G_UNLIKELY (self->plugin_settings == NULL)
    self->plugin_settings =
      g_hash_table_new_full (g_str_hash, g_str_equal, g_free, g_object_unref);

  if (!(settings = g_hash_table_lookup (self->plugin_settings, module_name)))
    {
      g_autofree gchar *path = NULL;

      path = g_strdup_printf ("/org/gnome/builder/plugins/%s/", module_name);
      settings = g_settings_new_with_path ("org.gnome.builder.plugin", path);
      g_hash_table_insert (self->plugin_settings, g_strdup (module_name), settings);

      g_signal_connect (settings,
                        "changed::enabled",
                        G_CALLBACK (ide_application_changed_plugin_cb),
                        plugin_info);
    }

  return settings;
}

static gboolean
ide_application_can_load_plugin (IdeApplication *self,
                                 PeasPluginInfo *plugin_info,
                                 GHashTable     *circular)
{
  PeasEngine *engine = peas_engine_get_default ();
  const char *module_name;
  const char *module_dir;
  const char * const *deps;
  GSettings *settings;

  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (IDE_IS_APPLICATION (self));
  g_assert (circular != NULL);

  if (plugin_info == NULL)
    return FALSE;

  module_dir = peas_plugin_info_get_module_dir (plugin_info);
  module_name = peas_plugin_info_get_module_name (plugin_info);

  if (g_hash_table_contains (circular, module_name))
    {
      g_warning ("Circular dependency found in module %s", module_name);
      return FALSE;
    }

  g_hash_table_add (circular, (gpointer)module_name);

  /* Make sure the plugin has not been disabled in settings. */
  settings = _ide_application_plugin_get_settings (self, plugin_info);
  if (!g_settings_get_boolean (settings, "enabled"))
    return FALSE;

  /*
   * If the plugin is not bundled within the Builder executable, then we
   * require that an X-Builder-ABI=major style extended data be
   * provided to ensure we have proper ABI.
   *
   * You could get around this by loading a plugin that then loads resouces
   * containing external data, but this is good enough for now.
   */

  if (!g_str_has_prefix (module_dir, "resource:///plugins/"))
    {
      const gchar *abi;

      if (!(abi = peas_plugin_info_get_external_data (plugin_info, "Builder-ABI")))
        {
          g_critical ("Refusing to load plugin %s because X-Builder-ABI is missing",
                      module_name);
          return FALSE;
        }

      if (g_strcmp0 (PACKAGE_ABI_S, abi) != 0)
        {
          g_critical ("Refusing to load plugin %s, expected ABI %d and got %s",
                      module_name, IDE_MAJOR_VERSION, abi);
          return FALSE;
        }
    }

  /*
   * If this plugin has dependencies, we need to check that the dependencies
   * can also be loaded.
   */
  if ((deps = peas_plugin_info_get_dependencies (plugin_info)))
    {
      for (guint i = 0; deps[i]; i++)
        {
          PeasPluginInfo *dep = peas_engine_get_plugin_info (engine, deps[i]);

          if (!ide_application_can_load_plugin (self, dep, circular))
            return FALSE;
        }
    }

  g_hash_table_remove (circular, (gpointer)module_name);

  return TRUE;
}

static void
ide_application_load_plugin_resources (IdeApplication *self,
                                       PeasEngine     *engine,
                                       PeasPluginInfo *plugin_info)
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
      g_autofree gchar *resource_path = NULL;
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

      resource_path = g_strdup_printf ("resource:///plugins/%s", module_name);
      _ide_application_add_resources (self, resource_path);
    }
}

void
_ide_application_load_plugin (IdeApplication *self,
                              PeasPluginInfo *plugin_info)
{
  PeasEngine *engine = peas_engine_get_default ();
  g_autoptr(GHashTable) circular = NULL;

  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (IDE_IS_APPLICATION (self));
  g_assert (plugin_info != NULL);

  circular = g_hash_table_new (g_str_hash, g_str_equal);

  if (ide_application_can_load_plugin (self, plugin_info, circular))
    peas_engine_load_plugin (engine, plugin_info);
}

static void
ide_application_plugins_load_plugin_after_cb (IdeApplication *self,
                                              PeasPluginInfo *plugin_info,
                                              PeasEngine     *engine)
{
  const gchar *data_dir;
  const gchar *module_dir;
  const gchar *module_name;

  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (IDE_IS_APPLICATION (self));
  g_assert (plugin_info != NULL);
  g_assert (PEAS_IS_ENGINE (engine));

  data_dir = peas_plugin_info_get_data_dir (plugin_info);
  module_dir = peas_plugin_info_get_module_dir (plugin_info);
  module_name = peas_plugin_info_get_module_name (plugin_info);

  g_debug ("Loaded plugin \"%s\" with module-dir \"%s\"",
           module_name, module_dir);

  if (peas_plugin_info_get_external_data (plugin_info, "Has-Resources"))
    {
      /* Possibly load bundled .gresource files if the plugin is not
       * embedded into the application (such as python3 modules).
       */
      ide_application_load_plugin_resources (self, engine, plugin_info);
    }

  /*
   * Only register resources if the path is to an embedded resource
   * or if it's not builtin (and therefore maybe doesn't use .gresource
   * files). That helps reduce the number IOPS we do.
   */
  if (g_str_has_prefix (data_dir, "resource://") ||
      !peas_plugin_info_is_builtin (plugin_info))
    _ide_application_add_resources (self, data_dir);
}

static void
ide_application_plugins_unload_plugin_after_cb (IdeApplication *self,
                                                PeasPluginInfo *plugin_info,
                                                PeasEngine     *engine)
{
  const gchar *module_dir;
  const gchar *module_name;

  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (IDE_IS_APPLICATION (self));
  g_assert (plugin_info != NULL);
  g_assert (PEAS_IS_ENGINE (engine));

  module_dir = peas_plugin_info_get_module_dir (plugin_info);
  module_name = peas_plugin_info_get_module_name (plugin_info);

  _ide_application_remove_resources (self, module_dir);

  g_debug ("Unloaded plugin \"%s\" with module-dir \"%s\"",
           module_name, module_dir);
}

/**
 * _ide_application_load_plugins_for_startup:
 *
 * This function will load all of the plugins that are candidates for
 * early-stage initialization. Usually, that is any plugin that has a
 * command-line handler and uses "X-At-Startup=true" in their .plugin
 * manifest.
 */
void
_ide_application_load_plugins_for_startup (IdeApplication *self)
{
  PeasEngine *engine;
  guint n_items;

  IDE_ENTRY;

  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (IDE_IS_APPLICATION (self));

  engine = peas_engine_get_default ();

  g_signal_connect_object (engine,
                           "load-plugin",
                           G_CALLBACK (ide_application_plugins_load_plugin_after_cb),
                           self,
                           G_CONNECT_SWAPPED | G_CONNECT_AFTER);

  g_signal_connect_object (engine,
                           "unload-plugin",
                           G_CALLBACK (ide_application_plugins_unload_plugin_after_cb),
                           self,
                           G_CONNECT_SWAPPED | G_CONNECT_AFTER);

  /* Ensure that our embedded plugins are allowed early access to
   * start loading (before we ever look at anything on disk). This
   * ensures that only embedded plugins can be used at startup,
   * saving us some precious disk I/O.
   */
  peas_engine_add_search_path (engine,
                               "resource:///plugins",
                               "resource:///plugins");

  /* If we are within the Flatpak, then load any extensions we've
   * found merged into the extensions directory.
   */
  if (ide_is_flatpak ())
    peas_engine_add_search_path (engine,
                                 "/app/extensions/lib/gnome-builder/plugins",
                                 "/app/extensions/lib/gnome-builder/plugins");

  /* Ensure we've rescanned to take the plugins into account */
  peas_engine_rescan_plugins (engine);

  /* Our first step is to load our "At-Startup" plugins, which may
   * contain things like command-line handlers. For example, the
   * greeter may handle command-line options and then show the
   * greeter workspace.
   */
  n_items = g_list_model_get_n_items (G_LIST_MODEL (engine));
  for (guint i = 0; i < n_items; i++)
    {
      g_autoptr(PeasPluginInfo) plugin_info = g_list_model_get_item (G_LIST_MODEL (engine), i);

      if (!peas_plugin_info_is_loaded (plugin_info) &&
          peas_plugin_info_get_external_data (plugin_info, "At-Startup"))
        _ide_application_load_plugin (self, plugin_info);
    }

  IDE_EXIT;
}

/**
 * _ide_application_load_plugins:
 * @self: a #IdeApplication
 *
 * This function loads any additional plugins that have not yet been
 * loaded during early startup.
 */
void
_ide_application_load_plugins (IdeApplication *self)
{
  g_autofree gchar *user_plugins_dir = NULL;
  PeasEngine *engine;
  guint n_items;

  g_assert (IDE_IS_APPLICATION (self));

  engine = peas_engine_get_default ();

  /* Now that we have gotten past our startup plugins (which must be
   * embedded into the gnome-builder executable, we can enable the
   * system plugins that are loaded from disk.
   */
  peas_engine_add_search_path (engine,
                               PACKAGE_LIBDIR"/gnome-builder/plugins",
                               PACKAGE_DATADIR"/plugins");

  if (ide_is_flatpak ())
    {
      g_autofree gchar *extensions_plugins_dir = NULL;
      g_autofree gchar *plugins_dir = NULL;

      plugins_dir = g_build_filename (g_get_home_dir (),
                                      ".local",
                                      "share",
                                      "gnome-builder",
                                      "plugins",
                                      NULL);
      peas_engine_add_search_path (engine, plugins_dir, plugins_dir);

      extensions_plugins_dir = g_build_filename ("/app",
                                                 "extensions",
                                                 "lib",
                                                 "gnome-builder",
                                                 "plugins",
                                                 NULL);
      peas_engine_add_search_path (engine, extensions_plugins_dir, extensions_plugins_dir);
    }

  user_plugins_dir = g_build_filename (g_get_user_data_dir (),
                                       "gnome-builder",
                                       "plugins",
                                       NULL);
  peas_engine_add_search_path (engine, user_plugins_dir, NULL);

  if (self->loaded_typelibs)
    peas_engine_enable_loader (engine, "gjs");

  peas_engine_rescan_plugins (engine);

  n_items = g_list_model_get_n_items (G_LIST_MODEL (engine));

  for (guint i = 0; i < n_items; i++)
    {
      g_autoptr(PeasPluginInfo) plugin_info = g_list_model_get_item (G_LIST_MODEL (engine), i);

      if (!peas_plugin_info_is_loaded (plugin_info))
        _ide_application_load_plugin (self, plugin_info);
    }
}

static void
ide_application_addin_added_cb (PeasExtensionSet *set,
                                PeasPluginInfo   *plugin_info,
                                GObject    *exten,
                                gpointer          user_data)
{
  IdeApplicationAddin *addin = (IdeApplicationAddin *)exten;
  IdeApplication *self = user_data;

  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (PEAS_IS_EXTENSION_SET (set));
  g_assert (plugin_info != NULL);
  g_assert (IDE_IS_APPLICATION_ADDIN (addin));
  g_assert (IDE_IS_APPLICATION (self));

  ide_application_addin_load (addin, self);
}

static void
ide_application_addin_removed_cb (PeasExtensionSet *set,
                                  PeasPluginInfo   *plugin_info,
                                  GObject    *exten,
                                  gpointer          user_data)
{
  IdeApplicationAddin *addin = (IdeApplicationAddin *)exten;
  IdeApplication *self = user_data;

  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (PEAS_IS_EXTENSION_SET (set));
  g_assert (plugin_info != NULL);
  g_assert (IDE_IS_APPLICATION_ADDIN (addin));
  g_assert (IDE_IS_APPLICATION (self));

  ide_application_addin_unload (addin, self);
}

/**
 * _ide_application_load_addins:
 * @self: a #IdeApplication
 *
 * Loads the #IdeApplicationAddin's for this application.
 */
void
_ide_application_load_addins (IdeApplication *self)
{
  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (IDE_IS_APPLICATION (self));
  g_assert (self->addins == NULL);

  self->addins = peas_extension_set_new (peas_engine_get_default (),
                                         IDE_TYPE_APPLICATION_ADDIN,
                                         NULL);

  g_signal_connect (self->addins,
                    "extension-added",
                    G_CALLBACK (ide_application_addin_added_cb),
                    self);

  g_signal_connect (self->addins,
                    "extension-removed",
                    G_CALLBACK (ide_application_addin_removed_cb),
                    self);

  peas_extension_set_foreach (self->addins,
                              ide_application_addin_added_cb,
                              self);
}

/**
 * _ide_application_unload_addins:
 * @self: a #IdeApplication
 *
 * Unloads all of the previously loaded #IdeApplicationAddin.
 */
void
_ide_application_unload_addins (IdeApplication *self)
{
  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (IDE_IS_APPLICATION (self));
  g_assert (self->addins != NULL);

  g_clear_object (&self->addins);
}
