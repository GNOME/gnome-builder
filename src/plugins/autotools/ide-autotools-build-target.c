/* ide-autotools-build-target.c
 *
 * Copyright 2016-2019 Christian Hergert <chergert@redhat.com>
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

#define G_LOG_DOMAIN "ide-autotools-build-target"

#include "ide-autotools-build-target.h"

struct _IdeAutotoolsBuildTarget
{
  IdeObject parent_instance;

  GFile *build_directory;
  GFile *install_directory;
  gchar *name;
};

enum {
  PROP_0,
  PROP_BUILD_DIRECTORY,
  PROP_INSTALL_DIRECTORY,
  PROP_NAME,
  N_PROPS
};

static void build_target_iface_init (IdeBuildTargetInterface *iface);

G_DEFINE_TYPE_EXTENDED (IdeAutotoolsBuildTarget, ide_autotools_build_target, IDE_TYPE_OBJECT, G_TYPE_FLAG_FINAL,
                        G_IMPLEMENT_INTERFACE (IDE_TYPE_BUILD_TARGET, build_target_iface_init))

static GParamSpec *properties [N_PROPS];

static void
ide_autotools_build_target_finalize (GObject *object)
{
  IdeAutotoolsBuildTarget *self = (IdeAutotoolsBuildTarget *)object;

  g_clear_object (&self->build_directory);
  g_clear_object (&self->install_directory);
  g_clear_pointer (&self->name, g_free);

  G_OBJECT_CLASS (ide_autotools_build_target_parent_class)->finalize (object);
}

static void
ide_autotools_build_target_get_property (GObject    *object,
                                         guint       prop_id,
                                         GValue     *value,
                                         GParamSpec *pspec)
{
  IdeAutotoolsBuildTarget *self = IDE_AUTOTOOLS_BUILD_TARGET (object);

  switch (prop_id)
    {
    case PROP_BUILD_DIRECTORY:
      g_value_set_object (value, self->build_directory);
      break;

    case PROP_INSTALL_DIRECTORY:
      g_value_set_object (value, self->install_directory);
      break;

    case PROP_NAME:
      g_value_set_string (value, self->name);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
ide_autotools_build_target_set_property (GObject      *object,
                                         guint         prop_id,
                                         const GValue *value,
                                         GParamSpec   *pspec)
{
  IdeAutotoolsBuildTarget *self = IDE_AUTOTOOLS_BUILD_TARGET (object);

  switch (prop_id)
    {
    case PROP_BUILD_DIRECTORY:
      self->build_directory = g_value_dup_object (value);
      break;

    case PROP_INSTALL_DIRECTORY:
      self->install_directory = g_value_dup_object (value);
      break;

    case PROP_NAME:
      self->name = g_value_dup_string (value);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
ide_autotools_build_target_class_init (IdeAutotoolsBuildTargetClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = ide_autotools_build_target_finalize;
  object_class->get_property = ide_autotools_build_target_get_property;
  object_class->set_property = ide_autotools_build_target_set_property;

  properties [PROP_BUILD_DIRECTORY] =
    g_param_spec_object ("build-directory",
                         NULL,
                         NULL,
                         G_TYPE_FILE,
                         (G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS));

  properties [PROP_INSTALL_DIRECTORY] =
    g_param_spec_object ("install-directory",
                         NULL,
                         NULL,
                         G_TYPE_FILE,
                         (G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS));

  properties [PROP_NAME] =
    g_param_spec_string ("name",
                         NULL,
                         NULL,
                         NULL,
                         (G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, N_PROPS, properties);
}

static void
ide_autotools_build_target_init (IdeAutotoolsBuildTarget *self)
{
}

static GFile *
ide_autotools_build_target_get_install_directory (IdeBuildTarget *target)
{
  IdeAutotoolsBuildTarget *self = (IdeAutotoolsBuildTarget *)target;

  if (self->install_directory != NULL)
    return g_object_ref (self->install_directory);

  return NULL;
}

static gchar *
ide_autotools_build_target_get_name (IdeBuildTarget *target)
{
  IdeAutotoolsBuildTarget *self = (IdeAutotoolsBuildTarget *)target;

  return g_strdup (self->name);
}

static void
build_target_iface_init (IdeBuildTargetInterface *iface)
{
  iface->get_install_directory = ide_autotools_build_target_get_install_directory;
  iface->get_name = ide_autotools_build_target_get_name;
}
