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
  g_autoptr(IdeTweaks) tweaks = NULL;
  g_autoptr(GString) string = NULL;

  _ide_tweaks_init ();

  tweaks = ide_tweaks_new ();
  string = g_string_new (NULL);

  for (guint i = 1; i < argc; i++)
    {
      const char *path = argv[i];
      g_autoptr(GFile) file = g_file_new_for_commandline_arg (path);
      g_autoptr(GError) error = NULL;

      if (!ide_tweaks_load_from_file (tweaks, file, NULL, &error))
        {
          g_printerr ("Failed to parse %s: %s\n", path, error->message);
          return EXIT_FAILURE;
        }
    }

  _ide_tweaks_item_printf (IDE_TWEAKS_ITEM (tweaks), string, 0);

  g_print ("%s", string->str);

  return EXIT_SUCCESS;
}
