/* ide-flatpak.c
 *
 * Copyright (C) 2016 Christian Hergert <chergert@redhat.com>
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

#include "ide-flatpak.h"

/**
 * ide_is_flatpak:
 *
 * This function checks to see if the application is running within
 * a flatpak. This might be useful for cases where you need to perform
 * a different command when you are in the bundled flatpak version.
 */
gboolean
ide_is_flatpak (void)
{
  static gboolean checked;
  static gboolean is_flatpak;

  if (!checked)
    {
      g_autofree gchar *path = NULL;

      path = g_build_filename (g_get_user_runtime_dir (),
                               "flatpak-info",
                               NULL);
      is_flatpak = g_file_test (path, G_FILE_TEST_EXISTS);
      checked = TRUE;
    }

  return is_flatpak;
}
