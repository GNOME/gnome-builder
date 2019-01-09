/* ide-language.c
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

#define G_LOG_DOMAIN "ide-language"

#include "config.h"

#include <libide-io.h>
#include <string.h>
#include <tmpl-glib.h>

#include "ide-language.h"

gchar *
ide_language_format_header (GtkSourceLanguage *self,
                            const gchar       *header)
{
  IdeLineReader reader;
  const gchar *first_prefix;
  const gchar *last_prefix;
  const gchar *line_prefix;
  const gchar *line;
  gboolean first = TRUE;
  GString *outstr;
  gsize len;
  guint prefix_len;

  g_return_val_if_fail (GTK_SOURCE_IS_LANGUAGE (self), NULL);
  g_return_val_if_fail (header != NULL, NULL);

  first_prefix = gtk_source_language_get_metadata (self, "block-comment-start");
  last_prefix = gtk_source_language_get_metadata (self, "block-comment-end");
  line_prefix = gtk_source_language_get_metadata (self, "line-comment-start");

  if ((g_strcmp0 (first_prefix, "/*") == 0) &&
      (g_strcmp0 (last_prefix, "*/") == 0))
    line_prefix = " *";

  if (first_prefix == NULL || last_prefix == NULL)
    {
      first_prefix = line_prefix;
      last_prefix = line_prefix;
    }

  prefix_len = strlen (first_prefix);

  outstr = g_string_new (NULL);

  ide_line_reader_init (&reader, (gchar *)header, -1);

  while (NULL != (line = ide_line_reader_next (&reader, &len)))
    {
      if (first)
        {
          g_string_append (outstr, first_prefix);
          first = FALSE;
        }
      else if (line_prefix == NULL)
        {
          for (guint i = 0; i < prefix_len; i++)
            g_string_append_c (outstr, ' ');
        }
      else
        {
          g_string_append (outstr, line_prefix);
        }

      if (len)
        {
          g_string_append_c (outstr, ' ');
          g_string_append_len (outstr, line, len);
        }

      /* Lines ending in expansion need an extra \n */
      if (outstr->len > 2 &&
          outstr->str[outstr->len - 2] == '}' &&
          outstr->str[outstr->len - 1] == '}')
        g_string_append_c (outstr, '\n');

      g_string_append_c (outstr, '\n');
    }

  if (last_prefix && g_strcmp0 (first_prefix, last_prefix) != 0)
    {
      if (line_prefix && *line_prefix == ' ')
        g_string_append_c (outstr, ' ');
      g_string_append (outstr, last_prefix);
      g_string_append_c (outstr, '\n');
    }

  return g_string_free (outstr, FALSE);
}
