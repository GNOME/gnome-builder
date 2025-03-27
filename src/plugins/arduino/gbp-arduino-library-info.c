/*
 * gbp-arduino-library-info.c
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

#define G_LOG_DOMAIN "gbp-arduino-library"

#include "config.h"

#include "gbp-arduino-library-info.h"

struct _GbpArduinoLibraryInfo
{
  GObject parent_instance;

  char *name;
  char *author;
  char *description;
  char **versions;
};

enum
{
  PROP_0,
  PROP_NAME,
  PROP_AUTHOR,
  PROP_DESCRIPTION,
  PROP_VERSIONS,
  N_PROPS
};

static GParamSpec *properties[N_PROPS];

G_DEFINE_FINAL_TYPE (GbpArduinoLibraryInfo, gbp_arduino_library_info, G_TYPE_OBJECT)

static void
gbp_arduino_library_info_finalize (GObject *object)
{
  GbpArduinoLibraryInfo *self = GBP_ARDUINO_LIBRARY_INFO (object);

  g_clear_pointer (&self->name, g_free);
  g_clear_pointer (&self->author, g_free);
  g_clear_pointer (&self->description, g_free);
  g_clear_pointer (&self->versions, g_strfreev);

  G_OBJECT_CLASS (gbp_arduino_library_info_parent_class)->finalize (object);
}

static void
gbp_arduino_library_info_get_property (GObject    *object,
                                       guint       prop_id,
                                       GValue     *value,
                                       GParamSpec *pspec)
{
  GbpArduinoLibraryInfo *self = GBP_ARDUINO_LIBRARY_INFO (object);

  switch (prop_id)
    {
    case PROP_NAME:
      g_value_set_string (value, self->name);
      break;
    case PROP_AUTHOR:
      g_value_set_string (value, self->author);
      break;
    case PROP_DESCRIPTION:
      g_value_set_string (value, self->description);
      break;
    case PROP_VERSIONS:
      g_value_set_boxed (value, self->versions);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
gbp_arduino_library_info_set_property (GObject      *object,
                                       guint         prop_id,
                                       const GValue *value,
                                       GParamSpec   *pspec)
{
  GbpArduinoLibraryInfo *self = GBP_ARDUINO_LIBRARY_INFO (object);

  switch (prop_id)
    {
    case PROP_NAME:
      g_set_str (&self->name, g_value_get_string (value));
      break;
    case PROP_AUTHOR:
      g_set_str (&self->author, g_value_get_string (value));
      break;
    case PROP_DESCRIPTION:
      g_set_str (&self->description, g_value_get_string (value));
      break;
    case PROP_VERSIONS:
      self->versions = g_value_dup_boxed (value);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
gbp_arduino_library_info_class_init (GbpArduinoLibraryInfoClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = gbp_arduino_library_info_finalize;
  object_class->get_property = gbp_arduino_library_info_get_property;
  object_class->set_property = gbp_arduino_library_info_set_property;

  properties[PROP_NAME] =
      g_param_spec_string ("name", NULL, NULL,
                           NULL,
                           G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS);

  properties[PROP_AUTHOR] =
      g_param_spec_string ("author", NULL, NULL,
                           NULL,
                           G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS);

  properties[PROP_DESCRIPTION] =
      g_param_spec_string ("description", NULL, NULL,
                           NULL,
                           G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS);

  properties[PROP_VERSIONS] =
      g_param_spec_boxed ("versions", NULL, NULL,
                          G_TYPE_STRV,
                          G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS);

  g_object_class_install_properties (object_class, N_PROPS, properties);
}

static void
gbp_arduino_library_info_init (GbpArduinoLibraryInfo *self)
{
}

GbpArduinoLibraryInfo *
gbp_arduino_library_info_new (const char *name,
                              const char *author,
                              const char *description,
                              const char **versions)
{
  return g_object_new (GBP_TYPE_ARDUINO_LIBRARY_INFO,
                       "name", name,
                       "author", author,
                       "description", description,
                       "versions", versions,
                       NULL);
}

const char *
gbp_arduino_library_info_get_name (GbpArduinoLibraryInfo *self)
{
  g_return_val_if_fail (GBP_IS_ARDUINO_LIBRARY_INFO (self), NULL);
  return self->name;
}

const char *
gbp_arduino_library_info_get_author (GbpArduinoLibraryInfo *self)
{
  g_return_val_if_fail (GBP_IS_ARDUINO_LIBRARY_INFO (self), NULL);
  return self->author;
}

const char *
gbp_arduino_library_info_get_description (GbpArduinoLibraryInfo *self)
{
  g_return_val_if_fail (GBP_IS_ARDUINO_LIBRARY_INFO (self), NULL);
  return self->description;
}

const char *const *
gbp_arduino_library_info_get_versions (GbpArduinoLibraryInfo *self)
{
  g_return_val_if_fail (GBP_IS_ARDUINO_LIBRARY_INFO (self), NULL);
  return (const char *const *) self->versions;
}

const char *
gbp_arduino_library_info_get_latest_version (GbpArduinoLibraryInfo *self)
{
  g_return_val_if_fail (GBP_IS_ARDUINO_LIBRARY_INFO (self), NULL);

  if (self->versions && self->versions[0] != NULL)
    {
      gsize i;
      for (i = 0; self->versions[i + 1] != NULL; i++);
      return self->versions[i];
    }

  return NULL;
}

