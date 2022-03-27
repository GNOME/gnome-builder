/* ide-source-view.h
 *
 * Copyright 2015-2019 Christian Hergert <christian@hergert.me>
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

#include <gtksourceview/gtksource.h>
#include <libide-code.h>

#include "ide-gutter.h"

G_BEGIN_DECLS

#define IDE_TYPE_SOURCE_VIEW (ide_source_view_get_type())

IDE_AVAILABLE_ALL
G_DECLARE_DERIVABLE_TYPE (IdeSourceView, ide_source_view, IDE, SOURCE_VIEW, GtkSourceView)

typedef enum
{
  IDE_CURSOR_COLUMN,
  IDE_CURSOR_SELECT,
  IDE_CURSOR_MATCH
} IdeCursorType;

typedef enum
{
  IDE_SOURCE_SCROLL_NONE = 0,
  IDE_SOURCE_SCROLL_BOTH = 1,
  IDE_SOURCE_SCROLL_X    = 1 << 1,
  IDE_SOURCE_SCROLL_Y    = 1 << 2,
} IdeSourceScrollAlign;

struct _IdeSourceViewClass
{
  GtkSourceViewClass parent_class;

  void (*focus_location)        (IdeSourceView           *self,
                                 IdeLocation             *location);
  void (*goto_definition)       (IdeSourceView           *self);
  void (*move_error)            (IdeSourceView           *self,
                                 GtkDirectionType         dir);
  void (*rebuild_highlight)     (IdeSourceView           *self);
  void (*request_documentation) (IdeSourceView           *self);
  void (*sort)                  (IdeSourceView           *self,
                                 gboolean                 ignore_case,
                                 gboolean                 reverse);
  void (*begin_rename)          (IdeSourceView           *self);
  void (*add_cursor)            (IdeSourceView           *self,
                                 guint                    type);
  void (*remove_cursors)        (IdeSourceView           *self);
};

IDE_AVAILABLE_ALL
guint                       ide_source_view_get_visual_column              (IdeSourceView              *self,
                                                                            const GtkTextIter          *location);
IDE_AVAILABLE_ALL
void                        ide_source_view_get_visual_position            (IdeSourceView              *self,
                                                                            guint                      *line,
                                                                            guint                      *line_column);
IDE_AVAILABLE_ALL
IdeFileSettings            *ide_source_view_get_file_settings              (IdeSourceView              *self);
IDE_AVAILABLE_ALL
const PangoFontDescription *ide_source_view_get_font_desc                  (IdeSourceView              *self);
IDE_AVAILABLE_ALL
PangoFontDescription       *ide_source_view_get_scaled_font_desc           (IdeSourceView              *self);
IDE_AVAILABLE_ALL
gboolean                    ide_source_view_get_highlight_current_line     (IdeSourceView              *self);
IDE_AVAILABLE_ALL
gboolean                    ide_source_view_get_insert_matching_brace      (IdeSourceView              *self);
IDE_AVAILABLE_ALL
void                        ide_source_view_get_iter_at_visual_column      (IdeSourceView              *self,
                                                                            guint                       column,
                                                                            GtkTextIter                *location);
IDE_AVAILABLE_ALL
gboolean                    ide_source_view_get_overwrite_braces           (IdeSourceView              *self);
IDE_AVAILABLE_ALL
gboolean                    ide_source_view_get_overwrite                  (IdeSourceView              *self);
IDE_AVAILABLE_ALL
guint                       ide_source_view_get_scroll_offset              (IdeSourceView              *self);
IDE_AVAILABLE_ALL
gboolean                    ide_source_view_get_show_grid_lines            (IdeSourceView              *self);
IDE_AVAILABLE_ALL
gboolean                    ide_source_view_get_show_line_changes          (IdeSourceView              *self);
IDE_AVAILABLE_ALL
gboolean                    ide_source_view_get_show_line_diagnostics      (IdeSourceView              *self);
IDE_AVAILABLE_ALL
gboolean                    ide_source_view_get_show_line_numbers          (IdeSourceView              *self);
IDE_AVAILABLE_ALL
gboolean                    ide_source_view_get_show_relative_line_numbers (IdeSourceView              *self);
IDE_AVAILABLE_ALL
void                        ide_source_view_get_visible_rect               (IdeSourceView              *self,
                                                                            GdkRectangle               *visible_rect);
IDE_AVAILABLE_ALL
void                        ide_source_view_set_font_desc                  (IdeSourceView              *self,
                                                                            const PangoFontDescription *font_desc);
IDE_AVAILABLE_ALL
void                        ide_source_view_set_font_name                  (IdeSourceView              *self,
                                                                            const gchar                *font_name);
IDE_AVAILABLE_ALL
void                        ide_source_view_set_highlight_current_line     (IdeSourceView              *self,
                                                                            gboolean                    highlight_current_line);
IDE_AVAILABLE_ALL
void                        ide_source_view_set_insert_matching_brace      (IdeSourceView              *self,
                                                                            gboolean                    insert_matching_brace);
IDE_AVAILABLE_ALL
void                        ide_source_view_set_overwrite_braces           (IdeSourceView              *self,
                                                                            gboolean                    overwrite_braces);
IDE_AVAILABLE_ALL
void                        ide_source_view_set_scroll_offset              (IdeSourceView              *self,
                                                                            guint                       scroll_offset);
IDE_AVAILABLE_ALL
void                        ide_source_view_set_show_grid_lines            (IdeSourceView              *self,
                                                                            gboolean                    show_grid_lines);
IDE_AVAILABLE_ALL
void                        ide_source_view_set_show_line_changes          (IdeSourceView              *self,
                                                                            gboolean                    show_line_changes);
IDE_AVAILABLE_ALL
void                        ide_source_view_set_show_line_diagnostics      (IdeSourceView              *self,
                                                                            gboolean                    show_line_diagnostics);
IDE_AVAILABLE_ALL
void                        ide_source_view_set_show_line_numbers          (IdeSourceView              *self,
                                                                            gboolean                    show_line_numbers);
IDE_AVAILABLE_ALL
void                        ide_source_view_set_show_relative_line_numbers (IdeSourceView              *self,
                                                                            gboolean                    show_relative_line_numbers);
IDE_AVAILABLE_ALL
gboolean                    ide_source_view_move_mark_onscreen             (IdeSourceView              *self,
                                                                            GtkTextMark                *mark);
IDE_AVAILABLE_ALL
gboolean                    ide_source_view_place_cursor_onscreen          (IdeSourceView              *self);
IDE_AVAILABLE_ALL
void                        ide_source_view_scroll_mark_onscreen           (IdeSourceView              *self,
                                                                            GtkTextMark                *mark,
                                                                            IdeSourceScrollAlign        use_align,
                                                                            gdouble                     alignx,
                                                                            gdouble                     aligny);
IDE_AVAILABLE_ALL
void                        ide_source_view_scroll_to_mark                 (IdeSourceView              *self,
                                                                            GtkTextMark                *mark,
                                                                            gdouble                     within_margin,
                                                                            IdeSourceScrollAlign        use_align,
                                                                            gdouble                     xalign,
                                                                            gdouble                     yalign,
                                                                            gboolean                    animate_scroll);
IDE_AVAILABLE_ALL
void                        ide_source_view_scroll_to_iter                 (IdeSourceView              *self,
                                                                            const GtkTextIter          *iter,
                                                                            gdouble                     within_margin,
                                                                            IdeSourceScrollAlign        use_align,
                                                                            gdouble                     xalign,
                                                                            gdouble                     yalign,
                                                                            gboolean                    animate_scroll);
IDE_AVAILABLE_ALL
void                        ide_source_view_scroll_to_insert               (IdeSourceView              *self);
IDE_AVAILABLE_ALL
void                        ide_source_view_set_gutter                     (IdeSourceView              *self,
                                                                            IdeGutter                  *gutter);

G_END_DECLS
