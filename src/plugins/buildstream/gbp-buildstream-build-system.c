/* gbp-buildstream-build-system.c
 *
 * Copyright 2016-2022 Christian Hergert <chergert@redhat.com>
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

#define G_LOG_DOMAIN "gbp-buildstream-build-system"

#include "config.h"

#include <libide-foundry.h>

#include "gbp-buildstream-build-system.h"

struct _GbpBuildstreamBuildSystem
{
  IdeObject  parent_instance;
  GFile     *project_file;
};

enum {
  PROP_0,
  PROP_PROJECT_FILE,
  N_PROPS
};

static char *
gbp_buildstream_build_system_get_id (IdeBuildSystem *build_system)
{
  return g_strdup ("buildstream");
}

static char *
gbp_buildstream_build_system_get_display_name (IdeBuildSystem *build_system)
{
  return g_strdup ("BuildStream");
}

static int
gbp_buildstream_build_system_get_priority (IdeBuildSystem *build_system)
{
  return 2000;
}

static void
build_system_iface_init (IdeBuildSystemInterface *iface)
{
  iface->get_id = gbp_buildstream_build_system_get_id;
  iface->get_display_name = gbp_buildstream_build_system_get_display_name;
  iface->get_priority = gbp_buildstream_build_system_get_priority;
}

G_DEFINE_FINAL_TYPE_WITH_CODE (GbpBuildstreamBuildSystem, gbp_buildstream_build_system, IDE_TYPE_OBJECT,
                               G_IMPLEMENT_INTERFACE (IDE_TYPE_BUILD_SYSTEM, build_system_iface_init))

static GParamSpec *properties [N_PROPS];

static void
gbp_buildstream_build_system_destroy (IdeObject *object)
{
  GbpBuildstreamBuildSystem *self = (GbpBuildstreamBuildSystem *)object;

  g_clear_object (&self->project_file);

  IDE_OBJECT_CLASS (gbp_buildstream_build_system_parent_class)->destroy (object);
}

static void
gbp_buildstream_build_system_get_property (GObject    *object,
                                           guint       prop_id,
                                           GValue     *value,
                                           GParamSpec *pspec)
{
  GbpBuildstreamBuildSystem *self = GBP_BUILDSTREAM_BUILD_SYSTEM (object);

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
gbp_buildstream_build_system_set_property (GObject      *object,
                                           guint         prop_id,
                                           const GValue *value,
                                           GParamSpec   *pspec)
{
  GbpBuildstreamBuildSystem *self = GBP_BUILDSTREAM_BUILD_SYSTEM (object);

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
gbp_buildstream_build_system_class_init (GbpBuildstreamBuildSystemClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  IdeObjectClass *i_object_class = IDE_OBJECT_CLASS (klass);

  object_class->get_property = gbp_buildstream_build_system_get_property;
  object_class->set_property = gbp_buildstream_build_system_set_property;

  i_object_class->destroy = gbp_buildstream_build_system_destroy;

  properties [PROP_PROJECT_FILE] =
    g_param_spec_object ("project-file",
                         "Project File",
                         "The project file (Buildstream.toml)",
                         G_TYPE_FILE,
                         (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, N_PROPS, properties);
}

static void
gbp_buildstream_build_system_init (GbpBuildstreamBuildSystem *self)
{
}
