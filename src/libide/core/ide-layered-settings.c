/* ide-layered-settings.c
 *
 * Copyright 2015-2022 Christian Hergert <chergert@redhat.com>
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

#define G_LOG_DOMAIN "ide-layered-settings"

#include "config.h"

#include <glib/gi18n.h>

#define G_SETTINGS_ENABLE_BACKEND
#include <gio/gsettingsbackend.h>

#include "gsettings-mapping.h"

#include "ide-layered-settings-private.h"
#include "ide-marshal.h"

struct _IdeLayeredSettings
{
  GObject           parent_instance;
  GPtrArray        *settings;
  GSettings        *memory_settings;
  char             *schema_id;
  char             *path;
};

G_DEFINE_TYPE (IdeLayeredSettings, ide_layered_settings, G_TYPE_OBJECT)

enum {
  PROP_0,
  PROP_PATH,
  PROP_SCHEMA_ID,
  N_PROPS
};

enum {
  CHANGED,
  N_SIGNALS
};

static GParamSpec *properties [N_PROPS];
static guint signals [N_SIGNALS];
static GSettingsBackend *memory_backend;

static GSettings *
ide_layered_settings_get_primary_settings (IdeLayeredSettings *self)
{
  g_assert (IDE_IS_LAYERED_SETTINGS (self));

  if (self->settings->len == 0)
    g_error ("No settings have been loaded. Aborting.");

  return g_ptr_array_index (self->settings, 0);
}

static void
ide_layered_settings_cache_key (IdeLayeredSettings *self,
                                const char         *key)
{
  g_autoptr(GVariant) value = NULL;
  GSettings *settings;

  g_assert (IDE_IS_LAYERED_SETTINGS (self));
  g_assert (key != NULL);
  g_assert (self->settings->len > 0);

  for (guint i = 0; i < self->settings->len; i++)
    {
      settings = g_ptr_array_index (self->settings, i);
      value = g_settings_get_user_value (settings, key);

      if (value != NULL)
        {
          g_settings_set_value (self->memory_settings, key, value);
          goto emit_changed;
        }
    }

  settings = g_ptr_array_index (self->settings, 0);
  value = g_settings_get_value (settings, key);
  g_settings_set_value (self->memory_settings, key, value);

emit_changed:
  g_signal_emit (self, signals[CHANGED], g_quark_from_string (key), key);
}

static void
ide_layered_settings_update_cache (IdeLayeredSettings *self)
{
  GSettingsSchemaSource *source;
  g_autoptr(GSettingsSchema) schema = NULL;
  g_auto(GStrv) keys = NULL;

  g_assert (IDE_IS_LAYERED_SETTINGS (self));

  source = g_settings_schema_source_get_default ();
  schema = g_settings_schema_source_lookup (source, self->schema_id, TRUE);

  if (schema == NULL)
    g_error ("Failed to locate schema: %s", self->schema_id);

  if ((keys = g_settings_schema_list_keys (schema)))
    {
      for (guint i = 0; keys[i]; i++)
        ide_layered_settings_cache_key (self, keys [i]);
    }
}

static void
ide_layered_settings_settings_changed_cb (IdeLayeredSettings *self,
                                          const char         *key,
                                          GSettings          *settings)
{
  g_assert (IDE_IS_LAYERED_SETTINGS (self));
  g_assert (key != NULL);
  g_assert (G_IS_SETTINGS (settings));

  ide_layered_settings_cache_key (self, key);
}

static void
ide_layered_settings_constructed (GObject *object)
{
  IdeLayeredSettings *self = (IdeLayeredSettings *)object;

  g_assert (IDE_IS_LAYERED_SETTINGS (self));
  g_assert (self->schema_id != NULL);
  g_assert (self->path != NULL);

  self->memory_settings = g_settings_new_with_backend_and_path (self->schema_id,
                                                                memory_backend,
                                                                self->path);

  G_OBJECT_CLASS (ide_layered_settings_parent_class)->constructed (object);
}

static void
ide_layered_settings_finalize (GObject *object)
{
  IdeLayeredSettings *self = (IdeLayeredSettings *)object;

  g_clear_pointer (&self->settings, g_ptr_array_unref);
  g_clear_pointer (&self->schema_id, g_free);
  g_clear_pointer (&self->path, g_free);

  g_clear_object (&self->memory_settings);

  G_OBJECT_CLASS (ide_layered_settings_parent_class)->finalize (object);
}

static void
ide_layered_settings_get_property (GObject    *object,
                                   guint       prop_id,
                                   GValue     *value,
                                   GParamSpec *pspec)
{
  IdeLayeredSettings *self = IDE_LAYERED_SETTINGS (object);

  switch (prop_id)
    {
    case PROP_SCHEMA_ID:
      g_value_set_string (value, self->schema_id);
      break;

    case PROP_PATH:
      g_value_set_string (value, self->path);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
ide_layered_settings_set_property (GObject      *object,
                                   guint         prop_id,
                                   const GValue *value,
                                   GParamSpec   *pspec)
{
  IdeLayeredSettings *self = IDE_LAYERED_SETTINGS (object);

  switch (prop_id)
    {
    case PROP_SCHEMA_ID:
      self->schema_id = g_value_dup_string (value);
      break;

    case PROP_PATH:
      self->path = g_value_dup_string (value);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
ide_layered_settings_class_init (IdeLayeredSettingsClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->constructed = ide_layered_settings_constructed;
  object_class->finalize = ide_layered_settings_finalize;
  object_class->get_property = ide_layered_settings_get_property;
  object_class->set_property = ide_layered_settings_set_property;

  properties [PROP_SCHEMA_ID] =
    g_param_spec_string ("schema-id",
                         "Schema Id",
                         "Schema Id",
                         NULL,
                         (G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS));

  properties [PROP_PATH] =
    g_param_spec_string ("path",
                         "Settings Path",
                         "Settings Path",
                         NULL,
                         (G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, N_PROPS, properties);

  signals [CHANGED] =
    g_signal_new ("changed",
                  G_TYPE_FROM_CLASS (klass),
                  G_SIGNAL_RUN_LAST | G_SIGNAL_DETAILED,
                  0,
                  NULL, NULL,
                  ide_marshal_VOID__STRING,
                  G_TYPE_NONE,
                  1,
                  G_TYPE_STRING | G_SIGNAL_TYPE_STATIC_SCOPE);
  g_signal_set_va_marshaller (signals [CHANGED],
                              G_TYPE_FROM_CLASS (klass),
                              ide_marshal_VOID__STRINGv);
}

static void
ide_layered_settings_init (IdeLayeredSettings *self)
{
  if G_UNLIKELY (memory_backend == NULL)
    memory_backend = g_memory_settings_backend_new ();

  self->settings = g_ptr_array_new_with_free_func (g_object_unref);
}

IdeLayeredSettings *
ide_layered_settings_new (const char *schema_id,
                          const char *path)
{
  g_return_val_if_fail (schema_id != NULL, NULL);
  g_return_val_if_fail (path != NULL, NULL);

  return g_object_new (IDE_TYPE_LAYERED_SETTINGS,
                       "schema-id", schema_id,
                       "path", path,
                       NULL);
}

GVariant *
ide_layered_settings_get_default_value (IdeLayeredSettings *self,
                                        const char         *key)
{
  g_return_val_if_fail (IDE_IS_LAYERED_SETTINGS (self), NULL);
  g_return_val_if_fail (key != NULL, NULL);

  return g_settings_get_default_value (ide_layered_settings_get_primary_settings (self), key);
}

GVariant *
ide_layered_settings_get_user_value (IdeLayeredSettings *self,
                                     const char         *key)
{
  g_return_val_if_fail (IDE_IS_LAYERED_SETTINGS (self), NULL);
  g_return_val_if_fail (self->settings != NULL, NULL);
  g_return_val_if_fail (key != NULL, NULL);

  for (guint i = 0; i < self->settings->len; i++)
    {
      GSettings *settings = g_ptr_array_index (self->settings, i);
      g_autoptr(GVariant) value = g_settings_get_user_value (settings, key);

      if (value != NULL)
        return g_steal_pointer (&value);
    }

  return NULL;
}

/**
 * ide_layered_settings_get_value:
 * @self: a #IdeLayeredSettings
 *
 * Gets the value of @key from the first layer that is modified.
 *
 * Returns: (transfer full): a #GVariant
 */
GVariant *
ide_layered_settings_get_value (IdeLayeredSettings *self,
                                const char         *key)
{
  g_return_val_if_fail (IDE_IS_LAYERED_SETTINGS (self), NULL);
  g_return_val_if_fail (key != NULL, NULL);

  for (guint i = 0; i < self->settings->len; i++)
    {
      GSettings *settings = g_ptr_array_index (self->settings, i);
      g_autoptr(GVariant) value = g_settings_get_user_value (settings, key);

      if (value != NULL)
        return g_steal_pointer (&value);
    }

  return g_settings_get_value (ide_layered_settings_get_primary_settings (self), key);
}

void
ide_layered_settings_set_value (IdeLayeredSettings *self,
                                const char         *key,
                                GVariant           *value)
{
  g_return_if_fail (IDE_IS_LAYERED_SETTINGS (self));
  g_return_if_fail (key != NULL);

  g_settings_set_value (ide_layered_settings_get_primary_settings (self), key, value);

  ide_layered_settings_cache_key (self, key);
}

#define DEFINE_GETTER(name, ret_type, func, ...)                        \
ret_type                                                                \
ide_layered_settings_get_##name (IdeLayeredSettings *self,              \
                                 const char         *key)               \
{                                                                       \
  GVariant *value;                                                      \
  ret_type ret;                                                         \
                                                                        \
  g_return_val_if_fail (IDE_IS_LAYERED_SETTINGS (self), (ret_type)0);   \
  g_return_val_if_fail (key != NULL, (ret_type)0);                      \
                                                                        \
  value = ide_layered_settings_get_value (self, key);                   \
  ret = g_variant_##func (value, ##__VA_ARGS__);                        \
  g_variant_unref (value);                                              \
                                                                        \
  return ret;                                                           \
}

DEFINE_GETTER (boolean, gboolean, get_boolean)
DEFINE_GETTER (double,  double,   get_double)
DEFINE_GETTER (int,     int,      get_int32)
DEFINE_GETTER (string,  char *,   dup_string, NULL)
DEFINE_GETTER (uint,    guint,    get_uint32)

#define DEFINE_SETTER(name, param_type, func)                           \
void                                                                    \
ide_layered_settings_set_##name (IdeLayeredSettings *self,              \
                                 const char        *key,                \
                                 param_type         val)                \
{                                                                       \
  GVariant *value;                                                      \
                                                                        \
  g_return_if_fail (IDE_IS_LAYERED_SETTINGS (self));                    \
  g_return_if_fail (key != NULL);                                       \
                                                                        \
  value = g_variant_##func (val);                                       \
  ide_layered_settings_set_value (self, key, value);                    \
}

DEFINE_SETTER (boolean, gboolean,      new_boolean)
DEFINE_SETTER (double,  double,        new_double)
DEFINE_SETTER (int,     int,           new_int32)
DEFINE_SETTER (string,  const char *,  new_string)
DEFINE_SETTER (uint,    guint,         new_uint32)

void
ide_layered_settings_append (IdeLayeredSettings *self,
                             GSettings          *settings)
{
  g_auto(GStrv) keys = NULL;

  g_return_if_fail (IDE_IS_LAYERED_SETTINGS (self));
  g_return_if_fail (G_IS_SETTINGS (settings));

  g_ptr_array_add (self->settings, g_object_ref (settings));

  /* Query all keys to ensure we get change notifications */
  keys = ide_layered_settings_list_keys (self);
  for (guint i = 0; keys[i]; i++)
    {
      GVariant *value = g_settings_get_value (settings, keys[i]);
      g_variant_unref (value);
    }

  g_signal_connect_object (settings,
                           "changed",
                           G_CALLBACK (ide_layered_settings_settings_changed_cb),
                           self,
                           G_CONNECT_SWAPPED);

  ide_layered_settings_update_cache (self);
}

static gboolean
get_inverted_boolean (GValue   *value,
                      GVariant *variant,
                      gpointer  user_data)
{
  if (g_variant_is_of_type (variant, G_VARIANT_TYPE_BOOLEAN))
    {
      g_value_set_boolean (value, !g_variant_get_boolean (variant));
      return TRUE;
    }

  return FALSE;
}

static GVariant *
set_inverted_boolean (const GValue       *value,
                      const GVariantType *type,
                      gpointer            user_data)
{
  if (G_VALUE_HOLDS (value, G_TYPE_BOOLEAN) &&
      g_variant_type_equal (type, G_VARIANT_TYPE_BOOLEAN))
    return g_variant_new_boolean (!g_value_get_boolean (value));

  return NULL;
}

void
ide_layered_settings_bind (IdeLayeredSettings *self,
                           const char         *key,
                           gpointer            object,
                           const char         *property,
                           GSettingsBindFlags  flags)
{
  GSettingsBindGetMapping get_mapping = NULL;
  GSettingsBindSetMapping set_mapping = NULL;

  g_return_if_fail (IDE_IS_LAYERED_SETTINGS (self));
  g_return_if_fail (key != NULL);
  g_return_if_fail (G_IS_OBJECT (object));
  g_return_if_fail (property != NULL);

  if (flags & G_SETTINGS_BIND_INVERT_BOOLEAN)
    {
      flags &= ~G_SETTINGS_BIND_INVERT_BOOLEAN;
      get_mapping = get_inverted_boolean;
      set_mapping = set_inverted_boolean;
    }

  ide_layered_settings_bind_with_mapping (self, key, object, property, flags,
                                          get_mapping, set_mapping, NULL, NULL);
}

/**
 * ide_layered_settings_bind_with_mapping:
 * @self: An #IdeLayeredSettings.
 * @key: the settings key to bind.
 * @object (type GObject.Object): the target object.
 * @property: the property on @object to apply.
 * @flags: flags for the binding.
 * @get_mapping: (scope notified) (closure user_data) (destroy destroy): the get mapping function
 * @set_mapping: (scope notified) (closure user_data) (destroy destroy): the set mapping function
 * @user_data: user data for @get_mapping and @set_mapping.
 * @destroy: destroy notify for @user_data.
 *
 * Creates a new binding similar to g_settings_bind_with_mapping() but applying
 * from the resolved value via the layered settings.
 */
void
ide_layered_settings_bind_with_mapping (IdeLayeredSettings      *self,
                                        const char              *key,
                                        gpointer                 object,
                                        const char              *property,
                                        GSettingsBindFlags       flags,
                                        GSettingsBindGetMapping  get_mapping,
                                        GSettingsBindSetMapping  set_mapping,
                                        gpointer                 user_data,
                                        GDestroyNotify           destroy)
{
  static const GSettingsBindFlags default_flags = G_SETTINGS_BIND_GET|G_SETTINGS_BIND_SET;
  GDestroyNotify get_destroy = destroy;
  GDestroyNotify set_destroy = destroy;

  g_return_if_fail (IDE_IS_LAYERED_SETTINGS (self));
  g_return_if_fail (key != NULL);
  g_return_if_fail (G_IS_OBJECT (object));
  g_return_if_fail (property != NULL);

  /* Make sure we have GET|SET flags if DEFAULT was specified */
  if ((flags & default_flags) == 0)
    flags |= default_flags;

  /* Ensure @destroy is only called once, on the longer living
   * setting binding (potential for longer living that is).
   */
  if ((flags & G_SETTINGS_BIND_SET) != 0)
    get_destroy = NULL;
  else
    set_destroy = NULL;

  /*
   * Our memory backend/settings are compiling the values from all of the
   * layers. Therefore, we only want to map reads from the memory backend. We
   * want to direct all writes to the topmost layer (found at index 0).
   */
  if ((flags & G_SETTINGS_BIND_GET) != 0)
    g_settings_bind_with_mapping (self->memory_settings, key, object, property,
                                  (flags & ~G_SETTINGS_BIND_SET),
                                  get_mapping, NULL, user_data, get_destroy);

  /* We bind writability directly to our toplevel layer. */
  if ((flags & G_SETTINGS_BIND_SET) != 0)
    g_settings_bind_with_mapping (ide_layered_settings_get_primary_settings (self),
                                  key, object, property, (flags & ~G_SETTINGS_BIND_GET),
                                  NULL, set_mapping, user_data, set_destroy);
}

void
ide_layered_settings_unbind (IdeLayeredSettings *self,
                             const char         *property)
{
  g_return_if_fail (IDE_IS_LAYERED_SETTINGS (self));
  g_return_if_fail (property != NULL);

  g_settings_unbind (ide_layered_settings_get_primary_settings (self), property);
  g_settings_unbind (memory_backend, property);
}

/**
 * ide_layered_settings_get_key:
 * @self: a #IdeLayeredSettings
 * @key: the name of the setting
 *
 * Gets the #GSettingsSchemaKey denoted by @key.
 *
 * It is a programming error to call this with a key that does not exist.
 *
 * Returns: (transfer full): a #GSettingsSchemaKey
 */
GSettingsSchemaKey *
ide_layered_settings_get_key (IdeLayeredSettings *self,
                              const char         *key)
{
  g_autoptr(GSettingsSchema) schema = NULL;
  GSettingsSchemaKey *ret;

  g_return_val_if_fail (IDE_IS_LAYERED_SETTINGS (self), NULL);
  g_return_val_if_fail (key != NULL, NULL);

  g_object_get (self->memory_settings,
                "settings-schema", &schema,
                NULL);
  g_assert (schema != NULL);

  ret = g_settings_schema_get_key (schema, key);
  g_assert (ret != NULL);

  return ret;
}

/**
 * ide_layered_settings_list_keys:
 * @self: a #IdeLayeredSettings
 *
 * Lists the available keys.
 *
 * Returns: (transfer full) (array zero-terminated=1) (element-type utf8):
 *   an array of keys that can be retrieved from the #IdeLayeredSettings.
 */
char **
ide_layered_settings_list_keys (IdeLayeredSettings *self)
{
  g_autoptr(GSettingsSchema) schema = NULL;

  g_return_val_if_fail (IDE_IS_LAYERED_SETTINGS (self), NULL);

  g_object_get (self->memory_settings,
                "settings-schema", &schema,
                NULL);

  return g_settings_schema_list_keys (schema);
}
