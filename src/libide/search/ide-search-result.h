/* ide-search-result.h
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

#ifndef IDE_SEARCH_RESULT_H
#define IDE_SEARCH_RESULT_H

#include <gio/gio.h>
#include <dazzle.h>

#include "diagnostics/ide-source-location.h"

G_BEGIN_DECLS

#define IDE_TYPE_SEARCH_RESULT (ide_search_result_get_type())

G_DECLARE_DERIVABLE_TYPE (IdeSearchResult, ide_search_result, IDE, SEARCH_RESULT, DzlSuggestion)

struct _IdeSearchResultClass
{
  DzlSuggestionClass parent_class;

  IdeSourceLocation *(*get_source_location) (IdeSearchResult *self);

  gpointer _reserved1;
  gpointer _reserved2;
  gpointer _reserved3;
  gpointer _reserved4;
  gpointer _reserved5;
  gpointer _reserved6;
  gpointer _reserved7;
  gpointer _reserved8;
};

IdeSearchResult   *ide_search_result_new                 (void);
IdeSourceLocation *ide_search_result_get_source_location (IdeSearchResult       *self);
gint               ide_search_result_compare             (gconstpointer          a,
                                                          gconstpointer          b);
gint               ide_search_result_get_priority        (IdeSearchResult       *self);
void               ide_search_result_set_priority        (IdeSearchResult       *self,
                                                          gint                   priority);
gfloat             ide_search_result_get_score           (IdeSearchResult       *self);
void               ide_search_result_set_score           (IdeSearchResult       *self,
                                                          gfloat                 score);

G_END_DECLS

#endif /* IDE_SEARCH_RESULT_H */
