/* ide-extension-adapter.c
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

#define G_LOG_DOMAIN "ide-extension-adapter"

#include "config.h"

#include <glib/gi18n.h>

#include "ide-extension-adapter.h"
#include "ide-extension-util-private.h"

struct _IdeExtensionAdapter
{
  IdeObject       parent_instance;

  PeasEngine     *engine;
  gchar          *key;
  gchar          *value;
  GObject        *extension;
  GSignalGroup   *settings_signals;
  GSettings      *settings;

  PeasPluginInfo *plugin_info;

  GType           interface_type;
  guint           queue_handler;
};

G_DEFINE_FINAL_TYPE (IdeExtensionAdapter, ide_extension_adapter, IDE_TYPE_OBJECT)

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

static gchar *
ide_extension_adapter_repr (IdeObject *object)
{
  IdeExtensionAdapter *self = (IdeExtensionAdapter *)object;

  g_assert (IDE_IS_EXTENSION_ADAPTER (self));

  return g_strdup_printf ("%s interface=“%s” key=“%s” value=“%s”",
                          G_OBJECT_TYPE_NAME (self),
                          g_type_name (self->interface_type),
                          self->key ?: "",
                          self->value ?: "");
}

static GSettings *
ide_extension_adapter_get_settings (IdeExtensionAdapter *self,
                                    PeasPluginInfo      *plugin_info)
{
  g_autofree gchar *path = NULL;

  g_assert (IDE_IS_MAIN_THREAD ());
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
  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (IDE_IS_EXTENSION_ADAPTER (self));

  g_signal_group_set_target (self->settings_signals, NULL);
  g_clear_object (&self->settings);

  if (plugin_info != NULL)
    {
      self->settings = ide_extension_adapter_get_settings (self, plugin_info);
      g_signal_group_set_target (self->settings_signals, self->settings);
    }
}

static void
ide_extension_adapter_set_extension (IdeExtensionAdapter *self,
                                     PeasPluginInfo      *plugin_info,
                                     GObject             *extension)
{
  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (IDE_IS_EXTENSION_ADAPTER (self));
  g_assert (!extension || self->interface_type != G_TYPE_INVALID);
  g_assert (!extension || g_type_is_a (G_OBJECT_TYPE (extension), self->interface_type));

  self->plugin_info = plugin_info;

  if (extension != self->extension)
    {
      if (IDE_IS_OBJECT (self->extension))
        ide_object_destroy (IDE_OBJECT (self->extension));

      g_set_object (&self->extension, extension);

      if (IDE_IS_OBJECT (extension))
        ide_object_append (IDE_OBJECT (self), IDE_OBJECT (extension));

      ide_extension_adapter_monitor (self, plugin_info);

      g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_EXTENSION]);
    }
}

static void
ide_extension_adapter_reload (IdeExtensionAdapter *self)
{
  PeasPluginInfo *best_match = NULL;
  GObject *extension = NULL;
  guint n_items;
  int best_match_priority = G_MININT;

  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (IDE_IS_EXTENSION_ADAPTER (self));
  g_assert (self->interface_type != G_TYPE_INVALID);

  if (!self->engine || !self->key || !self->value)
    {
      ide_extension_adapter_set_extension (self, NULL, NULL);
      return;
    }

  n_items = g_list_model_get_n_items (G_LIST_MODEL (self->engine));

  for (guint i = 0; i < n_items; i++)
    {
      g_autoptr(PeasPluginInfo) plugin_info = g_list_model_get_item (G_LIST_MODEL (self->engine), i);
      int priority = 0;

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
    extension = ide_extension_new (self->engine,
                                   best_match,
                                   self->interface_type,
                                   NULL);

  ide_extension_adapter_set_extension (self, best_match, extension);

  g_clear_object (&extension);
}

static gboolean
ide_extension_adapter_do_reload (gpointer data)
{
  IdeExtensionAdapter *self = data;

  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (IDE_IS_EXTENSION_ADAPTER (self));

  self->queue_handler = 0;

  if (self->interface_type != G_TYPE_INVALID)
    ide_extension_adapter_reload (self);

  return G_SOURCE_REMOVE;
}

static void
ide_extension_adapter_queue_reload (IdeExtensionAdapter *self)
{
  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (IDE_IS_EXTENSION_ADAPTER (self));

  g_clear_handle_id (&self->queue_handler, g_source_remove);
  self->queue_handler = g_timeout_add (0, ide_extension_adapter_do_reload, self);
}

static void
ide_extension_adapter__engine_load_plugin (IdeExtensionAdapter *self,
                                           PeasPluginInfo      *plugin_info,
                                           PeasEngine          *engine)
{
  g_assert (IDE_IS_MAIN_THREAD ());
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
  g_assert (IDE_IS_MAIN_THREAD ());
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
  g_return_if_fail (IDE_IS_MAIN_THREAD ());
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
  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (IDE_IS_EXTENSION_ADAPTER (self));
  g_assert (G_TYPE_IS_INTERFACE (interface_type));

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
  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (IDE_IS_EXTENSION_ADAPTER (self));
  g_assert (G_IS_SETTINGS (settings));

  if (ide_str_equal0 (changed_key, "disabled"))
    ide_extension_adapter_queue_reload (self);
}

static void
ide_extension_adapter_destroy (IdeObject *object)
{
  IdeExtensionAdapter *self = (IdeExtensionAdapter *)object;

  self->interface_type = G_TYPE_INVALID;

  g_clear_handle_id (&self->queue_handler, g_source_remove);

  ide_extension_adapter_monitor (self, NULL);

  IDE_OBJECT_CLASS (ide_extension_adapter_parent_class)->destroy (object);
}

static void
ide_extension_adapter_finalize (GObject *object)
{
  IdeExtensionAdapter *self = (IdeExtensionAdapter *)object;

  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (self->interface_type == G_TYPE_INVALID);
  g_assert (self->queue_handler == 0);

  g_clear_object (&self->extension);
  g_clear_object (&self->engine);
  g_clear_object (&self->settings);
  g_clear_object (&self->settings_signals);
  g_clear_pointer (&self->key, g_free);
  g_clear_pointer (&self->value, g_free);

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
  IdeObjectClass *i_object_class = IDE_OBJECT_CLASS (klass);

  object_class->finalize = ide_extension_adapter_finalize;
  object_class->get_property = ide_extension_adapter_get_property;
  object_class->set_property = ide_extension_adapter_set_property;

  i_object_class->destroy = ide_extension_adapter_destroy;
  i_object_class->repr = ide_extension_adapter_repr;

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

  self->settings_signals = g_signal_group_new (G_TYPE_SETTINGS);
  g_signal_group_connect_object (self->settings_signals,
                                   "changed::disabled",
                                   G_CALLBACK (ide_extension_adapter__changed_disabled),
                                   self,
                                   G_CONNECT_SWAPPED);
}

const gchar *
ide_extension_adapter_get_key (IdeExtensionAdapter *self)
{
  g_return_val_if_fail (IDE_IS_MAIN_THREAD (), NULL);
  g_return_val_if_fail (IDE_IS_EXTENSION_ADAPTER (self), NULL);

  return self->key;
}

void
ide_extension_adapter_set_key (IdeExtensionAdapter *self,
                               const gchar         *key)
{
  g_return_if_fail (IDE_IS_MAIN_THREAD ());
  g_return_if_fail (IDE_IS_EXTENSION_ADAPTER (self));

  if (g_set_str (&self->key, key))
    {
      ide_extension_adapter_queue_reload (self);
      g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_KEY]);
    }
}

const gchar *
ide_extension_adapter_get_value (IdeExtensionAdapter *self)
{
  g_return_val_if_fail (IDE_IS_MAIN_THREAD (), NULL);
  g_return_val_if_fail (IDE_IS_EXTENSION_ADAPTER (self), NULL);

  return self->value;
}

void
ide_extension_adapter_set_value (IdeExtensionAdapter *self,
                                 const gchar         *value)
{
  g_return_if_fail (IDE_IS_MAIN_THREAD ());
  g_return_if_fail (IDE_IS_EXTENSION_ADAPTER (self));

  if (g_set_str (&self->value, value))
    {
      if (self->interface_type != G_TYPE_INVALID)
        ide_extension_adapter_reload (self);
      g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_VALUE]);
    }
}

GType
ide_extension_adapter_get_interface_type (IdeExtensionAdapter *self)
{
  g_return_val_if_fail (IDE_IS_MAIN_THREAD (), G_TYPE_INVALID);
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
  g_return_val_if_fail (IDE_IS_MAIN_THREAD (), NULL);
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
  g_return_val_if_fail (IDE_IS_MAIN_THREAD (), NULL);
  g_return_val_if_fail (IDE_IS_EXTENSION_ADAPTER (self), NULL);

  return self->extension;
}

/**
 * ide_extension_adapter_new:
 * @parent: (nullable): An #IdeObject or %NULL
 * @engine: (allow-none): a #PeasEngine or %NULL
 * @interface_type: The #GType of the interface to be implemented.
 * @key: The key for matching extensions from plugin info external data.
 * @value: (allow-none): The value to use when matching keys.
 *
 * Creates a new #IdeExtensionAdapter.
 *
 * The #IdeExtensionAdapter object can be used to wrap an extension that might
 * need to change at runtime based on various changing parameters. For example,
 * it can watch the loading and unloading of plugins and reload the
 * #IdeExtensionAdapter:extension property.
 *
 * Additionally, it can match a specific plugin based on the @value provided.
 *
 * This uses #IdeExtensionPoint to create the extension implementation, which
 * means that extension points that are disabled (such as from the plugins
 * GSettings) will be ignored.  As such, if one plugin that is higher priority
 * than another, but is disabled, will be ignored and the secondary plugin will
 * be used.
 *
 * Returns: (transfer full): A newly created #IdeExtensionAdapter.
 */
IdeExtensionAdapter *
ide_extension_adapter_new (IdeObject   *parent,
                           PeasEngine  *engine,
                           GType        interface_type,
                           const gchar *key,
                           const gchar *value)
{
  IdeExtensionAdapter *self;

  g_return_val_if_fail (IDE_IS_MAIN_THREAD (), NULL);
  g_return_val_if_fail (!engine || PEAS_IS_ENGINE (engine), NULL);
  g_return_val_if_fail (G_TYPE_IS_INTERFACE (interface_type), NULL);
  g_return_val_if_fail (key != NULL, NULL);

  self = g_object_new (IDE_TYPE_EXTENSION_ADAPTER,
                       "engine", engine,
                       "interface-type", interface_type,
                       "key", key,
                       "value", value,
                       NULL);

  if (parent != NULL)
    ide_object_append (parent, IDE_OBJECT (self));

  return g_steal_pointer (&self);
}
