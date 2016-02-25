/* ide-posix.c
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

#include <string.h>
#include <sys/types.h>
#include <sys/user.h>
#include <sys/utsname.h>
#include <unistd.h>

#include "ide-posix.h"

gchar *
ide_get_system_arch (void)
{
  struct utsname u;
  const char *machine;

  if (uname (&u) < 0)
    return g_strdup ("unknown");

  /* config.sub doesn't accept amd64-OS */
  machine = strcmp (u.machine, "amd64") ? u.machine : "x86_64";

  return g_strdup (machine);
}

gsize
ide_get_system_page_size (void)
{
  return sysconf (_SC_PAGE_SIZE);
}
