/* ide-worker-process.h
 *
 * Copyright 2015-2019 Christian Hergert <chergert@redhat.com>
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

#define IDE_TYPE_WORKER_PROCESS (ide_worker_process_get_type())

G_DECLARE_FINAL_TYPE (IdeWorkerProcess, ide_worker_process, IDE, WORKER_PROCESS, GObject)

IdeWorkerProcess *ide_worker_process_new                 (const gchar          *argv0,
                                                          const gchar          *type,
                                                          const gchar          *dbus_address);
void              ide_worker_process_run                 (IdeWorkerProcess     *self);
void              ide_worker_process_quit                (IdeWorkerProcess     *self);
gpointer          ide_worker_process_create_proxy        (IdeWorkerProcess     *self,
                                                          GError              **error);
gboolean          ide_worker_process_matches_credentials (IdeWorkerProcess     *self,
                                                          GCredentials         *credentials);
void              ide_worker_process_set_connection      (IdeWorkerProcess     *self,
                                                          GDBusConnection      *connection);
void              ide_worker_process_get_proxy_async     (IdeWorkerProcess     *self,
                                                          GCancellable         *cancellable,
                                                          GAsyncReadyCallback   callback,
                                                          gpointer              user_data);
GDBusProxy       *ide_worker_process_get_proxy_finish    (IdeWorkerProcess     *self,
                                                          GAsyncResult         *result,
                                                          GError              **error);

G_END_DECLS
