/* ide-source-view.h
 *
 * Copyright 2022 Christian Hergert <chergert@redhat.com>
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

#if !defined (IDE_SOURCEVIEW_INSIDE) && !defined (IDE_SOURCEVIEW_COMPILATION)
# error "Only <libide-sourceview.h> can be included directly."
#endif

#include <libide-core.h>

#include <gtksourceview/gtksource.h>

G_BEGIN_DECLS

#define IDE_TYPE_SOURCE_VIEW (ide_source_view_get_type())

IDE_AVAILABLE_IN_ALL
G_DECLARE_FINAL_TYPE (IdeSourceView, ide_source_view, IDE, SOURCE_VIEW, GtkSourceView)

IDE_AVAILABLE_IN_ALL
GtkWidget                  *ide_source_view_new                         (void);
IDE_AVAILABLE_IN_ALL
void                        ide_source_view_add_controller              (IdeSourceView              *self,
                                                                         int                         priority,
                                                                         GtkEventController         *controller);
IDE_AVAILABLE_IN_ALL
void                        ide_source_view_remove_controller           (IdeSourceView              *self,
                                                                         GtkEventController         *controller);
IDE_AVAILABLE_IN_44
void                        ide_source_view_jump_to_insert              (IdeSourceView              *self);
IDE_AVAILABLE_IN_44
void                        ide_source_view_scroll_to_insert            (IdeSourceView              *self,
                                                                         GtkDirectionType            dir);
IDE_AVAILABLE_IN_ALL
char                       *ide_source_view_dup_position_label          (IdeSourceView              *self);
IDE_AVAILABLE_IN_ALL
void                        ide_source_view_get_visual_position         (IdeSourceView              *self,
                                                                         guint                      *line,
                                                                         guint                      *line_column);
IDE_AVAILABLE_IN_44
void                        ide_source_view_get_visual_position_range   (IdeSourceView              *self,
                                                                         guint                      *line,
                                                                         guint                      *line_column,
                                                                         guint                      *range);
IDE_AVAILABLE_IN_ALL
gboolean                    ide_source_view_get_highlight_current_line  (IdeSourceView              *self);
IDE_AVAILABLE_IN_ALL
void                        ide_source_view_set_highlight_current_line  (IdeSourceView              *self,
                                                                         gboolean                    highlight_current_line);
IDE_AVAILABLE_IN_ALL
double                      ide_source_view_get_zoom_level              (IdeSourceView              *self);
IDE_AVAILABLE_IN_ALL
void                        ide_source_view_set_font_desc               (IdeSourceView              *self,
                                                                         const PangoFontDescription *font_desc);
IDE_AVAILABLE_IN_ALL
const PangoFontDescription *ide_source_view_get_font_desc               (IdeSourceView              *self);
IDE_AVAILABLE_IN_ALL
void                        ide_source_view_prepend_menu                (IdeSourceView              *self,
                                                                         GMenuModel                 *menu_model);
IDE_AVAILABLE_IN_ALL
void                        ide_source_view_append_menu                 (IdeSourceView              *self,
                                                                         GMenuModel                 *menu_model);
IDE_AVAILABLE_IN_ALL
void                        ide_source_view_remove_menu                 (IdeSourceView              *self,
                                                                         GMenuModel                 *menu_model);
IDE_AVAILABLE_IN_ALL
void                        ide_source_view_jump_to_iter                (GtkTextView                *text_view,
                                                                         const GtkTextIter          *iter,
                                                                         double                      within_margin,
                                                                         gboolean                    use_align,
                                                                         double                      xalign,
                                                                         double                      yalign);
IDE_AVAILABLE_IN_ALL
void                        ide_source_view_get_iter_at_visual_position (IdeSourceView              *self,
                                                                         GtkTextIter                *iter,
                                                                         guint                       line,
                                                                         guint                       line_offset);
IDE_AVAILABLE_IN_ALL
gboolean                    ide_source_view_get_insert_matching_brace   (IdeSourceView              *self);
IDE_AVAILABLE_IN_ALL
void                        ide_source_view_set_insert_matching_brace   (IdeSourceView              *self,
                                                                         gboolean                    insert_matching_brace);
IDE_AVAILABLE_IN_ALL
gboolean                    ide_source_view_get_overwrite_braces        (IdeSourceView              *self);
IDE_AVAILABLE_IN_ALL
void                        ide_source_view_set_overwrite_braces        (IdeSourceView              *self,
                                                                         gboolean                    overwrite_braces);

IDE_AVAILABLE_IN_44
gboolean                    ide_source_view_get_enable_search_bubbles   (IdeSourceView              *self);
IDE_AVAILABLE_IN_44
void                        ide_source_view_set_enable_search_bubbles   (IdeSourceView              *self,
                                                                         gboolean                    enable_search_bubbles);

G_END_DECLS
