/* ide-extension-util.c
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

#define G_LOG_DOMAIN "ide-extension-util"

#include <stdlib.h>

#include "ide-extension-util.h"

gboolean
ide_extension_util_can_use_plugin (PeasEngine     *engine,
                                   PeasPluginInfo *plugin_info,
                                   GType           interface_type,
                                   const gchar    *key,
                                   const gchar    *value,
                                   gint           *priority)
{
  g_autofree gchar *path = NULL;
  g_autoptr(GSettings) settings = NULL;

  g_return_val_if_fail (plugin_info != NULL, FALSE);
  g_return_val_if_fail (g_type_is_a (interface_type, G_TYPE_INTERFACE), FALSE);
  g_return_val_if_fail (priority != NULL, FALSE);

  *priority = 0;

  /*
   * If we are restricting by plugin info keyword, ensure we have enough
   * information to do so.
   */
  if ((key != NULL) && (value == NULL))
    return FALSE;

  /*
   * If the plugin isn't loaded, then we shouldn't use it.
   */
  if (!peas_plugin_info_is_loaded (plugin_info))
    return FALSE;

  /*
   * If this plugin doesn't provide this type, we can't use it either.
   */
  if (!peas_engine_provides_extension (engine, plugin_info, interface_type))
    return FALSE;

  /*
   * Check that the plugin provides the match value we are looking for.
   * If key is NULL, then we aren't restricting by matching.
   */
  if (key != NULL)
    {
      g_autofree gchar *priority_name = NULL;
      g_auto(GStrv) values_array = NULL;
      const gchar *values;
      const gchar *priority_value;

      values = peas_plugin_info_get_external_data (plugin_info, key);
      values_array = g_strsplit (values ? values : "", ",", 0);
      if (!g_strv_contains ((const gchar * const *)values_array, value))
        return FALSE;

      priority_name = g_strdup_printf ("%s-Priority", key);
      priority_value = peas_plugin_info_get_external_data (plugin_info, priority_name);
      if (priority_value != NULL)
        *priority = atoi (priority_value);
    }

  /*
   * Ensure the plugin type isn't disabled by checking our GSettings
   * for the plugin type. There is an implicit plugin issue here, in that
   * two modules using different plugin loaders could have the same module
   * name. But we can enforce this issue socially.
   */
  path = g_strdup_printf ("/org/gnome/builder/extension-types/%s/%s/",
                          peas_plugin_info_get_module_name (plugin_info),
                          g_type_name (interface_type));
  settings = g_settings_new_with_path ("org.gnome.builder.extension-type", path);
  if (g_settings_get_boolean (settings, "disabled"))
    return FALSE;

  return TRUE;
}
