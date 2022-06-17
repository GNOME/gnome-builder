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

static void
test_run_context_default_handler (void)
{
  IdeRunContext *run_context;
  IdeSubprocessLauncher *launcher;
  g_autoptr(GError) error = NULL;

  run_context = ide_run_context_new ();

  ide_run_context_set_cwd (run_context, "/home/user");
  ide_run_context_set_argv (run_context, IDE_STRV_INIT ("ls", "-lsah"));
  ide_run_context_setenv (run_context, "USER", "user");
  ide_run_context_setenv (run_context, "UID", "1000");
  ide_run_context_take_fd (run_context, dup (STDOUT_FILENO), STDOUT_FILENO);

  ide_run_context_push (run_context, NULL, NULL, NULL);
  ide_run_context_set_argv (run_context, IDE_STRV_INIT ("wrapper", "--"));
  ide_run_context_set_environ (run_context, IDE_STRV_INIT ("USER=nobody"));

  launcher = ide_run_context_end (run_context, &error);
  g_assert_no_error (error);
  g_assert_true (IDE_IS_SUBPROCESS_LAUNCHER (launcher));

  g_assert_true (g_strv_equal (ide_subprocess_launcher_get_argv (launcher),
                               IDE_STRV_INIT ("wrapper", "--", "env", "USER=user", "ls", "-lsah")));
  g_assert_true (g_strv_equal (ide_subprocess_launcher_get_environ (launcher),
                               IDE_STRV_INIT ("USER=nobody")));

  g_assert_finalize_object (launcher);
  g_assert_finalize_object (run_context);
}

int
main (int   argc,
      char *argv[])
{
  g_test_init (&argc, &argv, NULL);
  g_test_add_func ("/Ide/Foundry/RunContext/default_handler", test_run_context_default_handler);
  return g_test_run ();
}
