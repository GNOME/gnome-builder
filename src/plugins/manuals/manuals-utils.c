/*
 * manuals-utils.c
 *
 * Copyright 2024 Christian Hergert <chergert@redhat.com>
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

#include "config.h"

#include <libide-io.h>

#include "manuals-utils.h"

static char *
get_os_info_from_os_release (const char *key_name,
                             const char *filename)
{
  g_autofree char *contents = NULL;
  IdeLineReader reader;
  char *line;
  gsize line_len;
  gsize len;

  if (!g_file_get_contents (filename, &contents, &len, NULL))
    return NULL;

  ide_line_reader_init (&reader, contents, len);

  while ((line = ide_line_reader_next (&reader, &line_len)))
    {
      line[line_len] = 0;

      if (g_str_has_prefix (line, key_name) &&
          line[strlen(key_name)] == '=')
        {
          const char *quoted = line + strlen (key_name) + 1;
          return g_shell_unquote (quoted, NULL);
        }
    }

  return NULL;
}

char *
manuals_get_os_info (const char *key_name)
{
  char *ret = NULL;

  if (g_file_test ("/.flatpak-info", G_FILE_TEST_EXISTS))
    ret = get_os_info_from_os_release (key_name, "/var/run/host/os-release");

  if (ret == NULL)
    ret = g_get_os_info (key_name);

  return ret;
}
