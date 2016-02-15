/* ide-autotools-build-task.h
 *
 * Copyright (C) 2015 Christian Hergert <christian@hergert.me>
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

#ifndef IDE_AUTOTOOLS_BUILD_TASK_H
#define IDE_AUTOTOOLS_BUILD_TASK_H

#include <gio/gio.h>

#include "ide-build-result.h"

G_BEGIN_DECLS

#define IDE_TYPE_AUTOTOOLS_BUILD_TASK (ide_autotools_build_task_get_type())

G_DECLARE_FINAL_TYPE (IdeAutotoolsBuildTask, ide_autotools_build_task, IDE, AUTOTOOLS_BUILD_TASK, IdeBuildResult)

GFile    *ide_autotools_build_task_get_directory  (IdeAutotoolsBuildTask  *self);
void      ide_autotools_build_task_add_target     (IdeAutotoolsBuildTask  *self);
void      ide_autotools_build_task_execute_async  (IdeAutotoolsBuildTask  *self,
                                                   IdeBuilderBuildFlags    flags,
                                                   GCancellable           *cancellable,
                                                   GAsyncReadyCallback     callback,
                                                   gpointer                user_data);
gboolean  ide_autotools_build_task_execute_finish (IdeAutotoolsBuildTask  *self,
                                                   GAsyncResult           *result,
                                                   GError                **error);

G_END_DECLS

#endif /* IDE_AUTOTOOLS_BUILD_TASK_H */
