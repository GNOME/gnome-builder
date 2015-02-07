/* ide-load-directory-task.h
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

#ifndef IDE_LOAD_DIRECTORY_TASK_H
#define IDE_LOAD_DIRECTORY_TASK_H

#include <gio/gio.h>

#include "ide-types.h"

G_BEGIN_DECLS

GTask *ide_load_directory_task_new (gpointer             source_object,
                                    GFile               *directory,
                                    IdeProjectItem      *parent,
                                    int                  io_priority,
                                    GCancellable        *cancellable,
                                    GAsyncReadyCallback  callback,
                                    gpointer             user_data);

G_END_DECLS

#endif /* IDE_LOAD_DIRECTORY_TASK_H */
