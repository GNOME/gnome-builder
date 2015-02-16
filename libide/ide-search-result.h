/* ide-search-result.h
 *
 * Copyright (C) 2015 Christian Hergert <christian@hergert.me>
 *
 * This file is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as
 * published by the Free Software Foundation; either version 3 of the
 * License, or (at your option) any later version.
 *
 * This file is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef IDE_SEARCH_RESULT_H
#define IDE_SEARCH_RESULT_H

#include "ide-object.h"

G_BEGIN_DECLS

#define IDE_TYPE_SEARCH_RESULT (ide_search_result_get_type())

G_DECLARE_DERIVABLE_TYPE (IdeSearchResult, ide_search_result, IDE, SEARCH_RESULT, IdeObject)

struct _IdeSearchResultClass
{
  IdeObjectClass parent;

  void (*activate) (IdeSearchResult *result);
};

IdeSearchResult *ide_search_result_new          (IdeContext            *context,
                                                 const gchar           *title,
                                                 const gchar           *subtitle,
                                                 gfloat                 score);
gfloat           ide_search_result_get_score    (IdeSearchResult       *result);
const gchar     *ide_search_result_get_title    (IdeSearchResult       *result);
const gchar     *ide_search_result_get_subtitle (IdeSearchResult       *result);
gint             ide_search_result_compare      (const IdeSearchResult *a,
                                                 const IdeSearchResult *b);
void             ide_search_result_activate     (IdeSearchResult       *result);

G_END_DECLS

#endif /* IDE_SEARCH_RESULT_H */
