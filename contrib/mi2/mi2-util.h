/* mi2-util.h
 *
 * Copyright (C) 2017 Christian Hergert <chergert@redhat.com>
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

#ifndef MI2_UTIL_H
#define MI2_UTIL_H

#include <glib.h>

G_BEGIN_DECLS

gchar    *mi2_util_parse_string (const gchar  *line,
                                 const gchar **endptr);
gchar    *mi2_util_parse_word   (const gchar  *line,
                                 const gchar **endptr);
GVariant *mi2_util_parse_record (const gchar  *line,
                                 const gchar **endptr);
GVariant *mi2_util_parse_list   (const gchar  *line,
                                 const gchar **endptr);

G_END_DECLS

#endif /* MI2_UTIL_H */
