/* ide-worker-manager.h
 *
 * Copyright 2015-2019 Christian Hergert <christian@hergert.me>
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

#include <gio/gio.h>

G_BEGIN_DECLS

#define IDE_TYPE_WORKER_MANAGER (ide_worker_manager_get_type())

G_DECLARE_FINAL_TYPE (IdeWorkerManager, ide_worker_manager, IDE, WORKER_MANAGER, GObject)

IdeWorkerManager *ide_worker_manager_new               (void);
void              ide_worker_manager_shutdown          (IdeWorkerManager     *self);
void              ide_worker_manager_get_worker_async  (IdeWorkerManager     *self,
                                                        const gchar          *plugin_name,
                                                        GCancellable         *cancellable,
                                                        GAsyncReadyCallback   callback,
                                                        gpointer              user_data);
GDBusProxy       *ide_worker_manager_get_worker_finish (IdeWorkerManager     *self,
                                                        GAsyncResult         *result,
                                                        GError              **error);

G_END_DECLS
