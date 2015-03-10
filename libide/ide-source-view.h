/* ide-source-view.h
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

#ifndef IDE_SOURCE_VIEW_H
#define IDE_SOURCE_VIEW_H

#include <gtksourceview/gtksource.h>

#include "ide-types.h"

G_BEGIN_DECLS

#define IDE_TYPE_SOURCE_VIEW            (ide_source_view_get_type())
#define IDE_SOURCE_VIEW(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), IDE_TYPE_SOURCE_VIEW, IdeSourceView))
#define IDE_SOURCE_VIEW_CONST(obj)      (G_TYPE_CHECK_INSTANCE_CAST ((obj), IDE_TYPE_SOURCE_VIEW, IdeSourceView const))
#define IDE_SOURCE_VIEW_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass),  IDE_TYPE_SOURCE_VIEW, IdeSourceViewClass))
#define IDE_IS_SOURCE_VIEW(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), IDE_TYPE_SOURCE_VIEW))
#define IDE_IS_SOURCE_VIEW_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass),  IDE_TYPE_SOURCE_VIEW))
#define IDE_SOURCE_VIEW_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj),  IDE_TYPE_SOURCE_VIEW, IdeSourceViewClass))

typedef struct _IdeSourceView      IdeSourceView;
typedef struct _IdeSourceViewClass IdeSourceViewClass;

/**
 * IdeSourceViewModeType:
 * @IDE_SOURCE_VIEW_MODE_TRANSIENT: Transient
 * @IDE_SOURCE_VIEW_MODE_PERMANENT: Permanent
 * @IDE_SOURCE_VIEW_MODE_MODAL: Modal
 *
 * The type of keyboard mode.
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
 * @IDE_SOURCE_VIEW_MOVEMENT_PAGE_UP: move a full page up.
 * @IDE_SOURCE_VIEW_MOVEMENT_PAGE_DOWN: move a full page down.
 * @IDE_SOURCE_VIEW_MOVEMENT_SCREEN_UP: move to viewport up by visible line, adjusting cursor
 *   to stay on screen if necessary.
 * @IDE_SOURCE_VIEW_MOVEMENT_SCREEN_DOWN: move to viewport down by visible line, adjusting cursor
 *   to stay on screen if necessary.
 * @IDE_SOURCE_VIEW_MOVEMENT_SCREEN_TOP: move to the top of the screen.
 * @IDE_SOURCE_VIEW_MOVEMENT_SCREEN_MIDDLE: move to the middle of the screen.
 * @IDE_SOURCE_VIEW_MOVEMENT_SCREEN_BOTTOM: move to the bottom of the screen.
 * @IDE_SOURCE_VIEW_MOVEMENT_MATCH_SPECIAL: move to match of brace, bracket, comment.
 * @IDE_SOURCE_VIEW_MOVEMENT_SCROLL_SCREEN_TOP: scroll until insert cursor is at screen top.
 * @IDE_SOURCE_VIEW_MOVEMENT_SCROLL_SCREEN_CENTER: scroll until insert cursor is at screen center.
 * @IDE_SOURCE_VIEW_MOVEMENT_SCROLL_SCREEN_BOTTOM: scroll until insert cursor is at screen bottom.
 *
 * The type of movement.
 *
 * Some of these movements may be modified by using the modify-repeat action.
 * First adjust the repeat and then perform the "movement" action.
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
  IDE_SOURCE_VIEW_MOVEMENT_PREVIOUS_WORD_START,
  IDE_SOURCE_VIEW_MOVEMENT_PREVIOUS_FULL_WORD_START,

  IDE_SOURCE_VIEW_MOVEMENT_PREVIOUS_WORD_END,
  IDE_SOURCE_VIEW_MOVEMENT_PREVIOUS_FULL_WORD_END,
  IDE_SOURCE_VIEW_MOVEMENT_NEXT_WORD_END,
  IDE_SOURCE_VIEW_MOVEMENT_NEXT_FULL_WORD_END,

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

  IDE_SOURCE_VIEW_MOVEMENT_PAGE_UP,
  IDE_SOURCE_VIEW_MOVEMENT_PAGE_DOWN,

  IDE_SOURCE_VIEW_MOVEMENT_SCREEN_UP,
  IDE_SOURCE_VIEW_MOVEMENT_SCREEN_DOWN,
  IDE_SOURCE_VIEW_MOVEMENT_SCREEN_TOP,
  IDE_SOURCE_VIEW_MOVEMENT_SCREEN_MIDDLE,
  IDE_SOURCE_VIEW_MOVEMENT_SCREEN_BOTTOM,

  IDE_SOURCE_VIEW_MOVEMENT_MATCH_SPECIAL,

  IDE_SOURCE_VIEW_MOVEMENT_SCROLL_SCREEN_TOP,
  IDE_SOURCE_VIEW_MOVEMENT_SCROLL_SCREEN_CENTER,
  IDE_SOURCE_VIEW_MOVEMENT_SCROLL_SCREEN_BOTTOM,

  IDE_SOURCE_VIEW_MOVEMENT_PREVIOUS_UNMATCHED_BRACE,
  IDE_SOURCE_VIEW_MOVEMENT_NEXT_UNMATCHED_BRACE,

  IDE_SOURCE_VIEW_MOVEMENT_PREVIOUS_UNMATCHED_PAREN,
  IDE_SOURCE_VIEW_MOVEMENT_NEXT_UNMATCHED_PAREN,
} IdeSourceViewMovement;

G_DEFINE_AUTOPTR_CLEANUP_FUNC (IdeSourceView, g_object_unref)

struct _IdeSourceView
{
  GtkSourceView parent;
};

struct _IdeSourceViewClass
{
  GtkSourceViewClass parent_class;

  void (*action)                      (IdeSourceView           *self,
                                       const gchar             *prefix,
                                       const gchar             *action_name,
                                       const gchar             *param);
  void (*append_to_count)             (IdeSourceView           *self,
                                       gint                     digit);
  void (*auto_indent)                 (IdeSourceView           *self);
  void (*change_case)                 (IdeSourceView           *self,
                                       GtkSourceChangeCaseType  type);
  void (*clear_count)                 (IdeSourceView           *self);
  void (*clear_selection)             (IdeSourceView           *self);
  void (*cycle_completion)            (IdeSourceView           *self,
                                       GtkDirectionType         direction);
  void (*delete_selection)            (IdeSourceView           *self);
  void (*indent_selection)            (IdeSourceView           *self,
                                       gint                     level);
  void (*insert_at_cursor_and_indent) (IdeSourceView           *self,
                                       const gchar             *str);
  void (*movement)                    (IdeSourceView           *self,
                                       IdeSourceViewMovement    movement,
                                       gboolean                 extend_selection,
                                       gboolean                 exclusive,
                                       gboolean                 apply_count);
  void (*join_lines)                  (IdeSourceView           *self);
  void (*jump)                        (IdeSourceView           *self,
                                       const GtkTextIter       *location);
  void (*paste_clipboard_extended)    (IdeSourceView           *self,
                                       gboolean                 smart_lines,
                                       gboolean                 after_cursor,
                                       gboolean                 place_cursor_at_original);
  void (*push_selection)              (IdeSourceView           *self);
  void (*pop_selection)               (IdeSourceView           *self);
  void (*pop_snippet)                 (IdeSourceView           *self,
                                       IdeSourceSnippet        *snippet);
  void (*push_snippet)                (IdeSourceView           *self,
                                       IdeSourceSnippet        *snippet,
                                       IdeSourceSnippetContext *context,
                                       const GtkTextIter       *location);
  void (*restore_insert_mark)         (IdeSourceView           *self);
  void (*save_insert_mark)            (IdeSourceView           *self);
  void (*selection_theatric)          (IdeSourceView           *self,
                                       IdeSourceViewTheatric    theatric);
  void (*set_mode)                    (IdeSourceView           *self,
                                       const gchar             *mode,
                                       IdeSourceViewModeType    type);
  void (*set_overwrite)               (IdeSourceView           *self,
                                       gboolean                 overwrite);
  void (*sort)                        (IdeSourceView           *self,
                                       gboolean                 ignore_case,
                                       gboolean                 reverse);
  void (*swap_selection_bounds)       (IdeSourceView           *self);
};

void                        ide_source_view_clear_snippets            (IdeSourceView              *self);
IdeBackForwardList         *ide_source_view_get_back_forward_list     (IdeSourceView              *self);
const PangoFontDescription *ide_source_view_get_font_desc             (IdeSourceView              *self);
gboolean                    ide_source_view_get_insert_matching_brace (IdeSourceView              *self);
gboolean                    ide_source_view_get_overwrite_braces      (IdeSourceView              *self);
guint                       ide_source_view_get_scroll_offset         (IdeSourceView              *self);
gboolean                    ide_source_view_get_show_grid_lines       (IdeSourceView              *self);
gboolean                    ide_source_view_get_show_line_changes     (IdeSourceView              *self);
gboolean                    ide_source_view_get_snippet_completion    (IdeSourceView              *self);
GType                       ide_source_view_get_type                  (void);
void                        ide_source_view_pop_snippet               (IdeSourceView              *self);
void                        ide_source_view_push_snippet              (IdeSourceView              *self,
                                                                       IdeSourceSnippet           *snippet);
void                        ide_source_view_set_font_desc             (IdeSourceView              *self,
                                                                       const PangoFontDescription *font_desc);
void                        ide_source_view_set_font_name             (IdeSourceView              *self,
                                                                       const gchar                *font_name);
void                        ide_source_view_set_insert_matching_brace (IdeSourceView              *self,
                                                                       gboolean                    insert_matching_brace);
void                        ide_source_view_set_overwrite_braces      (IdeSourceView              *self,
                                                                       gboolean                    overwrite_braces);
void                        ide_source_view_set_scroll_offset         (IdeSourceView              *self,
                                                                       guint                       scroll_offset);
void                        ide_source_view_set_show_grid_lines       (IdeSourceView              *self,
                                                                       gboolean                    show_grid_lines);
void                        ide_source_view_set_show_line_changes     (IdeSourceView              *self,
                                                                       gboolean                    show_line_changes);
void                        ide_source_view_set_snippet_completion    (IdeSourceView              *self,
                                                                       gboolean                    snippet_completion);
void                        ide_source_view_set_back_forward_list     (IdeSourceView              *self,
                                                                       IdeBackForwardList         *back_forward_list);
void                        ide_source_view_get_visible_rect          (IdeSourceView              *self,
                                                                       GdkRectangle               *visible_rect);
gboolean                    ide_source_view_move_mark_onscreen        (IdeSourceView              *self,
                                                                       GtkTextMark                *mark);
gboolean                    ide_source_view_place_cursor_onscreen     (IdeSourceView              *self);
void                        ide_source_view_scroll_mark_onscreen      (IdeSourceView              *self,
                                                                       GtkTextMark                *mark);
void                        ide_source_view_scroll_to_mark            (IdeSourceView              *self,
                                                                       GtkTextMark                *mark,
                                                                       gdouble                     within_margin,
                                                                       gboolean                    use_align,
                                                                       gdouble                     xalign,
                                                                       gdouble                     yalign);
void                        ide_source_view_scroll_to_iter            (IdeSourceView              *self,
                                                                       const GtkTextIter          *iter,
                                                                       gdouble                     within_margin,
                                                                       gboolean                    use_align,
                                                                       gdouble                     xalign,
                                                                       gdouble                     yalign);

G_END_DECLS

#endif /* IDE_SOURCE_VIEW_H */
