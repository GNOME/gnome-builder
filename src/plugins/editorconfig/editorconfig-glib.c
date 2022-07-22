/* editorconfig-glib.c
 *
 * Copyright 2015-2019 Christian Hergert <christian@hergert.me>
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

#define G_LOG_DOMAIN "editorconfig-glib"

#include "config.h"

#include <editorconfig/editorconfig.h>

#include "editorconfig-glib.h"

static void
_g_value_free (gpointer data)
{
  GValue *value = data;

  g_value_unset (value);
  g_free (value);
}

GHashTable *
editorconfig_glib_read (GFile         *file,
                        GCancellable  *cancellable,
                        GError       **error)
{
  editorconfig_handle handle = { 0 };
  GHashTable *ret = NULL;
  gchar *filename = NULL;
  gint code;
  gint count;
  guint i;

  filename = g_file_get_path (file);

  if (!filename)
    {
      /*
       * This sucks, but we need to basically rewrite editorconfig library
       * to support this. Not out of the question, but it is for today.
       */
      g_set_error (error,
                   G_IO_ERROR,
                   G_IO_ERROR_NOT_SUPPORTED,
                   "only local files are currently supported");
      return NULL;
    }

  handle = editorconfig_handle_init ();
  code = editorconfig_parse (filename, handle);

  switch (code)
    {
    case 0:
      break;

    case EDITORCONFIG_PARSE_NOT_FULL_PATH:
    case EDITORCONFIG_PARSE_MEMORY_ERROR:
    case EDITORCONFIG_PARSE_VERSION_TOO_NEW:
    default:
      g_set_error (error,
                   G_IO_ERROR,
                   G_IO_ERROR_FAILED,
                   "Failed to parse editorconfig.");
      goto cleanup;
    }

  count = editorconfig_handle_get_name_value_count (handle);

  ret = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, _g_value_free);

  for (i = 0; i < count; i++)
    {
      GValue *value;
      const gchar *key = NULL;
      const gchar *valuestr = NULL;

      value = g_new0 (GValue, 1);

      editorconfig_handle_get_name_value (handle, i, &key, &valuestr);

      if ((g_strcmp0 (key, "tab_width") == 0) ||
          (g_strcmp0 (key, "max_line_length") == 0) ||
          (g_strcmp0 (key, "indent_size") == 0))
        {
          g_value_init (value, G_TYPE_INT);
          g_value_set_int (value, g_ascii_strtoll (valuestr, NULL, 10));
        }
      else if ((g_strcmp0 (key, "insert_final_newline") == 0) ||
               (g_strcmp0 (key, "trim_trailing_whitespace") == 0))
        {
          char *lower = g_utf8_strdown (valuestr, -1);

          g_value_init (value, G_TYPE_BOOLEAN);

          if (g_strcmp0 (lower, "true") == 0 ||
              g_strcmp0 (lower, "yes") == 0 ||
              g_strcmp0 (lower, "t") == 0 ||
              g_strcmp0 (lower, "y") == 0 ||
              g_strcmp0 (lower, "1") == 0)
            g_value_set_boolean (value, TRUE);
          else if (g_strcmp0 (lower, "false") == 0 ||
                   g_strcmp0 (lower, "no") == 0 ||
                   g_strcmp0 (lower, "f") == 0 ||
                   g_strcmp0 (lower, "n") == 0 ||
                   g_strcmp0 (lower, "0") == 0)
            g_value_set_boolean (value, FALSE);
          else
            g_warning ("Unrecognized boolean value for %s: %s", key, valuestr);

          g_free (lower);
        }
      else
        {
          g_value_init (value, G_TYPE_STRING);
          g_value_set_string (value, valuestr);
        }

      g_hash_table_replace (ret, g_strdup (key), value);
    }

cleanup:
  editorconfig_handle_destroy (handle);
  g_free (filename);

  return ret;
}
