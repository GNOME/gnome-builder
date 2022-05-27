/* ide-simple-build-target.h
 *
 * Copyright 2018-2019 Christian Hergert <chergert@redhat.com>
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

#define IDE_TYPE_SIMPLE_BUILD_TARGET (ide_simple_build_target_get_type())

IDE_AVAILABLE_IN_ALL
G_DECLARE_DERIVABLE_TYPE (IdeSimpleBuildTarget, ide_simple_build_target, IDE, SIMPLE_BUILD_TARGET, IdeObject)

struct _IdeSimpleBuildTargetClass
{
  IdeObjectClass parent_class;

  /*< private >*/
  gpointer _reserved[8];
};

IDE_AVAILABLE_IN_ALL
IdeSimpleBuildTarget *ide_simple_build_target_new                   (IdeContext           *context);
IDE_AVAILABLE_IN_ALL
void                  ide_simple_build_target_set_install_directory (IdeSimpleBuildTarget *self,
                                                                     GFile                *install_directory);
IDE_AVAILABLE_IN_ALL
void                  ide_simple_build_target_set_name              (IdeSimpleBuildTarget *self,
                                                                     const gchar          *name);
IDE_AVAILABLE_IN_ALL
void                  ide_simple_build_target_set_priority          (IdeSimpleBuildTarget *self,
                                                                     gint                  priority);
IDE_AVAILABLE_IN_ALL
void                  ide_simple_build_target_set_argv              (IdeSimpleBuildTarget *self,
                                                                     const gchar * const  *argv);
IDE_AVAILABLE_IN_ALL
void                  ide_simple_build_target_set_cwd               (IdeSimpleBuildTarget *self,
                                                                     const gchar          *cwd);
IDE_AVAILABLE_IN_ALL
void                  ide_simple_build_target_set_language          (IdeSimpleBuildTarget *self,
                                                                     const gchar          *language);

G_END_DECLS
