/* ide-path.c
 *
 * Copyright 2018-2019 Christian Hergert <chergert@redhat.com>
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

#define G_LOG_DOMAIN "ide-posix"

#include "config.h"

#include <string.h>
#include <unistd.h>
#include <wordexp.h>

#include "ide-path.h"

/**
 * ide_path_expand:
 *
 * This function will expand various "shell-like" features of the provided
 * path using the POSIX wordexp(3) function. Command substitution will
 * not be enabled, but path features such as ~user will be expanded.
 *
 * Returns: (transfer full): A newly allocated string containing the
 *   expansion. A copy of the input string upon failure to expand.
 *
 * Since: 3.32
 */
gchar *
ide_path_expand (const gchar *path)
{
  wordexp_t state = { 0 };
  gchar *ret = NULL;
  int r;

  if (path == NULL)
    return NULL;

  r = wordexp (path, &state, WRDE_NOCMD);
  if (r == 0 && state.we_wordc > 0)
    ret = g_strdup (state.we_wordv [0]);
  wordfree (&state);

  if (!g_path_is_absolute (ret))
    {
      g_autofree gchar *freeme = ret;

      ret = g_build_filename (g_get_home_dir (), freeme, NULL);
    }

  return ret;
}

/**
 * ide_path_collapse:
 *
 * This function will collapse a path that starts with the users home
 * directory into a shorthand notation using ~/ for the home directory.
 *
 * If the path does not have the home directory as a prefix, it will
 * simply return a copy of @path.
 *
 * Returns: (transfer full): A new path, possibly collapsed.
 *
 * Since: 3.32
 */
gchar *
ide_path_collapse (const gchar *path)
{
  g_autofree gchar *expanded = NULL;

  if (path == NULL)
    return NULL;

  expanded = ide_path_expand (path);

  if (g_str_has_prefix (expanded, g_get_home_dir ()))
    return g_build_filename ("~",
                             expanded + strlen (g_get_home_dir ()),
                             NULL);

  return g_steal_pointer (&expanded);
}
