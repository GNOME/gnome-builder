/* ide-search-preview.h
 *
 * Copyright 2023 Christian Hergert <chergert@redhat.com>
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

#include <gtk/gtk.h>

#include <libide-core.h>

G_BEGIN_DECLS

#define IDE_TYPE_SEARCH_PREVIEW (ide_search_preview_get_type())

IDE_AVAILABLE_IN_44
G_DECLARE_DERIVABLE_TYPE (IdeSearchPreview, ide_search_preview, IDE, SEARCH_PREVIEW, GtkWidget)

struct _IdeSearchPreviewClass
{
  GtkWidgetClass parent_class;
};

IDE_AVAILABLE_IN_44
GtkWidget        *ide_search_preview_new          (void);
IDE_AVAILABLE_IN_44
const char       *ide_search_preview_get_title    (IdeSearchPreview *self);
IDE_AVAILABLE_IN_44
void              ide_search_preview_set_title    (IdeSearchPreview *self,
                                                   const char       *title);
IDE_AVAILABLE_IN_44
const char       *ide_search_preview_get_subtitle (IdeSearchPreview *self);
IDE_AVAILABLE_IN_44
void              ide_search_preview_set_subtitle (IdeSearchPreview *self,
                                                   const char       *title);
IDE_AVAILABLE_IN_44
double            ide_search_preview_get_progress (IdeSearchPreview *self);
IDE_AVAILABLE_IN_44
void              ide_search_preview_set_progress (IdeSearchPreview *self,
                                                   double            progress);
IDE_AVAILABLE_IN_44
GtkWidget        *ide_search_preview_get_child    (IdeSearchPreview *self);
IDE_AVAILABLE_IN_44
void              ide_search_preview_set_child    (IdeSearchPreview *self,
                                                   GtkWidget        *child);

G_END_DECLS
