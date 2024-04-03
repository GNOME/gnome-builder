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

/**
 * ide_create_project_id:
 * @name: the name of the project
 *
 * Escapes the project name into something suitable using as an id.
 * This can be uesd to determine the directory name when the project
 * name should be used.
 *
 * Returns: (transfer full): a new string
 */
gchar *
ide_create_project_id (const gchar *name)
{
  g_return_val_if_fail (name != NULL, NULL);

  return g_strdelimit (g_strdup (name), " /|<>\n\t", '-');
}
