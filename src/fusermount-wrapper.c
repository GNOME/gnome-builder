/* fusermount-wrapper.c
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

#include <errno.h>
#include <glib.h>
#include <sys/prctl.h>
#include <signal.h>

static gboolean
parse_fd (const gchar *str,
          gint        *fd)
{
  gint64 v;

  *fd = -1;

  if (str == NULL)
    return FALSE;

  v = g_ascii_strtoll (str, NULL, 10);

  if (v < 0 || v > G_MAXINT)
    return FALSE;

  if (v == 0 && errno == EINVAL)
    return FALSE;

  *fd = v;

  return TRUE;
}

static void
child_setup_func (gpointer data)
{
  prctl (PR_SET_PDEATHSIG, SIGKILL);
}

gint
main (gint   argc,
      gchar *argv[])
{
  g_autoptr(GPtrArray) new_argv = NULL;
  g_autofree gchar *path = NULL;
  g_autofree gchar *env_param = NULL;
  g_autofree gchar *fwd_param = NULL;
  const gchar *fuse_commfd_env;
  gint fuse_commfd = -1;
  gint exit_status;

  fuse_commfd_env = g_getenv ("_FUSE_COMMFD");

  if (!parse_fd (fuse_commfd_env, &fuse_commfd))
    return EXIT_FAILURE;

  env_param = g_strdup_printf ("--env=_FUSE_COMMFD=%s", fuse_commfd_env);
  fwd_param = g_strdup_printf ("--forward-fd=%d", fuse_commfd);

  if (!(path = g_find_program_in_path ("flatpak-spawn")))
    return EXIT_FAILURE;

  new_argv = g_ptr_array_new ();
  g_ptr_array_add (new_argv, path);
  g_ptr_array_add (new_argv, (gchar *)"--clear-env");
  g_ptr_array_add (new_argv, (gchar *)"--watch-bus");
  g_ptr_array_add (new_argv, (gchar *)"--host");
  g_ptr_array_add (new_argv, env_param);
  g_ptr_array_add (new_argv, fwd_param);
  g_ptr_array_add (new_argv, (gchar *)"fusermount");
  for (guint i = 1; i < argc; i++)
    g_ptr_array_add (new_argv, argv[i]);
  g_ptr_array_add (new_argv, NULL);

  g_spawn_sync (NULL,
                (gchar **)new_argv->pdata,
                NULL,
                (G_SPAWN_LEAVE_DESCRIPTORS_OPEN | G_SPAWN_CHILD_INHERITS_STDIN),
                child_setup_func,
                NULL,
                NULL,
                NULL,
                &exit_status,
                NULL);

  return exit_status;
}
