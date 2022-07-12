/* ide-terminal-util.c
 *
 * Copyright 2016-2019 Christian Hergert <chergert@redhat.com>
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

#define G_LOG_DOMAIN "ide-terminal-util"

#include "config.h"

#include <fcntl.h>
#include <libide-io.h>
#include <libide-threading.h>
#include <stdlib.h>
#include <unistd.h>
#include <vte/vte.h>

#include "ide-terminal-util.h"

int
ide_vte_pty_create_producer (VtePty *pty)
{
  int consumer_fd;

  g_return_val_if_fail (VTE_IS_PTY (pty), IDE_PTY_FD_INVALID);

  consumer_fd = vte_pty_get_fd (pty);
  if (consumer_fd == IDE_PTY_FD_INVALID)
    return IDE_PTY_FD_INVALID;

  return ide_pty_intercept_create_producer (consumer_fd, TRUE);
}
