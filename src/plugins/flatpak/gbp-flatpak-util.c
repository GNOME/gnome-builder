/* gbp-flatpak-util.c
 *
 * Copyright © 2016 Christian Hergert <chergert@redhat.com>
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

#include <flatpak.h>
#include <string.h>

#include "gbp-flatpak-util.h"

gchar *
gbp_flatpak_get_repo_dir (IdeConfiguration *configuration)
{
  g_autofree gchar *branch = NULL;
  g_autofree gchar *name = NULL;
  const gchar *runtime_id;
  IdeContext *context;
  IdeVcs *vcs;

  g_assert (IDE_IS_CONFIGURATION (configuration));

  context = ide_object_get_context (IDE_OBJECT (configuration));
  vcs = ide_context_get_vcs (context);
  branch = ide_vcs_get_branch_name (vcs);
  runtime_id = ide_configuration_get_runtime_id (configuration);

  if (branch != NULL)
    name = g_strdup_printf ("%s:%s", runtime_id, branch);
  else
    name = g_strdup (runtime_id);

  g_strdelimit (name, G_DIR_SEPARATOR_S, '-');

  return ide_context_cache_filename (context, "flatpak", "repos", name, NULL);
}

gchar *
gbp_flatpak_get_staging_dir (IdeConfiguration *configuration)
{
  g_autofree gchar *branch = NULL;
  g_autofree gchar *name = NULL;
  const gchar *runtime_id;
  IdeContext *context;
  IdeVcs *vcs;

  g_assert (IDE_IS_CONFIGURATION (configuration));

  context = ide_object_get_context (IDE_OBJECT (configuration));
  vcs = ide_context_get_vcs (context);
  branch = ide_vcs_get_branch_name (vcs);
  runtime_id = ide_configuration_get_runtime_id (configuration);

  if (branch != NULL)
    name = g_strdup_printf ("%s:%s", runtime_id, branch);
  else
    name = g_strdup (runtime_id);

  g_strdelimit (name, G_DIR_SEPARATOR_S, '-');

  return ide_context_cache_filename (context, "flatpak", "staging", name, NULL);
}

gboolean
gbp_flatpak_is_ignored (const gchar *name)
{
  if (name == NULL)
    return TRUE;

  return g_str_has_suffix (name, ".Locale") ||
         g_str_has_suffix (name, ".Debug") ||
         g_str_has_suffix (name, ".Docs") ||
         g_str_has_suffix (name, ".Sources") ||
         g_str_has_suffix (name, ".Var") ||
         g_str_has_prefix (name, "org.gtk.Gtk3theme.") ||
         strstr (name, ".GL.nvidia") != NULL ||
         strstr (name, ".GL32.nvidia") != NULL ||
         strstr (name, ".VAAPI") != NULL ||
         strstr (name, ".Icontheme") != NULL ||
         strstr (name, ".Extension") != NULL ||
         strstr (name, ".Gtk3theme") != NULL ||
         strstr (name, ".KStyle") != NULL ||
         strstr (name, ".PlatformTheme") != NULL;
}

