/* ide-search-result.h
 *
 * Copyright 2017-2021 Christian Hergert <chergert@redhat.com>
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

#include <gtk/gtk.h>

#include <libide-core.h>

#include "ide-search-preview.h"

G_BEGIN_DECLS

#define IDE_TYPE_SEARCH_RESULT (ide_search_result_get_type())

IDE_AVAILABLE_IN_ALL
G_DECLARE_DERIVABLE_TYPE (IdeSearchResult, ide_search_result, IDE, SEARCH_RESULT, GObject)

struct _IdeSearchResultClass
{
  GObjectClass parent_class;

  gboolean          (*matches)      (IdeSearchResult *self,
                                     const char      *query);
  void              (*activate)     (IdeSearchResult *self,
                                     GtkWidget       *last_focus);
  IdeSearchPreview *(*load_preview) (IdeSearchResult *self,
                                     IdeContext      *context);
};

IDE_AVAILABLE_IN_ALL
IdeSearchResult  *ide_search_result_new               (void);
IDE_AVAILABLE_IN_ALL
void              ide_search_result_activate          (IdeSearchResult *self,
                                                       GtkWidget       *last_focus);
IDE_AVAILABLE_IN_ALL
int               ide_search_result_compare           (gconstpointer    a,
                                                       gconstpointer    b);
IDE_AVAILABLE_IN_ALL
int               ide_search_result_get_priority      (IdeSearchResult *self);
IDE_AVAILABLE_IN_ALL
void              ide_search_result_set_priority      (IdeSearchResult *self,
                                                       int              priority);
IDE_AVAILABLE_IN_ALL
float             ide_search_result_get_score         (IdeSearchResult *self);
IDE_AVAILABLE_IN_ALL
void              ide_search_result_set_score         (IdeSearchResult *self,
                                                       gfloat           score);
IDE_AVAILABLE_IN_ALL
GdkPaintable     *ide_search_result_get_paintable     (IdeSearchResult *self);
IDE_AVAILABLE_IN_ALL
void              ide_search_result_set_paintable     (IdeSearchResult *self,
                                                       GdkPaintable    *paintable);
IDE_AVAILABLE_IN_ALL
GIcon            *ide_search_result_get_gicon         (IdeSearchResult *self);
IDE_AVAILABLE_IN_ALL
void              ide_search_result_set_gicon         (IdeSearchResult *self,
                                                       GIcon           *gicon);
IDE_AVAILABLE_IN_ALL
const char       *ide_search_result_get_title         (IdeSearchResult *self);
IDE_AVAILABLE_IN_ALL
void              ide_search_result_set_title         (IdeSearchResult *self,
                                                       const char      *title);
IDE_AVAILABLE_IN_ALL
const char       *ide_search_result_get_subtitle      (IdeSearchResult *self);
IDE_AVAILABLE_IN_ALL
void              ide_search_result_set_subtitle      (IdeSearchResult *self,
                                                       const char      *subtitle);
IDE_AVAILABLE_IN_ALL
gboolean          ide_search_result_get_use_markup    (IdeSearchResult *self);
IDE_AVAILABLE_IN_ALL
void              ide_search_result_set_use_markup    (IdeSearchResult *self,
                                                       gboolean         use_markup);
IDE_AVAILABLE_IN_ALL
gboolean          ide_search_result_get_use_underline (IdeSearchResult *self);
IDE_AVAILABLE_IN_ALL
void              ide_search_result_set_use_underline (IdeSearchResult *self,
                                                       gboolean         use_underline);
IDE_AVAILABLE_IN_44
const char       *ide_search_result_get_accelerator   (IdeSearchResult *self);
IDE_AVAILABLE_IN_44
void              ide_search_result_set_accelerator   (IdeSearchResult *self,
                                                       const char      *accelerator);
IDE_AVAILABLE_IN_44
IdeSearchPreview *ide_search_result_load_preview      (IdeSearchResult *self,
                                                       IdeContext      *context);

G_END_DECLS
