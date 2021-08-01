/* gbp-cmake-build-target.c
 *
 * Copyright 2017-2019 Christian Hergert <chergert@redhat.com>
 * Copyright 2017 Martin Blanchard <tchaik@gmx.com>
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

#define G_LOG_DOMAIN "gbp-cmake-build-target"

#include "gbp-cmake-build-target.h"

struct _GbpCMakeBuildTarget
{
  IdeObject parent_instance;

  GFile    *install_directory;
  gchar    *name;
};

static GFile *
gbp_cmake_build_target_get_install_directory (IdeBuildTarget *build_target)
{
  GbpCMakeBuildTarget *self = (GbpCMakeBuildTarget *)build_target;

  g_assert (GBP_IS_CMAKE_BUILD_TARGET (self));

  return self->install_directory ? g_object_ref (self->install_directory) : NULL;
}

static gchar *
gbp_cmake_build_target_get_name (IdeBuildTarget *build_target)
{
  GbpCMakeBuildTarget *self = (GbpCMakeBuildTarget *)build_target;

  g_assert (GBP_IS_CMAKE_BUILD_TARGET (self));

  return self->name ? g_strdup (self->name) : NULL;
}

static void
build_target_iface_init (IdeBuildTargetInterface *iface)
{
  iface->get_install_directory = gbp_cmake_build_target_get_install_directory;
  iface->get_name = gbp_cmake_build_target_get_name;
}

G_DEFINE_FINAL_TYPE_WITH_CODE (GbpCMakeBuildTarget, gbp_cmake_build_target, IDE_TYPE_OBJECT,
                         G_IMPLEMENT_INTERFACE (IDE_TYPE_BUILD_TARGET, build_target_iface_init))

static void
gbp_cmake_build_target_finalize (GObject *object)
{
  GbpCMakeBuildTarget *self = (GbpCMakeBuildTarget *)object;

  g_clear_object (&self->install_directory);
  g_clear_pointer (&self->name, g_free);

  G_OBJECT_CLASS (gbp_cmake_build_target_parent_class)->finalize (object);
}

static void
gbp_cmake_build_target_class_init (GbpCMakeBuildTargetClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = gbp_cmake_build_target_finalize;
}

static void
gbp_cmake_build_target_init (GbpCMakeBuildTarget *self)
{
}

IdeBuildTarget *
gbp_cmake_build_target_new (IdeContext *context,
                            GFile      *install_directory,
                            gchar      *name)
{
  GbpCMakeBuildTarget *self;

  g_return_val_if_fail (!context || IDE_IS_CONTEXT (context), NULL);
  g_return_val_if_fail (G_IS_FILE (install_directory), NULL);
  g_return_val_if_fail (name != NULL, NULL);

  self = g_object_new (GBP_TYPE_CMAKE_BUILD_TARGET, NULL);
  g_set_object (&self->install_directory, install_directory);
  self->name = g_strdup (name);

  return IDE_BUILD_TARGET (self);
}
