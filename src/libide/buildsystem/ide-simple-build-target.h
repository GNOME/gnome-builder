/* ide-simple-build-target.h
 *
 * Copyright 2018 Christian Hergert <chergert@redhat.com>
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

#include <ide.h>

G_BEGIN_DECLS

#define IDE_TYPE_SIMPLE_BUILD_TARGET (ide_simple_build_target_get_type())

G_DECLARE_DERIVABLE_TYPE (IdeSimpleBuildTarget, ide_simple_build_target, IDE, SIMPLE_BUILD_TARGET, IdeObject)

struct _IdeSimpleBuildTargetClass
{
  IdeObjectClass parent_class;

  /*< private >*/
  gpointer _reserved[8];
};

IdeSimpleBuildTarget *ide_simple_build_target_new                   (IdeContext           *context);
void                  ide_simple_build_target_set_install_directory (IdeSimpleBuildTarget *self,
                                                                     GFile                *install_directory);
void                  ide_simple_build_target_set_name              (IdeSimpleBuildTarget *self,
                                                                     const gchar          *name);
void                  ide_simple_build_target_set_priority          (IdeSimpleBuildTarget *self,
                                                                     gint                  priority);
void                  ide_simple_build_target_set_argv              (IdeSimpleBuildTarget *self,
                                                                     const gchar * const  *argv);
void                  ide_simple_build_target_set_cwd               (IdeSimpleBuildTarget *self,
                                                                     const gchar          *cwd);
void                  ide_simple_build_target_set_language          (IdeSimpleBuildTarget *self,
                                                                     const gchar          *language);

G_END_DECLS
