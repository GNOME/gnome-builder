/* ide-extension-adapter.c
 *
 * Copyright © 2015 Christian Hergert <christian@hergert.me>
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

#define G_LOG_DOMAIN "ide-extension-adapter"

#include <glib/gi18n.h>

#include "dazzle.h"

#include "plugins/ide-extension-adapter.h"
#include "plugins/ide-extension-util.h"

struct _IdeExtensionAdapter
{
  IdeObject       parent_instance;

  PeasEngine     *engine;
  gchar          *key;
  gchar          *value;
  GObject        *extension;
  DzlSignalGroup *settings_signals;
  GSettings      *settings;

  PeasPluginInfo *plugin_info;

  GType           interface_type;
  guint           queue_handler;
};

G_DEFINE_TYPE (IdeExtensionAdapter, ide_extension_adapter, IDE_TYPE_OBJECT)

enum {
  PROP_0,
  PROP_ENGINE,
  PROP_EXTENSION,
  PROP_INTERFACE_TYPE,
  PROP_KEY,
  PROP_VALUE,
  LAST_PROP
};

static GParamSpec *properties [LAST_PROP];

static GSettings *
ide_extension_adapter_get_settings (IdeExtensionAdapter *self,
                                    PeasPluginInfo      *plugin_info)
{
  g_autofree gchar *path = NULL;

  g_assert (IDE_IS_EXTENSION_ADAPTER (self));

  path = g_strdup_printf ("/org/gnome/builder/extension-types/%s/%s/",
                          peas_plugin_info_get_module_name (plugin_info),
                          g_type_name (self->interface_type));

  return g_settings_new_with_path ("org.gnome.builder.extension-type", path);
}

static void
ide_extension_adapter_monitor (IdeExtensionAdapter *self,
                               PeasPluginInfo      *plugin_info)
{
  g_assert (self != NULL);

  dzl_signal_group_set_target (self->settings_signals, NULL);
  g_clear_object (&self->settings);

  if (plugin_info != NULL)
    {
      self->settings = ide_extension_adapter_get_settings (self, plugin_info);
      dzl_signal_group_set_target (self->settings_signals, self->settings);
    }
}

static void
ide_extension_adapter_set_extension (IdeExtensionAdapter *self,
                                     PeasPluginInfo      *plugin_info,
                                     GObject             *extension)
{
  g_assert (IDE_IS_EXTENSION_ADAPTER (self));
  g_assert (self->interface_type != G_TYPE_INVALID);
  g_assert (!extension || g_type_is_a (G_OBJECT_TYPE (extension), self->interface_type));

  self->plugin_info = plugin_info;

  if (g_set_object (&self->extension, extension))
    {
      ide_extension_adapter_monitor (self, plugin_info);
      g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_EXTENSION]);
    }
}

static void
ide_extension_adapter_reload (IdeExtensionAdapter *self)
{
  const GList *plugins;
  PeasPluginInfo *best_match = NULL;
  PeasExtension *extension = NULL;
  gint best_match_priority = G_MININT;

  g_assert (IDE_IS_EXTENSION_ADAPTER (self));

  if (!self->engine || !self->key || !self->value || !self->interface_type)
    {
      ide_extension_adapter_set_extension (self, NULL, NULL);
      return;
    }

  plugins = peas_engine_get_plugin_list (self->engine);

  for (; plugins; plugins = plugins->next)
    {
      PeasPluginInfo *plugin_info = plugins->data;
      gint priority = 0;

      if (ide_extension_util_can_use_plugin (self->engine,
                                             plugin_info,
                                             self->interface_type,
                                             self->key,
                                             self->value,
                                             &priority))
        {
          if (priority > best_match_priority)
            {
              best_match = plugin_info;
              best_match_priority = priority;
            }
        }
    }

#if 0
  g_print ("Best match for %s=%s is %s\n",
           self->key, self->value,
           best_match ? peas_plugin_info_get_name (best_match) : "<none>");
#endif

  /*
   * If the desired extension matches our already loaded extension, then
   * ignore the attempt to create a new instance of the extension.
   */
  if ((self->extension != NULL) && (best_match != NULL) && (best_match == self->plugin_info))
    return;

  if (best_match != NULL)
    {
      IdeContext *context = ide_object_get_context (IDE_OBJECT (self));

      if (g_type_is_a (self->interface_type, IDE_TYPE_OBJECT))
        extension = ide_extension_new (self->engine,
                                       best_match,
                                       self->interface_type,
                                       "context", context,
                                       NULL);
      else
        {
          extension = ide_extension_new (self->engine,
                                         best_match,
                                         self->interface_type,
                                         NULL);
          /*
           * If the plugin object turned out to have IdeObject
           * as a base, try to set it now (even though we couldn't
           * do it at construction time).
           */
          if (IDE_IS_OBJECT (extension))
            ide_object_set_context (IDE_OBJECT (extension), context);
        }
    }

  ide_extension_adapter_set_extension (self, best_match, extension);

  g_clear_object (&extension);
}

static gboolean
ide_extension_adapter_do_reload (gpointer data)
{
  IdeExtensionAdapter *self = data;

  g_assert (IDE_IS_EXTENSION_ADAPTER (self));

  self->queue_handler = 0;
  ide_extension_adapter_reload (self);

  return G_SOURCE_REMOVE;
}

static void
ide_extension_adapter_queue_reload (IdeExtensionAdapter *self)
{
  g_assert (IDE_IS_EXTENSION_ADAPTER (self));

  if (self->queue_handler != 0)
    return;

  self->queue_handler = g_timeout_add (0, ide_extension_adapter_do_reload, self);
}

static void
ide_extension_adapter__engine_load_plugin (IdeExtensionAdapter *self,
                                           PeasPluginInfo      *plugin_info,
                                           PeasEngine          *engine)
{
  g_assert (IDE_IS_EXTENSION_ADAPTER (self));
  g_assert (plugin_info != NULL);
  g_assert (PEAS_IS_ENGINE (engine));

  if (peas_engine_provides_extension (self->engine, plugin_info, self->interface_type))
    ide_extension_adapter_queue_reload (self);
}

static void
ide_extension_adapter__engine_unload_plugin (IdeExtensionAdapter *self,
                                             PeasPluginInfo      *plugin_info,
                                             PeasEngine          *engine)
{
  g_assert (IDE_IS_EXTENSION_ADAPTER (self));
  g_assert (plugin_info != NULL);
  g_assert (PEAS_IS_ENGINE (engine));

  if (self->extension != NULL)
    {
      if (plugin_info == self->plugin_info)
        {
          g_clear_object (&self->extension);
          ide_extension_adapter_queue_reload (self);
        }
    }
}

static void
ide_extension_adapter_set_engine (IdeExtensionAdapter *self,
                                  PeasEngine          *engine)
{
  g_return_if_fail (IDE_IS_EXTENSION_ADAPTER (self));
  g_return_if_fail (!engine || PEAS_IS_ENGINE (engine));
  g_return_if_fail (self->engine == NULL);

  if (engine == NULL)
    engine = peas_engine_get_default ();

  self->engine = g_object_ref (engine);

  g_signal_connect_object (self->engine,
                           "load-plugin",
                           G_CALLBACK (ide_extension_adapter__engine_load_plugin),
                           self,
                           G_CONNECT_SWAPPED);

  g_signal_connect_object (self->engine,
                           "unload-plugin",
                           G_CALLBACK (ide_extension_adapter__engine_unload_plugin),
                           self,
                           G_CONNECT_SWAPPED);

  ide_extension_adapter_queue_reload (self);
}

static void
ide_extension_adapter_set_interface_type (IdeExtensionAdapter *self,
                                          GType                interface_type)
{
  g_return_if_fail (IDE_IS_EXTENSION_ADAPTER (self));
  g_return_if_fail (G_TYPE_IS_INTERFACE (interface_type));

  if (self->interface_type != interface_type)
    {
      self->interface_type = interface_type;
      ide_extension_adapter_queue_reload (self);
    }
}

static void
ide_extension_adapter__changed_disabled (IdeExtensionAdapter *self,
                                         const gchar         *changed_key,
                                         GSettings           *settings)
{
  g_assert (IDE_IS_EXTENSION_ADAPTER (self));
  g_assert (G_IS_SETTINGS (settings));

  if (dzl_str_equal0 (changed_key, "disabled"))
    ide_extension_adapter_queue_reload (self);
}

static void
ide_extension_adapter_finalize (GObject *object)
{
  IdeExtensionAdapter *self = (IdeExtensionAdapter *)object;

  if (self->queue_handler != 0)
    {
      g_source_remove (self->queue_handler);
      self->queue_handler = 0;
    }

  ide_extension_adapter_monitor (self, NULL);

  g_clear_object (&self->extension);
  g_clear_object (&self->engine);
  g_clear_object (&self->settings);
  g_clear_object (&self->settings_signals);
  g_clear_pointer (&self->key, g_free);
  g_clear_pointer (&self->value, g_free);

  self->interface_type = G_TYPE_INVALID;

  G_OBJECT_CLASS (ide_extension_adapter_parent_class)->finalize (object);
}

static void
ide_extension_adapter_get_property (GObject    *object,
                                    guint       prop_id,
                                    GValue     *value,
                                    GParamSpec *pspec)
{
  IdeExtensionAdapter *self = IDE_EXTENSION_ADAPTER (object);

  switch (prop_id)
    {
    case PROP_ENGINE:
      g_value_set_object (value, ide_extension_adapter_get_engine (self));
      break;

    case PROP_EXTENSION:
      g_value_set_object (value, ide_extension_adapter_get_extension (self));
      break;

    case PROP_INTERFACE_TYPE:
      g_value_set_gtype (value, ide_extension_adapter_get_interface_type (self));
      break;

    case PROP_KEY:
      g_value_set_string (value, ide_extension_adapter_get_key (self));
      break;

    case PROP_VALUE:
      g_value_set_string (value, ide_extension_adapter_get_value (self));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
ide_extension_adapter_set_property (GObject      *object,
                                    guint         prop_id,
                                    const GValue *value,
                                    GParamSpec   *pspec)
{
  IdeExtensionAdapter *self = IDE_EXTENSION_ADAPTER (object);

  switch (prop_id)
    {
    case PROP_ENGINE:
      ide_extension_adapter_set_engine (self, g_value_get_object (value));
      break;

    case PROP_INTERFACE_TYPE:
      ide_extension_adapter_set_interface_type (self, g_value_get_gtype (value));
      break;

    case PROP_KEY:
      ide_extension_adapter_set_key (self, g_value_get_string (value));
      break;

    case PROP_VALUE:
      ide_extension_adapter_set_value (self, g_value_get_string (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
ide_extension_adapter_class_init (IdeExtensionAdapterClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = ide_extension_adapter_finalize;
  object_class->get_property = ide_extension_adapter_get_property;
  object_class->set_property = ide_extension_adapter_set_property;

  properties [PROP_ENGINE] =
    g_param_spec_object ("engine",
                         "Engine",
                         "Engine",
                         PEAS_TYPE_ENGINE,
                         (G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS));

  properties [PROP_EXTENSION] =
    g_param_spec_object ("extension",
                         "Extension",
                         "The extension object.",
                         G_TYPE_OBJECT,
                         (G_PARAM_READABLE | G_PARAM_STATIC_STRINGS));

  properties [PROP_INTERFACE_TYPE] =
    g_param_spec_gtype ("interface-type",
                        "Interface Type",
                        "The GType of the extension interface.",
                        G_TYPE_INTERFACE,
                        (G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS));

  properties [PROP_KEY] =
    g_param_spec_string ("key",
                         "Key",
                         "The external data key to match from plugin info.",
                         NULL,
                         (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  properties [PROP_VALUE] =
    g_param_spec_string ("value",
                         "Value",
                         "The external data value to match from plugin info.",
                         NULL,
                         (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, LAST_PROP, properties);
}

static void
ide_extension_adapter_init (IdeExtensionAdapter *self)
{
  self->interface_type = G_TYPE_INVALID;

  self->settings_signals = dzl_signal_group_new (G_TYPE_SETTINGS);
  dzl_signal_group_connect_object (self->settings_signals,
                                   "changed::disabled",
                                   G_CALLBACK (ide_extension_adapter__changed_disabled),
                                   self,
                                   G_CONNECT_SWAPPED);
}

const gchar *
ide_extension_adapter_get_key (IdeExtensionAdapter *self)
{
  g_return_val_if_fail (IDE_IS_EXTENSION_ADAPTER (self), NULL);

  return self->key;
}

void
ide_extension_adapter_set_key (IdeExtensionAdapter *self,
                               const gchar         *key)
{
  g_return_if_fail (IDE_IS_EXTENSION_ADAPTER (self));

  if (!dzl_str_equal0 (self->key, key))
    {
      g_free (self->key);
      self->key = g_strdup (key);
      ide_extension_adapter_queue_reload (self);
      g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_KEY]);
    }
}

const gchar *
ide_extension_adapter_get_value (IdeExtensionAdapter *self)
{
  g_return_val_if_fail (IDE_IS_EXTENSION_ADAPTER (self), NULL);

  return self->value;
}

void
ide_extension_adapter_set_value (IdeExtensionAdapter *self,
                                 const gchar         *value)
{
  g_return_if_fail (IDE_IS_EXTENSION_ADAPTER (self));

  if (!dzl_str_equal0 (self->value, value))
    {
      g_free (self->value);
      self->value = g_strdup (value);
      ide_extension_adapter_reload (self);
      g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_VALUE]);
    }
}

GType
ide_extension_adapter_get_interface_type (IdeExtensionAdapter *self)
{
  g_return_val_if_fail (IDE_IS_EXTENSION_ADAPTER (self), G_TYPE_INVALID);

  return self->interface_type;
}

/**
 * ide_extension_adapter_get_engine:
 *
 * Gets the #IdeExtensionAdapter:engine property.
 *
 * Returns: (transfer none): a #PeasEngine.
 */
PeasEngine *
ide_extension_adapter_get_engine (IdeExtensionAdapter *self)
{
  g_return_val_if_fail (IDE_IS_EXTENSION_ADAPTER (self), NULL);

  return self->engine;
}

/**
 * ide_extension_adapter_get_extension:
 *
 * Gets the extension object managed by the adapter.
 *
 * Returns: (transfer none) (type GObject.Object): a #GObject or %NULL.
 */
gpointer
ide_extension_adapter_get_extension (IdeExtensionAdapter *self)
{
  g_return_val_if_fail (IDE_IS_EXTENSION_ADAPTER (self), NULL);

  return self->extension;
}

/**
 * ide_extension_adapter_new:
 * @context: An #IdeContext.
 * @engine: (allow-none): a #PeasEngine or %NULL.
 * @interface_type: The #GType of the interface to be implemented.
 * @key: The key for matching extensions from plugin info external data.
 * @value: (allow-none): The value to use when matching keys.
 *
 * Creates a new #IdeExtensionAdapter.
 *
 * The #IdeExtensionAdapter object can be used to wrap an extension that might need to change
 * at runtime based on various changing parameters. For example, it can watch the loading and
 * unloading of plugins and reload the #IdeExtensionAdapter:extension property.
 *
 * Additionally, it can match a specific plugin based on the @value provided.
 *
 * This uses #IdeExtensionPoint to create the extension implementation, which means that
 * extension points that are disabled (such as from the plugins GSettings) will be ignored.
 * As such, if one plugin that is higher priority than another, but is disabled, will be
 * ignored and the secondary plugin will be used.
 *
 * Returns: (transfer full): A newly created #IdeExtensionAdapter.
 */
IdeExtensionAdapter *
ide_extension_adapter_new (IdeContext  *context,
                           PeasEngine  *engine,
                           GType        interface_type,
                           const gchar *key,
                           const gchar *value)
{
  g_return_val_if_fail (!engine || PEAS_IS_ENGINE (engine), NULL);
  g_return_val_if_fail (G_TYPE_IS_INTERFACE (interface_type), NULL);
  g_return_val_if_fail (key != NULL, NULL);

  return g_object_new (IDE_TYPE_EXTENSION_ADAPTER,
                       "context", context,
                       "engine", engine,
                       "interface-type", interface_type,
                       "key", key,
                       "value", value,
                       NULL);
}
