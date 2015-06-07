/* gb-plugin.c
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

#include <gmodule.h>

#include "gb-plugins.h"

PeasPluginInfo **peas_register_types (void);

static GPtrArray *embedded_plugins;

void
gb_plugins_register (PeasPluginInfo *plugin_info)
{
  g_assert (plugin_info != NULL);

  g_print ("Registering plugin \"%s\"\n", peas_plugin_info_get_module_name (plugin_info));

  if (embedded_plugins == NULL)
    embedded_plugins = g_ptr_array_new ();

  g_ptr_array_add (embedded_plugins, plugin_info);
}

PeasPluginInfo **
peas_register_types (void)
{
  GPtrArray *copy;

  g_print ("peas_register_types called ...\n");

  copy = g_ptr_array_new ();

  if (embedded_plugins != NULL)
    {
      gsize i;

      for (i = 0; i < embedded_plugins->len; i++)
        g_ptr_array_add (copy, g_ptr_array_index (embedded_plugins, i));
    }

  g_ptr_array_add (copy, NULL);

  return (PeasPluginInfo **)g_ptr_array_free (copy, FALSE);
}

void
gb_plugins_load (void)
{
  PeasEngine *engine;
  GModule *module;
  gpointer symbol = NULL;
  gsize i;

  g_print ("Loading plugins...\n");

  module = g_module_open (NULL, G_MODULE_BIND_LAZY);
  if (g_module_symbol (module, "peas_register_types", &symbol))
    g_print ("Found\n");
  else
    g_print ("Not Found\n");

  g_print ("func at %p (%p)\n", symbol, (void *)peas_register_types);

  engine = peas_engine_get_default ();
  peas_engine_add_search_path (engine, "plugins", "plugins");
  peas_engine_rescan_plugins (engine);

  for (i = 0; i < embedded_plugins->len; i++)
    {
      PeasPluginInfo *info = g_ptr_array_index (embedded_plugins, i);
      g_print ("Loading %s\n", peas_plugin_info_get_name (info));
      peas_engine_load_plugin (engine, info);
    }
}
