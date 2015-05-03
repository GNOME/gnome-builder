/* ide-settings.c
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

#define G_LOG_DOMAIN "ide-settings"

#include <glib/gi18n.h>
#include <stdlib.h>

#include "ide-context.h"
#include "ide-project.h"
#include "ide-settings.h"

/**
 * SECTION:ide-settings
 * @title: IdeSettings
 * @short_description: Project and Application Preferences
 *
 * In Builder, we need support for settings at the user level (their chosen defaults) as well
 * as defaults for a project. #IdeSettings attempts to simplify this by providing a layered
 * approach to settings.
 *
 * If a setting has been set for the current project, it will be returned. If not, the users
 * preference will be returned. Setting a preference via #IdeSettings will always modify the
 * projects setting, not the users default settings.
 */

struct _IdeSettings
{
  IdeObject  parent_instance;

  gchar     *relative_path;
  gchar     *schema_id;

  GSettings *global_settings;
  GSettings *project_settings;
};

G_DEFINE_TYPE (IdeSettings, ide_settings, IDE_TYPE_OBJECT)

enum {
  PROP_0,
  PROP_RELATIVE_PATH,
  PROP_SCHEMA_ID,
  LAST_PROP
};

enum {
  CHANGED,
  LAST_SIGNAL
};

static GParamSpec *gParamSpecs [LAST_PROP];
static guint       gSignals [LAST_SIGNAL];

static void
ide_settings_set_relative_path (IdeSettings *self,
                                const gchar *relative_path)
{
  g_assert (IDE_IS_SETTINGS (self));
  g_assert (relative_path != NULL);

  if (relative_path != self->relative_path)
    {
      g_free (self->relative_path);
      self->relative_path = g_strdup (relative_path);
      g_object_notify_by_pspec (G_OBJECT (self), gParamSpecs [PROP_RELATIVE_PATH]);
    }
}

static void
ide_settings_set_schema_id (IdeSettings *self,
                            const gchar *schema_id)
{
  g_assert (IDE_IS_SETTINGS (self));
  g_assert (schema_id != NULL);

  if (schema_id != self->schema_id)
    {
      g_free (self->schema_id);
      self->schema_id = g_strdup (schema_id);
      g_object_notify_by_pspec (G_OBJECT (self), gParamSpecs [PROP_SCHEMA_ID]);
    }
}

static void
ide_settings__global_settings_changed (IdeSettings *self,
                                       const gchar *key,
                                       GSettings   *global_settings)
{
  g_assert (IDE_IS_SETTINGS (self));
  g_assert (key != NULL);
  g_assert (G_IS_SETTINGS (global_settings));

  g_signal_emit (self, gSignals [CHANGED], g_quark_from_string (key), key);
}

static void
ide_settings__project_settings_changed (IdeSettings *self,
                                        const gchar *key,
                                        GSettings   *project_settings)
{
  g_assert (IDE_IS_SETTINGS (self));
  g_assert (key != NULL);
  g_assert (G_IS_SETTINGS (project_settings));

  g_signal_emit (self, gSignals [CHANGED], g_quark_from_string (key), key);
}

static void
ide_settings_constructed (GObject *object)
{
  IdeSettings *self = (IdeSettings *)object;
  IdeContext *context;
  IdeProject *project;
  const gchar *project_name;
  gchar *path;

  G_OBJECT_CLASS (ide_settings_parent_class)->constructed (object);

  if (self->relative_path == NULL)
    {
      g_autoptr(GSettingsSchema) settings_schema = NULL;
      g_autoptr(GSettings) settings = NULL;
      const gchar *schema_path;

      settings = g_settings_new (self->schema_id);
      g_object_get (settings, "settings-schema", &settings_schema, NULL);
      schema_path = g_settings_schema_get_path (settings_schema);

      if (!g_str_has_prefix (schema_path, "/org/gnome/builder/"))
        {
          g_error ("Settings schema %s is not under path /org/gnome/builder/",
                   self->schema_id);
          abort ();
        }

      self->relative_path = g_strdup (schema_path + strlen ("/org/gnome/builder/"));
    }

  path = g_strdup_printf ("/org/gnome/builder/%s", self->relative_path);
  self->global_settings = g_settings_new_with_path (self->schema_id, path);
  g_free (path);

  context = ide_object_get_context (IDE_OBJECT (self));
  project = ide_context_get_project (context);
  project_name = ide_project_get_name (project);

  path = g_strdup_printf ("/org/gnome/builder/projects/%s/%s", project_name, self->relative_path);
  self->project_settings = g_settings_new_with_path (self->schema_id, path);
  g_free (path);

  g_signal_connect_object (self->global_settings,
                           "changed",
                           G_CALLBACK (ide_settings__global_settings_changed),
                           self,
                           G_CONNECT_SWAPPED);

  g_signal_connect_object (self->project_settings,
                           "changed",
                           G_CALLBACK (ide_settings__project_settings_changed),
                           self,
                           G_CONNECT_SWAPPED);
}

static void
ide_settings_finalize (GObject *object)
{
  IdeSettings *self = (IdeSettings *)object;

  g_clear_pointer (&self->relative_path, g_free);
  g_clear_pointer (&self->schema_id, g_free);

  G_OBJECT_CLASS (ide_settings_parent_class)->finalize (object);
}

static void
ide_settings_get_property (GObject    *object,
                           guint       prop_id,
                           GValue     *value,
                           GParamSpec *pspec)
{
  IdeSettings *self = IDE_SETTINGS (object);

  switch (prop_id)
    {
    case PROP_SCHEMA_ID:
      g_value_set_string (value, ide_settings_get_schema_id (self));
      break;

    case PROP_RELATIVE_PATH:
      g_value_set_string (value, ide_settings_get_relative_path (self));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
ide_settings_set_property (GObject      *object,
                           guint         prop_id,
                           const GValue *value,
                           GParamSpec   *pspec)
{
  IdeSettings *self = IDE_SETTINGS (object);

  switch (prop_id)
    {
    case PROP_SCHEMA_ID:
      ide_settings_set_schema_id (self, g_value_get_object (value));
      break;

    case PROP_RELATIVE_PATH:
      ide_settings_set_relative_path (self, g_value_get_object (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
ide_settings_class_init (IdeSettingsClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->constructed = ide_settings_constructed;
  object_class->finalize = ide_settings_finalize;
  object_class->get_property = ide_settings_get_property;
  object_class->set_property = ide_settings_set_property;

  gParamSpecs [PROP_RELATIVE_PATH] =
    g_param_spec_string ("relative-path",
                         _("Relative Path"),
                         _("Relative Path"),
                         NULL,
                         (G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (object_class, PROP_RELATIVE_PATH,
                                   gParamSpecs [PROP_RELATIVE_PATH]);

  gParamSpecs [PROP_SCHEMA_ID] =
    g_param_spec_string ("schema-id",
                         _("Schema Id"),
                         _("Schema Id"),
                         NULL,
                         (G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (object_class, PROP_SCHEMA_ID, gParamSpecs [PROP_SCHEMA_ID]);

  gSignals [CHANGED] =
    g_signal_new_class_handler ("changed",
                                G_TYPE_FROM_CLASS (klass),
                                G_SIGNAL_RUN_LAST | G_SIGNAL_DETAILED,
                                0,
                                NULL, NULL,
                                g_cclosure_marshal_VOID__STRING,
                                G_TYPE_NONE,
                                1,
                                G_TYPE_STRING);
}

static void
ide_settings_init (IdeSettings *self)
{
}

IdeSettings *
_ide_settings_new (IdeContext  *context,
                   const gchar *schema_id,
                   const gchar *relative_path)
{
  g_assert (IDE_IS_CONTEXT (context));
  g_assert (schema_id != NULL);
  g_assert (relative_path != NULL);

  return g_object_new (IDE_TYPE_SETTINGS,
                       "context", context,
                       "schema-id", schema_id,
                       "relative-path", relative_path,
                       NULL);
}

const gchar *
ide_settings_get_schema_id (IdeSettings *self)
{
  g_return_val_if_fail (IDE_IS_SETTINGS (self), NULL);

  return self->schema_id;
}

const gchar *
ide_settings_get_relative_path (IdeSettings *self)
{
  g_return_val_if_fail (IDE_IS_SETTINGS (self), NULL);

  return self->relative_path;
}

GVariant *
ide_settings_get_value (IdeSettings *self,
                        const gchar *key)
{
  GVariant *ret;

  g_return_val_if_fail (IDE_IS_SETTINGS (self), NULL);
  g_return_val_if_fail (key != NULL, NULL);

  ret = g_settings_get_user_value (self->project_settings, key);
  if (ret == NULL)
    ret = g_settings_get_value (self->global_settings, key);

  return ret;
}

GVariant *
ide_settings_get_user_value (IdeSettings *self,
                             const gchar *key)
{
  GVariant *ret;

  g_return_val_if_fail (IDE_IS_SETTINGS (self), NULL);
  g_return_val_if_fail (key != NULL, NULL);

  ret = g_settings_get_user_value (self->project_settings, key);
  if (ret == NULL)
    ret = g_settings_get_user_value (self->global_settings, key);

  return ret;
}

GVariant *
ide_settings_get_default_value (IdeSettings *self,
                                const gchar *key)
{
  g_return_val_if_fail (IDE_IS_SETTINGS (self), NULL);
  g_return_val_if_fail (key != NULL, NULL);

  return g_settings_get_default_value (self->global_settings, key);
}

#define DEFINE_GETTER(type, name, func_name, ...) \
type \
ide_settings_get_##name (IdeSettings *self, \
                         const gchar *key) \
{ \
  GVariant *value; \
  type ret; \
 \
  g_return_val_if_fail (IDE_IS_SETTINGS (self), (type)0); \
 \
  value = ide_settings_get_value (self, key); \
  ret = g_variant_##func_name (value, ##__VA_ARGS__); \
  g_variant_unref (value); \
 \
  return ret; \
}

DEFINE_GETTER (gboolean, boolean, get_boolean)
DEFINE_GETTER (gint, int, get_int32)
DEFINE_GETTER (guint, uint, get_uint32)
DEFINE_GETTER (double, double, get_double)
DEFINE_GETTER (gchar *, string, dup_string, NULL)

#define DEFINE_SETTER(type, name, new_func, ...) \
void \
ide_settings_set_##name (IdeSettings *self, \
                         const gchar *key, \
                         type value) \
{ \
  GVariant *variant; \
 \
  g_return_if_fail (IDE_IS_SETTINGS (self)); \
 \
  variant = g_variant_##new_func (value, ##__VA_ARGS__); \
  g_settings_set_value (self->project_settings, key, variant); \
}

DEFINE_SETTER (gboolean, boolean, new_boolean)
DEFINE_SETTER (gint, int, new_int32)
DEFINE_SETTER (guint, uint, new_uint32)
DEFINE_SETTER (double, double, new_double)
DEFINE_SETTER (const gchar *, string, new_string)

