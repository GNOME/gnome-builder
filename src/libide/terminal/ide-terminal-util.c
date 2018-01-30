/* ide-terminal-util.c
 *
 * Copyright © 2016 Christian Hergert <chergert@redhat.com>
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

#define G_LOG_DOMAIN "ide-terminal-util"

#include "config.h"

#include <fcntl.h>
#include <stdlib.h>
#include <unistd.h>

#include "terminal/ide-terminal-util.h"
#include "util/ptyintercept.h"

gint
ide_vte_pty_create_slave (VtePty *pty)
{
  gint master_fd;

  g_return_val_if_fail (VTE_IS_PTY (pty), PTY_FD_INVALID);

  master_fd = vte_pty_get_fd (pty);
  if (master_fd == PTY_FD_INVALID)
    return PTY_FD_INVALID;

  return pty_intercept_create_slave (master_fd, TRUE);
}
