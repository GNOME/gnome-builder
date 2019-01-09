/* ide-ctags-util.c
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

#include <string.h>

#include "ide-ctags-util.h"

const gchar * const *
ide_ctags_get_allowed_suffixes (const gchar *lang_id)
{
  static const gchar *c_languages[] = { ".c", ".h", ".cc", ".hh", ".cpp", ".hpp", ".cxx", ".hxx", NULL };
  static const gchar *vala_languages[] = { ".vala", NULL };
  static const gchar *python_languages[] = { ".py", NULL };
  static const gchar *js_languages[] = { ".js", NULL };
  static const gchar *ruby_languages[] = { ".rb", NULL };
  static const gchar *html_languages[] = { ".html", ".htm", ".tmpl", ".css", ".js", NULL };

  if (lang_id == NULL)
    return NULL;

  if (ide_str_equal0 (lang_id, "c") || ide_str_equal0 (lang_id, "chdr") || ide_str_equal0 (lang_id, "cpp"))
    return c_languages;
  else if (ide_str_equal0 (lang_id, "vala"))
    return vala_languages;
  else if (ide_str_equal0 (lang_id, "python"))
    return python_languages;
  else if (ide_str_equal0 (lang_id, "js"))
    return js_languages;
  else if (ide_str_equal0 (lang_id, "html"))
    return html_languages;
  else if (ide_str_equal0 (lang_id, "ruby"))
    return ruby_languages;
  else
    return NULL;
}

gboolean
ide_ctags_is_allowed (const IdeCtagsIndexEntry *entry,
                      const gchar * const      *allowed)
{
  if (allowed)
    {
      const gchar *dotptr = strrchr (entry->path, '.');
      gsize i;

      for (i = 0; allowed [i]; i++)
        if (ide_str_equal0 (dotptr, allowed [i]))
          return TRUE;
    }

  return FALSE;
}
