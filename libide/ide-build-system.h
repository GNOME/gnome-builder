/* ide-build-system.h
 *
 * Copyright (C) 2015 Christian Hergert <christian@hergert.me>
 *
 * This file is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as
 * published by the Free Software Foundation; either version 3 of the
 * License, or (at your option) any later version.
 *
 * This file is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef IDE_BUILD_SYSTEM_H
#define IDE_BUILD_SYSTEM_H

#include <gio/gio.h>

#include "ide-object.h"

G_BEGIN_DECLS

#define IDE_TYPE_BUILD_SYSTEM            (ide_build_system_get_type())
#define IDE_BUILD_SYSTEM_EXTENSION_POINT "org.gnome.libide.extensions.build-system"

G_DECLARE_DERIVABLE_TYPE (IdeBuildSystem, ide_build_system, IDE, BUILD_SYSTEM, IdeObject)

struct _IdeBuildSystemClass
{
  IdeObjectClass parent;

  IdeBuilder *(*get_builder)            (IdeBuildSystem       *system,
                                         GKeyFile             *config,
                                         IdeDevice            *device,
                                         GError              **error);
  void        (*get_build_flags_async)  (IdeBuildSystem       *self,
                                         IdeFile              *file,
                                         GCancellable         *cancellable,
                                         GAsyncReadyCallback   callback,
                                         gpointer              user_data);
  gchar     **(*get_build_flags_finish) (IdeBuildSystem       *self,
                                         GAsyncResult         *result,
                                         GError              **error);
};

void            ide_build_system_get_build_flags_async  (IdeBuildSystem       *self,
                                                         IdeFile              *file,
                                                         GCancellable         *cancellable,
                                                         GAsyncReadyCallback   callback,
                                                         gpointer              user_data);
gchar         **ide_build_system_get_build_flags_finish (IdeBuildSystem       *self,
                                                         GAsyncResult         *result,
                                                         GError              **error);
void            ide_build_system_new_async              (IdeContext           *context,
                                                         GFile                *project_file,
                                                         GCancellable         *cancellable,
                                                         GAsyncReadyCallback   callback,
                                                         gpointer              user_data);
IdeBuildSystem *ide_build_system_new_finish             (GAsyncResult         *result,
                                                         GError              **error);
IdeBuilder     *ide_build_system_get_builder            (IdeBuildSystem       *system,
                                                         GKeyFile             *config,
                                                         IdeDevice            *device,
                                                         GError              **error);

G_END_DECLS

#endif /* IDE_BUILD_SYSTEM_H */
