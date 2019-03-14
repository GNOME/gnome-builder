/* gbp-flatpak-util.c
 *
 * Copyright 2016-2019 Christian Hergert <chergert@redhat.com>
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

#define G_LOG_DOMAIN "gbp-flatpak-util"

#include <flatpak.h>
#include <string.h>
#include <libide-foundry.h>
#include <libide-vcs.h>

#include "gbp-flatpak-util.h"

gchar *
gbp_flatpak_get_repo_dir (IdeContext *context)
{
  return ide_context_cache_filename (context, "flatpak", "repo", NULL);
}

gchar *
gbp_flatpak_get_staging_dir (IdePipeline *pipeline)
{
  g_autofree gchar *branch = NULL;
  g_autofree gchar *name = NULL;
  g_autofree gchar *arch = NULL;
  g_autoptr (IdeTriplet) triplet = NULL;
  g_autoptr(IdeContext) context = NULL;
  g_autoptr(IdeVcs) vcs = NULL;
  g_autoptr(IdeToolchain) toolchain = NULL;

  g_assert (IDE_IS_PIPELINE (pipeline));

  context = ide_object_ref_context (IDE_OBJECT (pipeline));
  vcs = ide_vcs_ref_from_context (context);
  branch = ide_vcs_get_branch_name (vcs);
  arch = ide_pipeline_get_arch (pipeline);
  name = g_strdup_printf ("%s-%s", arch, branch);

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

gboolean
gbp_flatpak_split_id (const gchar  *str,
                      gchar       **id,
                      gchar       **arch,
                      gchar       **branch)
{
  g_auto(GStrv) parts = g_strsplit (str, "/", 0);
  guint i = 0;

  if (id)
    *id = NULL;

  if (arch)
    *arch = NULL;

  if (branch)
    *branch = NULL;

  if (parts[i] != NULL)
    {
      if (id != NULL)
        *id = g_strdup (parts[i]);
    }
  else
    {
      /* we require at least a runtime/app ID */
      return FALSE;
    }

  i++;

  if (parts[i] != NULL)
    {
      if (arch != NULL)
        *arch = g_strdup (parts[i]);
    }
  else
    return TRUE;

  i++;

  if (parts[i] != NULL)
    {
      if (branch != NULL && !ide_str_empty0 (parts[i]))
        *branch = g_strdup (parts[i]);
    }

  return TRUE;
}
