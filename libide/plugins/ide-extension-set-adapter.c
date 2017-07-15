/* ide-extension-set-adapter.c
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

#define G_LOG_DOMAIN "ide-extension-set-adapter"

#include <glib/gi18n.h>

#include "ide-context.h"
#include "ide-extension-set-adapter.h"
#include "ide-extension-util.h"
#include "ide-macros.h"

struct _IdeExtensionSetAdapter
{
  IdeObject   parent_instance;

  PeasEngine *engine;
  gchar      *key;
  gchar      *value;
  GHashTable *extensions;
  GPtrArray  *settings;

  GType       interface_type;

  guint       reload_handler;
};

G_DEFINE_TYPE (IdeExtensionSetAdapter, ide_extension_set_adapter, IDE_TYPE_OBJECT)

enum {
  EXTENSION_ADDED,
  EXTENSION_REMOVED,
  LAST_SIGNAL
};

enum {
  PROP_0,
  PROP_ENGINE,
  PROP_INTERFACE_TYPE,
  PROP_KEY,
  PROP_VALUE,
  LAST_PROP
};

static GParamSpec *properties [LAST_PROP];
static guint signals [LAST_SIGNAL];

static void ide_extension_set_adapter_queue_reload (IdeExtensionSetAdapter *);

static void
add_extension (IdeExtensionSetAdapter *self,
               PeasPluginInfo         *plugin_info,
               PeasExtension          *exten)
{
  g_assert (IDE_IS_EXTENSION_SET_ADAPTER (self));
  g_assert (plugin_info != NULL);
  g_assert (exten != NULL);
  g_assert (g_type_is_a (G_OBJECT_TYPE (exten), self->interface_type));

  g_hash_table_insert (self->extensions, plugin_info, exten);
  g_signal_emit (self, signals [EXTENSION_ADDED], 0, plugin_info, exten);
}

static void
remove_extension (IdeExtensionSetAdapter *self,
                  PeasPluginInfo         *plugin_info,
                  PeasExtension          *exten)
{
  g_assert (IDE_IS_EXTENSION_SET_ADAPTER (self));
  g_assert (plugin_info != NULL);
  g_assert (exten != NULL);
  g_assert (g_type_is_a (G_OBJECT_TYPE (exten), self->interface_type));

  g_object_ref (exten);
  g_hash_table_remove (self->extensions, plugin_info);
  g_signal_emit (self, signals [EXTENSION_REMOVED], 0, plugin_info, exten);
  g_object_unref (exten);
}

static void
ide_extension_set_adapter_enabled_changed (IdeExtensionSetAdapter *self,
                                           const gchar            *key,
                                           GSettings              *settings)
{
  g_assert (IDE_IS_EXTENSION_SET_ADAPTER (self));
  g_assert (key != NULL);
  g_assert (G_IS_SETTINGS (settings));

  ide_extension_set_adapter_queue_reload (self);
}

static void
watch_extension (IdeExtensionSetAdapter *self,
                 PeasPluginInfo         *plugin_info,
                 GType                   interface_type)
{
  GSettings *settings;
  gchar *path;

  g_assert (IDE_IS_EXTENSION_SET_ADAPTER (self));
  g_assert (plugin_info != NULL);
  g_assert (G_TYPE_IS_INTERFACE (interface_type));

  path = g_strdup_printf ("/org/gnome/builder/extension-types/%s/%s/",
                          peas_plugin_info_get_module_name (plugin_info),
                          g_type_name (interface_type));
  settings = g_settings_new_with_path ("org.gnome.builder.extension-type", path);

  g_ptr_array_add (self->settings, g_object_ref (settings));

  g_signal_connect_object (settings,
                           "changed::enabled",
                           G_CALLBACK (ide_extension_set_adapter_enabled_changed),
                           self,
                           G_CONNECT_SWAPPED);

  g_object_unref (settings);
  g_free (path);
}

static void
ide_extension_set_adapter_reload (IdeExtensionSetAdapter *self)
{
  IdeContext *context;
  const GList *plugins;

  g_assert (IDE_IS_EXTENSION_SET_ADAPTER (self));

  while (self->settings->len > 0)
    {
      GSettings *settings;

      settings = g_ptr_array_index (self->settings, self->settings->len - 1);
      g_signal_handlers_disconnect_by_func (settings,
                                            ide_extension_set_adapter_enabled_changed,
                                            self);
      g_ptr_array_remove_index (self->settings, self->settings->len - 1);
    }

  context = ide_object_get_context (IDE_OBJECT (self));
  plugins = peas_engine_get_plugin_list (self->engine);

  g_assert (IDE_IS_CONTEXT (context));

  for (; plugins; plugins = plugins->next)
    {
      PeasPluginInfo *plugin_info = plugins->data;
      gint priority;

      if (peas_engine_provides_extension (self->engine, plugin_info, self->interface_type))
        watch_extension (self, plugin_info, self->interface_type);

      if (ide_extension_util_can_use_plugin (self->engine,
                                             plugin_info,
                                             self->interface_type,
                                             self->key,
                                             self->value,
                                             &priority))
        {
          if (!g_hash_table_lookup (self->extensions, plugin_info))
            {
              PeasExtension *exten;

              if (g_type_is_a (self->interface_type, IDE_TYPE_OBJECT))
                exten = ide_extension_new (self->engine,
                                           plugin_info,
                                           self->interface_type,
                                           "context", context,
                                           NULL);
              else
                {
                  exten = ide_extension_new (self->engine,
                                             plugin_info,
                                             self->interface_type,
                                             NULL);
                  /*
                   * If the plugin object turned out to have IdeObject
                   * as a base, try to set it now (even though we couldn't
                   * do it at construction time).
                   */
                  if (IDE_IS_OBJECT (exten))
                    ide_object_set_context (IDE_OBJECT (exten), context);
                }

              add_extension (self, plugin_info, exten);
            }
        }
      else
        {
          PeasExtension *exten;

          if ((exten = g_hash_table_lookup (self->extensions, plugin_info)))
            remove_extension (self, plugin_info, exten);
        }
    }
}

static gboolean
ide_extension_set_adapter_do_reload (gpointer data)
{
  IdeExtensionSetAdapter *self = data;

  g_assert (IDE_IS_EXTENSION_SET_ADAPTER (self));

  self->reload_handler = 0;
  ide_extension_set_adapter_reload (self);

  return G_SOURCE_REMOVE;
}

static void
ide_extension_set_adapter_queue_reload (IdeExtensionSetAdapter *self)
{
  g_assert (IDE_IS_EXTENSION_SET_ADAPTER (self));

  if (self->reload_handler != 0)
    return;

  self->reload_handler = g_timeout_add (0, ide_extension_set_adapter_do_reload, self);
}

static void
ide_extension_set_adapter_set_engine (IdeExtensionSetAdapter *self,
                                      PeasEngine             *engine)
{
  g_assert (IDE_IS_EXTENSION_SET_ADAPTER (self));
  g_assert (PEAS_IS_ENGINE (engine));

  if (g_set_object (&self->engine, engine))
    {
      g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_ENGINE]);
      ide_extension_set_adapter_queue_reload (self);
    }
}

static void
ide_extension_set_adapter_set_interface_type (IdeExtensionSetAdapter *self,
                                              GType                   interface_type)
{
  g_assert (IDE_IS_EXTENSION_SET_ADAPTER (self));
  g_assert (G_TYPE_IS_INTERFACE (interface_type));

  if (interface_type != self->interface_type)
    {
      self->interface_type = interface_type;
      g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_INTERFACE_TYPE]);
      ide_extension_set_adapter_queue_reload (self);
    }
}

static void
ide_extension_set_adapter_finalize (GObject *object)
{
  IdeExtensionSetAdapter *self = (IdeExtensionSetAdapter *)object;

  while (self->settings->len > 0)
    {
      guint i = self->settings->len - 1;
      GSettings *settings = g_ptr_array_index (self->settings, i);

      g_signal_handlers_disconnect_by_func (settings,
                                            ide_extension_set_adapter_enabled_changed,
                                            self);
      g_ptr_array_remove_index (self->settings, i);
    }

  g_clear_object (&self->engine);
  g_clear_pointer (&self->key, g_free);
  g_clear_pointer (&self->value, g_free);
  g_clear_pointer (&self->extensions, g_hash_table_unref);
  g_clear_pointer (&self->settings, g_ptr_array_unref);

  G_OBJECT_CLASS (ide_extension_set_adapter_parent_class)->finalize (object);
}

static void
ide_extension_set_adapter_get_property (GObject    *object,
                                        guint       prop_id,
                                        GValue     *value,
                                        GParamSpec *pspec)
{
  IdeExtensionSetAdapter *self = IDE_EXTENSION_SET_ADAPTER (object);

  switch (prop_id)
    {
    case PROP_ENGINE:
      g_value_set_object (value, ide_extension_set_adapter_get_engine (self));
      break;

    case PROP_INTERFACE_TYPE:
      g_value_set_gtype (value, ide_extension_set_adapter_get_interface_type (self));
      break;

    case PROP_KEY:
      g_value_set_string (value, ide_extension_set_adapter_get_key (self));
      break;

    case PROP_VALUE:
      g_value_set_string (value, ide_extension_set_adapter_get_value (self));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
ide_extension_set_adapter_set_property (GObject      *object,
                                        guint         prop_id,
                                        const GValue *value,
                                        GParamSpec   *pspec)
{
  IdeExtensionSetAdapter *self = IDE_EXTENSION_SET_ADAPTER (object);

  switch (prop_id)
    {
    case PROP_ENGINE:
      ide_extension_set_adapter_set_engine (self, g_value_get_object (value));
      break;

    case PROP_INTERFACE_TYPE:
      ide_extension_set_adapter_set_interface_type (self, g_value_get_gtype (value));
      break;

    case PROP_KEY:
      ide_extension_set_adapter_set_key (self, g_value_get_string (value));
      break;

    case PROP_VALUE:
      ide_extension_set_adapter_set_value (self, g_value_get_string (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
ide_extension_set_adapter_class_init (IdeExtensionSetAdapterClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = ide_extension_set_adapter_finalize;
  object_class->get_property = ide_extension_set_adapter_get_property;
  object_class->set_property = ide_extension_set_adapter_set_property;

  properties [PROP_ENGINE] =
    g_param_spec_object ("engine",
                         "Engine",
                         "Engine",
                         PEAS_TYPE_ENGINE,
                         (G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS));

  properties [PROP_INTERFACE_TYPE] =
    g_param_spec_gtype ("interface-type",
                        "Interface Type",
                        "Interface Type",
                        G_TYPE_INTERFACE,
                        (G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS));

  properties [PROP_KEY] =
    g_param_spec_string ("key",
                         "Key",
                         "Key",
                         NULL,
                         (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  properties [PROP_VALUE] =
    g_param_spec_string ("value",
                         "Value",
                         "Value",
                         NULL,
                         (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, LAST_PROP, properties);

  signals [EXTENSION_ADDED] =
    g_signal_new ("extension-added",
                  G_TYPE_FROM_CLASS (klass),
                  G_SIGNAL_RUN_LAST,
                  0,
                  NULL, NULL, NULL,
                  G_TYPE_NONE,
                  2,
                  PEAS_TYPE_PLUGIN_INFO,
                  PEAS_TYPE_EXTENSION);

  signals [EXTENSION_REMOVED] =
    g_signal_new ("extension-removed",
                  G_TYPE_FROM_CLASS (klass),
                  G_SIGNAL_RUN_LAST,
                  0,
                  NULL, NULL, NULL,
                  G_TYPE_NONE,
                  2,
                  PEAS_TYPE_PLUGIN_INFO,
                  PEAS_TYPE_EXTENSION);
}

static void
ide_extension_set_adapter_init (IdeExtensionSetAdapter *self)
{
  self->settings = g_ptr_array_new_with_free_func (g_object_unref);
  self->extensions = g_hash_table_new_full (NULL, NULL, NULL, g_object_unref);
}

/**
 * ide_extension_set_adapter_get_engine:
 *
 * Gets the #IdeExtensionSetAdapter:engine property.
 *
 * Returns: (transfer none): A #PeasEngine.
 */
PeasEngine *
ide_extension_set_adapter_get_engine (IdeExtensionSetAdapter *self)
{
  g_return_val_if_fail (IDE_IS_EXTENSION_SET_ADAPTER (self), NULL);

  return self->engine;
}

GType
ide_extension_set_adapter_get_interface_type (IdeExtensionSetAdapter *self)
{
  g_return_val_if_fail (IDE_IS_EXTENSION_SET_ADAPTER (self), G_TYPE_INVALID);

  return self->interface_type;
}

const gchar *
ide_extension_set_adapter_get_key (IdeExtensionSetAdapter *self)
{
  g_return_val_if_fail (IDE_IS_EXTENSION_SET_ADAPTER (self), NULL);

  return self->key;
}

void
ide_extension_set_adapter_set_key (IdeExtensionSetAdapter *self,
                                   const gchar            *key)
{
  g_return_if_fail (IDE_IS_EXTENSION_SET_ADAPTER (self));

  if (!ide_str_equal0 (self->key, key))
    {
      g_free (self->key);
      self->key = g_strdup (key);
      g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_KEY]);
      ide_extension_set_adapter_queue_reload (self);
    }
}

const gchar *
ide_extension_set_adapter_get_value (IdeExtensionSetAdapter *self)
{
  g_return_val_if_fail (IDE_IS_EXTENSION_SET_ADAPTER (self), NULL);

  return self->value;
}

void
ide_extension_set_adapter_set_value (IdeExtensionSetAdapter *self,
                                     const gchar            *value)
{
  g_return_if_fail (IDE_IS_EXTENSION_SET_ADAPTER (self));

  if (!ide_str_equal0 (self->value, value))
    {
      g_free (self->value);
      self->value = g_strdup (value);
      g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_VALUE]);
      ide_extension_set_adapter_queue_reload (self);
    }
}

/**
 * ide_extension_set_adapter_foreach:
 * @self: A #IdeExtensionSetAdapter
 * @foreach_func: (scope call): A callback
 * @user_data: user data for @foreach_func
 *
 * Calls @foreach_func for every extension loaded by the extension set.
 */
void
ide_extension_set_adapter_foreach (IdeExtensionSetAdapter            *self,
                                   IdeExtensionSetAdapterForeachFunc  foreach_func,
                                   gpointer                           user_data)
{
  GHashTableIter iter;
  gpointer key;
  gpointer value;

  g_return_if_fail (IDE_IS_EXTENSION_SET_ADAPTER (self));
  g_return_if_fail (foreach_func != NULL);

  g_hash_table_iter_init (&iter, self->extensions);

  while (g_hash_table_iter_next (&iter, &key, &value))
    {
      PeasPluginInfo *plugin_info = key;
      PeasExtension *exten = value;

      foreach_func (self, plugin_info, exten, user_data);
    }
}

guint
ide_extension_set_adapter_get_n_extensions (IdeExtensionSetAdapter *self)
{
  g_return_val_if_fail (IDE_IS_EXTENSION_SET_ADAPTER (self), 0);

  if (self->extensions != NULL)
    return g_hash_table_size (self->extensions);

  return 0;
}

IdeExtensionSetAdapter *
ide_extension_set_adapter_new (IdeContext  *context,
                               PeasEngine  *engine,
                               GType        interface_type,
                               const gchar *key,
                               const gchar *value)
{
  g_return_val_if_fail (IDE_IS_CONTEXT (context), NULL);
  g_return_val_if_fail (!engine || PEAS_IS_ENGINE (engine), NULL);
  g_return_val_if_fail (G_TYPE_IS_INTERFACE (interface_type), NULL);
  g_return_val_if_fail (key != NULL, NULL);

  return g_object_new (IDE_TYPE_EXTENSION_SET_ADAPTER,
                       "context", context,
                       "engine", engine,
                       "interface-type", interface_type,
                       "key", key,
                       "value", value,
                       NULL);
}

/**
 * ide_extension_set_adapter_get_extension:
 * @self: a #IdeExtensionSetAdapter
 * @plugin_info: a #PeasPluginInfo
 *
 * Locates the extension owned by @plugin_info if such extension exists.
 *
 * Returns: (transfer none) (nullable): A #PeasExtension or %NULL
 */
PeasExtension *
ide_extension_set_adapter_get_extension (IdeExtensionSetAdapter *self,
                                         PeasPluginInfo         *plugin_info)
{
  g_return_val_if_fail (IDE_IS_EXTENSION_SET_ADAPTER (self), NULL);
  g_return_val_if_fail (plugin_info != NULL, NULL);

  return g_hash_table_lookup (self->extensions, plugin_info);
}
