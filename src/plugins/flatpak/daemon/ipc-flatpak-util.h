/* ipc-flatpak-util.h
 *
 * Copyright 2019 Christian Hergert <chergert@redhat.com>
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

static inline gboolean
complete_wrapped_error (GDBusMethodInvocation *invocation,
                        const GError          *error)
{
  g_autoptr(GError) wrapped = NULL;

  wrapped = g_error_new (G_IO_ERROR,
                         G_IO_ERROR_FAILED,
                         "The operation failed. The original error was \"%s\"",
                         error->message);
  g_dbus_method_invocation_return_gerror (invocation, wrapped);

  return TRUE;
}

G_END_DECLS
