/*
 * gbp-arduino-platform-info.c
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

#define G_LOG_DOMAIN "gbp-arduino-platform-info"

#include "config.h"

#include <libide-core.h>

#include "gbp-arduino-platform-info.h"

struct _GbpArduinoPlatformInfo
{
  GObject parent_instance;

  char  *name;
  char  *version;
  char **supported_fqbns;
  char  *maintainer;
  char  *id;
  char  *installed_version;
};

enum
{
  PROP_0,
  PROP_NAME,
  PROP_VERSION,
  PROP_SUPPORTED_FQBNS,
  PROP_MAINTAINER,
  PROP_ID,
  PROP_INSTALLED_VERSION,
  N_PROPS
};

G_DEFINE_FINAL_TYPE (GbpArduinoPlatformInfo, gbp_arduino_platform_info, G_TYPE_OBJECT)

static GParamSpec *properties[N_PROPS];

static void
gbp_arduino_platform_info_finalize (GObject *object)
{
  GbpArduinoPlatformInfo *self = GBP_ARDUINO_PLATFORM_INFO (object);

  g_clear_pointer (&self->name, g_free);
  g_clear_pointer (&self->version, g_free);
  g_clear_pointer (&self->supported_fqbns, g_strfreev);
  g_clear_pointer (&self->maintainer, g_free);
  g_clear_pointer (&self->id, g_free);
  g_clear_pointer (&self->installed_version, g_free);

  G_OBJECT_CLASS (gbp_arduino_platform_info_parent_class)->finalize (object);
}

static void
gbp_arduino_platform_info_get_property (GObject    *object,
                                      guint       prop_id,
                                      GValue     *value,
                                      GParamSpec *pspec)
{
  GbpArduinoPlatformInfo *self = GBP_ARDUINO_PLATFORM_INFO (object);

  switch (prop_id)
    {
    case PROP_NAME:
      g_value_set_string (value, self->name);
      break;
    case PROP_VERSION:
      g_value_set_string (value, self->version);
      break;
    case PROP_SUPPORTED_FQBNS:
      g_value_set_boxed (value, (const char *const *) self->supported_fqbns);
      break;
    case PROP_MAINTAINER:
      g_value_set_string (value, self->maintainer);
      break;
    case PROP_ID:
      g_value_set_string (value, self->id);
      break;
    case PROP_INSTALLED_VERSION:
      g_value_set_string (value, self->installed_version);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
gbp_arduino_platform_info_class_init (GbpArduinoPlatformInfoClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = gbp_arduino_platform_info_finalize;
  object_class->get_property = gbp_arduino_platform_info_get_property;

  properties[PROP_NAME] =
      g_param_spec_string ("name", NULL, NULL,
                           NULL,
                           G_PARAM_READABLE | G_PARAM_STATIC_STRINGS);

  properties[PROP_VERSION] =
      g_param_spec_string ("version", NULL, NULL,
                           NULL,
                           G_PARAM_READABLE | G_PARAM_STATIC_STRINGS);

  properties[PROP_SUPPORTED_FQBNS] =
      g_param_spec_boxed ("supported-fqbns", NULL,
                          NULL,
                          G_TYPE_STRV,
                          G_PARAM_READABLE | G_PARAM_STATIC_STRINGS);

  properties[PROP_MAINTAINER] =
      g_param_spec_string ("maintainer", NULL, NULL,
                           NULL,
                           G_PARAM_READABLE | G_PARAM_STATIC_STRINGS);

  properties[PROP_ID] =
      g_param_spec_string ("id", NULL, NULL,
                           NULL,
                           G_PARAM_READABLE | G_PARAM_STATIC_STRINGS);

  properties[PROP_INSTALLED_VERSION] =
      g_param_spec_string ("installed-version", NULL, NULL,
                           NULL,
                           G_PARAM_READABLE | G_PARAM_STATIC_STRINGS);

  g_object_class_install_properties (object_class, N_PROPS, properties);
}

static void
gbp_arduino_platform_info_init (GbpArduinoPlatformInfo *self)
{
}

GbpArduinoPlatformInfo *
gbp_arduino_platform_info_new (const char        *name,
                               const char        *version,
                               const char *const *supported_fqbns,
                               const char        *maintainer,
                               const char        *id,
                               const char        *installed_version)
{
  GbpArduinoPlatformInfo *self;

  self = g_object_new (GBP_TYPE_ARDUINO_PLATFORM_INFO, NULL);
  self->name = g_strdup (name);
  self->version = g_strdup (version);
  self->maintainer = g_strdup (maintainer);
  self->id = g_strdup (id);
  self->installed_version = g_strdup (installed_version);

  ide_set_strv (&self->supported_fqbns, supported_fqbns);

  return self;
}

const char *
gbp_arduino_platform_info_get_name (GbpArduinoPlatformInfo *self)
{
  g_return_val_if_fail (GBP_IS_ARDUINO_PLATFORM_INFO (self), NULL);
  return self->name;
}

const char *
gbp_arduino_platform_info_get_version (GbpArduinoPlatformInfo *self)
{
  g_return_val_if_fail (GBP_IS_ARDUINO_PLATFORM_INFO (self), NULL);
  return self->version;
}

const char *const *
gbp_arduino_platform_info_get_supported_fqbns (GbpArduinoPlatformInfo *self)
{
  g_return_val_if_fail (GBP_IS_ARDUINO_PLATFORM_INFO (self), NULL);
  return (const char *const *) self->supported_fqbns;
}

const char *
gbp_arduino_platform_info_get_maintainer (GbpArduinoPlatformInfo *self)
{
  g_return_val_if_fail (GBP_IS_ARDUINO_PLATFORM_INFO (self), NULL);
  return self->maintainer;
}

const char *
gbp_arduino_platform_info_get_id (GbpArduinoPlatformInfo *self)
{
  g_return_val_if_fail (GBP_IS_ARDUINO_PLATFORM_INFO (self), NULL);
  return self->id;
}

const char *
gbp_arduino_platform_info_get_installed_version (GbpArduinoPlatformInfo *self)
{
  g_return_val_if_fail (GBP_IS_ARDUINO_PLATFORM_INFO (self), NULL);
  return self->installed_version;
}
