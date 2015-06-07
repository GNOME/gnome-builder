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
gb_plugins_register (GbPluginRegisterFunc callback)
{
  g_assert (callback != NULL);

  if (embedded_plugins == NULL)
    embedded_plugins = g_ptr_array_new ();
  g_ptr_array_add (embedded_plugins, callback);
}

PeasPluginInfo **
peas_register_types (void)
{
  GPtrArray *ar;

  ar = g_ptr_array_new ();

  if (embedded_plugins != NULL)
    {
      gsize i;

      for (i = 0; i < embedded_plugins->len; i++)
        {
          g_autoptr(PeasObjectModule) module = NULL;
          PeasPluginInfo *plugin_info;
          GbPluginRegisterFunc callback;

          module = peas_object_module_new_embedded ();
          callback = g_ptr_array_index (embedded_plugins, i);
          plugin_info = callback (module);

          if (plugin_info != NULL)
            g_ptr_array_add (ar, plugin_info);
        }
    }

  g_ptr_array_add (ar, NULL);

  return (PeasPluginInfo **)g_ptr_array_free (ar, FALSE);
}

void
gb_plugins_load (void)
{
  PeasEngine *engine;
  const GList *list;

  engine = peas_engine_get_default ();
  list = peas_engine_get_plugin_list (engine);

  for (; list; list = list->next)
    {
      PeasPluginInfo *plugin_info = list->data;

      /*
       * TODO: Only load embedded.
       */

      g_print ("Loading: %s\n", peas_plugin_info_get_module_name (plugin_info));

      peas_engine_load_plugin (engine, plugin_info);
    }
}
