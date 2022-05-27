/* ide-build-target.h
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

#pragma once

#if !defined (IDE_FOUNDRY_INSIDE) && !defined (IDE_FOUNDRY_COMPILATION)
# error "Only <libide-foundry.h> can be included directly."
#endif

#include <libide-core.h>

#include "ide-foundry-types.h"

G_BEGIN_DECLS

#define IDE_TYPE_BUILD_TARGET (ide_build_target_get_type())

IDE_AVAILABLE_IN_ALL
G_DECLARE_INTERFACE (IdeBuildTarget, ide_build_target, IDE, BUILD_TARGET, IdeObject)

typedef enum
{
  IDE_ARTIFACT_KIND_NONE,
  IDE_ARTIFACT_KIND_EXECUTABLE,
  IDE_ARTIFACT_KIND_SHARED_LIBRARY,
  IDE_ARTIFACT_KIND_STATIC_LIBRARY,
  IDE_ARTIFACT_KIND_FILE,
} IdeArtifactKind;

struct _IdeBuildTargetInterface
{
  GTypeInterface parent_iface;

  GFile            *(*get_install_directory) (IdeBuildTarget *self);
  gchar            *(*get_name)              (IdeBuildTarget *self);
  gchar            *(*get_display_name)      (IdeBuildTarget *self);
  gint              (*get_priority)          (IdeBuildTarget *self);
  gchar           **(*get_argv)              (IdeBuildTarget *self);
  gchar            *(*get_cwd)               (IdeBuildTarget *self);
  gchar            *(*get_language)          (IdeBuildTarget *self);
  IdeArtifactKind   (*get_kind)              (IdeBuildTarget *self);
};

IDE_AVAILABLE_IN_ALL
GFile            *ide_build_target_get_install_directory (IdeBuildTarget       *self);
IDE_AVAILABLE_IN_ALL
gchar            *ide_build_target_get_name              (IdeBuildTarget       *self);
IDE_AVAILABLE_IN_ALL
gchar            *ide_build_target_get_display_name      (IdeBuildTarget       *self);
IDE_AVAILABLE_IN_ALL
gint              ide_build_target_get_priority          (IdeBuildTarget       *self);
IDE_AVAILABLE_IN_ALL
gchar           **ide_build_target_get_argv              (IdeBuildTarget       *self);
IDE_AVAILABLE_IN_ALL
gchar            *ide_build_target_get_cwd               (IdeBuildTarget       *self);
IDE_AVAILABLE_IN_ALL
gchar            *ide_build_target_get_language          (IdeBuildTarget       *self);
IDE_AVAILABLE_IN_ALL
gboolean          ide_build_target_get_install           (IdeBuildTarget       *self);
IDE_AVAILABLE_IN_ALL
IdeArtifactKind   ide_build_target_get_kind              (IdeBuildTarget       *self);
IDE_AVAILABLE_IN_ALL
gboolean          ide_build_target_compare               (const IdeBuildTarget *left,
                                                          const IdeBuildTarget *right);

G_END_DECLS
