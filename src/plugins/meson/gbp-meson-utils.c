/* gbp-meson-utils.c
 *
 * Copyright 2018 Collabora Ltd.
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
 * Authors: Corentin Noël <corentin.noel@collabora.com>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#define G_LOG_DOMAIN "gbp-meson-utils"

#include <glib/gi18n.h>

#include "gbp-meson-utils.h"

void
gbp_meson_key_file_set_string_quoted (GKeyFile    *keyfile,
                                      const gchar *group,
                                      const gchar *key,
                                      const gchar *unquoted_value)
{
  g_autofree gchar *quoted_value = NULL;

  g_return_if_fail (keyfile != NULL);
  g_return_if_fail (group != NULL);
  g_return_if_fail (key != NULL);
  g_return_if_fail (unquoted_value != NULL);

  quoted_value = g_strdup_printf ("'%s'", unquoted_value);
  g_key_file_set_string (keyfile, group, key, quoted_value);
}

void
gbp_meson_key_file_set_string_array_quoted (GKeyFile    *keyfile,
                                            const gchar *group,
                                            const gchar *key,
                                            const gchar *unquoted_value)
{
  g_autofree gchar *quoted_value = NULL;

  g_return_if_fail (keyfile != NULL);
  g_return_if_fail (group != NULL);
  g_return_if_fail (key != NULL);
  g_return_if_fail (unquoted_value != NULL);

  quoted_value = g_strdup_printf ("['%s']", unquoted_value);
  g_key_file_set_string (keyfile, group, key, quoted_value);
}

gchar *
gbp_meson_key_file_get_string_quoted (GKeyFile     *key_file,
                                      const gchar  *group_name,
                                      const gchar  *key,
                                      GError      **error)
{
  g_autofree gchar *value = NULL;
  value = g_key_file_get_string (key_file, group_name, key, error);
  /* We need to remove leading and trailing aportrophe */
  if (value != NULL)
    return g_utf8_substring (value, 1, g_utf8_strlen (value, -1) - 1);

  return NULL;
}

const gchar *
gbp_meson_get_toolchain_language (const gchar *meson_tool_name)
{
  g_return_val_if_fail (meson_tool_name != NULL, NULL);

  if (g_strcmp0 (meson_tool_name, "cpp") == 0)
    return IDE_TOOLCHAIN_LANGUAGE_CPLUSPLUS;

  if (g_strcmp0 (meson_tool_name, "valac") == 0)
    return IDE_TOOLCHAIN_LANGUAGE_VALA;

  return meson_tool_name;
}

const gchar *
gbp_meson_get_tool_display_name (const gchar *tool_id)
{
  g_return_val_if_fail (tool_id != NULL, NULL);

  if (g_strcmp0 (tool_id, IDE_TOOLCHAIN_TOOL_CC) == 0)
    return _("Compiler");
  if (g_strcmp0 (tool_id, IDE_TOOLCHAIN_TOOL_CPP) == 0)
    return _("Preprocessor");
  if (g_strcmp0 (tool_id, IDE_TOOLCHAIN_TOOL_AR) == 0)
    return _("Archiver");
  if (g_strcmp0 (tool_id, IDE_TOOLCHAIN_TOOL_LD) == 0)
    return _("Linker");
  if (g_strcmp0 (tool_id, IDE_TOOLCHAIN_TOOL_STRIP) == 0)
    return _("Strip");
  if (g_strcmp0 (tool_id, IDE_TOOLCHAIN_TOOL_EXEC) == 0)
    return _("Executable wrapper");
  if (g_strcmp0 (tool_id, IDE_TOOLCHAIN_TOOL_PKG_CONFIG) == 0)
    return _("Package Config");

  return tool_id;
}

const gchar *
gbp_meson_get_tool_binary_name (const gchar *tool_id)
{
  if (g_strcmp0 (tool_id, IDE_TOOLCHAIN_TOOL_PKG_CONFIG) == 0)
    return "pkgconfig";
  else if (g_strcmp0 (tool_id, IDE_TOOLCHAIN_TOOL_EXEC) == 0)
    return "exe_wrapper";
  else
    return tool_id;
}

const gchar *
gbp_meson_get_tool_id_from_binary (const gchar *meson_tool_name)
{
  g_return_val_if_fail (meson_tool_name != NULL, NULL);

  if (g_strcmp0 (meson_tool_name, "ar") == 0)
    return IDE_TOOLCHAIN_TOOL_AR;
  else if (g_strcmp0 (meson_tool_name, "strip") == 0)
    return IDE_TOOLCHAIN_TOOL_STRIP;
  else if (g_strcmp0 (meson_tool_name, "pkgconfig") == 0)
    return IDE_TOOLCHAIN_TOOL_PKG_CONFIG;
  else if (g_strcmp0 (meson_tool_name, "exe_wrapper") == 0)
    return IDE_TOOLCHAIN_TOOL_EXEC;
  else
    return IDE_TOOLCHAIN_TOOL_CC;
}

static gboolean
devenv_sanity_check (char *contents,
                     gsize len)
{
  IdeLineReader reader;
  char *line;
  gsize line_len;

  /* Failures tend to have an empty first line */
  if (*contents == '\n')
    return FALSE;

  ide_line_reader_init (&reader, contents, len);
  while ((line = ide_line_reader_next (&reader, &line_len)))
    {
      line[line_len] = 0;

      if (g_str_has_prefix (line, "ERROR:"))
        return FALSE;
    }

  return TRUE;
}

gboolean
gbp_meson_devenv_sanity_check (const gchar *path)
{
  g_autofree char *contents = NULL;
  gsize len;

  if (!g_file_get_contents (path, &contents, &len, NULL))
    return FALSE;

  return devenv_sanity_check (contents, len);
}
