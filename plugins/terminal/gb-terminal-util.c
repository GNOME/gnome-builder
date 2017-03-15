/* gb-terminal-util.c
 *
 * Copyright (C) 2016 Christian Hergert <chergert@redhat.com>
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

#define G_LOG_DOMAIN "gb-terminal-util"

#include "config.h"

#include <fcntl.h>
#include <stdlib.h>
#include <unistd.h>

#include "gb-terminal-util.h"

gint
gb_vte_pty_create_slave (VtePty *pty)
{
  gint master_fd;
#ifdef HAVE_PTSNAME_R
  char name[PATH_MAX + 1];
#else
  const char *name;
#endif

  g_assert (VTE_IS_PTY (pty));

  if (-1 == (master_fd = vte_pty_get_fd (pty)))
    return -1;

  if (grantpt (master_fd) != 0)
    return -1;

  if (unlockpt (master_fd) != 0)
    return -1;

#ifdef HAVE_PTSNAME_R
  if (ptsname_r (master_fd, name, sizeof name - 1) != 0)
    return -1;
  name[sizeof name - 1] = '\0';
#else
  if (NULL == (name = ptsname (master_fd)))
    return -1;
#endif

  return open (name, O_RDWR | O_CLOEXEC);
}
