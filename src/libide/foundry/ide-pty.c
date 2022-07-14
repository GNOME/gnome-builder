/* ide-pty.c
 *
 * Copyright 2022 Christian Hergert <chergert@redhat.com>
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

#define G_LOG_DOMAIN "ide-pty"

#include "config.h"

#include <libide-io.h>

#include "ide-pty.h"

/**
 * ide_pty_new_sync:
 * @error: a location for a #GError or %NULL
 *
 * Creates a new #VtePty suitable for Builder to be able to pass the
 * PTY across PTY namespaces on Linux.
 *
 * Use this instead of vte_pty_new_sync() or similar.
 *
 * Returns: (transfer full): a #VtePty if successful, otherwise %NULL
 *   and @error is set.
 */
VtePty *
ide_pty_new_sync (GError **error)
{
  VtePty *ret;
  int fd;

  fd = ide_pty_intercept_create_consumer ();

  if (fd == IDE_PTY_FD_INVALID)
    {
      int errsv = errno;
      g_set_error_literal (error,
                           G_IO_ERROR,
                           g_io_error_from_errno (errsv),
                           g_strerror (errsv));
      return NULL;
    }

  if ((ret = vte_pty_new_foreign_sync (fd, NULL, error)))
    {
      if (!vte_pty_set_utf8 (ret, TRUE, error))
        g_clear_object (&ret);
    }

  return ret;
}
