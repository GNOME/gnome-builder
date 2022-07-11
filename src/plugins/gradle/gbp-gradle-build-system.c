/* gbp-gradle-build-system.c
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

#define G_LOG_DOMAIN "gbp-gradle-build-system"

#include "config.h"

#include "gbp-gradle-build-system.h"

struct _GbpGradleBuildSystem
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
gbp_gradle_build_system_get_id (IdeBuildSystem *build_system)
{
  return g_strdup ("gradle");
}

static char *
gbp_gradle_build_system_get_display_name (IdeBuildSystem *build_system)
{
  return g_strdup ("Gradle");
}

static int
gbp_gradle_build_system_get_priority (IdeBuildSystem *build_system)
{
  return 2000;
}

static void
build_system_iface_init (IdeBuildSystemInterface *iface)
{
  iface->get_id = gbp_gradle_build_system_get_id;
  iface->get_display_name = gbp_gradle_build_system_get_display_name;
  iface->get_priority = gbp_gradle_build_system_get_priority;
}

G_DEFINE_FINAL_TYPE_WITH_CODE (GbpGradleBuildSystem, gbp_gradle_build_system, IDE_TYPE_OBJECT,
                               G_IMPLEMENT_INTERFACE (IDE_TYPE_BUILD_SYSTEM, build_system_iface_init))

static GParamSpec *properties [N_PROPS];

static void
gbp_gradle_build_system_dispose (GObject *object)
{
  GbpGradleBuildSystem *self = (GbpGradleBuildSystem *)object;

  g_clear_object (&self->project_file);

  G_OBJECT_CLASS (gbp_gradle_build_system_parent_class)->dispose (object);
}

static void
gbp_gradle_build_system_get_property (GObject    *object,
                                      guint       prop_id,
                                      GValue     *value,
                                      GParamSpec *pspec)
{
  GbpGradleBuildSystem *self = GBP_GRADLE_BUILD_SYSTEM (object);

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
gbp_gradle_build_system_set_property (GObject      *object,
                                      guint         prop_id,
                                      const GValue *value,
                                      GParamSpec   *pspec)
{
  GbpGradleBuildSystem *self = GBP_GRADLE_BUILD_SYSTEM (object);

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
gbp_gradle_build_system_class_init (GbpGradleBuildSystemClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->dispose = gbp_gradle_build_system_dispose;
  object_class->get_property = gbp_gradle_build_system_get_property;
  object_class->set_property = gbp_gradle_build_system_set_property;

  properties [PROP_PROJECT_FILE] =
    g_param_spec_object ("project-file",
                         "Project File",
                         "The project file (Gradle.toml)",
                         G_TYPE_FILE,
                         (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, N_PROPS, properties);
}

static void
gbp_gradle_build_system_init (GbpGradleBuildSystem *self)
{
}

char *
gbp_gradle_build_system_get_project_dir (GbpGradleBuildSystem *self)
{
  g_autoptr(GFile) workdir = NULL;
  g_autofree char *base = NULL;
  IdeContext *context;

  g_return_val_if_fail (GBP_IS_GRADLE_BUILD_SYSTEM (self), NULL);

  context = ide_object_get_context (IDE_OBJECT (self));
  workdir = ide_context_ref_workdir (context);

  if (self->project_file == NULL)
    return g_strdup (g_file_peek_path (workdir));

  base = g_file_get_basename (self->project_file);

  if (strcasecmp (base, "build.gradle") == 0)
    {
      g_autoptr(GFile) parent = g_file_get_parent (self->project_file);
      return g_file_get_path (parent);
    }

  return g_file_get_path (self->project_file);
}
