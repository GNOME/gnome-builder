/* gb-search-result.h
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

#ifndef GB_SEARCH_RESULT_H
#define GB_SEARCH_RESULT_H

#include <glib-object.h>

#include "gb-search-types.h"

G_BEGIN_DECLS

#define GB_SEARCH_RESULT(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), GB_TYPE_SEARCH_RESULT, GbSearchResult))
#define GB_SEARCH_RESULT_CONST(obj)      (G_TYPE_CHECK_INSTANCE_CAST ((obj), GB_TYPE_SEARCH_RESULT, GbSearchResult const))
#define GB_SEARCH_RESULT_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass),  GB_TYPE_SEARCH_RESULT, GbSearchResultClass))
#define GB_IS_SEARCH_RESULT(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GB_TYPE_SEARCH_RESULT))
#define GB_IS_SEARCH_RESULT_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass),  GB_TYPE_SEARCH_RESULT))
#define GB_SEARCH_RESULT_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj),  GB_TYPE_SEARCH_RESULT, GbSearchResultClass))

struct _GbSearchResult
{
  GObject parent;

  /*< private >*/
  GbSearchResultPrivate *priv;
};

struct _GbSearchResultClass
{
  GObjectClass parent;

  void (*activate) (GbSearchResult *result);
};

GbSearchResult *gb_search_result_new          (const gchar          *title,
                                               const gchar          *subtitle,
                                               gfloat                score);
gfloat          gb_search_result_get_score    (GbSearchResult       *result);
const gchar    *gb_search_result_get_title    (GbSearchResult       *result);
const gchar    *gb_search_result_get_subtitle (GbSearchResult       *result);
gint            gb_search_result_compare      (const GbSearchResult *a,
                                               const GbSearchResult *b);
void            gb_search_result_activate     (GbSearchResult       *result);

G_END_DECLS

#endif /* GB_SEARCH_RESULT_H */
