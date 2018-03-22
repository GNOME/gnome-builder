/* ide-build-manager.h
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

#include <gio/gio.h>

#include "ide-version-macros.h"

#include "ide-object.h"

#include "buildsystem/ide-build-pipeline.h"

G_BEGIN_DECLS

#define IDE_TYPE_BUILD_MANAGER (ide_build_manager_get_type())

G_DECLARE_FINAL_TYPE (IdeBuildManager, ide_build_manager, IDE, BUILD_MANAGER, IdeObject)

IDE_AVAILABLE_IN_ALL
gboolean          ide_build_manager_get_busy            (IdeBuildManager       *self);
IDE_AVAILABLE_IN_ALL
gboolean          ide_build_manager_get_can_build       (IdeBuildManager       *self);
IDE_AVAILABLE_IN_ALL
gchar            *ide_build_manager_get_message         (IdeBuildManager       *self);
IDE_AVAILABLE_IN_ALL
GDateTime        *ide_build_manager_get_last_build_time (IdeBuildManager       *self);
IDE_AVAILABLE_IN_ALL
GTimeSpan         ide_build_manager_get_running_time    (IdeBuildManager       *self);
IDE_AVAILABLE_IN_ALL
void              ide_build_manager_cancel              (IdeBuildManager       *self);
IDE_AVAILABLE_IN_ALL
IdeBuildPipeline *ide_build_manager_get_pipeline        (IdeBuildManager       *self);
IDE_AVAILABLE_IN_ALL
void              ide_build_manager_rebuild_async       (IdeBuildManager       *self,
                                                         IdeBuildPhase          phase,
                                                         GCancellable          *cancellable,
                                                         GAsyncReadyCallback    callback,
                                                         gpointer               user_data);
IDE_AVAILABLE_IN_ALL
gboolean          ide_build_manager_rebuild_finish      (IdeBuildManager       *self,
                                                         GAsyncResult          *result,
                                                         GError               **error);
IDE_AVAILABLE_IN_ALL
void              ide_build_manager_execute_async       (IdeBuildManager       *self,
                                                         IdeBuildPhase          phase,
                                                         GCancellable          *cancellable,
                                                         GAsyncReadyCallback    callback,
                                                         gpointer               user_data);
IDE_AVAILABLE_IN_ALL
gboolean          ide_build_manager_execute_finish      (IdeBuildManager       *self,
                                                         GAsyncResult          *result,
                                                         GError               **error);
IDE_AVAILABLE_IN_ALL
void              ide_build_manager_clean_async         (IdeBuildManager       *self,
                                                         IdeBuildPhase          phase,
                                                         GCancellable          *cancellable,
                                                         GAsyncReadyCallback    callback,
                                                         gpointer               user_data);
IDE_AVAILABLE_IN_ALL
gboolean          ide_build_manager_clean_finish        (IdeBuildManager       *self,
                                                         GAsyncResult          *result,
                                                         GError               **error);

G_END_DECLS
