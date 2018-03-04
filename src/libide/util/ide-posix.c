/* ide-posix.c
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

#include <string.h>
#include <sys/types.h>
#include <sys/user.h>
#include <sys/utsname.h>
#include <unistd.h>
#include <wordexp.h>

#include "util/ide-posix.h"

gchar *
ide_create_host_triplet (const gchar *arch,
                         const gchar *kernel,
                         const gchar *system)
{
  if (arch == NULL || kernel == NULL)
    return g_strdup (ide_get_system_type ());
  else if (system == NULL)
    return g_strdup_printf ("%s-%s", arch, kernel);
  else
    return g_strdup_printf ("%s-%s-%s", arch, kernel, system);
}

const gchar *
ide_get_system_type (void)
{
  static gchar *system_type;
  g_autofree gchar *os_lower = NULL;
  const gchar *machine = NULL;
  struct utsname u;

  if (system_type != NULL)
    return system_type;

  if (uname (&u) < 0)
    return g_strdup ("unknown");

  os_lower = g_utf8_strdown (u.sysname, -1);

  /* config.sub doesn't accept amd64-OS */
  machine = strcmp (u.machine, "amd64") ? u.machine : "x86_64";

  /*
   * TODO: Clearly we want to discover "gnu", but that should be just fine
   *       for a default until we try to actually run on something non-gnu.
   *       Which seems unlikely at the moment. If you run FreeBSD, you can
   *       probably fix this for me :-) And while you're at it, make the
   *       uname() call more portable.
   */

#ifdef __GLIBC__
  system_type = g_strdup_printf ("%s-%s-%s", machine, os_lower, "gnu");
#else
  system_type = g_strdup_printf ("%s-%s", machine, os_lower);
#endif

  return system_type;
}

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

/**
 * ide_path_expand:
 *
 * This function will expand various "shell-like" features of the provided
 * path using the POSIX wordexp(3) function. Command substitution will
 * not be enabled, but path features such as ~user will be expanded.
 *
 * Returns: (transfer full): A newly allocated string containing the
 *   expansion. A copy of the input string upon failure to expand.
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
