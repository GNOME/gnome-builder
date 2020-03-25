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

#include "ide-completion-types.h"
#include "ide-gutter.h"
#include "ide-snippet-types.h"

G_BEGIN_DECLS

#define IDE_TYPE_SOURCE_VIEW (ide_source_view_get_type())

IDE_AVAILABLE_IN_3_32
G_DECLARE_DERIVABLE_TYPE (IdeSourceView, ide_source_view, IDE, SOURCE_VIEW, GtkSourceView)

typedef enum
{
  IDE_CURSOR_COLUMN,
  IDE_CURSOR_SELECT,
  IDE_CURSOR_MATCH
} IdeCursorType;

/**
 * IdeSourceViewModeType:
 * @IDE_SOURCE_VIEW_MODE_TRANSIENT: Transient
 * @IDE_SOURCE_VIEW_MODE_PERMANENT: Permanent
 * @IDE_SOURCE_VIEW_MODE_MODAL: Modal
 *
 * The type of keyboard mode.
 *
 * Since: 3.32
 */
typedef enum
{
  IDE_SOURCE_VIEW_MODE_TYPE_TRANSIENT,
  IDE_SOURCE_VIEW_MODE_TYPE_PERMANENT,
  IDE_SOURCE_VIEW_MODE_TYPE_MODAL
} IdeSourceViewModeType;

/**
 * IdeSourceViewTheatric:
 * @IDE_SOURCE_VIEW_THEATRIC_EXPAND: expand from selection location.
 * @IDE_SOURCE_VIEW_THEATRIC_SHRINK: shrink from selection location.
 *
 * The style of theatric.
 *
 * Since: 3.32
 */

typedef enum
{
  IDE_SOURCE_VIEW_THEATRIC_EXPAND,
  IDE_SOURCE_VIEW_THEATRIC_SHRINK,
} IdeSourceViewTheatric;

/**
 * IdeSourceViewMovement:
 * @IDE_SOURCE_VIEW_MOVEMENT_NEXT_OFFSET: move to next character in the file.
 *   This includes line breaks.
 * @IDE_SOURCE_VIEW_MOVEMENT_PREVIOUS_OFFSET: move to previous character in the file.
 *   This includes line breaks.
 * @IDE_SOURCE_VIEW_MOVEMENT_NTH_CHAR: move to nth character in line. Use a repeat to
 *   specify the target character within the line.
 * @IDE_SOURCE_VIEW_MOVEMENT_PREVIOUS_CHAR: move to previous character in line.
 * @IDE_SOURCE_VIEW_MOVEMENT_NEXT_CHAR: move to next character in line.
 * @IDE_SOURCE_VIEW_MOVEMENT_FIRST_CHAR: move to line offset of zero.
 * @IDE_SOURCE_VIEW_MOVEMENT_FIRST_NONSPACE_CHAR: move to first non-whitespace character in line.
 * @IDE_SOURCE_VIEW_MOVEMENT_MIDDLE_CHAR: move to the middle character in the line.
 * @IDE_SOURCE_VIEW_MOVEMENT_LAST_CHAR: move to the last character in the line. this can be
 *   inclusve or exclusive. inclusive is equivalent to %IDE_SOURCE_VIEW_MOVEMENT_LINE_END.
 * @IDE_SOURCE_VIEW_MOVEMENT_NEXT_SUB_WORD_START: move to the next sub-word start, similar
 *   to the default in GtkTextView. This includes the underline character as a word break,
 *   as is common in Emacs.
 * @IDE_SOURCE_VIEW_MOVEMENT_PREVIOUS_SUB_WORD_START: move to the previous sub-wird start,
 *   similar to the default in GtkTextView. This includes the underline character as a
 *   word break, as is common in Emacs.
 * @IDE_SOURCE_VIEW_MOVEMENT_PREVIOUS_WORD_START: move to beginning of previous word.
 * @IDE_SOURCE_VIEW_MOVEMENT_NEXT_WORD_START: move to beginning of next word.
 * @IDE_SOURCE_VIEW_MOVEMENT_PREVIOUS_WORD_END: move to end of previous word.
 * @IDE_SOURCE_VIEW_MOVEMENT_NEXT_WORD_END: move to end of next word.
 * @IDE_SOURCE_VIEW_MOVEMENT_SENTENCE_START: move to beginning of sentance.
 * @IDE_SOURCE_VIEW_MOVEMENT_SENTENCE_END: move to end of sentance.
 * @IDE_SOURCE_VIEW_MOVEMENT_PARAGRAPH_START: move to start of paragraph.
 * @IDE_SOURCE_VIEW_MOVEMENT_PARAGRAPH_END: move to end of paragraph.
 * @IDE_SOURCE_VIEW_MOVEMENT_PREVIOUS_LINE: move to previous line, keeping line offset if possible.
 * @IDE_SOURCE_VIEW_MOVEMENT_NEXT_LINE: move to next line, keeping line offset if possible.
 * @IDE_SOURCE_VIEW_MOVEMENT_FIRST_LINE: move to first line in file, line offset of zero.
 * @IDE_SOURCE_VIEW_MOVEMENT_NTH_LINE: move to nth line, line offset of zero. use repeat to
 *   select the given line number.
 * @IDE_SOURCE_VIEW_MOVEMENT_LAST_LINE: move to last line in file, with line offset of zero.
 * @IDE_SOURCE_VIEW_MOVEMENT_LINE_PERCENTAGE: move to line based on percentage. Use repeat to
 *   specify the percentage, 0 to 100.
 * @IDE_SOURCE_VIEW_MOVEMENT_LINE_CHARS: special selection to select all line characters up to the
 *   cursor position. special care will be taken if the line is blank to select only the blank
 *   space if any. otherwise, the line break will be selected.
 * @IDE_SOURCE_VIEW_MOVEMENT_LINE_END: This will move you to the location of the newline at the
 *   end of the current line. It does not support exclusive will not select the newline, while
 *   inclusive will select the newline.
 * @IDE_SOURCE_VIEW_MOVEMENT_HALF_PAGE_UP: move half a page up.
 * @IDE_SOURCE_VIEW_MOVEMENT_HALF_PAGE_DOWN: move half a page down.
 * @IDE_SOURCE_VIEW_MOVEMENT_HALF_PAGE_LEFT: move half a page left.
 * @IDE_SOURCE_VIEW_MOVEMENT_HALF_PAGE_RIGHT: move half a page right.
 * @IDE_SOURCE_VIEW_MOVEMENT_PAGE_UP: move a full page up.
 * @IDE_SOURCE_VIEW_MOVEMENT_PAGE_UP_LINES: move a full page up, but extend to whole line.
 * @IDE_SOURCE_VIEW_MOVEMENT_PAGE_DOWN: move a full page down.
 * @IDE_SOURCE_VIEW_MOVEMENT_PAGE_DOWN_LINES: move a full page down, but extend to whole line.
 * @IDE_SOURCE_VIEW_MOVEMENT_SCREEN_UP: move to viewport up by visible line, adjusting cursor
 *   to stay on screen if necessary.
 * @IDE_SOURCE_VIEW_MOVEMENT_SCREEN_DOWN: move to viewport down by visible line, adjusting cursor
 *   to stay on screen if necessary.
 * @IDE_SOURCE_VIEW_MOVEMENT_SCREEN_LEFT: move to viewport left by visible char, adjusting cursor
 *   to stay on screen if necessary.
 * @IDE_SOURCE_VIEW_MOVEMENT_SCREEN_RIGHT: move to viewport right by visible char, adjusting cursor
 *   to stay on screen if necessary.
 * @IDE_SOURCE_VIEW_MOVEMENT_SCREEN_TOP: move to the top of the screen.
 * @IDE_SOURCE_VIEW_MOVEMENT_SCREEN_MIDDLE: move to the middle of the screen.
 * @IDE_SOURCE_VIEW_MOVEMENT_SCREEN_BOTTOM: move to the bottom of the screen.
 * @IDE_SOURCE_VIEW_MOVEMENT_MATCH_SPECIAL: move to match of brace, bracket, comment.
 * @IDE_SOURCE_VIEW_MOVEMENT_SCROLL_SCREEN_TOP: scroll until insert cursor or [count]th line is at screen top.
 * @IDE_SOURCE_VIEW_MOVEMENT_SCROLL_SCREEN_CENTER: scroll until insert cursor or [count]th line is at screen center.
 * @IDE_SOURCE_VIEW_MOVEMENT_SCROLL_SCREEN_BOTTOM: scroll until insert cursor or [count]th line is at screen bottom.
 * @IDE_SOURCE_VIEW_MOVEMENT_SCROLL_SCREEN_LEFT: scroll until insert cursor or [count]th char is at screen left.
 * @IDE_SOURCE_VIEW_MOVEMENT_SCROLL_SCREEN_RIGHT: scroll until insert cursor or [count]th char is at screen right.
 * @IDE_SOURCE_VIEW_MOVEMENT_NEXT_MATCH_SEARCH_CHAR: move to the next matching char according to f and t in vim.
 * @IDE_SOURCE_VIEW_MOVEMENT_PREVIOUS_MATCH_SEARCH_CHAR: move to the previous matching char according to F and T in vim.
 * @IDE_SOURCE_VIEW_MOVEMENT_SMART_HOME: Moves to the first non-whitespace character unless
 *   already positioned there. Otherwise, it moves to the first character.
 *
 * The type of movement.
 *
 * Some of these movements may be modified by using the modify-repeat action.
 * First adjust the repeat and then perform the "movement" action.
 *
 * Since: 3.32
 */
typedef enum
{
  IDE_SOURCE_VIEW_MOVEMENT_PREVIOUS_OFFSET,
  IDE_SOURCE_VIEW_MOVEMENT_NEXT_OFFSET,

  IDE_SOURCE_VIEW_MOVEMENT_NTH_CHAR,
  IDE_SOURCE_VIEW_MOVEMENT_PREVIOUS_CHAR,
  IDE_SOURCE_VIEW_MOVEMENT_NEXT_CHAR,
  IDE_SOURCE_VIEW_MOVEMENT_FIRST_CHAR,
  IDE_SOURCE_VIEW_MOVEMENT_FIRST_NONSPACE_CHAR,
  IDE_SOURCE_VIEW_MOVEMENT_MIDDLE_CHAR,
  IDE_SOURCE_VIEW_MOVEMENT_LAST_CHAR,

  IDE_SOURCE_VIEW_MOVEMENT_NEXT_WORD_START,
  IDE_SOURCE_VIEW_MOVEMENT_NEXT_FULL_WORD_START,
  IDE_SOURCE_VIEW_MOVEMENT_NEXT_SUB_WORD_START,
  IDE_SOURCE_VIEW_MOVEMENT_PREVIOUS_SUB_WORD_START,
  IDE_SOURCE_VIEW_MOVEMENT_PREVIOUS_WORD_START,
  IDE_SOURCE_VIEW_MOVEMENT_PREVIOUS_FULL_WORD_START,

  IDE_SOURCE_VIEW_MOVEMENT_PREVIOUS_WORD_END,
  IDE_SOURCE_VIEW_MOVEMENT_PREVIOUS_FULL_WORD_END,
  IDE_SOURCE_VIEW_MOVEMENT_NEXT_WORD_END,
  IDE_SOURCE_VIEW_MOVEMENT_NEXT_FULL_WORD_END,

  IDE_SOURCE_VIEW_MOVEMENT_NEXT_WORD_START_NEWLINE_STOP,
  IDE_SOURCE_VIEW_MOVEMENT_NEXT_FULL_WORD_START_NEWLINE_STOP,
  IDE_SOURCE_VIEW_MOVEMENT_PREVIOUS_WORD_START_NEWLINE_STOP,
  IDE_SOURCE_VIEW_MOVEMENT_PREVIOUS_FULL_WORD_START_NEWLINE_STOP,

  IDE_SOURCE_VIEW_MOVEMENT_PREVIOUS_WORD_END_NEWLINE_STOP,
  IDE_SOURCE_VIEW_MOVEMENT_PREVIOUS_FULL_WORD_END_NEWLINE_STOP,
  IDE_SOURCE_VIEW_MOVEMENT_NEXT_WORD_END_NEWLINE_STOP,
  IDE_SOURCE_VIEW_MOVEMENT_NEXT_FULL_WORD_END_NEWLINE_STOP,

  IDE_SOURCE_VIEW_MOVEMENT_SENTENCE_START,
  IDE_SOURCE_VIEW_MOVEMENT_SENTENCE_END,

  IDE_SOURCE_VIEW_MOVEMENT_PARAGRAPH_START,
  IDE_SOURCE_VIEW_MOVEMENT_PARAGRAPH_END,

  IDE_SOURCE_VIEW_MOVEMENT_PREVIOUS_LINE,
  IDE_SOURCE_VIEW_MOVEMENT_NEXT_LINE,

  IDE_SOURCE_VIEW_MOVEMENT_FIRST_LINE,
  IDE_SOURCE_VIEW_MOVEMENT_NTH_LINE,
  IDE_SOURCE_VIEW_MOVEMENT_LAST_LINE,
  IDE_SOURCE_VIEW_MOVEMENT_LINE_PERCENTAGE,

  IDE_SOURCE_VIEW_MOVEMENT_LINE_CHARS,
  IDE_SOURCE_VIEW_MOVEMENT_LINE_END,

  IDE_SOURCE_VIEW_MOVEMENT_HALF_PAGE_UP,
  IDE_SOURCE_VIEW_MOVEMENT_HALF_PAGE_DOWN,
  IDE_SOURCE_VIEW_MOVEMENT_HALF_PAGE_LEFT,
  IDE_SOURCE_VIEW_MOVEMENT_HALF_PAGE_RIGHT,

  IDE_SOURCE_VIEW_MOVEMENT_PAGE_UP,
  IDE_SOURCE_VIEW_MOVEMENT_PAGE_UP_LINES,
  IDE_SOURCE_VIEW_MOVEMENT_PAGE_DOWN,
  IDE_SOURCE_VIEW_MOVEMENT_PAGE_DOWN_LINES,

  IDE_SOURCE_VIEW_MOVEMENT_SCREEN_UP,
  IDE_SOURCE_VIEW_MOVEMENT_SCREEN_DOWN,
  IDE_SOURCE_VIEW_MOVEMENT_SCREEN_LEFT,
  IDE_SOURCE_VIEW_MOVEMENT_SCREEN_RIGHT,
  IDE_SOURCE_VIEW_MOVEMENT_SCREEN_TOP,
  IDE_SOURCE_VIEW_MOVEMENT_SCREEN_MIDDLE,
  IDE_SOURCE_VIEW_MOVEMENT_SCREEN_BOTTOM,

  IDE_SOURCE_VIEW_MOVEMENT_MATCH_SPECIAL,

  IDE_SOURCE_VIEW_MOVEMENT_SCROLL_SCREEN_TOP,
  IDE_SOURCE_VIEW_MOVEMENT_SCROLL_SCREEN_CENTER,
  IDE_SOURCE_VIEW_MOVEMENT_SCROLL_SCREEN_BOTTOM,
  IDE_SOURCE_VIEW_MOVEMENT_SCROLL_SCREEN_LEFT,
  IDE_SOURCE_VIEW_MOVEMENT_SCROLL_SCREEN_RIGHT,

  IDE_SOURCE_VIEW_MOVEMENT_PREVIOUS_UNMATCHED_BRACE,
  IDE_SOURCE_VIEW_MOVEMENT_NEXT_UNMATCHED_BRACE,

  IDE_SOURCE_VIEW_MOVEMENT_PREVIOUS_UNMATCHED_PAREN,
  IDE_SOURCE_VIEW_MOVEMENT_NEXT_UNMATCHED_PAREN,

  IDE_SOURCE_VIEW_MOVEMENT_NEXT_MATCH_MODIFIER,
  IDE_SOURCE_VIEW_MOVEMENT_PREVIOUS_MATCH_MODIFIER,

  IDE_SOURCE_VIEW_MOVEMENT_NEXT_MATCH_SEARCH_CHAR,
  IDE_SOURCE_VIEW_MOVEMENT_PREVIOUS_MATCH_SEARCH_CHAR,

  IDE_SOURCE_VIEW_MOVEMENT_SMART_HOME,
} IdeSourceViewMovement;

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

  void (*append_to_count)             (IdeSourceView           *self,
                                       gint                     digit);
  void (*auto_indent)                 (IdeSourceView           *self);
  void (*begin_macro)                 (IdeSourceView           *self);
  void (*capture_modifier)            (IdeSourceView           *self);
  void (*clear_count)                 (IdeSourceView           *self);
  void (*clear_modifier)              (IdeSourceView           *self);
  void (*clear_search)                (IdeSourceView           *self);
  void (*clear_selection)             (IdeSourceView           *self);
  void (*clear_snippets)              (IdeSourceView           *self);
  void (*cycle_completion)            (IdeSourceView           *self,
                                       GtkDirectionType         direction);
  void (*delete_selection)            (IdeSourceView           *self);
  void (*end_macro)                   (IdeSourceView           *self);
  void (*focus_location)              (IdeSourceView           *self,
                                       IdeLocation       *location);
  void (*goto_definition)             (IdeSourceView           *self);
  void (*hide_completion)             (IdeSourceView           *self);
  void (*indent_selection)            (IdeSourceView           *self,
                                       gint                     level);
  void (*insert_at_cursor_and_indent) (IdeSourceView           *self,
                                       const gchar             *str);
  void (*insert_modifier)             (IdeSourceView           *self,
                                       gboolean                 use_count);
  void (*jump)                        (IdeSourceView           *self,
                                       const GtkTextIter       *from,
                                       const GtkTextIter       *to);
  void (*movement)                    (IdeSourceView           *self,
                                       IdeSourceViewMovement    movement,
                                       gboolean                 extend_selection,
                                       gboolean                 exclusive,
                                       gboolean                 apply_count);
  void (*move_error)                  (IdeSourceView           *self,
                                       GtkDirectionType         dir);
  void (*move_search)                 (IdeSourceView           *self,
                                       GtkDirectionType         dir,
                                       gboolean                 extend_selection,
                                       gboolean                 select_match,
                                       gboolean                 exclusive,
                                       gboolean                 apply_count,
                                       gboolean                 at_word_boundaries);
  void (*paste_clipboard_extended)    (IdeSourceView           *self,
                                       gboolean                 smart_lines,
                                       gboolean                 after_cursor,
                                       gboolean                 place_cursor_at_original);
  void (*push_selection)              (IdeSourceView           *self);
  void (*pop_selection)               (IdeSourceView           *self);
  void (*rebuild_highlight)           (IdeSourceView           *self);
  void (*replay_macro)                (IdeSourceView           *self,
                                       gboolean                 use_count);
  void (*request_documentation)       (IdeSourceView           *self);
  void (*restore_insert_mark)         (IdeSourceView           *self);
  void (*save_command)                (IdeSourceView           *self);
  void (*save_search_char)            (IdeSourceView           *self);
  void (*save_insert_mark)            (IdeSourceView           *self);
  void (*select_inner)                (IdeSourceView           *self,
                                       const gchar             *inner_left,
                                       const gchar             *inner_right,
                                       gboolean                 exclusive,
                                       gboolean                 string_mode);
  void (*select_tag)                  (IdeSourceView           *self,
                                       gboolean                 exclusive);
  void (*selection_theatric)          (IdeSourceView           *self,
                                       IdeSourceViewTheatric    theatric);
  void (*set_mode)                    (IdeSourceView           *self,
                                       const gchar             *mode,
                                       IdeSourceViewModeType    type);
  void (*set_overwrite)               (IdeSourceView           *self,
                                       gboolean                 overwrite);
  void (*set_search_text)             (IdeSourceView           *self,
                                       const gchar             *search_text,
                                       gboolean                 from_selection);
  void (*sort)                        (IdeSourceView           *self,
                                       gboolean                 ignore_case,
                                       gboolean                 reverse);
  void (*swap_selection_bounds)       (IdeSourceView           *self);
  void (*increase_font_size)          (IdeSourceView           *self);
  void (*decrease_font_size)          (IdeSourceView           *self);
  void (*reset_font_size)             (IdeSourceView           *self);
  void (*begin_rename)                (IdeSourceView           *self);
  void (*add_cursor)                  (IdeSourceView           *self,
                                       guint                    type);
  void (*remove_cursors)              (IdeSourceView           *self);
  void (*copy_clipboard_extended)     (IdeSourceView           *self);

  /*< private >*/
  gpointer _reserved[32];
};

IDE_AVAILABLE_IN_3_32
gboolean                    ide_source_view_has_snippet                    (IdeSourceView              *self);
IDE_AVAILABLE_IN_3_32
void                        ide_source_view_clear_snippets                 (IdeSourceView              *self);
IDE_AVAILABLE_IN_3_32
IdeSnippet                 *ide_source_view_get_current_snippet            (IdeSourceView              *self);
IDE_AVAILABLE_IN_3_32
guint                       ide_source_view_get_visual_column              (IdeSourceView              *self,
                                                                            const GtkTextIter          *location);
IDE_AVAILABLE_IN_3_32
void                        ide_source_view_get_visual_position            (IdeSourceView              *self,
                                                                            guint                      *line,
                                                                            guint                      *line_column);
IDE_AVAILABLE_IN_3_32
gint                        ide_source_view_get_count                      (IdeSourceView              *self);
IDE_AVAILABLE_IN_3_32
IdeFileSettings            *ide_source_view_get_file_settings              (IdeSourceView              *self);
IDE_AVAILABLE_IN_3_32
const PangoFontDescription *ide_source_view_get_font_desc                  (IdeSourceView              *self);
IDE_AVAILABLE_IN_3_32
PangoFontDescription       *ide_source_view_get_scaled_font_desc           (IdeSourceView              *self);
IDE_AVAILABLE_IN_3_32
gboolean                    ide_source_view_get_highlight_current_line     (IdeSourceView              *self);
IDE_AVAILABLE_IN_3_32
gboolean                    ide_source_view_get_insert_matching_brace      (IdeSourceView              *self);
IDE_AVAILABLE_IN_3_32
void                        ide_source_view_get_iter_at_visual_column      (IdeSourceView              *self,
                                                                            guint                       column,
                                                                            GtkTextIter                *location);
IDE_AVAILABLE_IN_3_32
const gchar                *ide_source_view_get_mode_display_name          (IdeSourceView              *self);
IDE_AVAILABLE_IN_3_32
const gchar                *ide_source_view_get_mode_name                  (IdeSourceView              *self);
IDE_AVAILABLE_IN_3_32
gboolean                    ide_source_view_get_overwrite_braces           (IdeSourceView              *self);
IDE_AVAILABLE_IN_3_32
gboolean                    ide_source_view_get_overwrite                  (IdeSourceView              *self);
IDE_AVAILABLE_IN_3_32
guint                       ide_source_view_get_scroll_offset              (IdeSourceView              *self);
IDE_AVAILABLE_IN_3_32
gboolean                    ide_source_view_get_show_grid_lines            (IdeSourceView              *self);
IDE_AVAILABLE_IN_3_32
gboolean                    ide_source_view_get_show_line_changes          (IdeSourceView              *self);
IDE_AVAILABLE_IN_3_32
gboolean                    ide_source_view_get_show_line_diagnostics      (IdeSourceView              *self);
IDE_AVAILABLE_IN_3_32
gboolean                    ide_source_view_get_show_line_numbers          (IdeSourceView              *self);
IDE_AVAILABLE_IN_3_36
gboolean                    ide_source_view_get_show_relative_line_numbers (IdeSourceView              *self);
IDE_AVAILABLE_IN_3_32
gboolean                    ide_source_view_get_snippet_completion         (IdeSourceView              *self);
IDE_AVAILABLE_IN_3_32
gboolean                    ide_source_view_get_spell_checking             (IdeSourceView              *self);
IDE_AVAILABLE_IN_3_32
void                        ide_source_view_get_visible_rect               (IdeSourceView              *self,
                                                                            GdkRectangle               *visible_rect);
IDE_AVAILABLE_IN_3_32
void                        ide_source_view_jump                           (IdeSourceView              *self,
                                                                            const GtkTextIter          *from,
                                                                            const GtkTextIter          *to);
IDE_AVAILABLE_IN_3_32
void                        ide_source_view_pop_snippet                    (IdeSourceView              *self);
IDE_AVAILABLE_IN_3_32
void                        ide_source_view_push_snippet                   (IdeSourceView              *self,
                                                                            IdeSnippet                 *snippet,
                                                                            const GtkTextIter          *location);
IDE_AVAILABLE_IN_3_32
void                        ide_source_view_rollback_search                (IdeSourceView              *self);
IDE_AVAILABLE_IN_3_32
void                        ide_source_view_save_search                    (IdeSourceView              *self,
                                                                            const gchar                *search_text);
IDE_AVAILABLE_IN_3_32
void                        ide_source_view_set_count                      (IdeSourceView              *self,
                                                                            gint                        count);
IDE_AVAILABLE_IN_3_32
void                        ide_source_view_set_font_desc                  (IdeSourceView              *self,
                                                                            const PangoFontDescription *font_desc);
IDE_AVAILABLE_IN_3_32
void                        ide_source_view_set_font_name                  (IdeSourceView              *self,
                                                                            const gchar                *font_name);
IDE_AVAILABLE_IN_3_32
void                        ide_source_view_set_highlight_current_line     (IdeSourceView              *self,
                                                                            gboolean                    highlight_current_line);
IDE_AVAILABLE_IN_3_32
void                        ide_source_view_set_insert_matching_brace      (IdeSourceView              *self,
                                                                            gboolean                    insert_matching_brace);
IDE_AVAILABLE_IN_3_32
void                        ide_source_view_set_misspelled_word            (IdeSourceView              *self,
                                                                            GtkTextIter                *start,
                                                                            GtkTextIter                *end);
IDE_AVAILABLE_IN_3_32
void                        ide_source_view_set_overwrite_braces           (IdeSourceView              *self,
                                                                            gboolean                    overwrite_braces);
IDE_AVAILABLE_IN_3_32
void                        ide_source_view_set_scroll_offset              (IdeSourceView              *self,
                                                                            guint                       scroll_offset);
IDE_AVAILABLE_IN_3_32
void                        ide_source_view_set_show_grid_lines            (IdeSourceView              *self,
                                                                            gboolean                    show_grid_lines);
IDE_AVAILABLE_IN_3_32
void                        ide_source_view_set_show_line_changes          (IdeSourceView              *self,
                                                                            gboolean                    show_line_changes);
IDE_AVAILABLE_IN_3_32
void                        ide_source_view_set_show_line_diagnostics      (IdeSourceView              *self,
                                                                            gboolean                    show_line_diagnostics);
IDE_AVAILABLE_IN_3_32
void                        ide_source_view_set_show_line_numbers          (IdeSourceView              *self,
                                                                            gboolean                    show_line_numbers);
IDE_AVAILABLE_IN_3_36
void                        ide_source_view_set_show_relative_line_numbers (IdeSourceView              *self,
                                                                            gboolean                    show_relative_line_numbers);
IDE_AVAILABLE_IN_3_32
void                        ide_source_view_set_snippet_completion         (IdeSourceView              *self,
                                                                            gboolean                    snippet_completion);
IDE_AVAILABLE_IN_3_32
void                        ide_source_view_set_spell_checking             (IdeSourceView              *self,
                                                                            gboolean                    enable);
IDE_AVAILABLE_IN_3_32
gboolean                    ide_source_view_move_mark_onscreen             (IdeSourceView              *self,
                                                                            GtkTextMark                *mark);
IDE_AVAILABLE_IN_3_32
gboolean                    ide_source_view_place_cursor_onscreen          (IdeSourceView              *self);
IDE_AVAILABLE_IN_3_32
void                        ide_source_view_clear_search                   (IdeSourceView              *self);
IDE_AVAILABLE_IN_3_32
void                        ide_source_view_scroll_mark_onscreen           (IdeSourceView              *self,
                                                                            GtkTextMark                *mark,
                                                                            IdeSourceScrollAlign        use_align,
                                                                            gdouble                     alignx,
                                                                            gdouble                     aligny);
IDE_AVAILABLE_IN_3_32
void                        ide_source_view_scroll_to_mark                 (IdeSourceView              *self,
                                                                            GtkTextMark                *mark,
                                                                            gdouble                     within_margin,
                                                                            IdeSourceScrollAlign        use_align,
                                                                            gdouble                     xalign,
                                                                            gdouble                     yalign,
                                                                            gboolean                    animate_scroll);
IDE_AVAILABLE_IN_3_32
void                        ide_source_view_scroll_to_iter                 (IdeSourceView              *self,
                                                                            const GtkTextIter          *iter,
                                                                            gdouble                     within_margin,
                                                                            IdeSourceScrollAlign        use_align,
                                                                            gdouble                     xalign,
                                                                            gdouble                     yalign,
                                                                            gboolean                    animate_scroll);
IDE_AVAILABLE_IN_3_32
void                        ide_source_view_scroll_to_insert               (IdeSourceView              *self);
IDE_AVAILABLE_IN_3_32
IdeCompletion              *ide_source_view_get_completion                 (IdeSourceView              *self);
IDE_AVAILABLE_IN_3_32
gboolean                    ide_source_view_is_processing_key              (IdeSourceView              *self);
IDE_AVAILABLE_IN_3_32
void                        ide_source_view_set_gutter                     (IdeSourceView              *self,
                                                                            IdeGutter                  *gutter);

G_END_DECLS
