/* test-gstyle-palette.c
 *
 * Copyright 2016 sebastien lafargue <slafargue@gnome.org>
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

#include <glib.h>
#include <gdk/gdk.h>
#include <gtk/gtk.h>

#include "gstyle-palette.h"

static GstylePalette *
load_palette (const gchar *name)
{
  GstylePalette *palette;
  g_autoptr (GFile) file = NULL;
  g_autofree gchar *uri = NULL;
  g_autofree gchar *path = NULL;
  GError *error = NULL;
  GCancellable *cancellable = NULL;

  path = g_strdup_printf (TEST_DATA_DIR"/%s", name);
  file = g_file_new_for_path (path);
  palette = gstyle_palette_new_from_file (file, cancellable, &error);
  if (palette == NULL)
    printf ("error: %s\n", error->message);
  else
    {
      uri = g_file_get_uri (file);

      printf ("Palette:\n\turi:'%s'\n\tname:'%s'\n\tid:'%s'\n\tnb colors:%i\n",
              uri,
              gstyle_palette_get_name (palette),
              gstyle_palette_get_id (palette),
              gstyle_palette_get_len (palette));
    }

  return palette;
}

static void
test_palette (void)
{
  GstylePalette *palette;

  printf ("\n");
  palette = load_palette ("palette.xml");
  g_object_unref (palette);

  palette = load_palette ("palette.gpl");
  g_object_unref (palette);
}

int
main (gint   argc,
      gchar *argv[])
{
  g_test_init (&argc, &argv, NULL);

  g_test_add_func ("/Gstyle/palette", test_palette);

  return g_test_run ();
}
