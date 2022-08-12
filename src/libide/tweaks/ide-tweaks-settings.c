/* ide-tweaks-settings.c
 *
 * Copyright 2022 Christian Hergert <chergert@redhat.com>
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

#define G_LOG_DOMAIN "ide-tweaks-settings"

#include "config.h"

#include "ide-tweaks-settings.h"

struct _IdeTweaksSettings
{
  IdeTweaksItem parent_instance;
  char *schema_id;
  char *schema_path;
  guint application_only : 1;
};

enum {
  PROP_0,
  PROP_APPLICATION_ONLY,
  PROP_SCHEMA_ID,
  PROP_SCHEMA_PATH,
  N_PROPS
};

G_DEFINE_FINAL_TYPE (IdeTweaksSettings, ide_tweaks_settings, IDE_TYPE_TWEAKS_ITEM)

static GParamSpec *properties [N_PROPS];

static void
ide_tweaks_settings_dispose (GObject *object)
{
  IdeTweaksSettings *self = (IdeTweaksSettings *)object;

  g_clear_pointer (&self->schema_id, g_free);
  g_clear_pointer (&self->schema_path, g_free);

  G_OBJECT_CLASS (ide_tweaks_settings_parent_class)->dispose (object);
}

static void
ide_tweaks_settings_get_property (GObject    *object,
                                  guint       prop_id,
                                  GValue     *value,
                                  GParamSpec *pspec)
{
  IdeTweaksSettings *self = IDE_TWEAKS_SETTINGS (object);

  switch (prop_id)
    {
    case PROP_APPLICATION_ONLY:
      g_value_set_boolean (value, ide_tweaks_settings_get_application_only (self));
      break;

    case PROP_SCHEMA_ID:
      g_value_set_string (value, ide_tweaks_settings_get_schema_id (self));
      break;

    case PROP_SCHEMA_PATH:
      g_value_set_string (value, ide_tweaks_settings_get_schema_path (self));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
ide_tweaks_settings_set_property (GObject      *object,
                                  guint         prop_id,
                                  const GValue *value,
                                  GParamSpec   *pspec)
{
  IdeTweaksSettings *self = IDE_TWEAKS_SETTINGS (object);

  switch (prop_id)
    {
    case PROP_APPLICATION_ONLY:
      ide_tweaks_settings_set_application_only (self, g_value_get_boolean (value));
      break;

    case PROP_SCHEMA_ID:
      ide_tweaks_settings_set_schema_id (self, g_value_get_string (value));
      break;

    case PROP_SCHEMA_PATH:
      ide_tweaks_settings_set_schema_path (self, g_value_get_string (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
ide_tweaks_settings_class_init (IdeTweaksSettingsClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->dispose = ide_tweaks_settings_dispose;
  object_class->get_property = ide_tweaks_settings_get_property;
  object_class->set_property = ide_tweaks_settings_set_property;

  properties[PROP_APPLICATION_ONLY] =
    g_param_spec_boolean ("application-only", NULL, NULL,
                          FALSE,
                          (G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS));

  properties[PROP_SCHEMA_ID] =
    g_param_spec_string ("schema-id", NULL, NULL,
                         NULL,
                         (G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS));

  properties[PROP_SCHEMA_PATH] =
    g_param_spec_string ("schema-path", NULL, NULL,
                         NULL,
                         (G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, N_PROPS, properties);
}

static void
ide_tweaks_settings_init (IdeTweaksSettings *self)
{
}

IdeTweaksSettings *
ide_tweaks_settings_new (void)
{
  return g_object_new (IDE_TYPE_TWEAKS_SETTINGS, NULL);
}

const char *
ide_tweaks_settings_get_schema_id (IdeTweaksSettings *self)
{
  g_return_val_if_fail (IDE_IS_TWEAKS_SETTINGS (self), NULL);

  return self->schema_id;
}

const char *
ide_tweaks_settings_get_schema_path (IdeTweaksSettings *self)
{
  g_return_val_if_fail (IDE_IS_TWEAKS_SETTINGS (self), NULL);

  return self->schema_path;
}

gboolean
ide_tweaks_settings_get_application_only (IdeTweaksSettings *self)
{
  g_return_val_if_fail (IDE_IS_TWEAKS_SETTINGS (self), FALSE);

  return self->application_only;
}

void
ide_tweaks_settings_set_schema_id (IdeTweaksSettings *self,
                                   const char        *schema_id)
{
  g_return_if_fail (IDE_IS_TWEAKS_SETTINGS (self));

  if (ide_set_string (&self->schema_id, schema_id))
    {
      g_free (self->schema_id);
      self->schema_id = g_strdup (schema_id);
      g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_SCHEMA_ID]);
    }
}

void
ide_tweaks_settings_set_schema_path (IdeTweaksSettings *self,
                                     const char        *schema_path)
{
  g_return_if_fail (IDE_IS_TWEAKS_SETTINGS (self));

  if (ide_set_string (&self->schema_path, schema_path))
    {
      g_free (self->schema_path);
      self->schema_path = g_strdup (schema_path);
      g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_SCHEMA_PATH]);
    }
}

void
ide_tweaks_settings_set_application_only (IdeTweaksSettings *self,
                                          gboolean           application_only)
{
  g_return_if_fail (IDE_IS_TWEAKS_SETTINGS (self));

  application_only = !!application_only;

  if (self->application_only != application_only)
    {
      self->application_only = application_only;
      g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_APPLICATION_ONLY]);
    }
}

/**
 * ide_tweaks_settings_create_action_group:
 * @self: a #IdeTweaksSettings
 * @project_id: the project identifier
 *
 * Creates an action group containing the settings.
 *
 * Some effort is taken to return an existing instance of the
 * action group so that they are not needlessly created.
 *
 * Returns: (transfer full) (nullable): a #GActionGroup if successful;
 *   otherwise %NULL if not enough information is available.
 */
GActionGroup *
ide_tweaks_settings_create_action_group (IdeTweaksSettings *self,
                                         const char        *project_id)
{
  IdeTweaksItem *root;
  GActionGroup *cached;
  IdeSettings *settings;
  g_autofree char *hash_key = NULL;

  g_return_val_if_fail (IDE_IS_TWEAKS_SETTINGS (self), NULL);

  if (self->schema_id == NULL)
    return NULL;

  hash_key = g_strdup_printf ("IdeSettings<%s|%s|%s>",
                              self->schema_id,
                              self->schema_path ? self->schema_path : (project_id ? project_id : "__app__"),
                              self->application_only ? "app" : "project");

  root = ide_tweaks_item_get_root (IDE_TWEAKS_ITEM (self));

  if ((cached = g_object_get_data (G_OBJECT (root), hash_key)))
    {
      g_assert (G_IS_ACTION_GROUP (cached));
      return g_object_ref (cached);
    }

  if (self->schema_path == NULL)
    settings = ide_settings_new (project_id, self->schema_id);
  else if (self->application_only)
    settings = ide_settings_new_with_path (NULL, self->schema_id, self->schema_path);
  else
    settings = ide_settings_new_with_path (project_id, self->schema_id, self->schema_path);

  g_object_set_data_full (G_OBJECT (root),
                          hash_key,
                          g_object_ref (settings),
                          g_object_unref);

  return G_ACTION_GROUP (settings);
}
