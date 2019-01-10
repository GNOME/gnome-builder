/* test-ide-compile-commands.c
 *
 * Copyright 2017-2019 Christian Hergert <chergert@redhat.com>
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

#include <libide-foundry.h>

static void
test_compile_commands_basic (void)
{
  g_autoptr(IdeCompileCommands) commands = NULL;
  g_autoptr(GFile) missing = g_file_new_for_path ("missing");
  g_autoptr(GFile) data_file = NULL;
  g_autoptr(GFile) expected_file = NULL;
  g_autoptr(GFile) dir = NULL;
  g_autoptr(GFile) vala = NULL;
  g_autoptr(GError) error = NULL;
  g_autofree gchar *data_path = NULL;
  g_autofree gchar *dir_path = NULL;
  g_auto(GStrv) cmdstrv = NULL;
  g_auto(GStrv) valastrv = NULL;
  gboolean r;

  commands = ide_compile_commands_new ();

  /* Test missing info before we've loaded */
  g_assert (NULL == ide_compile_commands_lookup (commands, missing, NULL, NULL, NULL));

  /* Now load our test file */
  data_path = g_build_filename (TEST_DATA_DIR, "test-compile-commands.json", NULL);
  data_file = g_file_new_for_path (data_path);
  r = ide_compile_commands_load (commands, data_file, NULL, &error);
  g_assert_no_error (error);
  g_assert_cmpint (r, ==, TRUE);

  /* Now lookup a file that should exist in the database */
  expected_file = g_file_new_for_path ("/build/gnome-builder/subprojects/libgd/libgd/gd-types-catalog.c");
  cmdstrv = ide_compile_commands_lookup (commands, expected_file, NULL, &dir, &error);
  g_assert_no_error (error);
  g_assert (cmdstrv != NULL);
  /* ccache cc should have been removed. */
  /* relative -I paths should have been resolved */
  g_assert_cmpstr (cmdstrv[0], ==, "-I/build/gnome-builder/build/subprojects/libgd/libgd/gd@sha");
  dir_path = g_file_get_path (dir);
  g_assert_cmpstr (dir_path, ==, "/build/gnome-builder/build");

  /* Vala files don't need to match on exact filename, just something dot vala */
  vala = g_file_new_for_path ("whatever.vala");
  valastrv = ide_compile_commands_lookup (commands, vala, NULL, NULL, &error);
  g_assert_no_error (error);
  g_assert (valastrv != NULL);
  g_assert_cmpstr (valastrv[0], ==, "--pkg");
  g_assert_cmpstr (valastrv[1], ==, "json-glib-1.0");
  g_assert_cmpstr (valastrv[2], ==, "--pkg");
  g_assert_cmpstr (valastrv[3], ==, "gtksourceview-4");
}

gint
main (gint argc,
      gchar *argv[])
{
  g_test_init (&argc, &argv, NULL);
  g_test_add_func ("/Ide/CompileCommands/basic", test_compile_commands_basic);
  return g_test_run ();
}
