/* ide-projects-global.c
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
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#define G_LOG_DOMAIN "ide-projects-global"

#include "config.h"

#include <glib/gi18n.h>
#include <libide-io.h>

#include "ide-projects-global.h"

static GSettings *g_settings;
static gchar *projects_directory;

static void
on_projects_directory_changed_cb (GSettings   *settings,
                                  const gchar *key,
                                  gpointer     user_data)
{
  g_assert (G_IS_SETTINGS (settings));
  g_assert (key != NULL);

  g_clear_pointer (&projects_directory, g_free);
}

/**
 * ide_get_projects_dir:
 *
 * Gets the directory to store projects within.
 *
 * First, this checks GSettings for a directory. If that directory exists,
 * it is returned.
 *
 * If not, it then checks for the non-translated name "Projects" in the
 * users home directory. If it exists, that is returned.
 *
 * If that does not exist, and a GSetting path existed, but was non-existant
 * that is returned.
 *
 * If the GSetting was empty, the translated name "Projects" is returned.
 *
 * Returns: (not nullable) (transfer full): a #GFile
 *
 * Since: 3.32
 */
const gchar *
ide_get_projects_dir (void)
{
  g_return_val_if_fail (IDE_IS_MAIN_THREAD (), NULL);

  if G_UNLIKELY (g_settings == NULL)
    {
      g_settings = g_settings_new ("org.gnome.builder");
      g_signal_connect (g_settings,
                        "changed::projects-directory",
                        G_CALLBACK (on_projects_directory_changed_cb),
                        NULL);
    }

  if G_UNLIKELY (projects_directory == NULL)
    {
      g_autofree gchar *dir = g_settings_get_string (g_settings, "projects-directory");
      g_autofree gchar *expanded = ide_path_expand (dir);
      g_autofree gchar *projects = NULL;
      g_autofree gchar *translated = NULL;

      if (g_file_test (expanded, G_FILE_TEST_IS_DIR))
        {
          projects_directory = g_steal_pointer (&expanded);
          goto completed;
        }

      projects = g_build_filename (g_get_home_dir (), "Projects", NULL);

      if (g_file_test (projects, G_FILE_TEST_IS_DIR))
        {
          projects_directory = g_steal_pointer (&projects);
          goto completed;
        }

      if (!ide_str_empty0 (dir) && !ide_str_empty0 (expanded))
        {
          projects_directory = g_steal_pointer (&expanded);
          goto completed;
        }

      translated = g_build_filename (g_get_home_dir (), _("Projects"), NULL);
      projects_directory = g_steal_pointer (&translated);
    }

completed:

  return projects_directory;
}

/**
 * ide_create_project_id:
 * @name: the name of the project
 *
 * Escapes the project name into something suitable using as an id.
 * This can be uesd to determine the directory name when the project
 * name should be used.
 *
 * Returns: (transfer full): a new string
 *
 * Since: 3.32
 */
gchar *
ide_create_project_id (const gchar *name)
{
  g_return_val_if_fail (name != NULL, NULL);

  return g_strdelimit (g_strdup (name), " /|<>\n\t", '-');
}
