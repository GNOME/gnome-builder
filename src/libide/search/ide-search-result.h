/* ide-search-result.h
 *
 * Copyright 2017-2019 Christian Hergert <chergert@redhat.com>
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

#pragma once

#if !defined (IDE_SEARCH_INSIDE) && !defined (IDE_SEARCH_COMPILATION)
# error "Only <libide-search.h> can be included directly."
#endif

#include <libide-core.h>
#include <dazzle.h>

G_BEGIN_DECLS

#define IDE_TYPE_SEARCH_RESULT (ide_search_result_get_type())

IDE_AVAILABLE_IN_3_32
G_DECLARE_DERIVABLE_TYPE (IdeSearchResult, ide_search_result, IDE, SEARCH_RESULT, DzlSuggestion)

struct _IdeSearchResultClass
{
  DzlSuggestionClass parent_class;

  void (*activate) (IdeSearchResult *self,
                    GtkWidget       *last_focus);

  /*< private >*/
  gpointer _reserved[8];
};

IDE_AVAILABLE_IN_3_32
IdeSearchResult *ide_search_result_new          (void);
IDE_AVAILABLE_IN_3_32
void             ide_search_result_activate     (IdeSearchResult *self,
                                                 GtkWidget       *last_focus);
IDE_AVAILABLE_IN_3_32
gint             ide_search_result_compare      (gconstpointer    a,
                                                 gconstpointer    b);
IDE_AVAILABLE_IN_3_32
gint             ide_search_result_get_priority (IdeSearchResult *self);
IDE_AVAILABLE_IN_3_32
void             ide_search_result_set_priority (IdeSearchResult *self,
                                                 gint             priority);
IDE_AVAILABLE_IN_3_32
gfloat           ide_search_result_get_score    (IdeSearchResult *self);
IDE_AVAILABLE_IN_3_32
void             ide_search_result_set_score    (IdeSearchResult *self,
                                                 gfloat           score);
IDE_AVAILABLE_IN_3_32
void             ide_search_result_set_icon     (IdeSearchResult *self,
                                                 GIcon           *icon);

G_END_DECLS
