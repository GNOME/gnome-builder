/* ipc-git-util.h
 *
 * Copyright 2019 Christian Hergert <chergert@redhat.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#pragma once

#include <glib/gi18n.h>

static inline gboolean
complete_wrapped_error (GDBusMethodInvocation *invocation,
                        const GError          *error)
{
  g_autoptr(GError) wrapped = NULL;

  wrapped = g_error_new (G_IO_ERROR,
                         G_IO_ERROR_FAILED,
                         _("The operation failed. The original error was \"%s\""),
                         error->message);
  g_dbus_method_invocation_return_gerror (invocation, wrapped);

  return TRUE;
}
