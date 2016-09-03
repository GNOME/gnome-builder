/* ide-build-command-queue.h
 *
 * Copyright (C) 2016 Christian Hergert <chergert@redhat.com>
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

#ifndef IDE_BUILD_COMMAND_QUEUE_H
#define IDE_BUILD_COMMAND_QUEUE_H

#include "ide-types.h"

G_BEGIN_DECLS

#define IDE_TYPE_BUILD_COMMAND_QUEUE (ide_build_command_queue_get_type())

G_DECLARE_FINAL_TYPE (IdeBuildCommandQueue, ide_build_command_queue, IDE, BUILD_COMMAND_QUEUE, GObject)

IdeBuildCommandQueue *ide_build_command_queue_new            (void);
void                  ide_build_command_queue_append         (IdeBuildCommandQueue  *self,
                                                              IdeBuildCommand       *command);
gboolean              ide_build_command_queue_execute        (IdeBuildCommandQueue  *self,
                                                              IdeRuntime            *runtime,
                                                              IdeEnvironment        *environment,
                                                              IdeBuildResult        *build_result,
                                                              GCancellable          *cancellable,
                                                              GError               **error);
void                  ide_build_command_queue_execute_async  (IdeBuildCommandQueue  *self,
                                                              IdeRuntime            *runtime,
                                                              IdeEnvironment        *environment,
                                                              IdeBuildResult        *build_result,
                                                              GCancellable          *cancellable,
                                                              GAsyncReadyCallback    callback,
                                                              gpointer               user_data);
gboolean              ide_build_command_queue_execute_finish (IdeBuildCommandQueue  *self,
                                                              GAsyncResult          *result,
                                                              GError               **error);
IdeBuildCommandQueue *ide_build_command_queue_copy           (IdeBuildCommandQueue  *self);

G_END_DECLS

#endif /* IDE_BUILD_COMMAND_QUEUE_H */
