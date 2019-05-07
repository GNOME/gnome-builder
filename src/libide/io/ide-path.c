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

#include <libide-threading.h>

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

gboolean
ide_path_is_c_like (const gchar *path)
{
  const gchar *dot;

  if (path == NULL)
    return FALSE;

  if ((dot = strrchr (path, '.')))
    return ide_str_equal (dot, ".c") || ide_str_equal (dot, ".h");

  return FALSE;
}

gboolean
ide_path_is_cpp_like (const gchar *path)
{
  static const gchar *cpplike[] = {
    ".cc", ".cpp", ".c++", ".cxx",
    ".hh", ".hpp", ".h++", ".hxx",
  };
  const gchar *dot;

  if (path == NULL)
    return FALSE;

  if ((dot = strrchr (path, '.')))
    {
      for (guint i = 0; i < G_N_ELEMENTS (cpplike); i++)
        {
          if (ide_str_equal (dot, cpplike[i]))
            return TRUE;
        }
    }

  return FALSE;
}

/**
 * ide_find_program_in_host_path:
 * @program: the name of the executable
 *
 * Like g_find_program_in_path() but checks the host system which may not be
 * the same as the container we're running within.
 *
 * Returns: (transfer full) (nullable): a path or %NULL
 *
 * Since: 3.34
 */
gchar *
ide_find_program_in_host_path (const gchar *program)
{
  if (ide_is_flatpak ())
    {
      g_autoptr(IdeSubprocessLauncher) launcher = NULL;
      g_autoptr(IdeSubprocess) subprocess = NULL;

      /* It is possible to do this by looking in /var/run/host since we have
       * access to --filesystem=home. However, that would not include things
       * that could be in an altered path in the users session (which we would
       * otherwise want to find.
       */

      if (program == NULL)
        return NULL;

      launcher = ide_subprocess_launcher_new (G_SUBPROCESS_FLAGS_STDOUT_PIPE |
                                              G_SUBPROCESS_FLAGS_STDERR_SILENCE);
      ide_subprocess_launcher_set_run_on_host (launcher, TRUE);
      ide_subprocess_launcher_push_argv (launcher, "which");
      ide_subprocess_launcher_push_argv (launcher, program);

      if ((subprocess = ide_subprocess_launcher_spawn (launcher, NULL, NULL)))
        {
          g_autofree gchar *path = NULL;

          if (ide_subprocess_communicate_utf8 (subprocess, NULL, NULL, &path, NULL, NULL))
            {
              g_strstrip (path);

              if (!ide_str_empty0 (path))
                return g_steal_pointer (&path);
            }
        }

      return NULL;
    }
  else
    {
      return g_find_program_in_path (program);
    }
}
