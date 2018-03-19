/* ide-build-system.h
 *
 * Copyright 2015 Christian Hergert <christian@hergert.me>
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

#include <gio/gio.h>

#include "ide-version-macros.h"

#include "ide-object.h"

G_BEGIN_DECLS

#define IDE_TYPE_BUILD_SYSTEM (ide_build_system_get_type())

IDE_AVAILABLE_IN_ALL
G_DECLARE_INTERFACE (IdeBuildSystem, ide_build_system, IDE, BUILD_SYSTEM, IdeObject)

struct _IdeBuildSystemInterface
{
  GTypeInterface parent_iface;

  gint        (*get_priority)                      (IdeBuildSystem       *self);
  void        (*get_build_flags_async)             (IdeBuildSystem       *self,
                                                    IdeFile              *file,
                                                    GCancellable         *cancellable,
                                                    GAsyncReadyCallback   callback,
                                                    gpointer              user_data);
  gchar     **(*get_build_flags_finish)            (IdeBuildSystem       *self,
                                                    GAsyncResult         *result,
                                                    GError              **error);
  void        (*get_build_flags_for_files_async)   (IdeBuildSystem       *self,
                                                    GPtrArray            *files,
                                                    GCancellable         *cancellable,
                                                    GAsyncReadyCallback   callback,
                                                    gpointer              user_data);
  GHashTable *(*get_build_flags_for_files_finish)  (IdeBuildSystem       *self,
                                                    GAsyncResult         *result,
                                                    GError              **error);
  gchar      *(*get_builddir)                      (IdeBuildSystem       *self,
                                                    IdeBuildPipeline     *pipeline);
  gchar      *(*get_id)                            (IdeBuildSystem       *self);
  gchar      *(*get_display_name)                  (IdeBuildSystem       *self);
  gboolean    (*supports_toolchain)                (IdeBuildSystem       *self,
                                                    IdeToolchain         *toolchain);
};

IDE_AVAILABLE_IN_ALL
gchar          *ide_build_system_get_id                            (IdeBuildSystem       *self);
IDE_AVAILABLE_IN_ALL
gchar          *ide_build_system_get_display_name                  (IdeBuildSystem       *self);
IDE_AVAILABLE_IN_ALL
void            ide_build_system_new_async                         (IdeContext           *context,
                                                                    GFile                *project_file,
                                                                    const gchar          *build_system_hint,
                                                                    GCancellable         *cancellable,
                                                                    GAsyncReadyCallback   callback,
                                                                    gpointer              user_data);
IDE_AVAILABLE_IN_ALL
IdeBuildSystem *ide_build_system_new_finish                        (GAsyncResult         *result,
                                                                    GError              **error);
IDE_AVAILABLE_IN_ALL
gint            ide_build_system_get_priority                      (IdeBuildSystem       *self);
IDE_AVAILABLE_IN_3_28
gchar          *ide_build_system_get_builddir                      (IdeBuildSystem       *self,
                                                                    IdeBuildPipeline     *pipeline);
IDE_AVAILABLE_IN_ALL
void            ide_build_system_get_build_flags_async             (IdeBuildSystem       *self,
                                                                    IdeFile              *file,
                                                                    GCancellable         *cancellable,
                                                                    GAsyncReadyCallback   callback,
                                                                    gpointer              user_data);
IDE_AVAILABLE_IN_ALL
gchar         **ide_build_system_get_build_flags_finish            (IdeBuildSystem       *self,
                                                                    GAsyncResult         *result,
                                                                    GError              **error);
IDE_AVAILABLE_IN_ALL
void            ide_build_system_get_build_flags_for_files_async   (IdeBuildSystem       *self,
                                                                    GPtrArray            *files,
                                                                    GCancellable         *cancellable,
                                                                    GAsyncReadyCallback   callback,
                                                                    gpointer              user_data);
IDE_AVAILABLE_IN_ALL
GHashTable     *ide_build_system_get_build_flags_for_files_finish  (IdeBuildSystem       *self,
                                                                    GAsyncResult         *result,
                                                                    GError              **error);
IDE_AVAILABLE_IN_3_28
void            ide_build_system_get_build_flags_for_dir_async     (IdeBuildSystem       *self,
                                                                    GFile                *directory,
                                                                    GCancellable         *cancellable,
                                                                    GAsyncReadyCallback   callback,
                                                                    gpointer              user_data);
IDE_AVAILABLE_IN_3_28
GHashTable     *ide_build_system_get_build_flags_for_dir_finish    (IdeBuildSystem       *self,
                                                                    GAsyncResult         *result,
                                                                    GError              **error);
void            _ide_build_system_set_project_file                 (IdeBuildSystem       *self,
                                                                    GFile                *project_file) G_GNUC_INTERNAL;
IDE_AVAILABLE_IN_3_30
gboolean        ide_build_system_supports_toolchain                (IdeBuildSystem       *self,
                                                                    IdeToolchain         *toolchain);

G_END_DECLS
