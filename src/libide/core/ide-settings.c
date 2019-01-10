/* ide-settings.c
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

#define G_LOG_DOMAIN "ide-settings"

#include "config.h"

#include <dazzle.h>
#include <glib/gi18n.h>
#include <stdlib.h>

#include "ide-settings.h"

/**
 * SECTION:ide-settings
 * @title: IdeSettings
 * @short_description: Settings with per-project overrides
 *
 * In Builder, we need support for settings at the user level (their chosen
 * defaults) as well as defaults for a project. #IdeSettings attempts to
 * simplify this by providing a layered approach to settings.
 *
 * If a setting has been set for the current project, it will be returned. If
 * not, the users preference will be returned. Setting a preference via
 * #IdeSettings will always modify the projects setting, not the users default
 * settings.
 *
 * Since: 3.32
 */

struct _IdeSettings
{
  GObject              parent_instance;

  DzlSettingsSandwich *settings_sandwich;
  gchar               *relative_path;
  gchar               *schema_id;
  gchar               *project_id;
  guint                ignore_project_settings : 1;
};

G_DEFINE_TYPE (IdeSettings, ide_settings, G_TYPE_OBJECT)

enum {
  PROP_0,
  PROP_RELATIVE_PATH,
  PROP_SCHEMA_ID,
  PROP_IGNORE_PROJECT_SETTINGS,
  PROP_PROJECT_ID,
  N_PROPS
};

enum {
  CHANGED,
  N_SIGNALS
};

static GParamSpec *properties [N_PROPS];
static guint signals [N_SIGNALS];

static void
ide_settings_set_ignore_project_settings (IdeSettings *self,
                                          gboolean     ignore_project_settings)
{
  g_return_if_fail (IDE_IS_SETTINGS (self));

  ignore_project_settings = !!ignore_project_settings;

  if (ignore_project_settings != self->ignore_project_settings)
    {
      self->ignore_project_settings = ignore_project_settings;
      g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_IGNORE_PROJECT_SETTINGS]);
    }
}

static void
ide_settings_set_relative_path (IdeSettings *self,
                                const gchar *relative_path)
{
  g_assert (IDE_IS_SETTINGS (self));
  g_assert (relative_path != NULL);

  if (*relative_path == '/')
    relative_path++;

  if (!ide_str_equal0 (relative_path, self->relative_path))
    {
      g_free (self->relative_path);
      self->relative_path = g_strdup (relative_path);
      g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_RELATIVE_PATH]);
    }
}

static void
ide_settings_set_schema_id (IdeSettings *self,
                            const gchar *schema_id)
{
  g_assert (IDE_IS_SETTINGS (self));
  g_assert (schema_id != NULL);

  if (!ide_str_equal0 (schema_id, self->schema_id))
    {
      g_free (self->schema_id);
      self->schema_id = g_strdup (schema_id);
      g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_SCHEMA_ID]);
    }
}

static void
ide_settings_constructed (GObject *object)
{
  IdeSettings *self = (IdeSettings *)object;
  g_autofree gchar *full_path = NULL;
  GSettings *settings;
  gchar *path;

  IDE_ENTRY;

  G_OBJECT_CLASS (ide_settings_parent_class)->constructed (object);

  if (self->schema_id == NULL)
    {
      g_error ("You must provide IdeSettings:schema-id");
      abort ();
    }

  if (self->relative_path == NULL)
    {
      g_autoptr(GSettingsSchema) schema = NULL;
      GSettingsSchemaSource *source;
      const gchar *schema_path;

      source = g_settings_schema_source_get_default ();
      schema = g_settings_schema_source_lookup (source, self->schema_id, TRUE);

      if (schema == NULL)
        {
          g_error ("Could not locate schema %s", self->schema_id);
          abort ();
        }

      schema_path = g_settings_schema_get_path (schema);

      if ((schema_path != NULL) && !g_str_has_prefix (schema_path, "/org/gnome/builder/"))
        {
          g_error ("Schema path MUST be under /org/gnome/builder/");
          abort ();
        }
      else if (schema_path == NULL)
        {
          self->relative_path = g_strdup ("");
        }
      else
        {
          self->relative_path = g_strdup (schema_path + strlen ("/org/gnome/builder/"));
        }
    }

  g_assert (self->relative_path != NULL);
  g_assert (self->relative_path [0] != '/');
  g_assert ((self->relative_path [0] == 0) || g_str_has_suffix (self->relative_path, "/"));

  full_path = g_strdup_printf ("/org/gnome/builder/%s", self->relative_path);
  self->settings_sandwich = dzl_settings_sandwich_new (self->schema_id, full_path);

  /* Add our project relative settings */
  if (self->ignore_project_settings == FALSE)
    {
      path = g_strdup_printf ("/org/gnome/builder/projects/%s/%s",
                              self->project_id, self->relative_path);
      settings = g_settings_new_with_path (self->schema_id, path);
      dzl_settings_sandwich_append (self->settings_sandwich, settings);
      g_clear_object (&settings);
      g_free (path);
    }

  /* Add our application global (user defaults) settings */
  settings = g_settings_new_with_path (self->schema_id, full_path);
  dzl_settings_sandwich_append (self->settings_sandwich, settings);
  g_clear_object (&settings);

  IDE_EXIT;
}

static void
ide_settings_finalize (GObject *object)
{
  IdeSettings *self = (IdeSettings *)object;

  g_clear_object (&self->settings_sandwich);
  g_clear_pointer (&self->relative_path, g_free);
  g_clear_pointer (&self->schema_id, g_free);
  g_clear_pointer (&self->project_id, g_free);

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
    case PROP_PROJECT_ID:
      g_value_set_string (value, self->project_id);
      break;

    case PROP_SCHEMA_ID:
      g_value_set_string (value, ide_settings_get_schema_id (self));
      break;

    case PROP_RELATIVE_PATH:
      g_value_set_string (value, ide_settings_get_relative_path (self));
      break;

    case PROP_IGNORE_PROJECT_SETTINGS:
      g_value_set_boolean (value, ide_settings_get_ignore_project_settings (self));
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
    case PROP_PROJECT_ID:
      self->project_id = g_value_dup_string (value);
      break;

    case PROP_SCHEMA_ID:
      ide_settings_set_schema_id (self, g_value_get_string (value));
      break;

    case PROP_RELATIVE_PATH:
      ide_settings_set_relative_path (self, g_value_get_string (value));
      break;

    case PROP_IGNORE_PROJECT_SETTINGS:
      ide_settings_set_ignore_project_settings (self, g_value_get_boolean (value));
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

  properties [PROP_PROJECT_ID] =
    g_param_spec_string ("project-id",
                         "Project Id",
                         "The identifier for the project",
                         NULL,
                         (G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS));

  properties [PROP_IGNORE_PROJECT_SETTINGS] =
    g_param_spec_boolean ("ignore-project-settings",
                         "Ignore Project Settings",
                         "If project settings should be ignored.",
                         FALSE,
                         (G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS));

  properties [PROP_RELATIVE_PATH] =
    g_param_spec_string ("relative-path",
                         "Relative Path",
                         "Relative Path",
                         NULL,
                         (G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS));

  properties [PROP_SCHEMA_ID] =
    g_param_spec_string ("schema-id",
                         "Schema ID",
                         "Schema ID",
                         NULL,
                         (G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, N_PROPS, properties);

  signals [CHANGED] =
    g_signal_new ("changed",
                  G_TYPE_FROM_CLASS (klass),
                  G_SIGNAL_RUN_LAST | G_SIGNAL_DETAILED,
                  0,
                  NULL, NULL, NULL,
                  G_TYPE_NONE,
                  1,
                  G_TYPE_STRING);
}

static void
ide_settings_init (IdeSettings *self)
{
}

IdeSettings *
ide_settings_new (const gchar *project_id,
                  const gchar *schema_id,
                  const gchar *relative_path,
                  gboolean     ignore_project_settings)
{
  IdeSettings *ret;

  IDE_ENTRY;

  g_assert (project_id != NULL);
  g_assert (schema_id != NULL);
  g_assert (relative_path != NULL);

  ret = g_object_new (IDE_TYPE_SETTINGS,
                      "project-id", project_id,
                      "ignore-project-settings", ignore_project_settings,
                      "relative-path", relative_path,
                      "schema-id", schema_id,
                      NULL);

  IDE_RETURN (ret);
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

gboolean
ide_settings_get_ignore_project_settings (IdeSettings *self)
{
  g_return_val_if_fail (IDE_IS_SETTINGS (self), FALSE);

  return self->ignore_project_settings;
}

GVariant *
ide_settings_get_default_value (IdeSettings *self,
                                const gchar *key)
{
  g_return_val_if_fail (IDE_IS_SETTINGS (self), NULL);
  g_return_val_if_fail (key != NULL, NULL);

  return dzl_settings_sandwich_get_default_value (self->settings_sandwich, key);
}

GVariant *
ide_settings_get_user_value (IdeSettings *self,
                             const gchar *key)
{
  g_return_val_if_fail (IDE_IS_SETTINGS (self), NULL);
  g_return_val_if_fail (key != NULL, NULL);

  return dzl_settings_sandwich_get_user_value (self->settings_sandwich, key);
}

GVariant *
ide_settings_get_value (IdeSettings *self,
                        const gchar *key)
{
  g_return_val_if_fail (IDE_IS_SETTINGS (self), NULL);
  g_return_val_if_fail (key != NULL, NULL);

  return dzl_settings_sandwich_get_value (self->settings_sandwich, key);
}

void
ide_settings_set_value (IdeSettings *self,
                        const gchar *key,
                        GVariant    *value)
{
  g_return_if_fail (IDE_IS_SETTINGS (self));
  g_return_if_fail (key != NULL);

  return dzl_settings_sandwich_set_value (self->settings_sandwich, key, value);
}

gboolean
ide_settings_get_boolean (IdeSettings *self,
                          const gchar *key)
{
  g_return_val_if_fail (IDE_IS_SETTINGS (self), FALSE);
  g_return_val_if_fail (key != NULL, FALSE);

  return dzl_settings_sandwich_get_boolean (self->settings_sandwich, key);
}

gdouble
ide_settings_get_double (IdeSettings *self,
                         const gchar *key)
{
  g_return_val_if_fail (IDE_IS_SETTINGS (self), 0.0);
  g_return_val_if_fail (key != NULL, 0.0);

  return dzl_settings_sandwich_get_double (self->settings_sandwich, key);
}

gint
ide_settings_get_int (IdeSettings *self,
                      const gchar *key)
{
  g_return_val_if_fail (IDE_IS_SETTINGS (self), 0);
  g_return_val_if_fail (key != NULL, 0);

  return dzl_settings_sandwich_get_int (self->settings_sandwich, key);
}

gchar *
ide_settings_get_string (IdeSettings *self,
                         const gchar *key)
{
  g_return_val_if_fail (IDE_IS_SETTINGS (self), NULL);
  g_return_val_if_fail (key != NULL, NULL);

  return dzl_settings_sandwich_get_string (self->settings_sandwich, key);
}

guint
ide_settings_get_uint (IdeSettings *self,
                       const gchar *key)
{
  g_return_val_if_fail (IDE_IS_SETTINGS (self), 0);
  g_return_val_if_fail (key != NULL, 0);

  return dzl_settings_sandwich_get_uint (self->settings_sandwich, key);
}

void
ide_settings_set_boolean (IdeSettings *self,
                          const gchar *key,
                          gboolean     val)
{
  g_return_if_fail (IDE_IS_SETTINGS (self));
  g_return_if_fail (key != NULL);

  dzl_settings_sandwich_set_boolean (self->settings_sandwich, key, val);
}

void
ide_settings_set_double (IdeSettings *self,
                         const gchar *key,
                         gdouble      val)
{
  g_return_if_fail (IDE_IS_SETTINGS (self));
  g_return_if_fail (key != NULL);

  dzl_settings_sandwich_set_double (self->settings_sandwich, key, val);
}

void
ide_settings_set_int (IdeSettings *self,
                      const gchar *key,
                      gint         val)
{
  g_return_if_fail (IDE_IS_SETTINGS (self));
  g_return_if_fail (key != NULL);

  dzl_settings_sandwich_set_int (self->settings_sandwich, key, val);
}

void
ide_settings_set_string (IdeSettings *self,
                         const gchar *key,
                         const gchar *val)
{
  g_return_if_fail (IDE_IS_SETTINGS (self));
  g_return_if_fail (key != NULL);

  dzl_settings_sandwich_set_string (self->settings_sandwich, key, val);
}

void
ide_settings_set_uint (IdeSettings *self,
                       const gchar *key,
                       guint        val)
{
  g_return_if_fail (IDE_IS_SETTINGS (self));
  g_return_if_fail (key != NULL);

  dzl_settings_sandwich_set_uint (self->settings_sandwich, key, val);
}

void
ide_settings_bind (IdeSettings        *self,
                   const gchar        *key,
                   gpointer            object,
                   const gchar        *property,
                   GSettingsBindFlags  flags)
{
  g_return_if_fail (IDE_IS_SETTINGS (self));
  g_return_if_fail (key != NULL);
  g_return_if_fail (G_IS_OBJECT (object));
  g_return_if_fail (property != NULL);

  dzl_settings_sandwich_bind (self->settings_sandwich, key, object, property, flags);
}

/**
 * ide_settings_bind_with_mapping:
 * @self: An #IdeSettings
 * @key: The settings key
 * @object: the object to bind to
 * @property: the property of @object to bind to
 * @flags: flags for the binding
 * @get_mapping: (allow-none) (scope notified): variant to value mapping
 * @set_mapping: (allow-none) (scope notified): value to variant mapping
 * @user_data: user data for @get_mapping and @set_mapping
 * @destroy: destroy function to cleanup @user_data.
 *
 * Like ide_settings_bind() but allows transforming to and from settings storage using
 * @get_mapping and @set_mapping transformation functions.
 *
 * Call ide_settings_unbind() to unbind the mapping.
 *
 * Since: 3.32
 */
void
ide_settings_bind_with_mapping (IdeSettings             *self,
                                const gchar             *key,
                                gpointer                 object,
                                const gchar             *property,
                                GSettingsBindFlags       flags,
                                GSettingsBindGetMapping  get_mapping,
                                GSettingsBindSetMapping  set_mapping,
                                gpointer                 user_data,
                                GDestroyNotify           destroy)
{
  g_return_if_fail (IDE_IS_SETTINGS (self));
  g_return_if_fail (key != NULL);
  g_return_if_fail (G_IS_OBJECT (object));
  g_return_if_fail (property != NULL);

  dzl_settings_sandwich_bind_with_mapping (self->settings_sandwich, key, object, property, flags,
                                           get_mapping, set_mapping, user_data, destroy);
}

void
ide_settings_unbind (IdeSettings *self,
                     const gchar *property)
{
  g_return_if_fail (IDE_IS_SETTINGS (self));
  g_return_if_fail (property != NULL);

  dzl_settings_sandwich_unbind (self->settings_sandwich, property);
}
