/* fuzzy.h
 *
 * Copyright (C) 2015 Christian Hergert <christian@hergert.me>
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

#ifndef FUZZY_H
#define FUZZY_H

#include <glib.h>

G_BEGIN_DECLS

typedef struct _Fuzzy      Fuzzy;
typedef struct _FuzzyMatch FuzzyMatch;

struct _FuzzyMatch
{
   const gchar *key;
   gpointer     value;
   gfloat       score;
   guint        id;
};

Fuzzy     *fuzzy_new                (gboolean        case_sensitive);
Fuzzy     *fuzzy_new_with_free_func (gboolean        case_sensitive,
                                     GDestroyNotify  free_func);
void       fuzzy_set_free_func      (Fuzzy          *fuzzy,
                                     GDestroyNotify  free_func);
void       fuzzy_begin_bulk_insert  (Fuzzy          *fuzzy);
void       fuzzy_end_bulk_insert    (Fuzzy          *fuzzy);
gboolean   fuzzy_contains           (Fuzzy          *fuzzy,
                                     const gchar    *key);
void       fuzzy_insert             (Fuzzy          *fuzzy,
                                     const gchar    *key,
                                     gpointer        value);
GArray    *fuzzy_match              (Fuzzy          *fuzzy,
                                     const gchar    *needle,
                                     gsize           max_matches);
void       fuzzy_remove             (Fuzzy          *fuzzy,
                                     const gchar    *key);
Fuzzy     *fuzzy_ref                (Fuzzy          *fuzzy);
void       fuzzy_unref              (Fuzzy          *fuzzy);
gchar     *fuzzy_highlight          (Fuzzy          *fuzzy,
                                     const gchar    *str,
                                     const gchar    *query);

G_END_DECLS

#endif /* FUZZY_H */
