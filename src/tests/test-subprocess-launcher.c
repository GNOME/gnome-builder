/* test-subprocess-launcher.c
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

#include <fcntl.h>
#include <glib/gstdio.h>
#include <libide-threading.h>
#include <unistd.h>

static void
test_basic (void)
{
  g_autoptr(IdeSubprocessLauncher) launcher = NULL;
  g_autoptr(IdeSubprocess) process = NULL;
  g_autoptr(GError) error = NULL;

  launcher = ide_subprocess_launcher_new (0);
  g_assert (launcher != NULL);

  ide_subprocess_launcher_push_argv (launcher, "true");

  process = ide_subprocess_launcher_spawn (launcher, NULL, &error);
  g_assert (process != NULL);
  g_assert (error == NULL);
  g_assert_cmpint (ide_subprocess_wait_check (process, NULL, &error), !=, 0);
}

static void
test_communicate (void)
{
  IdeSubprocessLauncher *launcher;
  g_autoptr(IdeSubprocess) subprocess = NULL;
  g_autoptr(GError) error = NULL;
  g_autofree gchar *stdout_buf = NULL;
  gboolean r;

  launcher = ide_subprocess_launcher_new (G_SUBPROCESS_FLAGS_STDOUT_PIPE);
  ide_subprocess_launcher_push_argv (launcher, "ls");

  subprocess = ide_subprocess_launcher_spawn (launcher, NULL, &error);
  g_assert_no_error (error);
  g_assert (subprocess != NULL);

  r = ide_subprocess_communicate_utf8 (subprocess, NULL, NULL, &stdout_buf, NULL, &error);
  g_assert_no_error (error);
  g_assert_cmpint (r, ==, TRUE);

  g_assert (stdout_buf != NULL);
  g_assert (g_utf8_validate (stdout_buf, -1, NULL));
}

static void
test_stdout_fd (void)
{
  IdeSubprocessLauncher *launcher;
  g_autoptr(IdeSubprocess) subprocess = NULL;
  g_autoptr(GError) error = NULL;
  g_autofree gchar *pattern = NULL;
  gchar buffer[4096];
  gboolean r;
  gint fd;

  launcher = ide_subprocess_launcher_new (G_SUBPROCESS_FLAGS_STDERR_SILENCE);
  ide_subprocess_launcher_push_argv (launcher, "ls");

  pattern = g_build_filename (g_get_tmp_dir (), "makecache-XXXXXX", NULL);
  fd = g_mkstemp (pattern);
  g_assert_cmpint (fd, !=, -1);

  ide_subprocess_launcher_take_stdout_fd (launcher, dup (fd));

  subprocess = ide_subprocess_launcher_spawn (launcher, NULL, &error);
  g_assert_no_error (error);
  g_assert (subprocess != NULL);

  r = ide_subprocess_wait (subprocess, NULL, &error);
  g_assert_no_error (error);
  g_assert_cmpint (r, ==, TRUE);

  r = lseek (fd, 0, SEEK_SET);
  g_assert_cmpint (r, ==, 0);

  r = read (fd, buffer, sizeof buffer);
  g_assert_cmpint (r, >, 0);

  r = g_unlink (pattern);
  g_assert_cmpint (r, ==, 0);

  close (fd);
}

static int
check_args (IdeSubprocessLauncher *launcher,
            const gchar *argv0,
            ...)
{
  va_list args;
  const gchar * const * actual_argv;
  guint num_args;
  gchar *item;

  g_assert (IDE_IS_SUBPROCESS_LAUNCHER (launcher));

  actual_argv = ide_subprocess_launcher_get_argv (launcher);

  if (actual_argv == NULL && argv0 == NULL)
    return 1;
  else if (actual_argv == NULL || argv0 == NULL)
    return 0;

  num_args = 0;
  if (g_strcmp0 (argv0, actual_argv[num_args++]) != 0)
    return 0;

  va_start (args, argv0);
  while (NULL != (item = va_arg (args, gchar *)))
    {
      const gchar *next_arg = NULL;
      next_arg = actual_argv[num_args++];
      if (g_strcmp0 (next_arg, item) != 0)
        {
          va_end (args);
          return 0;
        }
    }
  va_end (args);

  if (actual_argv[num_args] == NULL)
    return 1;
  else
    return 0;
}

static void
test_argv_manipulation (void)
{
  g_autoptr(IdeSubprocessLauncher) launcher = NULL;
  g_autofree gchar *popped = NULL;

  launcher = ide_subprocess_launcher_new (0);
  g_assert (launcher != NULL);
  g_object_add_weak_pointer (G_OBJECT (launcher), (gpointer *)&launcher);

  ide_subprocess_launcher_push_argv (launcher, "echo");
  ide_subprocess_launcher_push_argv (launcher, "world");
  ide_subprocess_launcher_insert_argv (launcher, 1, "hello");
  g_assert_cmpint (check_args (launcher, "echo", "hello", "world", NULL), !=, 0);

  ide_subprocess_launcher_replace_argv (launcher, 2, "universe");
  g_assert_cmpint (check_args (launcher, "echo", "hello", "universe", NULL), !=, 0);

  popped = ide_subprocess_launcher_pop_argv (launcher);
  g_assert_cmpstr (popped, ==, "universe");
  g_assert_cmpint (check_args (launcher, "echo", "hello", NULL), !=, 0);

  g_object_unref (launcher);
  g_assert (launcher == NULL);
}

gint
main (gint   argc,
      gchar *argv[])
{
  g_test_init (&argc, &argv, NULL);
  g_test_add_func ("/Ide/SubprocessLauncher/basic", test_basic);
  g_test_add_func ("/Ide/SubprocessLauncher/communicate", test_communicate);
  g_test_add_func ("/Ide/SubprocessLauncher/argv-manipulation", test_argv_manipulation);
  g_test_add_func ("/Ide/SubprocessLauncher/take_stdout_fd", test_stdout_fd);
  return g_test_run ();
}
