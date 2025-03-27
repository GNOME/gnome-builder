/*
 * gbp-arduino-platform.c
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

#define G_LOG_DOMAIN "gbp-arduino-platform"

#include "config.h"

#include "gbp-arduino-platform.h"

struct _GbpArduinoPlatform
{
  GObject parent_instance;

  char *name;
  char *version;
  char *index_url;
};

enum
{
  PROP_0,
  PROP_NAME,
  PROP_VERSION,
  PROP_INDEX_URL,
  N_PROPS
};

G_DEFINE_FINAL_TYPE (GbpArduinoPlatform, gbp_arduino_platform, G_TYPE_OBJECT)

static GParamSpec *properties[N_PROPS];

static void
gbp_arduino_platform_finalize (GObject *object)
{
  GbpArduinoPlatform *self = GBP_ARDUINO_PLATFORM (object);

  g_clear_pointer (&self->name, g_free);
  g_clear_pointer (&self->version, g_free);
  g_clear_pointer (&self->index_url, g_free);

  G_OBJECT_CLASS (gbp_arduino_platform_parent_class)->finalize (object);
}

static void
gbp_arduino_platform_get_property (GObject   *object,
                                   guint      prop_id,
                                   GValue     *value,
                                   GParamSpec *pspec)
{
  GbpArduinoPlatform *self = GBP_ARDUINO_PLATFORM (object);

  switch (prop_id)
    {
    case PROP_NAME:
      g_value_set_string (value, self->name);
      break;
    case PROP_VERSION:
      g_value_set_string (value, self->version);
      break;
    case PROP_INDEX_URL:
      g_value_set_string (value, self->index_url);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
gbp_arduino_platform_set_property (GObject      *object,
                                   guint         prop_id,
                                   const GValue *value,
                                   GParamSpec   *pspec)
{
  GbpArduinoPlatform *self = GBP_ARDUINO_PLATFORM (object);

  switch (prop_id)
    {
    case PROP_NAME:
      g_set_str (&self->name, g_value_get_string (value));
      break;
    case PROP_VERSION:
      g_set_str (&self->version, g_value_get_string (value));
      break;
    case PROP_INDEX_URL:
      g_set_str (&self->index_url, g_value_get_string (value));
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
gbp_arduino_platform_class_init (GbpArduinoPlatformClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = gbp_arduino_platform_finalize;
  object_class->get_property = gbp_arduino_platform_get_property;
  object_class->set_property = gbp_arduino_platform_set_property;

  properties[PROP_NAME] =
      g_param_spec_string ("name", NULL, NULL,
                           NULL,
                           G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);

  properties[PROP_VERSION] =
      g_param_spec_string ("version", NULL, NULL,
                           NULL,
                           G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);

  properties[PROP_INDEX_URL] =
      g_param_spec_string ("index-url", NULL, NULL,
                           NULL,
                           G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);

  g_object_class_install_properties (object_class, N_PROPS, properties);
}

static void
gbp_arduino_platform_init (GbpArduinoPlatform *self)
{
}

const char *
gbp_arduino_platform_get_name (GbpArduinoPlatform *self)
{
  g_return_val_if_fail (GBP_IS_ARDUINO_PLATFORM (self), NULL);
  return self->name;
}

const char *
gbp_arduino_platform_get_version (GbpArduinoPlatform *self)
{
  g_return_val_if_fail (GBP_IS_ARDUINO_PLATFORM (self), NULL);
  return self->version;
}

const char *
gbp_arduino_platform_get_index_url (GbpArduinoPlatform *self)
{
  g_return_val_if_fail (GBP_IS_ARDUINO_PLATFORM (self), NULL);
  return self->index_url;
}

void
gbp_arduino_platform_set_index_url (GbpArduinoPlatform *self,
                                    const char         *index_url)
{
  g_return_if_fail (GBP_IS_ARDUINO_PLATFORM (self));

  if (g_set_str (&self->index_url, index_url))
    g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_INDEX_URL]);
}

GbpArduinoPlatform *
gbp_arduino_platform_new (const char *name,
                          const char *version,
                          const char *index_url)
{
  return g_object_new (GBP_TYPE_ARDUINO_PLATFORM,
                       "name", name,
                       "version", version,
                       "index-url", index_url,
                       NULL);
}

