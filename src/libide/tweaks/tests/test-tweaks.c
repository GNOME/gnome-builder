/* test-tweaks.c
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

#include <unistd.h>

#include <libide-tweaks.h>

#include "ide-tweaks-init.h"
#include "ide-tweaks-item-private.h"

int
main (int   argc,
      char *argv[])
{
  g_autoptr(GOptionContext) context = NULL;
  g_autoptr(IdeTweaks) tweaks = NULL;
  g_autoptr(GString) string = NULL;
  g_autoptr(GError) error = NULL;
  g_autofree char *expected = NULL;
  g_autofree char *expected_contents = NULL;
  gsize len = 0;
  const GOptionEntry entries[] = {
    { "expected", 'e', 0, G_OPTION_ARG_FILENAME, &expected, "File containing expected output" },
    { NULL }
  };

  _ide_tweaks_init ();

  context = g_option_context_new ("- test tweaks ui merging");
  g_option_context_add_main_entries (context, entries, NULL);

  if (!g_option_context_parse (context, &argc, &argv, &error))
    {
      g_printerr ("%s\n", error->message);
      return EXIT_FAILURE;
    }

  tweaks = ide_tweaks_new ();
  string = g_string_new (NULL);

  for (guint i = 1; i < argc; i++)
    {
      const char *path = argv[i];
      g_autoptr(GFile) file = g_file_new_for_commandline_arg (path);

      if (!ide_tweaks_load_from_file (tweaks, file, NULL, &error))
        g_error ("Failed to parse %s: %s", path, error->message);
    }

  _ide_tweaks_item_printf (IDE_TWEAKS_ITEM (tweaks), string, 0);

  if (!expected)
    {
      g_print ("%s", string->str);
      return EXIT_SUCCESS;
    }

  if (!g_file_get_contents (expected, &expected_contents, &len, &error))
    g_error ("Failed to load expected contents: %s: %s", expected, error->message);

  if (ide_str_equal0 (expected_contents, string->str))
    return EXIT_SUCCESS;

  g_printerr ("Contents did not match.\n"
              "\n"
              "Expected:\n"
              "=========\n"
              "%s\n"
              "\n"
              "Got:\n"
              "====\n"
              "%s\n",
              expected_contents,
              string->str);

  return EXIT_FAILURE;
}
