/* ide-buildconfig-build-target.c
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

#define G_LOG_DOMAIN "ide-buildconfig-build-target"

#include <libide-foundry.h>

#include "ide-buildconfig-build-target.h"

struct _IdeBuildconfigBuildTarget
{
  IdeObject parent;

  char **command;
};

enum {
  PROP_0,
  PROP_COMMAND,
  N_PROPS
};

static char *
ide_buildconfig_build_target_get_display_name (IdeBuildTarget *build_target)
{
  IdeBuildconfigBuildTarget *self = IDE_BUILDCONFIG_BUILD_TARGET (build_target);

  if (self->command == NULL || self->command[0] == NULL)
    return NULL;

  return g_strdup_printf ("%s <span fgalpha='32767' size='smaller'>(.buildconfig)</span>", self->command[0]);
}

static char *
ide_buildconfig_build_target_get_name (IdeBuildTarget *build_target)
{
  IdeBuildconfigBuildTarget *self = IDE_BUILDCONFIG_BUILD_TARGET (build_target);

  if (self->command == NULL || self->command[0] == NULL)
    return NULL;

  return g_strdup (self->command[0]);
}

static char **
ide_buildconfig_build_target_get_argv (IdeBuildTarget *build_target)
{
  IdeBuildconfigBuildTarget *self = IDE_BUILDCONFIG_BUILD_TARGET (build_target);

  return g_strdupv (self->command);
}

static GFile *
ide_buildconfig_build_target_get_install_directory (IdeBuildTarget *build_target)
{
  return NULL;
}

static gint
ide_buildconfig_build_target_get_priority (IdeBuildTarget *build_target)
{
  return -50;
}

static void
build_target_iface_init (IdeBuildTargetInterface *iface)
{
  iface->get_display_name = ide_buildconfig_build_target_get_display_name;
  iface->get_name = ide_buildconfig_build_target_get_name;
  iface->get_argv = ide_buildconfig_build_target_get_argv;
  iface->get_install_directory = ide_buildconfig_build_target_get_install_directory;
  iface->get_priority = ide_buildconfig_build_target_get_priority;
}

G_DEFINE_FINAL_TYPE_WITH_CODE (IdeBuildconfigBuildTarget, ide_buildconfig_build_target, IDE_TYPE_OBJECT,
                               G_IMPLEMENT_INTERFACE (IDE_TYPE_BUILD_TARGET, build_target_iface_init))

static GParamSpec *properties [N_PROPS];

static void
ide_buildconfig_build_target_finalize (GObject *object)
{
  IdeBuildconfigBuildTarget *self = (IdeBuildconfigBuildTarget *)object;

  g_clear_pointer (&self->command, g_strfreev);

  G_OBJECT_CLASS (ide_buildconfig_build_target_parent_class)->finalize (object);
}

static void
ide_buildconfig_build_target_get_property (GObject    *object,
                                       guint       prop_id,
                                       GValue     *value,
                                       GParamSpec *pspec)
{
  IdeBuildconfigBuildTarget *self = IDE_BUILDCONFIG_BUILD_TARGET (object);

  switch (prop_id)
    {
    case PROP_COMMAND:
      g_value_set_boxed (value, self->command);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
ide_buildconfig_build_target_set_property (GObject      *object,
                                       guint         prop_id,
                                       const GValue *value,
                                       GParamSpec   *pspec)
{
  IdeBuildconfigBuildTarget *self = IDE_BUILDCONFIG_BUILD_TARGET (object);

  switch (prop_id)
    {
    case PROP_COMMAND:
      self->command = g_value_dup_boxed (value);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
ide_buildconfig_build_target_class_init (IdeBuildconfigBuildTargetClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = ide_buildconfig_build_target_finalize;
  object_class->get_property = ide_buildconfig_build_target_get_property;
  object_class->set_property = ide_buildconfig_build_target_set_property;

  properties [PROP_COMMAND] =
    g_param_spec_boxed ("command",
                         NULL,
                         NULL,
                         G_TYPE_STRV,
                         (G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, N_PROPS, properties);
}

static void
ide_buildconfig_build_target_init (IdeBuildconfigBuildTarget *self)
{
}
