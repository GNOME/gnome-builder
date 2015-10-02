/* gb-string.h
 *
 * Copyright (C) 2014 Christian Hergert <christian@hergert.me>
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

#ifndef GB_STRING_H
#define GB_STRING_H

#include <glib.h>

G_BEGIN_DECLS

#define gb_str_equal0(s1,s2) \
  (((s1) == (s2)) || ((s1) && (s2) && g_str_equal(s1,s2)))

typedef enum
{
  GB_HIGHLIGHT_UNDERLINE,
  GB_HIGHLIGHT_BOLD,
} GbHighlightType;

gchar *gb_str_highlight      (const gchar     *src,
                              const gchar     *match);
gchar *gb_str_highlight_full (const gchar     *str,
                              const gchar     *match,
                              gboolean         insensitive,
                              GbHighlightType  type);
gboolean gb_str_simple_match (const gchar     *haystack,
                              const gchar     *needle_down);

G_END_DECLS

#endif /* GB_STRING_H */
