/* gbp-make-build-system.c
 *
 * Copyright 2017 Matthew Leeds <mleeds@redhat.com>
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

#define G_LOG_DOMAIN "gbp-make-build-system"

#include "config.h"

#include <libide-foundry.h>

#include "gbp-make-build-system.h"

struct _GbpMakeBuildSystem
{
  IdeObject  parent_instance;
  GFile     *project_file;
  GFile     *make_dir;
};

enum {
  PROP_0,
  PROP_PROJECT_FILE,
  N_PROPS
};

static char *
gbp_make_build_system_get_id (IdeBuildSystem *build_system)
{
  return g_strdup ("make");
}

static char *
gbp_make_build_system_get_display_name (IdeBuildSystem *build_system)
{
  return g_strdup ("Make");
}

static int
gbp_make_build_system_get_priority (IdeBuildSystem *build_system)
{
  return 0;
}

static char *
gbp_make_build_system_get_builddir (IdeBuildSystem *build_system,
                                    IdePipeline    *pipeline)
{
  GbpMakeBuildSystem *self = (GbpMakeBuildSystem *)build_system;

  g_assert (GBP_IS_MAKE_BUILD_SYSTEM (self));
  g_assert (IDE_IS_PIPELINE (pipeline));
  g_assert (G_IS_FILE (self->make_dir));

  return g_file_get_path (self->make_dir);
}

static void
build_system_iface_init (IdeBuildSystemInterface *iface)
{
  iface->get_id = gbp_make_build_system_get_id;
  iface->get_display_name = gbp_make_build_system_get_display_name;
  iface->get_priority = gbp_make_build_system_get_priority;
  iface->get_builddir = gbp_make_build_system_get_builddir;
}

G_DEFINE_FINAL_TYPE_WITH_CODE (GbpMakeBuildSystem, gbp_make_build_system, IDE_TYPE_OBJECT,
                               G_IMPLEMENT_INTERFACE (IDE_TYPE_BUILD_SYSTEM, build_system_iface_init))

static GParamSpec *properties [N_PROPS];

static void
gbp_make_build_system_set_project_file (GbpMakeBuildSystem *self,
                                        GFile              *file)
{
  g_assert (GBP_IS_MAKE_BUILD_SYSTEM (self));
  g_assert (!file || G_IS_FILE (file));

  if (g_set_object (&self->project_file, file))
    {
      g_clear_object (&self->make_dir);

      if (file != NULL)
        {
          if (g_file_query_file_type (file, 0, NULL) != G_FILE_TYPE_DIRECTORY)
            self->make_dir = g_file_get_parent (file);
          else
            self->make_dir = g_object_ref (file);
        }
    }
}

static void
gbp_make_build_system_destroy (IdeObject *object)
{
  GbpMakeBuildSystem *self = (GbpMakeBuildSystem *)object;

  g_clear_object (&self->project_file);
  g_clear_object (&self->make_dir);

  IDE_OBJECT_CLASS (gbp_make_build_system_parent_class)->destroy (object);
}

static void
gbp_make_build_system_get_property (GObject    *object,
                                     guint       prop_id,
                                     GValue     *value,
                                     GParamSpec *pspec)
{
  GbpMakeBuildSystem *self = GBP_MAKE_BUILD_SYSTEM (object);

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
gbp_make_build_system_set_property (GObject      *object,
                                     guint         prop_id,
                                     const GValue *value,
                                     GParamSpec   *pspec)
{
  GbpMakeBuildSystem *self = GBP_MAKE_BUILD_SYSTEM (object);

  switch (prop_id)
    {
    case PROP_PROJECT_FILE:
      gbp_make_build_system_set_project_file (self, g_value_get_object (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
gbp_make_build_system_class_init (GbpMakeBuildSystemClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  IdeObjectClass *i_object_class = IDE_OBJECT_CLASS (klass);

  object_class->get_property = gbp_make_build_system_get_property;
  object_class->set_property = gbp_make_build_system_set_property;

  i_object_class->destroy = gbp_make_build_system_destroy;

  properties [PROP_PROJECT_FILE] =
    g_param_spec_object ("project-file",
                         "Project File",
                         "The project file (Make.toml)",
                         G_TYPE_FILE,
                         (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, N_PROPS, properties);
}

static void
gbp_make_build_system_init (GbpMakeBuildSystem *self)
{
}
