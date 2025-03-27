/*
 * gbp-arduino-profile.c
 *
 * Copyright 2025 Nokse <nokse@posteo.com>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this library; if not, see <http://www.gnu.org/licenses/>.
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#define G_LOG_DOMAIN "gbp-arduino-profile"

#include "config.h"

#include <libide-core.h>
#include <libide-gui.h>

#include "gbp-arduino-application-addin.h"
#include "gbp-arduino-platform.h"
#include "gbp-arduino-profile.h"

struct _GbpArduinoProfile
{
  IdeConfig parent_instance;

  char *port;
  char *protocol;
  char *programmer;
  char *fqbn;
  char *notes;

  char **libraries;
  GListStore *platforms;
};

enum
{
  PROP_0,
  PROP_PORT,
  PROP_PROTOCOL,
  PROP_PROGRAMMER,
  PROP_FQBN,
  PROP_NOTES,
  PROP_LIBRARIES,
  PROP_PLATFORMS,
  N_PROPS
};

static GParamSpec *properties[N_PROPS];

G_DEFINE_FINAL_TYPE (GbpArduinoProfile, gbp_arduino_profile, IDE_TYPE_CONFIG)

static char *
gbp_arduino_profile_get_description (IdeConfig *config)
{
  return g_strdup ("Arduino");
}

static void
gbp_arduino_profile_finalize (GObject *object)
{
  GbpArduinoProfile *self = (GbpArduinoProfile *) object;

  g_clear_pointer (&self->port, g_free);
  g_clear_pointer (&self->protocol, g_free);
  g_clear_pointer (&self->programmer, g_free);
  g_clear_pointer (&self->fqbn, g_free);
  g_clear_pointer (&self->notes, g_free);
  g_clear_pointer (&self->libraries, g_strfreev);
  g_clear_object (&self->platforms);

  G_OBJECT_CLASS (gbp_arduino_profile_parent_class)->finalize (object);
}

static void
gbp_arduino_profile_get_property (GObject    *object,
                                  guint       prop_id,
                                  GValue     *value,
                                  GParamSpec *pspec)
{
  GbpArduinoProfile *self = GBP_ARDUINO_PROFILE (object);

  switch (prop_id)
    {
    case PROP_PORT:
      g_value_set_string (value, self->port);
      break;
    case PROP_PROTOCOL:
      g_value_set_string (value, self->protocol);
      break;
    case PROP_PROGRAMMER:
      g_value_set_string (value, self->programmer);
      break;
    case PROP_FQBN:
      g_value_set_string (value, self->fqbn);
      break;
    case PROP_NOTES:
      g_value_set_string (value, self->notes);
      break;
    case PROP_LIBRARIES:
      g_value_set_boxed (value, self->libraries);
      break;
    case PROP_PLATFORMS:
      g_value_set_object (value, self->platforms);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
gbp_arduino_profile_set_property (GObject      *object,
                                  guint         prop_id,
                                  const GValue *value,
                                  GParamSpec   *pspec)
{
  GbpArduinoProfile *self = GBP_ARDUINO_PROFILE (object);

  switch (prop_id)
    {
    case PROP_PORT:
      gbp_arduino_profile_set_port (self, g_value_get_string (value));
      break;
    case PROP_PROTOCOL:
      gbp_arduino_profile_set_protocol (self, g_value_get_string (value));
      break;
    case PROP_PROGRAMMER:
      gbp_arduino_profile_set_programmer (self, g_value_get_string (value));
      break;
    case PROP_FQBN:
      gbp_arduino_profile_set_fqbn (self, g_value_get_string (value));
      break;
    case PROP_NOTES:
      gbp_arduino_profile_set_notes (self, g_value_get_string (value));
      break;
    case PROP_LIBRARIES:
      gbp_arduino_profile_set_libraries (self, g_value_get_boxed (value));
      break;
    case PROP_PLATFORMS:
      g_set_object (&self->platforms, g_value_get_object (value));
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
gbp_arduino_profile_class_init (GbpArduinoProfileClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  IdeConfigClass *config_class = IDE_CONFIG_CLASS (klass);

  object_class->finalize = gbp_arduino_profile_finalize;
  object_class->get_property = gbp_arduino_profile_get_property;
  object_class->set_property = gbp_arduino_profile_set_property;

  config_class->get_description = gbp_arduino_profile_get_description;

  properties[PROP_PORT] =
      g_param_spec_string ("port", NULL, NULL,
                           NULL,
                           G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);

  properties[PROP_PROTOCOL] =
      g_param_spec_string ("protocol", NULL, NULL,
                           NULL,
                           G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);

  properties[PROP_PROGRAMMER] =
      g_param_spec_string ("programmer", NULL, NULL,
                           NULL,
                           G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);

  properties[PROP_FQBN] =
      g_param_spec_string ("fqbn", NULL, NULL,
                           NULL,
                           G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);

  properties[PROP_NOTES] =
      g_param_spec_string ("notes", NULL, NULL,
                           NULL,
                           G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);

  properties[PROP_LIBRARIES] =
      g_param_spec_boxed ("libraries", NULL, NULL,
                          G_TYPE_STRV,
                          (G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS));

  properties[PROP_PLATFORMS] =
      g_param_spec_object ("platforms", NULL, NULL,
                           G_TYPE_LIST_STORE,
                           G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);

  g_object_class_install_properties (object_class, N_PROPS, properties);
}

static void
gbp_arduino_profile_init (GbpArduinoProfile *self)
{
  self->libraries = NULL;
  self->platforms = g_list_store_new (GBP_TYPE_ARDUINO_PLATFORM);
}

void
gbp_arduino_profile_set_libraries (GbpArduinoProfile *self,
                                   const char *const *libraries)
{
  g_return_if_fail (IDE_IS_CONFIG (self));

  if (ide_set_strv (&self->libraries, libraries))
    {
      g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_LIBRARIES]);
      ide_config_set_dirty (IDE_CONFIG (self), TRUE);
    }
}

gboolean
gbp_arduino_profile_add_library (GbpArduinoProfile *self,
                                 const char        *new_library)
{
  g_return_val_if_fail (GBP_IS_ARDUINO_PROFILE (self), FALSE);
  g_return_val_if_fail (new_library != NULL, FALSE);

  /* Check if library already exists */
  if (self->libraries != NULL)
    {
      for (guint i = 0; self->libraries[i] != NULL; i++)
        {
          if (ide_str_equal (self->libraries[i], new_library))
            return FALSE;
        }
    }

  if (ide_strv_add_to_set (&self->libraries, g_strdup (new_library)))
    {
      g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_LIBRARIES]);
      ide_config_set_dirty (IDE_CONFIG (self), TRUE);
      return TRUE;
    }

  return FALSE;
}

void
gbp_arduino_profile_remove_library (GbpArduinoProfile *self,
                                    const char        *library)
{
  g_return_if_fail (GBP_IS_ARDUINO_PROFILE (self));
  g_return_if_fail (library != NULL);

  if (ide_strv_remove_from_set (self->libraries, library))
    {
      g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_LIBRARIES]);
      ide_config_set_dirty (IDE_CONFIG (self), TRUE);
    }
}

const char *const *
gbp_arduino_profile_get_libraries (GbpArduinoProfile *self)
{
  g_return_val_if_fail (GBP_IS_ARDUINO_PROFILE (self), NULL);
  return (const char *const *) self->libraries;
}

gboolean
gbp_arduino_profile_add_platform (GbpArduinoProfile  *self,
                                  GbpArduinoPlatform *new_platform)
{
  guint n_items;
  gboolean platform_already_present = FALSE;

  g_return_val_if_fail (GBP_IS_ARDUINO_PROFILE (self), FALSE);
  g_return_val_if_fail (GBP_IS_ARDUINO_PLATFORM (new_platform), FALSE);

  n_items = g_list_model_get_n_items (G_LIST_MODEL (self->platforms));

  /* Make sure it isn't already present */
  for (guint i = 0; i < n_items; i++)
    {
      g_autoptr(GbpArduinoPlatform) platform = g_list_model_get_item (G_LIST_MODEL (self->platforms), i);
      const char *new_platform_name = gbp_arduino_platform_get_name (new_platform);
      const char *platform_name = gbp_arduino_platform_get_name (platform);

      if (ide_str_equal (platform_name, new_platform_name))
        {
          platform_already_present = TRUE;
          break;
        }
    }

  if (!platform_already_present)
    {
      g_list_store_append (self->platforms, new_platform);
      g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_PLATFORMS]);
      ide_config_set_dirty (IDE_CONFIG (self), TRUE);
      return TRUE;
    }

  return FALSE;
}

void
gbp_arduino_profile_remove_platform (GbpArduinoProfile  *self,
                                     GbpArduinoPlatform *platform)
{
  guint n_items;

  g_return_if_fail (GBP_IS_ARDUINO_PROFILE (self));
  g_return_if_fail (GBP_IS_ARDUINO_PLATFORM (platform));

  n_items = g_list_model_get_n_items (G_LIST_MODEL (self->platforms));

  for (guint i = 0; i < n_items; i++)
    {
      g_autoptr(GbpArduinoPlatform) item = g_list_model_get_item (G_LIST_MODEL (self->platforms), i);
      if (item == platform)
        {
          g_list_store_remove (self->platforms, i);
          g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_PLATFORMS]);
          ide_config_set_dirty (IDE_CONFIG (self), TRUE);
          break;
        }
    }
}

GListModel *
gbp_arduino_profile_get_platforms (GbpArduinoProfile *self)
{
  g_return_val_if_fail (GBP_IS_ARDUINO_PROFILE (self), NULL);
  return G_LIST_MODEL (self->platforms);
}

const char *
gbp_arduino_profile_get_port (GbpArduinoProfile *self)
{
  g_return_val_if_fail (GBP_IS_ARDUINO_PROFILE (self), NULL);
  return self->port;
}

void
gbp_arduino_profile_set_port (GbpArduinoProfile *self,
                              const char        *port)
{
  g_return_if_fail (GBP_IS_ARDUINO_PROFILE (self));

  if (g_set_str (&self->port, port) != 0)
    {
      g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_PORT]);
      ide_config_set_dirty (IDE_CONFIG (self), TRUE);
    }
}

const char *
gbp_arduino_profile_get_protocol (GbpArduinoProfile *self)
{
  g_return_val_if_fail (GBP_IS_ARDUINO_PROFILE (self), NULL);
  return self->protocol;
}

void
gbp_arduino_profile_set_protocol (GbpArduinoProfile *self,
                                  const char        *protocol)
{
  g_return_if_fail (GBP_IS_ARDUINO_PROFILE (self));

  if (g_set_str (&self->protocol, protocol) != 0)
    {
      g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_PROTOCOL]);
      ide_config_set_dirty (IDE_CONFIG (self), TRUE);
    }
}

const char *
gbp_arduino_profile_get_programmer (GbpArduinoProfile *self)
{
  g_return_val_if_fail (GBP_IS_ARDUINO_PROFILE (self), NULL);
  return self->programmer;
}

void
gbp_arduino_profile_set_programmer (GbpArduinoProfile *self,
                                    const char        *programmer)
{
  g_return_if_fail (GBP_IS_ARDUINO_PROFILE (self));

  if (g_set_str (&self->programmer, programmer) != 0)
    {
      g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_PROGRAMMER]);
      ide_config_set_dirty (IDE_CONFIG (self), TRUE);
    }
}

const char *
gbp_arduino_profile_get_fqbn (GbpArduinoProfile *self)
{
  g_return_val_if_fail (GBP_IS_ARDUINO_PROFILE (self), NULL);
  return self->fqbn;
}

void
gbp_arduino_profile_set_fqbn (GbpArduinoProfile *self,
                              const char        *fqbn)
{
  g_return_if_fail (GBP_IS_ARDUINO_PROFILE (self));

  if (g_set_str (&self->fqbn, fqbn) != 0)
    {
      g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_FQBN]);
      ide_config_set_dirty (IDE_CONFIG (self), TRUE);
    }
}

const char *
gbp_arduino_profile_get_notes (GbpArduinoProfile *self)
{
  g_return_val_if_fail (GBP_IS_ARDUINO_PROFILE (self), NULL);
  return self->notes;
}

void
gbp_arduino_profile_set_notes (GbpArduinoProfile *self,
                               const char        *notes)
{
  g_return_if_fail (GBP_IS_ARDUINO_PROFILE (self));

  if (g_set_str (&self->notes, notes))
    {
      g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_NOTES]);
      ide_config_set_dirty (IDE_CONFIG (self), TRUE);
    }
}

void
gbp_arduino_profile_reset (GbpArduinoProfile *self)
{
  gbp_arduino_profile_set_notes(self, "");
  gbp_arduino_profile_set_port(self, "");
  gbp_arduino_profile_set_protocol(self, "");
  gbp_arduino_profile_set_programmer(self, "");
  gbp_arduino_profile_set_fqbn(self, "");

  g_clear_pointer (&self->libraries, g_strfreev);

  if (self->platforms != NULL)
    {
      g_list_store_remove_all (self->platforms);
      g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_PLATFORMS]);
    }
}

