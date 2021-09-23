/* ide-settings-sandwich.c
 *
 * Copyright (C) 2015 Christian Hergert <christian@hergert.me>
 *
 * This file is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as
 * published by the Free Software Foundation; either version 3 of the
 * License, or (at your option) any later version.
 *
 * This file is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#define G_LOG_DOMAIN "ide-settings-sandwich"
#define G_SETTINGS_ENABLE_BACKEND

#include "config.h"

#include <gio/gsettingsbackend.h>
#include <glib/gi18n.h>

#include "ide-settings-sandwich-private.h"

struct _IdeSettingsSandwich
{
  GObject           parent_instance;
  GPtrArray        *settings;
  GSettingsBackend *memory_backend;
  GSettings        *memory_settings;
  gchar            *schema_id;
  gchar            *path;
};

G_DEFINE_TYPE (IdeSettingsSandwich, ide_settings_sandwich, G_TYPE_OBJECT)

enum {
  PROP_0,
  PROP_PATH,
  PROP_SCHEMA_ID,
  LAST_PROP
};

static GParamSpec *properties [LAST_PROP];

static GSettings *
ide_settings_sandwich_get_primary_settings (IdeSettingsSandwich *self)
{
  g_assert (IDE_IS_SETTINGS_SANDWICH (self));

  if (self->settings->len == 0)
    {
      g_error ("No settings have been loaded. Aborting.");
      g_assert_not_reached ();
      return NULL;
    }

  return g_ptr_array_index (self->settings, 0);
}

static void
ide_settings_sandwich_cache_key (IdeSettingsSandwich *self,
                                 const gchar         *key)
{
  GSettings *settings;
  g_autoptr(GVariant) value = NULL;
  gsize i;

  g_assert (IDE_IS_SETTINGS_SANDWICH (self));
  g_assert (key != NULL);
  g_assert (self->settings->len > 0);

  for (i = 0; i < self->settings->len; i++)
    {
      settings = g_ptr_array_index (self->settings, i);
      value = g_settings_get_user_value (settings, key);

      if (value != NULL)
        {
          g_settings_set_value (self->memory_settings, key, value);
          return;
        }
    }

  settings = g_ptr_array_index (self->settings, 0);
  value = g_settings_get_value (settings, key);
  g_settings_set_value (self->memory_settings, key, value);
}

static void
ide_settings_sandwich_update_cache (IdeSettingsSandwich *self)
{
  GSettingsSchemaSource *source;
  GSettingsSchema *schema;
  gchar **keys;
  gsize i;

  g_assert (IDE_IS_SETTINGS_SANDWICH (self));

  source = g_settings_schema_source_get_default ();
  schema = g_settings_schema_source_lookup (source, self->schema_id, TRUE);

  if (schema == NULL)
    {
      g_error ("Failed to locate schema: %s", self->schema_id);
      return;
    }

  keys = g_settings_schema_list_keys (schema);

  for (i = 0; keys [i]; i++)
    ide_settings_sandwich_cache_key (self, keys [i]);

  g_settings_schema_unref (schema);
  g_strfreev (keys);
}

static void
ide_settings_sandwich__settings_changed (IdeSettingsSandwich *self,
                                         const gchar         *key,
                                         GSettings           *settings)
{
  g_assert (IDE_IS_SETTINGS_SANDWICH (self));
  g_assert (key != NULL);
  g_assert (G_IS_SETTINGS (settings));

  ide_settings_sandwich_cache_key (self, key);
}

static void
ide_settings_sandwich_constructed (GObject *object)
{
  IdeSettingsSandwich *self = (IdeSettingsSandwich *)object;

  g_assert (IDE_IS_SETTINGS_SANDWICH (self));
  g_assert (self->schema_id != NULL);
  g_assert (self->path != NULL);

  self->memory_settings = g_settings_new_with_backend_and_path (self->schema_id,
                                                                self->memory_backend,
                                                                self->path);

  G_OBJECT_CLASS (ide_settings_sandwich_parent_class)->constructed (object);
}

static void
ide_settings_sandwich_finalize (GObject *object)
{
  IdeSettingsSandwich *self = (IdeSettingsSandwich *)object;

  g_clear_pointer (&self->settings, g_ptr_array_unref);
  g_clear_pointer (&self->schema_id, g_free);
  g_clear_pointer (&self->path, g_free);
  g_clear_object (&self->memory_backend);

  G_OBJECT_CLASS (ide_settings_sandwich_parent_class)->finalize (object);
}

static void
ide_settings_sandwich_get_property (GObject    *object,
                                    guint       prop_id,
                                    GValue     *value,
                                    GParamSpec *pspec)
{
  IdeSettingsSandwich *self = IDE_SETTINGS_SANDWICH (object);

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
ide_settings_sandwich_set_property (GObject      *object,
                                    guint         prop_id,
                                    const GValue *value,
                                    GParamSpec   *pspec)
{
  IdeSettingsSandwich *self = IDE_SETTINGS_SANDWICH (object);

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
ide_settings_sandwich_class_init (IdeSettingsSandwichClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->constructed = ide_settings_sandwich_constructed;
  object_class->finalize = ide_settings_sandwich_finalize;
  object_class->get_property = ide_settings_sandwich_get_property;
  object_class->set_property = ide_settings_sandwich_set_property;

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

  g_object_class_install_properties (object_class, LAST_PROP, properties);
}

static void
ide_settings_sandwich_init (IdeSettingsSandwich *self)
{
  self->settings = g_ptr_array_new_with_free_func (g_object_unref);
  self->memory_backend = g_memory_settings_backend_new ();
}

IdeSettingsSandwich *
ide_settings_sandwich_new (const gchar *schema_id,
                           const gchar *path)
{
  g_return_val_if_fail (schema_id != NULL, NULL);
  g_return_val_if_fail (path != NULL, NULL);

  return g_object_new (IDE_TYPE_SETTINGS_SANDWICH,
                       "schema-id", schema_id,
                       "path", path,
                       NULL);
}

GVariant *
ide_settings_sandwich_get_default_value (IdeSettingsSandwich *self,
                                         const gchar         *key)
{
  GSettings *settings;
  GVariant *ret;

  g_return_val_if_fail (IDE_IS_SETTINGS_SANDWICH (self), NULL);
  g_return_val_if_fail (key != NULL, NULL);

  settings = ide_settings_sandwich_get_primary_settings (self);
  ret = g_settings_get_default_value (settings, key);

  return ret;
}

GVariant *
ide_settings_sandwich_get_user_value (IdeSettingsSandwich *self,
                                      const gchar         *key)
{
  gsize i;

  g_return_val_if_fail (IDE_IS_SETTINGS_SANDWICH (self), NULL);
  g_return_val_if_fail (key != NULL, NULL);

  for (i = 0; i < self->settings->len; i++)
    {
      GSettings *settings;
      GVariant *value;

      settings = g_ptr_array_index (self->settings, i);
      value = g_settings_get_user_value (settings, key);
      if (value != NULL)
        return value;
    }

  return NULL;
}

GVariant *
ide_settings_sandwich_get_value (IdeSettingsSandwich *self,
                                 const gchar         *key)
{
  GSettings *settings;
  GVariant *ret;
  gsize i;

  g_return_val_if_fail (IDE_IS_SETTINGS_SANDWICH (self), NULL);
  g_return_val_if_fail (key != NULL, NULL);


  for (i = 0; i < self->settings->len; i++)
    {
      settings = g_ptr_array_index (self->settings, i);
      ret = g_settings_get_user_value (settings, key);
      if (ret != NULL)
        return ret;
    }

  settings = ide_settings_sandwich_get_primary_settings (self);
  ret = g_settings_get_value (settings, key);

  return ret;
}

void
ide_settings_sandwich_set_value (IdeSettingsSandwich *self,
                                 const gchar         *key,
                                 GVariant            *value)
{
  GSettings *settings;

  g_return_if_fail (IDE_IS_SETTINGS_SANDWICH (self));
  g_return_if_fail (key != NULL);

  settings = ide_settings_sandwich_get_primary_settings (self);
  g_settings_set_value (settings, key, value);
}

#define DEFINE_GETTER(name, ret_type, func, ...)                        \
ret_type                                                                \
ide_settings_sandwich_get_##name (IdeSettingsSandwich *self,            \
                                  const gchar         *key)             \
{                                                                       \
  GVariant *value;                                                      \
  ret_type ret;                                                         \
                                                                        \
  g_return_val_if_fail (IDE_IS_SETTINGS_SANDWICH (self), (ret_type)0);  \
  g_return_val_if_fail (key != NULL, (ret_type)0);                      \
                                                                        \
  value = ide_settings_sandwich_get_value (self, key);                  \
  ret = g_variant_##func (value, ##__VA_ARGS__);                        \
  g_variant_unref (value);                                              \
                                                                        \
  return ret;                                                           \
}

DEFINE_GETTER (boolean, gboolean, get_boolean)
DEFINE_GETTER (double,  gdouble,  get_double)
DEFINE_GETTER (int,     gint,     get_int32)
DEFINE_GETTER (string,  gchar *,  dup_string, NULL)
DEFINE_GETTER (uint,    guint,    get_uint32)

#define DEFINE_SETTER(name, param_type, func)                           \
void                                                                    \
ide_settings_sandwich_set_##name (IdeSettingsSandwich *self,            \
                                  const gchar         *key,             \
                                  param_type           val)             \
{                                                                       \
  GVariant *value;                                                      \
                                                                        \
  g_return_if_fail (IDE_IS_SETTINGS_SANDWICH (self));                   \
  g_return_if_fail (key != NULL);                                       \
                                                                        \
  value = g_variant_##func (val);                                       \
  ide_settings_sandwich_set_value (self, key, value);                   \
}

DEFINE_SETTER (boolean, gboolean,      new_boolean)
DEFINE_SETTER (double,  gdouble,       new_double)
DEFINE_SETTER (int,     gint,          new_int32)
DEFINE_SETTER (string,  const gchar *, new_string)
DEFINE_SETTER (uint,    guint,         new_uint32)

void
ide_settings_sandwich_append (IdeSettingsSandwich *self,
                              GSettings           *settings)
{
  g_return_if_fail (IDE_IS_SETTINGS_SANDWICH (self));
  g_return_if_fail (G_IS_SETTINGS (settings));

  g_ptr_array_add (self->settings, g_object_ref (settings));

#if 0
  {
    g_autofree gchar *schema_id = NULL;
    g_autofree gchar *path = NULL;

    g_object_get (settings,
                  "schema-id", &schema_id,
                  "path", &path,
                  NULL);
  }
#endif

  g_signal_connect_object (settings,
                           "changed",
                           G_CALLBACK (ide_settings_sandwich__settings_changed),
                           self,
                           G_CONNECT_SWAPPED);

  ide_settings_sandwich_update_cache (self);
}

void
ide_settings_sandwich_bind (IdeSettingsSandwich *self,
                            const gchar         *key,
                            gpointer             object,
                            const gchar         *property,
                            GSettingsBindFlags   flags)
{
  g_return_if_fail (IDE_IS_SETTINGS_SANDWICH (self));
  g_return_if_fail (key != NULL);
  g_return_if_fail (G_IS_OBJECT (object));
  g_return_if_fail (property != NULL);

  ide_settings_sandwich_bind_with_mapping (self, key, object, property, flags,
                                           NULL, NULL, NULL, NULL);
}

/**
 * ide_settings_sandwich_bind_with_mapping:
 * @self: An #IdeSettingsSandwich.
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
 * from the resolved value via the settings sandwich.
 */
void
ide_settings_sandwich_bind_with_mapping (IdeSettingsSandwich     *self,
                                         const gchar             *key,
                                         gpointer                 object,
                                         const gchar             *property,
                                         GSettingsBindFlags       flags,
                                         GSettingsBindGetMapping  get_mapping,
                                         GSettingsBindSetMapping  set_mapping,
                                         gpointer                 user_data,
                                         GDestroyNotify           destroy)
{
  GSettings *settings;

  g_return_if_fail (IDE_IS_SETTINGS_SANDWICH (self));
  g_return_if_fail (key != NULL);
  g_return_if_fail (G_IS_OBJECT (object));
  g_return_if_fail (property != NULL);

  /*
   * Our memory backend/settings are compiling the values from all of the layers of our
   * sandwich. Therefore, we only want to map reads from the memory backend. We want to direct
   * all writes to the topmost layer of the sandwich (found at index 0).
   */
  if ((flags & G_SETTINGS_BIND_GET) != 0)
    g_settings_bind_with_mapping (self->memory_settings, key, object, property,
                                  (flags & ~G_SETTINGS_BIND_SET),
                                  get_mapping, set_mapping, user_data, destroy);

  /*
   * We bind writability directly to our toplevel layer of the sandwich.
   */
  settings = ide_settings_sandwich_get_primary_settings (self);
  if ((flags & G_SETTINGS_BIND_SET) != 0)
    g_settings_bind_with_mapping (settings, key, object, property, (flags & ~G_SETTINGS_BIND_GET),
                                  get_mapping, set_mapping, user_data, destroy);
}

void
ide_settings_sandwich_unbind (IdeSettingsSandwich *self,
                              const gchar         *property)
{
  GSettings *settings;

  g_return_if_fail (IDE_IS_SETTINGS_SANDWICH (self));
  g_return_if_fail (property != NULL);

  settings = ide_settings_sandwich_get_primary_settings (self);

  g_settings_unbind (settings, property);
  g_settings_unbind (self->memory_backend, property);
}
