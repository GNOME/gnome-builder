/* ide-extension-set-adapter.c
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

#define G_LOG_DOMAIN "ide-extension-set-adapter"

#include "config.h"

#include <stdlib.h>

#include <glib/gi18n.h>

#include <gio/gio.h>

#include "ide-marshal.h"

#include "ide-extension-set-adapter.h"
#include "ide-extension-util-private.h"

struct _IdeExtensionSetAdapter
{
  IdeObject   parent_instance;

  PeasEngine *engine;
  gchar      *key;
  gchar      *value;
  GHashTable *extensions;
  GPtrArray  *settings;
  GPtrArray  *extensions_array;

  GType       interface_type;

  guint       reload_handler;
};

static GType
ide_extension_set_adapter_get_item_type (GListModel *model)
{
  return IDE_EXTENSION_SET_ADAPTER (model)->interface_type;
}

static guint
ide_extension_set_adapter_get_n_items (GListModel *model)
{
  return IDE_EXTENSION_SET_ADAPTER (model)->extensions_array->len;
}

static gpointer
ide_extension_set_adapter_get_item (GListModel *model,
                                    guint       position)
{
  IdeExtensionSetAdapter *self = IDE_EXTENSION_SET_ADAPTER (model);

  if (position < self->extensions_array->len)
    return g_object_ref (g_ptr_array_index (self->extensions_array, position));

  return NULL;
}

static void
list_model_iface_init (GListModelInterface *iface)
{
  iface->get_item_type = ide_extension_set_adapter_get_item_type;
  iface->get_n_items = ide_extension_set_adapter_get_n_items;
  iface->get_item = ide_extension_set_adapter_get_item;
}

G_DEFINE_FINAL_TYPE_WITH_CODE (IdeExtensionSetAdapter, ide_extension_set_adapter, IDE_TYPE_OBJECT,
                               G_IMPLEMENT_INTERFACE (G_TYPE_LIST_MODEL, list_model_iface_init))

enum {
  EXTENSIONS_LOADED,
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

static gchar *
ide_extension_set_adapter_repr (IdeObject *object)
{
  IdeExtensionSetAdapter *self = (IdeExtensionSetAdapter *)object;

  g_assert (IDE_IS_EXTENSION_SET_ADAPTER (self));

  return g_strdup_printf ("%s interface=\"%s\" key=\"%s\" value=\"%s\"",
                          G_OBJECT_TYPE_NAME (self),
                          g_type_name (self->interface_type),
                          self->key ?: "",
                          self->value ?: "");
}

static void
add_extension (IdeExtensionSetAdapter *self,
               PeasPluginInfo         *plugin_info,
               GObject          *exten)
{
  guint position;

  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (IDE_IS_EXTENSION_SET_ADAPTER (self));
  g_assert (plugin_info != NULL);
  g_assert (exten != NULL);
  g_assert (g_type_is_a (G_OBJECT_TYPE (exten), self->interface_type));

  g_hash_table_insert (self->extensions, plugin_info, exten);

  /* Ensure that we take the reference in case it's a floating ref */
  if (G_IS_INITIALLY_UNOWNED (exten) && g_object_is_floating (exten))
    g_object_ref_sink (exten);

  /*
   * If the plugin object turned out to have IdeObject as a
   * base, make it a child of ourselves, because we're an
   * IdeObject too and that gives it access to the context.
   */
  if (IDE_IS_OBJECT (exten))
    ide_object_append (IDE_OBJECT (self), IDE_OBJECT (exten));

  position = self->extensions_array->len;
  g_ptr_array_add (self->extensions_array, exten);
  g_list_model_items_changed (G_LIST_MODEL (self), position, 0, 1);

  g_signal_emit (self, signals [EXTENSION_ADDED], 0, plugin_info, exten);
}

static void
remove_extension (IdeExtensionSetAdapter *self,
                  PeasPluginInfo         *plugin_info,
                  GObject          *exten)
{
  g_autoptr(GObject) hold = NULL;
  guint position;

  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (IDE_IS_EXTENSION_SET_ADAPTER (self));
  g_assert (plugin_info != NULL);
  g_assert (exten != NULL);
  g_assert (self->interface_type == G_TYPE_INVALID ||
            g_type_is_a (G_OBJECT_TYPE (exten), self->interface_type));

  hold = g_object_ref (exten);

  IDE_TRACE_MSG ("Unloading extension %s",
                 G_OBJECT_TYPE_NAME (hold));

  g_hash_table_remove (self->extensions, plugin_info);

  if (g_ptr_array_find (self->extensions_array, exten, &position))
    {
      g_ptr_array_remove_index (self->extensions_array, position);
      g_list_model_items_changed (G_LIST_MODEL (self), position, 1, 0);
    }
  else
    {
      g_assert_not_reached ();
    }

  g_signal_emit (self, signals [EXTENSION_REMOVED], 0, plugin_info, hold);

  if (IDE_IS_OBJECT (hold))
    ide_object_destroy (IDE_OBJECT (hold));
}

static void
ide_extension_set_adapter_enabled_changed (IdeExtensionSetAdapter *self,
                                           const gchar            *key,
                                           GSettings              *settings)
{
  g_assert (IDE_IS_MAIN_THREAD ());
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
  g_autoptr(GSettings) settings = NULL;
  g_autofree char *path = NULL;

  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (IDE_IS_EXTENSION_SET_ADAPTER (self));
  g_assert (plugin_info != NULL);
  g_assert (G_TYPE_IS_INTERFACE (interface_type) || G_TYPE_IS_OBJECT (interface_type));

  path = g_strdup_printf ("/org/gnome/builder/extension-types/%s/%s/",
                          peas_plugin_info_get_module_name (plugin_info),
                          g_type_name (interface_type));
  settings = g_settings_new_with_path ("org.gnome.builder.extension-type", path);

  g_ptr_array_add (self->settings, g_object_ref (settings));

  /* We have to fetch the key once to get changed events */
  g_settings_get_boolean (settings, "enabled");

  g_signal_connect_object (settings,
                           "changed::enabled",
                           G_CALLBACK (ide_extension_set_adapter_enabled_changed),
                           self,
                           G_CONNECT_SWAPPED);
}

static void
ide_extension_set_adapter_reload (IdeExtensionSetAdapter *self)
{
  guint n_items;

  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (IDE_IS_EXTENSION_SET_ADAPTER (self));
  g_assert (self->interface_type != G_TYPE_INVALID);

  while (self->settings->len > 0)
    {
      GSettings *settings;

      settings = g_ptr_array_index (self->settings, self->settings->len - 1);
      g_signal_handlers_disconnect_by_func (settings,
                                            ide_extension_set_adapter_enabled_changed,
                                            self);
      g_ptr_array_remove_index (self->settings, self->settings->len - 1);
    }

  n_items = g_list_model_get_n_items (G_LIST_MODEL (self->engine));

  for (guint i = 0; i < n_items; i++)
    {
      g_autoptr(PeasPluginInfo) plugin_info = g_list_model_get_item (G_LIST_MODEL (self->engine), i);
      int priority;

      if (!peas_plugin_info_is_loaded (plugin_info))
        continue;

      if (!peas_engine_provides_extension (self->engine, plugin_info, self->interface_type))
        continue;

      watch_extension (self, plugin_info, self->interface_type);

      if (ide_extension_util_can_use_plugin (self->engine,
                                             plugin_info,
                                             self->interface_type,
                                             self->key,
                                             self->value,
                                             &priority))
        {
          if (!g_hash_table_contains (self->extensions, plugin_info))
            {
              GObject *exten;

              exten = ide_extension_new (self->engine,
                                         plugin_info,
                                         self->interface_type,
                                         NULL);

              if (exten != NULL)
                add_extension (self, plugin_info, exten);
            }
        }
      else
        {
          GObject *exten;

          if ((exten = g_hash_table_lookup (self->extensions, plugin_info)))
            remove_extension (self, plugin_info, exten);
        }
    }

  g_signal_emit (self, signals [EXTENSIONS_LOADED], 0);
}

static gboolean
ide_extension_set_adapter_do_reload (gpointer data)
{
  IdeExtensionSetAdapter *self = data;

  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (IDE_IS_EXTENSION_SET_ADAPTER (self));

  self->reload_handler = 0;

  if (self->interface_type != G_TYPE_INVALID)
    ide_extension_set_adapter_reload (self);

  return G_SOURCE_REMOVE;
}

static void
ide_extension_set_adapter_queue_reload (IdeExtensionSetAdapter *self)
{
  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (IDE_IS_EXTENSION_SET_ADAPTER (self));

  g_clear_handle_id (&self->reload_handler, g_source_remove);

  self->reload_handler = g_idle_add_full (G_PRIORITY_HIGH,
                                          ide_extension_set_adapter_do_reload,
                                          self,
                                          NULL);
}

static void
ide_extension_set_adapter_load_plugin (IdeExtensionSetAdapter *self,
                                       PeasPluginInfo         *plugin_info,
                                       PeasEngine             *engine)
{
  g_assert (IDE_IS_EXTENSION_SET_ADAPTER (self));
  g_assert (plugin_info != NULL);
  g_assert (PEAS_IS_ENGINE (engine));

  ide_extension_set_adapter_queue_reload (self);
}

static void
ide_extension_set_adapter_unload_plugin (IdeExtensionSetAdapter *self,
                                         PeasPluginInfo         *plugin_info,
                                         PeasEngine             *engine)
{
  GObject *exten;

  g_assert (IDE_IS_EXTENSION_SET_ADAPTER (self));
  g_assert (plugin_info != NULL);
  g_assert (PEAS_IS_ENGINE (engine));

  if ((exten = g_hash_table_lookup (self->extensions, plugin_info)))
    {
      remove_extension (self, plugin_info, exten);
      g_hash_table_remove (self->extensions, plugin_info);
    }
}

static void
ide_extension_set_adapter_set_engine (IdeExtensionSetAdapter *self,
                                      PeasEngine             *engine)
{
  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (IDE_IS_EXTENSION_SET_ADAPTER (self));
  g_assert (!engine || PEAS_IS_ENGINE (engine));

  if (engine == NULL)
    engine = peas_engine_get_default ();

  if (g_set_object (&self->engine, engine))
    {
      g_signal_connect_object (self->engine, "load-plugin",
                               G_CALLBACK (ide_extension_set_adapter_load_plugin),
                               self,
                               G_CONNECT_AFTER | G_CONNECT_SWAPPED);
      g_signal_connect_object (self->engine, "unload-plugin",
                               G_CALLBACK (ide_extension_set_adapter_unload_plugin),
                               self,
                               G_CONNECT_SWAPPED);
      g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_ENGINE]);
      ide_extension_set_adapter_queue_reload (self);
    }
}

static void
ide_extension_set_adapter_set_interface_type (IdeExtensionSetAdapter *self,
                                              GType                   interface_type)
{
  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (IDE_IS_EXTENSION_SET_ADAPTER (self));
  g_assert (G_TYPE_IS_INTERFACE (interface_type) || G_TYPE_IS_OBJECT (interface_type));

  if (interface_type != self->interface_type)
    {
      self->interface_type = interface_type;
      g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_INTERFACE_TYPE]);
      ide_extension_set_adapter_queue_reload (self);
    }
}

static void
ide_extension_set_adapter_destroy (IdeObject *object)
{
  IdeExtensionSetAdapter *self = (IdeExtensionSetAdapter *)object;
  g_autoptr(GHashTable) extensions = NULL;
  GHashTableIter iter;
  gpointer key;
  gpointer value;

  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (IDE_IS_EXTENSION_SET_ADAPTER (self));

  self->interface_type = G_TYPE_INVALID;
  g_clear_handle_id (&self->reload_handler, g_source_remove);

  /*
   * Steal the extensions so we can be re-entrant safe and not break
   * any assumptions about extensions being a real pointer.
   */
  extensions = g_steal_pointer (&self->extensions);
  self->extensions = g_hash_table_new_full (NULL, NULL, NULL, g_object_unref);

  g_hash_table_iter_init (&iter, extensions);

  while (g_hash_table_iter_next (&iter, &key, &value))
    {
      PeasPluginInfo *plugin_info = key;
      GObject *exten = value;

      remove_extension (self, plugin_info, exten);
      g_hash_table_iter_remove (&iter);
    }

  IDE_OBJECT_CLASS (ide_extension_set_adapter_parent_class)->destroy (object);
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
  g_clear_pointer (&self->extensions_array, g_ptr_array_unref);
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
  IdeObjectClass *i_object_class = IDE_OBJECT_CLASS (klass);

  object_class->finalize = ide_extension_set_adapter_finalize;
  object_class->get_property = ide_extension_set_adapter_get_property;
  object_class->set_property = ide_extension_set_adapter_set_property;

  i_object_class->destroy = ide_extension_set_adapter_destroy;
  i_object_class->repr = ide_extension_set_adapter_repr;

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
                        G_TYPE_OBJECT,
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
                  NULL, NULL,
                  ide_marshal_VOID__BOXED_OBJECT,
                  G_TYPE_NONE,
                  2,
                  PEAS_TYPE_PLUGIN_INFO | G_SIGNAL_TYPE_STATIC_SCOPE,
                  G_TYPE_OBJECT | G_SIGNAL_TYPE_STATIC_SCOPE);
  g_signal_set_va_marshaller (signals [EXTENSION_ADDED],
                              G_TYPE_FROM_CLASS (klass),
                              ide_marshal_VOID__BOXED_OBJECTv);

  signals [EXTENSION_REMOVED] =
    g_signal_new ("extension-removed",
                  G_TYPE_FROM_CLASS (klass),
                  G_SIGNAL_RUN_LAST,
                  0,
                  NULL, NULL,
                  ide_marshal_VOID__BOXED_OBJECT,
                  G_TYPE_NONE,
                  2,
                  PEAS_TYPE_PLUGIN_INFO | G_SIGNAL_TYPE_STATIC_SCOPE,
                  G_TYPE_OBJECT | G_SIGNAL_TYPE_STATIC_SCOPE);
  g_signal_set_va_marshaller (signals [EXTENSION_REMOVED],
                              G_TYPE_FROM_CLASS (klass),
                              ide_marshal_VOID__BOXED_OBJECTv);

  signals [EXTENSIONS_LOADED] =
    g_signal_new ("extensions-loaded",
                  G_TYPE_FROM_CLASS (klass),
                  G_SIGNAL_RUN_LAST,
                  0,
                  NULL, NULL,
                  ide_marshal_VOID__VOID,
                  G_TYPE_NONE, 0);
  g_signal_set_va_marshaller (signals [EXTENSIONS_LOADED],
                              G_TYPE_FROM_CLASS (klass),
                              ide_marshal_VOID__VOIDv);
}

static void
ide_extension_set_adapter_init (IdeExtensionSetAdapter *self)
{
  self->settings = g_ptr_array_new_with_free_func (g_object_unref);
  self->extensions = g_hash_table_new_full (NULL, NULL, NULL, g_object_unref);
  self->extensions_array = g_ptr_array_new ();
}

/**
 * ide_extension_set_adapter_get_engine:
 *
 * Gets the #IdeExtensionSetAdapter:engine property.
 *
 * Returns: (transfer none): a #PeasEngine.
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
  g_return_if_fail (IDE_IS_MAIN_THREAD ());
  g_return_if_fail (IDE_IS_EXTENSION_SET_ADAPTER (self));

  if (g_set_str (&self->key, key))
    {
      g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_KEY]);
      ide_extension_set_adapter_queue_reload (self);
    }
}

const gchar *
ide_extension_set_adapter_get_value (IdeExtensionSetAdapter *self)
{
  g_return_val_if_fail (IDE_IS_MAIN_THREAD (), NULL);
  g_return_val_if_fail (IDE_IS_EXTENSION_SET_ADAPTER (self), NULL);

  return self->value;
}

void
ide_extension_set_adapter_set_value (IdeExtensionSetAdapter *self,
                                     const gchar            *value)
{
  g_return_if_fail (IDE_IS_MAIN_THREAD ());
  g_return_if_fail (IDE_IS_EXTENSION_SET_ADAPTER (self));

  IDE_TRACE_MSG ("Setting extension adapter %s value to \"%s\"",
                 g_type_name (self->interface_type),
                 value ?: "");

  if (g_set_str (&self->value, value))
    {
      g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_VALUE]);
      ide_extension_set_adapter_queue_reload (self);
    }
}

/**
 * ide_extension_set_adapter_foreach:
 * @self: an #IdeExtensionSetAdapter
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
  guint n_items;

  g_return_if_fail (IDE_IS_EXTENSION_SET_ADAPTER (self));
  g_return_if_fail (foreach_func != NULL);

  /*
   * Use the ordered list of plugins as it is sorted including any
   * dependencies of plugins.
   */

  n_items = g_list_model_get_n_items (G_LIST_MODEL (self->engine));

  for (guint i = 0; i < n_items; i++)
    {
      g_autoptr(PeasPluginInfo) plugin_info = g_list_model_get_item (G_LIST_MODEL (self->engine), i);
      GObject *exten = g_hash_table_lookup (self->extensions, plugin_info);

      if (exten != NULL)
        foreach_func (self, plugin_info, exten, user_data);
    }
}

typedef struct
{
  PeasPluginInfo *plugin_info;
  GObject  *exten;
  gint            priority;
} SortedInfo;

static gint
sort_by_priority (gconstpointer a,
                  gconstpointer b)
{
  const SortedInfo *sa = a;
  const SortedInfo *sb = b;

  /* Greater values are higher priority */
  /* FIXME: ^ Clearly this is not the case??? */

  if (sa->priority < sb->priority)
    return -1;
  else if (sa->priority > sb->priority)
    return 1;
  else
    return 0;
}

/**
 * ide_extension_set_adapter_foreach_by_priority:
 * @self: an #IdeExtensionSetAdapter
 * @foreach_func: (scope call): A callback
 * @user_data: user data for @foreach_func
 *
 * Calls @foreach_func for every extension loaded by the extension set.
 */
void
ide_extension_set_adapter_foreach_by_priority (IdeExtensionSetAdapter            *self,
                                               IdeExtensionSetAdapterForeachFunc  foreach_func,
                                               gpointer                           user_data)
{
  g_autoptr(GArray) sorted = NULL;
  g_autofree gchar *prio_key = NULL;
  GHashTableIter iter;
  gpointer key;
  gpointer value;

  g_return_if_fail (IDE_IS_MAIN_THREAD ());
  g_return_if_fail (IDE_IS_EXTENSION_SET_ADAPTER (self));
  g_return_if_fail (foreach_func != NULL);

  if (self->key == NULL)
    {
      ide_extension_set_adapter_foreach (self, foreach_func, user_data);
      return;
    }

  prio_key = g_strdup_printf ("%s-Priority", self->key);
  sorted = g_array_new (FALSE, FALSE, sizeof (SortedInfo));

  g_hash_table_iter_init (&iter, self->extensions);

  while (g_hash_table_iter_next (&iter, &key, &value))
    {
      PeasPluginInfo *plugin_info = key;
      GObject *exten = value;
      const gchar *priostr = peas_plugin_info_get_external_data (plugin_info, prio_key);
      gint prio = priostr ? atoi (priostr) : 0;
      SortedInfo info = { plugin_info, exten, prio };

      g_array_append_val (sorted, info);
    }

  g_array_sort (sorted, sort_by_priority);

  for (guint i = 0; i < sorted->len; i++)
    {
      const SortedInfo *info = &g_array_index (sorted, SortedInfo, i);

      foreach_func (self, info->plugin_info, info->exten, user_data);
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
ide_extension_set_adapter_new (IdeObject   *parent,
                               PeasEngine  *engine,
                               GType        interface_type,
                               const gchar *key,
                               const gchar *value)
{
  IdeExtensionSetAdapter *ret;

  g_return_val_if_fail (IDE_IS_MAIN_THREAD (), NULL);
  g_return_val_if_fail (!parent || IDE_IS_OBJECT (parent), NULL);
  g_return_val_if_fail (!engine || PEAS_IS_ENGINE (engine), NULL);
  g_return_val_if_fail (G_TYPE_IS_INTERFACE (interface_type) ||
                        G_TYPE_IS_OBJECT (interface_type), NULL);

  ret = g_object_new (IDE_TYPE_EXTENSION_SET_ADAPTER,
                      "engine", engine,
                      "interface-type", interface_type,
                      "key", key,
                      "value", value,
                      NULL);

  if (parent != NULL)
    ide_object_append (parent, IDE_OBJECT (ret));

  /* If we have a reload queued, just process it immediately so that
   * there is some determinism in plugin loading.
   */
  if (ret->reload_handler != 0)
    {
      g_clear_handle_id (&ret->reload_handler, g_source_remove);
      ide_extension_set_adapter_do_reload (ret);
    }

  return ret;
}

/**
 * ide_extension_set_adapter_get_extension:
 * @self: a #IdeExtensionSetAdapter
 * @plugin_info: a #PeasPluginInfo
 *
 * Locates the extension owned by @plugin_info if such extension exists.
 *
 * Returns: (transfer none) (nullable): a #GObject or %NULL
 */
GObject *
ide_extension_set_adapter_get_extension (IdeExtensionSetAdapter *self,
                                         PeasPluginInfo         *plugin_info)
{
  g_return_val_if_fail (IDE_IS_MAIN_THREAD (), NULL);
  g_return_val_if_fail (IDE_IS_EXTENSION_SET_ADAPTER (self), NULL);
  g_return_val_if_fail (plugin_info != NULL, NULL);

  return g_hash_table_lookup (self->extensions, plugin_info);
}
