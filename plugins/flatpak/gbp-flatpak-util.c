/* gbp-flatpak-util.c
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

#define G_LOG_DOMAIN "gbp-flatpak-util"

#include "gbp-flatpak-util.h"

gchar *
gbp_flatpak_get_repo_dir (IdeConfiguration *configuration)
{
  const gchar *project_id;
  const gchar *runtime_id;
  IdeContext *context;
  IdeProject *project;

  g_assert (IDE_IS_CONFIGURATION (configuration));

  runtime_id = ide_configuration_get_runtime_id (configuration);
  context = ide_object_get_context (IDE_OBJECT (configuration));
  project = ide_context_get_project (context);
  project_id = ide_project_get_id (project);

  return g_build_filename (g_get_user_cache_dir (),
                           "gnome-builder",
                           "flatpak",
                           "repos",
                           project_id,
                           runtime_id,
                           NULL);
}

gchar *
gbp_flatpak_get_staging_dir (IdeConfiguration *configuration)
{
  const gchar *project_id;
  const gchar *runtime_id;
  IdeContext *context;
  IdeProject *project;

  g_assert (IDE_IS_CONFIGURATION (configuration));

  runtime_id = ide_configuration_get_runtime_id (configuration);
  context = ide_object_get_context (IDE_OBJECT (configuration));
  project = ide_context_get_project (context);
  project_id = ide_project_get_id (project);

  return g_build_filename (g_get_user_cache_dir (),
                           "gnome-builder",
                           "flatpak",
                           "staging",
                           project_id,
                           runtime_id,
                           NULL);
}
