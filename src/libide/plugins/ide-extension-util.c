/* ide-extension-util.c
 *
 * Copyright 2015-2019 Christian Hergert <christian@hergert.me>
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

#define G_LOG_DOMAIN "ide-extension-util"

#include "config.h"

#include <libide-core.h>
#include <gobject/gvaluecollector.h>
#include <stdlib.h>

#include "ide-extension-util-private.h"

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
  g_return_val_if_fail (g_type_is_a (interface_type, G_TYPE_INTERFACE) ||
                        g_type_is_a (interface_type, G_TYPE_OBJECT), FALSE);
  g_return_val_if_fail (priority != NULL, FALSE);

  *priority = 0;

  /*
   * If we are restricting by plugin info keyword, ensure we have enough
   * information to do so.
   */
  if ((key != NULL) && (value == NULL))
    {
      const gchar *found;

      /* If the plugin has the key and its empty, or doesn't have the key,
       * then we can assume it wants the equivalent of "*".
       */
      found = peas_plugin_info_get_external_data (plugin_info, key);
      if (ide_str_empty0 (found))
        return TRUE;

      return FALSE;
    }

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
      g_autofree gchar *delimit = NULL;
      g_auto(GStrv) values_array = NULL;
      const gchar *values;
      const gchar *priority_value;

      values = peas_plugin_info_get_external_data (plugin_info, key);
      /* Canonicalize input (for both , and ;) */
      delimit = g_strdelimit (g_strdup (values ? values : ""), ";,", ';');
      values_array = g_strsplit (delimit, ";", 0);

      /* An empty value implies "*" to match anything */
      if (!values || g_strv_contains ((const gchar * const *)values_array, "*"))
        return TRUE;

      /* Otherwise actually check that the key/value matches */
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
  return g_settings_get_boolean (settings, "enabled");
}

static void
clear_param (gpointer data)
{
G_GNUC_BEGIN_IGNORE_DEPRECATIONS
  GParameter *p = data;
  g_value_unset (&p->value);
G_GNUC_END_IGNORE_DEPRECATIONS
}

static GType
find_property_type (GType        type,
                    const gchar *name)
{
  g_autoptr(GArray) types = NULL;
  g_autofree GType *prereqs = NULL;
  guint n_prereqs = 0;
  GType ancestor = type;

  g_assert (name != NULL);
  g_assert (G_TYPE_IS_INTERFACE (type));

  /* Collect all the types to locate properties from */
  types = g_array_new (FALSE, FALSE, sizeof (GType));
  for (ancestor = type; ancestor != 0; ancestor = g_type_parent (ancestor))
    g_array_append_val (types, ancestor);
  prereqs = g_type_interface_prerequisites (type, &n_prereqs);
  for (guint i = 0; i < n_prereqs; i++)
    g_array_append_val (types, prereqs[i]);

  /* Try to locate the property within the types */
  for (guint i = 0; i < types->len; i++)
    {
      GType prereq_type = g_array_index (types, GType, i);
      GObjectClass *klass, *unref_class = NULL;
      GTypeInterface *iface, *unref_iface = NULL;
      GParamSpec *pspec = NULL;
      GType ret = G_TYPE_INVALID;

      if (G_TYPE_IS_FUNDAMENTAL (prereq_type))
        continue;

      if (!G_TYPE_IS_OBJECT (prereq_type) && !G_TYPE_IS_INTERFACE (prereq_type))
        continue;

      if (G_TYPE_IS_OBJECT (prereq_type))
        {
          if (!(klass = g_type_class_peek (prereq_type)))
            klass = unref_class = g_type_class_ref (prereq_type);
          pspec = g_object_class_find_property (klass, name);
        }
      else if (G_TYPE_IS_INTERFACE (prereq_type))
        {
          if (!(iface = g_type_default_interface_peek (prereq_type)))
            iface = unref_iface = g_type_default_interface_ref (prereq_type);
          pspec = g_object_interface_find_property (iface, name);
        }
      else
        g_assert_not_reached ();

      if (pspec != NULL)
        ret = pspec->value_type;

      g_clear_pointer (&unref_class, g_type_class_unref);
      g_clear_pointer (&unref_iface, g_type_default_interface_unref);

      if (ret != G_TYPE_INVALID)
        return ret;
    }

  return G_TYPE_INVALID;
}

static GArray *
collect_parameters (GType        type,
                    const gchar *first_property,
                    va_list      args)
{
  const gchar *property = first_property;
  g_autoptr(GArray) params = NULL;

G_GNUC_BEGIN_IGNORE_DEPRECATIONS

  params = g_array_new (FALSE, FALSE, sizeof (GParameter));
  g_array_set_clear_func (params, clear_param);

  while (property != NULL)
    {
      GType property_type = find_property_type (type, property);
      GParameter param = { property };
      g_autofree gchar *errmsg = NULL;

      if (property_type == G_TYPE_INVALID)
        {
          g_warning ("Unknown property \"%s\" from interface %s", property, g_type_name (type));
          break;
        }

      G_VALUE_COLLECT_INIT (&param.value, property_type, args, 0, &errmsg);

      if (errmsg)
        {
          g_warning ("Error collecting property: %s", errmsg);
          break;
        }

      g_array_append_val (params, param);

      property = va_arg (args, const gchar *);
    }

  if (property != NULL)
    return NULL;

G_GNUC_END_IGNORE_DEPRECATIONS

  return g_steal_pointer (&params);
}

/**
 * ide_extension_set_new:
 *
 * This function acts like peas_extension_set_new() except that it allows for
 * us to pass properties that are in the parent class.
 *
 * It does this by duplicating some of the GParameter stuff that libpeas does
 * but looking at base-classes in addition to interface properties.
 *
 * Returns: (transfer full): a #PeasExtensionSet.
 *
 * Since: 3.32
 */
PeasExtensionSet *
ide_extension_set_new (PeasEngine     *engine,
                       GType           type,
                       const gchar    *first_property,
                       ...)
{
  PeasExtensionSet *ret;
  va_list args;

  va_start (args, first_property);
  ret = peas_extension_set_new_valist (engine, type, first_property, args);
  va_end (args);

  return ret;
}

PeasExtension *
ide_extension_new (PeasEngine     *engine,
                   PeasPluginInfo *plugin_info,
                   GType           type,
                   const gchar    *first_property,
                   ...)
{
  g_autoptr(GArray) params = NULL;
  va_list args;

  g_return_val_if_fail (!engine || PEAS_IS_ENGINE (engine), NULL);
  g_return_val_if_fail (G_TYPE_IS_INTERFACE (type) || G_TYPE_IS_OBJECT (type), NULL);

  if (engine == NULL)
    engine = peas_engine_get_default ();

  va_start (args, first_property);
  params = collect_parameters (type, first_property, args);
  va_end (args);

  if (params == NULL)
    return NULL;

G_GNUC_BEGIN_IGNORE_DEPRECATIONS

  return peas_engine_create_extensionv (engine,
                                        plugin_info,
                                        type,
                                        params->len,
                                        (GParameter *)(gpointer)params->data);

G_GNUC_END_IGNORE_DEPRECATIONS
}
