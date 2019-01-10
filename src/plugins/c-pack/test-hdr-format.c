/* test-hdr-format.c
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
 */

#include "hdr-format.c"

gint
main (gint argc,
      gchar *argv[])
{
  g_autofree gchar *contents = NULL;
  g_autofree gchar *ret = NULL;
  g_autoptr(GError) error = NULL;
  gsize len;

  if (argc < 2)
    {
      g_printerr ("usage: %s FILENAME\n", argv[0]);
      return 1;
    }

  if (!g_file_get_contents (argv[1], &contents, &len, &error))
    {
      g_printerr ("%s\n", error->message);
      return 1;
    }

  ret = hdr_format_string (contents, len);

  g_print ("%s\n", ret);

  return 0;
}
