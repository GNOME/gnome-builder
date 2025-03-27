/*
 * gbp-arduino-build-system.c
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

#define G_LOG_DOMAIN "gbp-arduino-build-system"
#define ARDUINO "arduino-cli"

#include "config.h"

#include "gbp-arduino-build-system.h"

struct _GbpArduinoBuildSystem
{
  IdeObject parent_instance;

  GFile *project_file;
};

enum
{
  PROP_0,
  PROP_PROJECT_FILE,
  N_PROPS
};

static char *
gbp_arduino_build_system_get_id (IdeBuildSystem *build_system)
{
  return g_strdup (ARDUINO);
}

static char *
gbp_arduino_build_system_get_display_name (IdeBuildSystem *build_system)
{
  return g_strdup ("Arduino");
}

static int
gbp_arduino_build_system_get_priority (IdeBuildSystem *build_system)
{
  return -200;
}

static gboolean
gbp_arduino_build_system_supports_language (IdeBuildSystem *build_system,
                                            const char     *language)
{
  return g_strv_contains (IDE_STRV_INIT ("c", "cpp", "chdr", "cpphdr"), language);
}

static void
build_system_iface_init (IdeBuildSystemInterface *iface)
{
  iface->get_id = gbp_arduino_build_system_get_id;
  iface->get_display_name = gbp_arduino_build_system_get_display_name;
  iface->get_priority = gbp_arduino_build_system_get_priority;
  iface->supports_language = gbp_arduino_build_system_supports_language;
}

G_DEFINE_FINAL_TYPE_WITH_CODE (GbpArduinoBuildSystem, gbp_arduino_build_system, IDE_TYPE_OBJECT, G_IMPLEMENT_INTERFACE (IDE_TYPE_BUILD_SYSTEM, build_system_iface_init))

static GParamSpec *properties[N_PROPS];

static void
gbp_arduino_build_system_destroy (IdeObject *object)
{
  GbpArduinoBuildSystem *self = (GbpArduinoBuildSystem *) object;

  g_clear_object (&self->project_file);

  IDE_OBJECT_CLASS (gbp_arduino_build_system_parent_class)->destroy (object);
}

static void
gbp_arduino_build_system_get_property (GObject    *object,
                                       guint       prop_id,
                                       GValue     *value,
                                       GParamSpec *pspec)
{
  GbpArduinoBuildSystem *self = GBP_ARDUINO_BUILD_SYSTEM (object);

  switch (prop_id)
    {
    case PROP_PROJECT_FILE:
      g_value_set_object (value, self->project_file);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
gbp_arduino_build_system_set_property (GObject      *object,
                                       guint         prop_id,
                                       const GValue *value,
                                       GParamSpec   *pspec)
{
  GbpArduinoBuildSystem *self = GBP_ARDUINO_BUILD_SYSTEM (object);

  switch (prop_id)
    {
    case PROP_PROJECT_FILE:
      g_set_object (&self->project_file, g_value_get_object (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
gbp_arduino_build_system_class_init (GbpArduinoBuildSystemClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  IdeObjectClass *i_object_class = IDE_OBJECT_CLASS (klass);

  object_class->get_property = gbp_arduino_build_system_get_property;
  object_class->set_property = gbp_arduino_build_system_set_property;

  i_object_class->destroy = gbp_arduino_build_system_destroy;

  properties[PROP_PROJECT_FILE] =
      g_param_spec_object ("project-file", NULL, NULL,
                           G_TYPE_FILE,
                           G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);

  g_object_class_install_properties (object_class, N_PROPS, properties);
}

static void
gbp_arduino_build_system_init (GbpArduinoBuildSystem *self)
{
}

char *
gbp_arduino_build_system_get_project_dir (GbpArduinoBuildSystem *self)
{
  g_autoptr (GFile) workdir = NULL;
  g_autofree char *base = NULL;
  IdeContext *context;

  g_return_val_if_fail (GBP_IS_ARDUINO_BUILD_SYSTEM (self), NULL);

  context = ide_object_get_context (IDE_OBJECT (self));
  workdir = ide_context_ref_workdir (context);

  if (self->project_file == NULL)
    {
      return g_strdup (g_file_peek_path (workdir));
    }

  base = g_file_get_basename (self->project_file);

  if (strcasecmp (base, "sketch.yaml") == 0 || strcasecmp (base, "sketch.yml") == 0)
    {
      g_autoptr (GFile) parent = g_file_get_parent (self->project_file);
      return g_file_get_path (parent);
    }

  return g_file_get_path (self->project_file);
}

char *
gbp_arduino_build_system_get_sketch_yaml_path (GbpArduinoBuildSystem *self)
{
  g_autofree char *base = NULL;
  g_autoptr (GFile) child = NULL;

  g_return_val_if_fail (GBP_IS_ARDUINO_BUILD_SYSTEM (self), NULL);

  base = g_file_get_basename (self->project_file);
  if (strcasecmp (base, "sketch.yaml") == 0 || strcasecmp (base, "sketch.yml") == 0)
    {
      return g_file_get_path (self->project_file);
    }

  child = g_file_get_child (self->project_file, "sketch.yaml");
  return g_file_get_path (child);
}

char *
gbp_arduino_build_system_locate_arduino (GbpArduinoBuildSystem *self)
{
  g_return_val_if_fail (!self || GBP_IS_ARDUINO_BUILD_SYSTEM (self), NULL);

  return g_strdup (ARDUINO);
}

