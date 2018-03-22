/* ide-build-target.h
 *
 * Copyright 2016 Christian Hergert <chergert@redhat.com>
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
 */

#pragma once

#include <glib-object.h>

#include "ide-version-macros.h"

#include "ide-object.h"

G_BEGIN_DECLS

#define IDE_TYPE_BUILD_TARGET (ide_build_target_get_type())

IDE_AVAILABLE_IN_ALL
G_DECLARE_INTERFACE (IdeBuildTarget, ide_build_target, IDE, BUILD_TARGET, IdeObject)

struct _IdeBuildTargetInterface
{
  GTypeInterface parent_iface;

  GFile  *(*get_install_directory) (IdeBuildTarget *self);
  gchar  *(*get_name)              (IdeBuildTarget *self);
  gint    (*get_priority)          (IdeBuildTarget *self);
  gchar **(*get_argv)              (IdeBuildTarget *self);
  gchar  *(*get_cwd)               (IdeBuildTarget *self);
  gchar  *(*get_language)          (IdeBuildTarget *self);
};

IDE_AVAILABLE_IN_ALL
GFile     *ide_build_target_get_install_directory (IdeBuildTarget       *self);
IDE_AVAILABLE_IN_ALL
gchar     *ide_build_target_get_name              (IdeBuildTarget       *self);
IDE_AVAILABLE_IN_3_28
gint       ide_build_target_get_priority          (IdeBuildTarget       *self);
IDE_AVAILABLE_IN_3_28
gchar    **ide_build_target_get_argv              (IdeBuildTarget       *self);
IDE_AVAILABLE_IN_3_28
gchar     *ide_build_target_get_cwd               (IdeBuildTarget       *self);
IDE_AVAILABLE_IN_3_28
gchar     *ide_build_target_get_language          (IdeBuildTarget       *self);
IDE_AVAILABLE_IN_3_28
gboolean   ide_build_target_compare               (const IdeBuildTarget *left,
                                                   const IdeBuildTarget *right);

G_END_DECLS
