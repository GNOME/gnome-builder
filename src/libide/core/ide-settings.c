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

#include <glib/gi18n.h>

#include <stdlib.h>

#include "ide-debug.h"
#include "ide-macros.h"
#include "ide-marshal.h"
#include "ide-settings.h"
#include "ide-layered-settings-private.h"

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
 */

struct _IdeSettings
{
  GObject             parent_instance;
  IdeLayeredSettings *layered_settings;
  char               *schema_id;
  char               *project_id;
  char               *path;
  char               *path_suffix;
};

static void action_group_iface_init (GActionGroupInterface *iface);

G_DEFINE_FINAL_TYPE_WITH_CODE (IdeSettings, ide_settings, G_TYPE_OBJECT,
                               G_IMPLEMENT_INTERFACE (G_TYPE_ACTION_GROUP, action_group_iface_init))

enum {
  PROP_0,
  PROP_PATH,
  PROP_PATH_SUFFIX,
  PROP_PROJECT_ID,
  PROP_SCHEMA_ID,
  N_PROPS
};

enum {
  CHANGED,
  N_SIGNALS
};

static GParamSpec *properties [N_PROPS];
static guint signals [N_SIGNALS];

static const GVariantType *
_g_variant_type_intern (const GVariantType *type)
{
  g_autofree char *str = NULL;

  if (type == NULL)
    return NULL;

  str = g_variant_type_dup_string (type);
  return G_VARIANT_TYPE (g_intern_string (str));
}

static void
ide_settings_set_schema_id (IdeSettings *self,
                            const char  *schema_id)
{
  g_assert (IDE_IS_SETTINGS (self));
  g_assert (schema_id != NULL);

  if (g_set_str (&self->schema_id, schema_id))
    g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_SCHEMA_ID]);
}

static void
ide_settings_layered_settings_changed_cb (IdeSettings        *self,
                                          const char         *key,
                                          IdeLayeredSettings *layered_settings)
{
  g_autoptr(GVariant) value = NULL;

  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (key != NULL);
  g_assert (IDE_IS_LAYERED_SETTINGS (layered_settings));

  g_signal_emit (self, signals [CHANGED], g_quark_from_string (key), key);

  value = ide_layered_settings_get_value (self->layered_settings, key);
  g_action_group_action_state_changed (G_ACTION_GROUP (self), key, value);
}

char *
ide_settings_resolve_schema_path (const char *schema_id,
                                  const char *project_id,
                                  const char *path_suffix)
{
  g_autoptr(GSettingsSchema) schema = NULL;
  GSettingsSchemaSource *source;
  g_autofree char *real_path_suffix = NULL;
  const char *schema_path;

  g_return_val_if_fail (schema_id != NULL, NULL);

  /* Normalize our path suffix if we were provided one */
  if (path_suffix != NULL && !g_str_has_suffix (path_suffix, "/"))
    path_suffix = real_path_suffix = g_strconcat (path_suffix, "/", NULL);

  source = g_settings_schema_source_get_default ();

  if (!(schema = g_settings_schema_source_lookup (source, schema_id, TRUE)))
    {
      g_critical ("Failed to locate schema %s", schema_id);
      return NULL;
    }

  if ((schema_path = g_settings_schema_get_path (schema)))
    {
      if (project_id != NULL)
        g_critical ("Attempt to resolve non-relocatable schema %s with project-id %s",
                    schema_id, project_id);
      return g_strdup (schema_path);
    }

  if (!g_str_has_prefix (schema_id, "org.gnome.builder."))
    {
      g_critical ("Relocatable schemas must be prefixed with org.gnome.builder.");
      return NULL;
    }
  else if (g_str_equal (schema_id, "org.gnome.builder.project"))
    {
      if (project_id != NULL)
        return g_strconcat ("/org/gnome/builder/projects/", project_id, "/", path_suffix, NULL);
      else
        return g_strconcat ("/org/gnome/builder/projects/", path_suffix, NULL);
    }
  else if (g_str_equal (schema_id, "org.gnome.builder.editor.language"))
    {
      /* This is a special case so that we don't have to migrate users settings
       * from one path to another. Otherwise, we'd be perfectly fine doing it
       * other way (but that'd require changes to ide-language-defaults.c too).
       *
       * Bug: https://gitlab.gnome.org/GNOME/gnome-builder/-/issues/1813
       */
      if (project_id != NULL)
        return g_strconcat ("/org/gnome/builder/projects/", project_id, "/editor/language/", path_suffix, NULL);
      else
        return g_strconcat ("/org/gnome/builder/editor/language/", path_suffix, NULL);
    }
  else
    {
      const char *suffix = schema_id + strlen ("org.gnome.builder.");
      g_autofree char *escaped = g_strdelimit (g_strdup (suffix), ".", '/');

      if (project_id != NULL)
        return g_strconcat ("/org/gnome/builder/projects/", project_id, "/", escaped, "/", path_suffix, NULL);
      else
        return g_strconcat ("/org/gnome/builder/projects/", escaped, "/", path_suffix, NULL);
    }
}

static void
ide_settings_constructed (GObject *object)
{
  IdeSettings *self = (IdeSettings *)object;
  g_autoptr(GSettingsSchema) schema = NULL;
  g_autoptr(GSettings) app_settings = NULL;
  gboolean relocatable;

  IDE_ENTRY;

  G_OBJECT_CLASS (ide_settings_parent_class)->constructed (object);

  if (self->schema_id == NULL)
    g_error ("You must set %s:schema-id during construction", G_OBJECT_TYPE_NAME (self));

  if (!ide_str_equal0 (self->schema_id, "org.gnome.builder") &&
      !g_str_has_prefix (self->schema_id, "org.gnome.builder."))
    g_error ("You must use a schema prefixed with org.gnome.builder. (%s)",
             self->schema_id);

  if (self->path != NULL)
    {
      if (!g_str_has_prefix (self->path, "/org/gnome/builder/"))
        g_error ("You must use a path that begins with /org/gnome/builder/");
      else if (!g_str_has_suffix (self->path, "/"))
        g_error ("Settings paths must end in /");
    }
  else
    {
      if (!(self->path = ide_settings_resolve_schema_path (self->schema_id, NULL, self->path_suffix)))
        g_error ("Failed to generate application path for %s", self->schema_id);
    }

  /* Create settings for the app-level layer, we'll append it last */
  app_settings = g_settings_new_with_path (self->schema_id, self->path);
  g_object_get (app_settings, "settings-schema", &schema, NULL);
  relocatable = g_settings_schema_get_path (schema) == NULL;

  self->layered_settings = ide_layered_settings_new (self->schema_id, self->path);

  g_signal_connect_object (self->layered_settings,
                           "changed",
                           G_CALLBACK (ide_settings_layered_settings_changed_cb),
                           self,
                           G_CONNECT_SWAPPED);

  /* Add project layer if we need one */
  if (relocatable && self->project_id != NULL)
    {
      g_autofree char *project_path = ide_settings_resolve_schema_path (self->schema_id, self->project_id, self->path_suffix);
      g_autoptr(GSettings) project_settings = g_settings_new_with_path (self->schema_id, project_path);

      ide_layered_settings_append (self->layered_settings, project_settings);
    }

  /* Add our application global (user defaults) settings as fallbacks */
  ide_layered_settings_append (self->layered_settings, app_settings);

  IDE_EXIT;
}

static void
ide_settings_finalize (GObject *object)
{
  IdeSettings *self = (IdeSettings *)object;

  g_clear_object (&self->layered_settings);

  g_clear_pointer (&self->schema_id, g_free);
  g_clear_pointer (&self->project_id, g_free);
  g_clear_pointer (&self->path, g_free);
  g_clear_pointer (&self->path_suffix, g_free);

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
    case PROP_PATH:
      g_value_set_string (value, self->path);
      break;

    case PROP_PATH_SUFFIX:
      g_value_set_string (value, self->path_suffix);
      break;

    case PROP_PROJECT_ID:
      g_value_set_string (value, self->project_id);
      break;

    case PROP_SCHEMA_ID:
      g_value_set_string (value, ide_settings_get_schema_id (self));
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
    case PROP_PATH:
      self->path = g_value_dup_string (value);
      break;

    case PROP_PATH_SUFFIX:
      self->path_suffix = g_value_dup_string (value);
      break;

    case PROP_PROJECT_ID:
      self->project_id = g_value_dup_string (value);
      break;

    case PROP_SCHEMA_ID:
      ide_settings_set_schema_id (self, g_value_get_string (value));
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

  properties [PROP_PATH] =
    g_param_spec_string ("path",
                         "Path",
                         "The path to use for for app settings",
                         NULL,
                         (G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS));

  properties [PROP_PATH_SUFFIX] =
    g_param_spec_string ("path-suffix",
                         "Path Suffix",
                         "A path suffix to append when generating schema paths",
                         NULL,
                         (G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS));

  properties [PROP_PROJECT_ID] =
    g_param_spec_string ("project-id",
                         "Project Id",
                         "The identifier for the project",
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
ide_settings_init (IdeSettings *self)
{
}

IdeSettings *
ide_settings_new (const char *project_id,
                  const char *schema_id)
{
  IdeSettings *ret;

  IDE_ENTRY;

  g_return_val_if_fail (schema_id != NULL, NULL);

  ret = g_object_new (IDE_TYPE_SETTINGS,
                      "project-id", project_id,
                      "schema-id", schema_id,
                      NULL);

  IDE_RETURN (ret);
}

IdeSettings *
ide_settings_new_with_path (const char *project_id,
                            const char *schema_id,
                            const char *path)
{
  IdeSettings *ret;

  IDE_ENTRY;

  g_return_val_if_fail (schema_id != NULL, NULL);

  ret = g_object_new (IDE_TYPE_SETTINGS,
                      "project-id", project_id,
                      "schema-id", schema_id,
                      "path", path,
                      NULL);

  IDE_RETURN (ret);
}

const char *
ide_settings_get_schema_id (IdeSettings *self)
{
  g_return_val_if_fail (IDE_IS_SETTINGS (self), NULL);

  return self->schema_id;
}

GVariant *
ide_settings_get_default_value (IdeSettings *self,
                                const char  *key)
{
  g_return_val_if_fail (IDE_IS_SETTINGS (self), NULL);
  g_return_val_if_fail (key != NULL, NULL);

  return ide_layered_settings_get_default_value (self->layered_settings, key);
}

GVariant *
ide_settings_get_user_value (IdeSettings *self,
                             const char  *key)
{
  g_return_val_if_fail (IDE_IS_SETTINGS (self), NULL);
  g_return_val_if_fail (key != NULL, NULL);

  return ide_layered_settings_get_user_value (self->layered_settings, key);
}

GVariant *
ide_settings_get_value (IdeSettings *self,
                        const char  *key)
{
  g_return_val_if_fail (IDE_IS_SETTINGS (self), NULL);
  g_return_val_if_fail (key != NULL, NULL);

  return ide_layered_settings_get_value (self->layered_settings, key);
}

void
ide_settings_set_value (IdeSettings *self,
                        const char  *key,
                        GVariant    *value)
{
  g_return_if_fail (IDE_IS_SETTINGS (self));
  g_return_if_fail (key != NULL);

  return ide_layered_settings_set_value (self->layered_settings, key, value);
}

gboolean
ide_settings_get_boolean (IdeSettings *self,
                          const char  *key)
{
  g_return_val_if_fail (IDE_IS_SETTINGS (self), FALSE);
  g_return_val_if_fail (key != NULL, FALSE);

  return ide_layered_settings_get_boolean (self->layered_settings, key);
}

double
ide_settings_get_double (IdeSettings *self,
                         const char  *key)
{
  g_return_val_if_fail (IDE_IS_SETTINGS (self), 0.0);
  g_return_val_if_fail (key != NULL, 0.0);

  return ide_layered_settings_get_double (self->layered_settings, key);
}

int
ide_settings_get_int (IdeSettings *self,
                      const char  *key)
{
  g_return_val_if_fail (IDE_IS_SETTINGS (self), 0);
  g_return_val_if_fail (key != NULL, 0);

  return ide_layered_settings_get_int (self->layered_settings, key);
}

char *
ide_settings_get_string (IdeSettings *self,
                         const char  *key)
{
  g_return_val_if_fail (IDE_IS_SETTINGS (self), NULL);
  g_return_val_if_fail (key != NULL, NULL);

  return ide_layered_settings_get_string (self->layered_settings, key);
}

guint
ide_settings_get_uint (IdeSettings *self,
                       const char  *key)
{
  g_return_val_if_fail (IDE_IS_SETTINGS (self), 0);
  g_return_val_if_fail (key != NULL, 0);

  return ide_layered_settings_get_uint (self->layered_settings, key);
}

void
ide_settings_set_boolean (IdeSettings *self,
                          const char  *key,
                          gboolean     val)
{
  g_return_if_fail (IDE_IS_SETTINGS (self));
  g_return_if_fail (key != NULL);

  ide_layered_settings_set_boolean (self->layered_settings, key, val);
}

void
ide_settings_set_double (IdeSettings *self,
                         const char  *key,
                         double       val)
{
  g_return_if_fail (IDE_IS_SETTINGS (self));
  g_return_if_fail (key != NULL);

  ide_layered_settings_set_double (self->layered_settings, key, val);
}

void
ide_settings_set_int (IdeSettings *self,
                      const char  *key,
                      int          val)
{
  g_return_if_fail (IDE_IS_SETTINGS (self));
  g_return_if_fail (key != NULL);

  ide_layered_settings_set_int (self->layered_settings, key, val);
}

void
ide_settings_set_string (IdeSettings *self,
                         const char  *key,
                         const char  *val)
{
  g_return_if_fail (IDE_IS_SETTINGS (self));
  g_return_if_fail (key != NULL);

  ide_layered_settings_set_string (self->layered_settings, key, val);
}

void
ide_settings_set_uint (IdeSettings *self,
                       const char  *key,
                       guint        val)
{
  g_return_if_fail (IDE_IS_SETTINGS (self));
  g_return_if_fail (key != NULL);

  ide_layered_settings_set_uint (self->layered_settings, key, val);
}

void
ide_settings_bind (IdeSettings        *self,
                   const char         *key,
                   gpointer            object,
                   const char         *property,
                   GSettingsBindFlags  flags)
{
  g_return_if_fail (IDE_IS_SETTINGS (self));
  g_return_if_fail (key != NULL);
  g_return_if_fail (G_IS_OBJECT (object));
  g_return_if_fail (property != NULL);

  ide_layered_settings_bind (self->layered_settings, key, object, property, flags);
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
 */
void
ide_settings_bind_with_mapping (IdeSettings             *self,
                                const char              *key,
                                gpointer                 object,
                                const char              *property,
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

  ide_layered_settings_bind_with_mapping (self->layered_settings, key, object, property, flags,
                                          get_mapping, set_mapping, user_data, destroy);
}

void
ide_settings_unbind (IdeSettings *self,
                     const char  *property)
{
  g_return_if_fail (IDE_IS_SETTINGS (self));
  g_return_if_fail (property != NULL);

  ide_layered_settings_unbind (self->layered_settings, property);
}

static gboolean
ide_settings_has_action (GActionGroup *group,
                         const char   *action_name)
{
  IdeSettings *self = IDE_SETTINGS (group);
  g_auto(GStrv) keys = ide_layered_settings_list_keys (self->layered_settings);

  return g_strv_contains ((const char * const *)keys, action_name);
}

static char **
ide_settings_list_actions (GActionGroup *group)
{
  IdeSettings *self = IDE_SETTINGS (group);
  return ide_layered_settings_list_keys (self->layered_settings);
}

static gboolean
ide_settings_get_action_enabled (GActionGroup *group,
                                 const char   *action_name)
{
  return TRUE;
}

static GVariant *
ide_settings_get_action_state (GActionGroup *group,
                               const char   *action_name)
{
  IdeSettings *self = IDE_SETTINGS (group);

  return ide_layered_settings_get_value (self->layered_settings, action_name);
}

static GVariant *
ide_settings_get_action_state_hint (GActionGroup *group,
                                    const char   *action_name)
{
  IdeSettings *self = IDE_SETTINGS (group);
  g_autoptr(GSettingsSchemaKey) key = ide_layered_settings_get_key (self->layered_settings, action_name);
  return g_settings_schema_key_get_range (key);
}

static void
ide_settings_change_action_state (GActionGroup *group,
                                  const char   *action_name,
                                  GVariant     *value)
{
  IdeSettings *self = IDE_SETTINGS (group);
  g_autoptr(GSettingsSchemaKey) key = ide_layered_settings_get_key (self->layered_settings, action_name);

  if (g_variant_is_of_type (value, g_settings_schema_key_get_value_type (key)) &&
      g_settings_schema_key_range_check (key, value))
    {
      g_autoptr(GVariant) hold = g_variant_ref_sink (value);

      ide_layered_settings_set_value (self->layered_settings, action_name, hold);
      g_action_group_action_state_changed (group, action_name, hold);
    }
}

static const GVariantType *
ide_settings_get_action_state_type (GActionGroup *group,
                                    const char   *action_name)
{
  IdeSettings *self = IDE_SETTINGS (group);
  g_autoptr(GSettingsSchemaKey) key = ide_layered_settings_get_key (self->layered_settings, action_name);
  const GVariantType *type = g_settings_schema_key_get_value_type (key);

  return _g_variant_type_intern (type);
}

static void
ide_settings_activate_action (GActionGroup *group,
                              const char   *action_name,
                              GVariant     *parameter)
{
  IdeSettings *self = IDE_SETTINGS (group);
  g_autoptr(GSettingsSchemaKey) key = ide_layered_settings_get_key (self->layered_settings, action_name);
  g_autoptr(GVariant) default_value = g_settings_schema_key_get_default_value (key);

  if (g_variant_is_of_type (default_value, G_VARIANT_TYPE_BOOLEAN))
    {
      GVariant *old;

      if (parameter != NULL)
        return;

      old = ide_settings_get_action_state (group, action_name);
      parameter = g_variant_new_boolean (!g_variant_get_boolean (old));
      g_variant_unref (old);
    }

  g_action_group_change_action_state (group, action_name, parameter);
}

static const GVariantType *
ide_settings_get_action_parameter_type (GActionGroup *group,
                                        const char   *action_name)
{
  IdeSettings *self = IDE_SETTINGS (group);
  g_autoptr(GSettingsSchemaKey) key = ide_layered_settings_get_key (self->layered_settings, action_name);
  const GVariantType *type = g_settings_schema_key_get_value_type (key);

  if (g_variant_type_equal (type, G_VARIANT_TYPE_BOOLEAN))
    return NULL;

  return _g_variant_type_intern (type);
}

static void
action_group_iface_init (GActionGroupInterface *iface)
{
  iface->has_action = ide_settings_has_action;
  iface->list_actions = ide_settings_list_actions;
  iface->get_action_parameter_type = ide_settings_get_action_parameter_type;
  iface->get_action_enabled = ide_settings_get_action_enabled;
  iface->get_action_state = ide_settings_get_action_state;
  iface->get_action_state_hint = ide_settings_get_action_state_hint;
  iface->get_action_state_type = ide_settings_get_action_state_type;
  iface->change_action_state = ide_settings_change_action_state;
  iface->activate_action = ide_settings_activate_action;
}

IdeSettings *
ide_settings_new_relocatable_with_suffix (const char *project_id,
                                          const char *schema_id,
                                          const char *path_suffix)
{
  return g_object_new (IDE_TYPE_SETTINGS,
                       "project-id", project_id,
                       "schema-id", schema_id,
                       "path-suffix", path_suffix,
                       NULL);
}
