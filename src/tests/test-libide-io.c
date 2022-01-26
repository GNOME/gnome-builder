/* test-libide-core.c
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

#include <libide-io.h>

static void
test_expand (const char *path,
             char       *expected)
{
  g_autofree char *expand = ide_path_expand (path);
  g_assert_cmpstr (expand, ==, expected);
  g_free (expected);
}

static void
test_path_expand (void)
{
  test_expand ("~/", g_build_filename (g_get_home_dir (), G_DIR_SEPARATOR_S, NULL));
  test_expand ("$HOME/foo", g_build_filename (g_get_home_dir (), "foo", NULL));
  test_expand ("foo", g_build_filename (g_get_home_dir (), "foo", NULL));
}

gint
main (int argc,
      char *argv[])
{
  g_test_init (&argc, &argv, NULL);
  g_test_add_func ("/libide-io/path/expand", test_path_expand);
  return g_test_run ();
}

