/* ide-worker.c
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

#define G_LOG_DOMAIN "ide-worker"

#include "config.h"

#include <libide-core.h>

#include "ide-worker.h"

G_DEFINE_INTERFACE (IdeWorker, ide_worker, G_TYPE_OBJECT)

static void
ide_worker_default_init (IdeWorkerInterface *iface)
{
}

void
ide_worker_register_service (IdeWorker       *self,
                             GDBusConnection *connection)
{
  g_return_if_fail (IDE_IS_WORKER (self));
  g_return_if_fail (G_IS_DBUS_CONNECTION (connection));

  IDE_WORKER_GET_IFACE (self)->register_service (self, connection);
}

/**
 * ide_worker_create_proxy:
 * @self: An #IdeWorker.
 * @connection: a #GDBusConnection connected to the worker process.
 * @error: (allow-none): a location for a #GError, or %NULL.
 *
 * Creates a new proxy to be connected to the subprocess peer on the other
 * end of @connection.
 *
 * Returns: (transfer full): a #GDBusProxy or %NULL.
 *
 * Since: 3.32
 */
GDBusProxy *
ide_worker_create_proxy (IdeWorker        *self,
                         GDBusConnection  *connection,
                         GError          **error)
{
  g_return_val_if_fail (IDE_IS_WORKER (self), NULL);
  g_return_val_if_fail (G_IS_DBUS_CONNECTION (connection), NULL);

  return IDE_WORKER_GET_IFACE (self)->create_proxy (self, connection, error);
}
