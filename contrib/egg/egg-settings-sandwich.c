/* egg-settings-sandwich.c
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

#define G_LOG_DOMAIN "egg-settings-sandwich"
#define G_SETTINGS_ENABLE_BACKEND

#include <gio/gsettingsbackend.h>
#include <glib/gi18n.h>

#include "egg-settings-sandwich.h"

struct _EggSettingsSandwich
{
  GObject           parent_instance;
  GPtrArray        *settings;
  GSettingsBackend *memory_backend;
  GSettings        *memory_settings;
  gchar            *schema_id;
  gchar            *path;
};

G_DEFINE_TYPE (EggSettingsSandwich, egg_settings_sandwich, G_TYPE_OBJECT)

enum {
  PROP_0,
  PROP_PATH,
  PROP_SCHEMA_ID,
  LAST_PROP
};

static GParamSpec *gParamSpecs [LAST_PROP];

static GSettings *
egg_settings_sandwich_get_primary_settings (EggSettingsSandwich *self)
{
  g_assert (EGG_IS_SETTINGS_SANDWICH (self));

  if (self->settings->len == 0)
    {
      g_error ("No settings have been loaded. Aborting.");
      g_assert_not_reached ();
      return NULL;
    }

  return g_ptr_array_index (self->settings, 0);
}

static void
egg_settings_sandwich_cache_key (EggSettingsSandwich *self,
                                 const gchar         *key)
{
  GSettings *settings;
  GVariant *value;
  gsize i;

  g_assert (EGG_IS_SETTINGS_SANDWICH (self));
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
egg_settings_sandwich_update_cache (EggSettingsSandwich *self)
{
  GSettingsSchemaSource *source;
  GSettingsSchema *schema;
  gchar **keys;
  gsize i;

  g_assert (EGG_IS_SETTINGS_SANDWICH (self));

  source = g_settings_schema_source_get_default ();
  schema = g_settings_schema_source_lookup (source, self->schema_id, TRUE);

  if (schema == NULL)
    {
      g_error ("Failed to locate schema: %s", self->schema_id);
      return;
    }

  keys = g_settings_schema_list_keys (schema);

  for (i = 0; keys [i]; i++)
    egg_settings_sandwich_cache_key (self, keys [i]);

  g_settings_schema_unref (schema);
  g_strfreev (keys);
}

static void
egg_settings_sandwich__settings_changed (EggSettingsSandwich *self,
                                         const gchar         *key,
                                         GSettings           *settings)
{
  g_assert (EGG_IS_SETTINGS_SANDWICH (self));
  g_assert (key != NULL);
  g_assert (G_IS_SETTINGS (settings));

  egg_settings_sandwich_cache_key (self, key);
}

static void
egg_settings_sandwich_constructed (GObject *object)
{
  EggSettingsSandwich *self = (EggSettingsSandwich *)object;

  g_assert (EGG_IS_SETTINGS_SANDWICH (self));
  g_assert (self->schema_id != NULL);
  g_assert (self->path != NULL);

  self->memory_settings = g_settings_new_with_backend_and_path (self->schema_id,
                                                                self->memory_backend,
                                                                self->path);

  G_OBJECT_CLASS (egg_settings_sandwich_parent_class)->constructed (object);
}

static void
egg_settings_sandwich_finalize (GObject *object)
{
  EggSettingsSandwich *self = (EggSettingsSandwich *)object;

  g_clear_pointer (&self->settings, g_ptr_array_unref);
  g_clear_pointer (&self->schema_id, g_free);
  g_clear_pointer (&self->path, g_free);
  g_clear_object (&self->memory_backend);

  G_OBJECT_CLASS (egg_settings_sandwich_parent_class)->finalize (object);
}

static void
egg_settings_sandwich_get_property (GObject    *object,
                                    guint       prop_id,
                                    GValue     *value,
                                    GParamSpec *pspec)
{
  EggSettingsSandwich *self = EGG_SETTINGS_SANDWICH (object);

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
egg_settings_sandwich_set_property (GObject      *object,
                                    guint         prop_id,
                                    const GValue *value,
                                    GParamSpec   *pspec)
{
  EggSettingsSandwich *self = EGG_SETTINGS_SANDWICH (object);

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
egg_settings_sandwich_class_init (EggSettingsSandwichClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->constructed = egg_settings_sandwich_constructed;
  object_class->finalize = egg_settings_sandwich_finalize;
  object_class->get_property = egg_settings_sandwich_get_property;
  object_class->set_property = egg_settings_sandwich_set_property;

  gParamSpecs [PROP_SCHEMA_ID] =
    g_param_spec_string ("schema-id",
                         _("Schema Id"),
                         _("Schema Id"),
                         NULL,
                         (G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS));

  gParamSpecs [PROP_PATH] =
    g_param_spec_string ("path",
                         _("Settings Path"),
                         _("Settings Path"),
                         NULL,
                         (G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, LAST_PROP, gParamSpecs);
}

static void
egg_settings_sandwich_init (EggSettingsSandwich *self)
{
  self->settings = g_ptr_array_new_with_free_func (g_object_unref);
  self->memory_backend = g_memory_settings_backend_new ();
}

EggSettingsSandwich *
egg_settings_sandwich_new (const gchar *schema_id,
                           const gchar *path)
{
  g_return_val_if_fail (schema_id != NULL, NULL);
  g_return_val_if_fail (path != NULL, NULL);

  return g_object_new (EGG_TYPE_SETTINGS_SANDWICH,
                       "schema-id", schema_id,
                       "path", path,
                       NULL);
}

GVariant *
egg_settings_sandwich_get_default_value (EggSettingsSandwich *self,
                                         const gchar         *key)
{
  GSettings *settings;
  GVariant *ret;

  g_return_val_if_fail (EGG_IS_SETTINGS_SANDWICH (self), NULL);
  g_return_val_if_fail (key != NULL, NULL);

  settings = egg_settings_sandwich_get_primary_settings (self);
  ret = g_settings_get_default_value (settings, key);

  return ret;
}

GVariant *
egg_settings_sandwich_get_user_value (EggSettingsSandwich *self,
                                      const gchar         *key)
{
  gsize i;

  g_return_val_if_fail (EGG_IS_SETTINGS_SANDWICH (self), NULL);
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
egg_settings_sandwich_get_value (EggSettingsSandwich *self,
                                 const gchar         *key)
{
  GSettings *settings;
  GVariant *ret;
  gsize i;

  g_return_val_if_fail (EGG_IS_SETTINGS_SANDWICH (self), NULL);
  g_return_val_if_fail (key != NULL, NULL);


  for (i = 0; i < self->settings->len; i++)
    {
      settings = g_ptr_array_index (self->settings, i);
      ret = g_settings_get_user_value (settings, key);
      if (ret != NULL)
        return ret;
    }

  settings = egg_settings_sandwich_get_primary_settings (self);
  ret = g_settings_get_value (settings, key);

  return ret;
}

void
egg_settings_sandwich_set_value (EggSettingsSandwich *self,
                                 const gchar         *key,
                                 GVariant            *value)
{
  GSettings *settings;

  g_return_if_fail (EGG_IS_SETTINGS_SANDWICH (self));
  g_return_if_fail (key != NULL);

  settings = egg_settings_sandwich_get_primary_settings (self);
  g_settings_set_value (settings, key, value);
}

#define DEFINE_GETTER(name, ret_type, func, ...)                        \
ret_type                                                                \
egg_settings_sandwich_get_##name (EggSettingsSandwich *self,            \
                                  const gchar         *key)             \
{                                                                       \
  GVariant *value;                                                      \
  ret_type ret;                                                         \
                                                                        \
  g_return_val_if_fail (EGG_IS_SETTINGS_SANDWICH (self), (ret_type)0);  \
  g_return_val_if_fail (key != NULL, (ret_type)0);                      \
                                                                        \
  value = egg_settings_sandwich_get_value (self, key);                  \
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
egg_settings_sandwich_set_##name (EggSettingsSandwich *self,            \
                                  const gchar         *key,             \
                                  param_type           val)             \
{                                                                       \
  GVariant *value;                                                      \
                                                                        \
  g_return_if_fail (EGG_IS_SETTINGS_SANDWICH (self));                   \
  g_return_if_fail (key != NULL);                                       \
                                                                        \
  value = g_variant_##func (val);                                       \
  egg_settings_sandwich_set_value (self, key, value);                   \
}

DEFINE_SETTER (boolean, gboolean,      new_boolean)
DEFINE_SETTER (double,  gdouble,       new_double)
DEFINE_SETTER (int,     gint,          new_int32)
DEFINE_SETTER (string,  const gchar *, new_string)
DEFINE_SETTER (uint,    guint,         new_uint32)

void
egg_settings_sandwich_append (EggSettingsSandwich *self,
                              GSettings           *settings)
{
  g_return_if_fail (EGG_IS_SETTINGS_SANDWICH (self));
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
                           G_CALLBACK (egg_settings_sandwich__settings_changed),
                           self,
                           G_CONNECT_SWAPPED);

  egg_settings_sandwich_update_cache (self);
}

void
egg_settings_sandwich_bind (EggSettingsSandwich *self,
                            const gchar         *key,
                            gpointer             object,
                            const gchar         *property,
                            GSettingsBindFlags   flags)
{
  g_return_if_fail (EGG_IS_SETTINGS_SANDWICH (self));
  g_return_if_fail (key != NULL);
  g_return_if_fail (G_IS_OBJECT (object));
  g_return_if_fail (property != NULL);

  egg_settings_sandwich_bind_with_mapping (self, key, object, property, flags,
                                           NULL, NULL, NULL, NULL);
}

void
egg_settings_sandwich_bind_with_mapping (EggSettingsSandwich     *self,
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

  g_return_if_fail (EGG_IS_SETTINGS_SANDWICH (self));
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
  settings = egg_settings_sandwich_get_primary_settings (self);
  if ((flags & G_SETTINGS_BIND_SET) != 0)
    g_settings_bind_with_mapping (settings, key, object, property, (flags & ~G_SETTINGS_BIND_GET),
                                  get_mapping, set_mapping, user_data, destroy);
}

void
egg_settings_sandwich_unbind (EggSettingsSandwich *self,
                              const gchar         *property)
{
  GSettings *settings;

  g_return_if_fail (EGG_IS_SETTINGS_SANDWICH (self));
  g_return_if_fail (property != NULL);

  settings = egg_settings_sandwich_get_primary_settings (self);

  g_settings_unbind (settings, property);
  g_settings_unbind (self->memory_backend, property);
}
