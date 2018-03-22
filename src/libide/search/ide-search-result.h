/* ide-search-result.h
 *
 * Copyright 2017 Christian Hergert <chergert@redhat.com>
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

#pragma once

#include <gio/gio.h>
#include <dazzle.h>

#include "ide-version-macros.h"

#include "diagnostics/ide-source-location.h"

G_BEGIN_DECLS

#define IDE_TYPE_SEARCH_RESULT (ide_search_result_get_type())

G_DECLARE_DERIVABLE_TYPE (IdeSearchResult, ide_search_result, IDE, SEARCH_RESULT, DzlSuggestion)

struct _IdeSearchResultClass
{
  DzlSuggestionClass parent_class;

  IdeSourceLocation *(*get_source_location) (IdeSearchResult *self);

  /*< private >*/
  gpointer _reserved1;
  gpointer _reserved2;
  gpointer _reserved3;
  gpointer _reserved4;
  gpointer _reserved5;
  gpointer _reserved6;
  gpointer _reserved7;
  gpointer _reserved8;
};

IDE_AVAILABLE_IN_ALL
IdeSearchResult   *ide_search_result_new                 (void);
IDE_AVAILABLE_IN_ALL
IdeSourceLocation *ide_search_result_get_source_location (IdeSearchResult       *self);
IDE_AVAILABLE_IN_ALL
gint               ide_search_result_compare             (gconstpointer          a,
                                                          gconstpointer          b);
IDE_AVAILABLE_IN_ALL
gint               ide_search_result_get_priority        (IdeSearchResult       *self);
IDE_AVAILABLE_IN_ALL
void               ide_search_result_set_priority        (IdeSearchResult       *self,
                                                          gint                   priority);
IDE_AVAILABLE_IN_ALL
gfloat             ide_search_result_get_score           (IdeSearchResult       *self);
IDE_AVAILABLE_IN_ALL
void               ide_search_result_set_score           (IdeSearchResult       *self,
                                                          gfloat                 score);

G_END_DECLS
