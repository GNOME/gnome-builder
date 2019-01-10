/* test-line-reader.c
 *
 * Copyright 2018 Christian Hergert <chergert@redhat.com>
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

#include <libide-io.h>
#include <string.h>

static void
test_line_reader_basic (void)
{
  IdeLineReader reader;
  gchar *str;
  gsize len;

  ide_line_reader_init (&reader, (gchar *)"a\nb\nc\r\nd\ne", -1);

  str = ide_line_reader_next (&reader, &len);
  g_assert_cmpint (len, ==, 1);
  g_assert (strncmp (str, "a", 1) == 0);

  str = ide_line_reader_next (&reader, &len);
  g_assert_cmpint (len, ==, 1);
  g_assert (strncmp (str, "b", 1) == 0);

  str = ide_line_reader_next (&reader, &len);
  g_assert_cmpint (len, ==, 1);
  g_assert (strncmp (str, "c", 1) == 0);

  str = ide_line_reader_next (&reader, &len);
  g_assert_cmpint (len, ==, 1);
  g_assert (strncmp (str, "d", 1) == 0);

  str = ide_line_reader_next (&reader, &len);
  g_assert_cmpint (len, ==, 1);
  g_assert (strncmp (str, "e", 1) == 0);
}

gint
main (gint   argc,
      gchar *argv[])
{
  g_test_init (&argc, &argv, NULL);
  g_test_add_func ("/Ide/LineReader/basic", test_line_reader_basic);
  return g_test_run ();
}
