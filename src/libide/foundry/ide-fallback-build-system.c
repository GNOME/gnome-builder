/* ide-fallback-build-system.c
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

#define G_LOG_DOMAIN "fallback-build-system"

#include "config.h"

#include <glib/gi18n.h>

#include "ide-fallback-build-system.h"

struct _IdeFallbackBuildSystem
{
  IdeObject  parent_instance;
  GFile     *project_file;
};

static void build_system_init (IdeBuildSystemInterface *iface);

G_DEFINE_FINAL_TYPE_WITH_CODE (IdeFallbackBuildSystem,
                         ide_fallback_build_system,
                         IDE_TYPE_OBJECT,
                         G_IMPLEMENT_INTERFACE (IDE_TYPE_BUILD_SYSTEM, build_system_init))

enum {
  PROP_0,
  PROP_PROJECT_FILE,
  LAST_PROP
};

static GParamSpec *properties [LAST_PROP];

static void
ide_fallback_build_system_finalize (GObject *object)
{
  IdeFallbackBuildSystem *self = (IdeFallbackBuildSystem *)object;

  g_clear_object (&self->project_file);

  G_OBJECT_CLASS (ide_fallback_build_system_parent_class)->finalize (object);
}

static void
ide_fallback_build_system_get_property (GObject    *object,
                                        guint       prop_id,
                                        GValue     *value,
                                        GParamSpec *pspec)
{
  IdeFallbackBuildSystem *self = IDE_FALLBACK_BUILD_SYSTEM (object);

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
ide_fallback_build_system_set_property (GObject      *object,
                                        guint         prop_id,
                                        const GValue *value,
                                        GParamSpec   *pspec)
{
  IdeFallbackBuildSystem *self = IDE_FALLBACK_BUILD_SYSTEM (object);

  switch (prop_id)
    {
    case PROP_PROJECT_FILE:
      self->project_file = g_value_dup_object (value);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
ide_fallback_build_system_class_init (IdeFallbackBuildSystemClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = ide_fallback_build_system_finalize;
  object_class->get_property = ide_fallback_build_system_get_property;
  object_class->set_property = ide_fallback_build_system_set_property;

  /**
   * IdeFallbackBuildSystem:project-file:
   *
   * The "project-file" property is the primary file representing the
   * projects build system.
   */
  properties [PROP_PROJECT_FILE] =
    g_param_spec_object ("project-file",
                         "Project File",
                         "The path of the project file.",
                         G_TYPE_FILE,
                         (G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, LAST_PROP, properties);
}

static void
ide_fallback_build_system_init (IdeFallbackBuildSystem *self)
{
}

static gint
ide_fallback_build_system_get_priority (IdeBuildSystem *build_system)
{
  return 1000000;
}

static gchar *
ide_fallback_build_system_get_id (IdeBuildSystem *build_system)
{
  return g_strdup ("fallback");
}

static gchar *
ide_fallback_build_system_get_display_name (IdeBuildSystem *build_system)
{
  return g_strdup (_("Fallback"));
}

static void
build_system_init (IdeBuildSystemInterface *iface)
{
  iface->get_priority = ide_fallback_build_system_get_priority;
  iface->get_id = ide_fallback_build_system_get_id;
  iface->get_display_name = ide_fallback_build_system_get_display_name;
}

/**
 * ide_fallback_build_system_new:
 *
 * Creates a new #IdeFallbackBuildSystem.
 *
 * Returns: (transfer full): an #IdeBuildSystem
 */
IdeBuildSystem *
ide_fallback_build_system_new (void)
{
  return g_object_new (IDE_TYPE_FALLBACK_BUILD_SYSTEM, NULL);
}
