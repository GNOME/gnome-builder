/* test-run-context.c
 *
 * Copyright 2022 Christian Hergert <chergert@redhat.com>
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

#include <unistd.h>

#include <libide-foundry.h>

static int
sort_strv_strcmpptr (gconstpointer a,
                     gconstpointer b,
                     gpointer      user_data)
{
  const char *stra = *(const char **)a;
  const char *strb = *(const char **)b;

  return g_strcmp0 (stra, strb);
}

static char **
sort_strv (const char * const *strv)
{
  char **copy = g_strdupv ((char **)strv);
  gsize n_elements = g_strv_length (copy);
  g_sort_array (copy, n_elements, sizeof (char *), sort_strv_strcmpptr, NULL);
  return copy;
}

static void
compare_strv_unordered (const gchar * const *strv,
                        const gchar * const *expected_strv)
{
  char **strv_sorted;
  char **expected_strv_sorted;

  if (expected_strv == NULL)
    {
      g_assert_true (strv == NULL || strv[0] == NULL);
      return;
    }

  g_assert_nonnull (strv);

  g_assert_cmpint (g_strv_length ((char **)strv), ==, g_strv_length ((char **)expected_strv));

  strv_sorted = sort_strv (strv);
  expected_strv_sorted = sort_strv (strv);

  for (guint i = 0; strv_sorted[i]; i++)
    g_assert_cmpstr (strv_sorted[i], ==, expected_strv_sorted[i]);

  g_strfreev (strv_sorted);
  g_strfreev (expected_strv_sorted);
}

static void
test_run_context_environ (void)
{
  IdeRunContext *run_context;

  run_context = ide_run_context_new ();

  ide_run_context_setenv (run_context, "FOO", "BAR");
  compare_strv_unordered (ide_run_context_get_environ (run_context),
                          IDE_STRV_INIT ("FOO=BAR"));
  g_assert_cmpstr (ide_run_context_getenv (run_context, "FOO"), ==, "BAR");

  ide_run_context_setenv (run_context, "FOO", "123");
  compare_strv_unordered (ide_run_context_get_environ (run_context),
                          IDE_STRV_INIT ("FOO=123"));

  ide_run_context_setenv (run_context, "ABC", "DEF");
  compare_strv_unordered (ide_run_context_get_environ (run_context),
                          IDE_STRV_INIT ("FOO=123", "ABC=DEF"));

  ide_run_context_unsetenv (run_context, "FOO");
  compare_strv_unordered (ide_run_context_get_environ (run_context),
                          IDE_STRV_INIT ("ABC=DEF"));

  g_assert_finalize_object (run_context);
}

static void
test_run_context_argv (void)
{
  IdeRunContext *run_context;

  run_context = ide_run_context_new ();

  ide_run_context_prepend_argv (run_context, "1");
  ide_run_context_prepend_argv (run_context, "0");
  ide_run_context_append_argv (run_context, "2");
  ide_run_context_append_args (run_context, IDE_STRV_INIT ("3", "4"));
  ide_run_context_prepend_args (run_context, IDE_STRV_INIT ("a", "b"));

  g_assert_true (g_strv_equal (ide_run_context_get_argv (run_context),
                               IDE_STRV_INIT ("a", "b", "0", "1", "2", "3", "4")));

  g_assert_finalize_object (run_context);
}

static void
test_run_context_default_handler (void)
{
  IdeRunContext *run_context;
  IdeSubprocessLauncher *launcher;
  g_autoptr(GError) error = NULL;

  run_context = ide_run_context_new ();

  ide_run_context_set_argv (run_context, IDE_STRV_INIT ("wrapper", "--"));
  ide_run_context_set_environ (run_context, IDE_STRV_INIT ("USER=nobody"));

  ide_run_context_push (run_context, NULL, NULL, NULL);
  ide_run_context_set_cwd (run_context, "/home/user");
  ide_run_context_set_argv (run_context, IDE_STRV_INIT ("ls", "-lsah"));
  ide_run_context_setenv (run_context, "USER", "user");
  ide_run_context_setenv (run_context, "UID", "1000");
  ide_run_context_take_fd (run_context, dup (STDOUT_FILENO), STDOUT_FILENO);

  launcher = ide_run_context_end (run_context, &error);
  g_assert_no_error (error);
  g_assert_true (IDE_IS_SUBPROCESS_LAUNCHER (launcher));

  g_assert_true (g_strv_equal (ide_subprocess_launcher_get_argv (launcher),
                               IDE_STRV_INIT ("wrapper", "--", "env", "UID=1000", "USER=user", "ls", "-lsah")));
  g_assert_true (g_strv_equal (ide_subprocess_launcher_get_environ (launcher),
                               IDE_STRV_INIT ("USER=nobody")));

  g_assert_finalize_object (launcher);
  g_assert_finalize_object (run_context);
}

static gboolean
custom_handler (IdeRunContext       *run_context,
                const char * const  *argv,
                const char * const  *env,
                const char          *cwd,
                IdeUnixFDMap        *unix_fd_map,
                gpointer             user_data,
                GError             **error)
{
  const char * const *subenv = ide_run_context_get_environ (run_context);

  for (guint i = 0; subenv[i]; i++)
    {
      g_autofree char *arg = g_strdup_printf ("--env=%s", subenv[i]);
      ide_run_context_prepend_argv (run_context, arg);
    }

  ide_run_context_prepend_args (run_context, argv);
  ide_run_context_set_environ (run_context, env);

  return TRUE;
}

static void
test_run_context_custom_handler (void)
{
  IdeRunContext *run_context;
  IdeSubprocessLauncher *launcher;
  g_autoptr(GError) error = NULL;

  run_context = ide_run_context_new ();

  ide_run_context_set_argv (run_context, IDE_STRV_INIT ("ls", "-lsah"));
  ide_run_context_setenv (run_context, "USER", "user");
  ide_run_context_setenv (run_context, "UID", "1000");

  ide_run_context_push (run_context, custom_handler, NULL, NULL);
  ide_run_context_set_argv (run_context, IDE_STRV_INIT ("flatpak", "build"));

  launcher = ide_run_context_end (run_context, &error);
  g_assert_no_error (error);
  g_assert_true (IDE_IS_SUBPROCESS_LAUNCHER (launcher));

  g_assert_true (g_strv_equal (ide_subprocess_launcher_get_argv (launcher),
                               IDE_STRV_INIT ("flatpak", "build", "--env=UID=1000", "--env=USER=user", "ls", "-lsah")));

  g_assert_finalize_object (launcher);
  g_assert_finalize_object (run_context);
}

static void
test_run_context_push_shell (void)
{
  IdeRunContext *run_context;
  IdeSubprocessLauncher *launcher;
  GError *error = NULL;

  run_context = ide_run_context_new ();
  ide_run_context_push_shell (run_context, TRUE);
  ide_run_context_setenv (run_context, "PATH", "path");
  ide_run_context_append_argv (run_context, "which");
  ide_run_context_append_argv (run_context, "foo");

  launcher = ide_run_context_end (run_context, &error);
  g_assert_no_error (error);
  g_assert_nonnull (launcher);

  g_assert_true (g_strv_equal (ide_subprocess_launcher_get_argv (launcher),
                               IDE_STRV_INIT ("/bin/sh", "-l", "-c", "env 'PATH=path' 'which' 'foo'")));

  g_assert_finalize_object (launcher);
  g_assert_finalize_object (run_context);
}

int
main (int   argc,
      char *argv[])
{
  g_test_init (&argc, &argv, NULL);
  g_test_add_func ("/Ide/Foundry/RunContext/environ", test_run_context_environ);
  g_test_add_func ("/Ide/Foundry/RunContext/argv", test_run_context_argv);
  g_test_add_func ("/Ide/Foundry/RunContext/default_handler", test_run_context_default_handler);
  g_test_add_func ("/Ide/Foundry/RunContext/custom_handler", test_run_context_custom_handler);
  g_test_add_func ("/Ide/Foundry/RunContext/push_shell", test_run_context_push_shell);
  return g_test_run ();
}
