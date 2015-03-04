/* ide-source-view-movement-helper.c
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

#define G_LOG_DOMAIN "ide-source-view"

#include "ide-debug.h"
#include "ide-enums.h"
#include "ide-source-view-movements.h"

typedef struct
{
  gunichar jump_to;
  gunichar jump_from;
  guint    depth;
} MatchingBracketState;

static void
ide_source_view_movements_get_selection (IdeSourceView *self,
                                         GtkTextIter   *insert,
                                         GtkTextIter   *selection)
{
  GtkTextBuffer *buffer;
  GtkTextMark *mark;

  g_assert (IDE_IS_SOURCE_VIEW (self));

  buffer = gtk_text_view_get_buffer (GTK_TEXT_VIEW (self));

  if (insert)
    {
      mark = gtk_text_buffer_get_insert (buffer);
      gtk_text_buffer_get_iter_at_mark (buffer, insert, mark);
    }

  if (selection)
    {
      mark = gtk_text_buffer_get_selection_bound (buffer);
      gtk_text_buffer_get_iter_at_mark (buffer, selection, mark);
    }
}

static void
ide_source_view_movements_select_range (IdeSourceView     *self,
                                        const GtkTextIter *insert,
                                        const GtkTextIter *selection,
                                        gboolean           extend_selection)
{
  GtkTextBuffer *buffer;
  GtkTextMark *mark;

  g_assert (IDE_IS_SOURCE_VIEW (self));

  buffer = gtk_text_view_get_buffer (GTK_TEXT_VIEW (self));

  if (extend_selection)
    gtk_text_buffer_select_range (buffer, insert, selection);
  else
    gtk_text_buffer_select_range (buffer, insert, insert);

  mark = gtk_text_buffer_get_insert (buffer);
  gtk_text_view_scroll_mark_onscreen (GTK_TEXT_VIEW (self), mark);
}

static void
ide_source_view_movements_nth_char (IdeSourceView         *self,
                                    IdeSourceViewMovement  movement,
                                    gboolean               extend_selection,
                                    gint                   param)
{
  GtkTextIter insert;
  GtkTextIter selection;

  ide_source_view_movements_get_selection (self, &insert, &selection);

  gtk_text_iter_set_line_offset (&insert, 0);

  for (; param > 0; param--)
    {
      if (gtk_text_iter_ends_line (&insert))
        break;
      gtk_text_iter_forward_char (&insert);
    }

  ide_source_view_movements_select_range (self, &insert, &selection, extend_selection);
}

static void
ide_source_view_movements_previous_char (IdeSourceView         *self,
                                         IdeSourceViewMovement  movement,
                                         gboolean               extend_selection,
                                         gint                   param)
{
  GtkTextIter insert;
  GtkTextIter selection;

  param = MAX (1, param);

  ide_source_view_movements_get_selection (self, &insert, &selection);

  for (; param; param--)
    {
      if (gtk_text_iter_starts_line (&insert))
        break;
      gtk_text_iter_backward_char (&insert);
    }

  ide_source_view_movements_select_range (self, &insert, &selection, extend_selection);
}

static void
ide_source_view_movements_next_char (IdeSourceView         *self,
                                     IdeSourceViewMovement  movement,
                                     gboolean               extend_selection,
                                     gint                   param)
{
  GtkTextIter insert;
  GtkTextIter selection;

  param = MAX (1, param);

  ide_source_view_movements_get_selection (self, &insert, &selection);

  for (; param; param--)
    {
      if (gtk_text_iter_ends_line (&insert))
        break;
      gtk_text_iter_forward_char (&insert);
    }

  ide_source_view_movements_select_range (self, &insert, &selection, extend_selection);
}

static void
ide_source_view_movements_first_char (IdeSourceView         *self,
                                      IdeSourceViewMovement  movement,
                                      gboolean               extend_selection,
                                      gint                   param)
{
  GtkTextIter insert;
  GtkTextIter selection;

  ide_source_view_movements_get_selection (self, &insert, &selection);

  gtk_text_iter_set_line_offset (&insert, 0);

  ide_source_view_movements_select_range (self, &insert, &selection, extend_selection);
}

static void
ide_source_view_movements_first_nonspace_char (IdeSourceView         *self,
                                               IdeSourceViewMovement  movement,
                                               gboolean               extend_selection,
                                               gint                   param)
{
  GtkTextIter insert;
  GtkTextIter selection;
  gunichar ch;

  ide_source_view_movements_get_selection (self, &insert, &selection);

  gtk_text_iter_set_line_offset (&insert, 0);

  while (!gtk_text_iter_ends_line (&insert) &&
         (ch = gtk_text_iter_get_char (&insert)) &&
         g_unichar_isspace (ch))
    gtk_text_iter_forward_char (&insert);

  ide_source_view_movements_select_range (self, &insert, &selection, extend_selection);
}

static void
ide_source_view_movements_middle_char (IdeSourceView         *self,
                                       IdeSourceViewMovement  movement,
                                       gboolean               extend_selection,
                                       gint                   param)
{
  GtkTextIter insert;
  GtkTextIter selection;
  guint line_offset;

  ide_source_view_movements_get_selection (self, &insert, &selection);

  gtk_text_iter_forward_to_line_end (&insert);
  line_offset = gtk_text_iter_get_line_offset (&insert);
  gtk_text_iter_set_line_offset (&insert, line_offset >> 1);

  ide_source_view_movements_select_range (self, &insert, &selection, extend_selection);
}

static void
ide_source_view_movements_last_char (IdeSourceView         *self,
                                     IdeSourceViewMovement  movement,
                                     gboolean               extend_selection,
                                     gint                   param)
{
  GtkTextIter insert;
  GtkTextIter selection;

  ide_source_view_movements_get_selection (self, &insert, &selection);

  gtk_text_iter_forward_to_line_end (&insert);

  ide_source_view_movements_select_range (self, &insert, &selection, extend_selection);
}

static void
ide_source_view_movements_first_line (IdeSourceView         *self,
                                      IdeSourceViewMovement  movement,
                                      gboolean               extend_selection,
                                      gint                   param)
{
  GtkTextIter insert;
  GtkTextIter selection;

  ide_source_view_movements_get_selection (self, &insert, &selection);

  gtk_text_iter_set_line (&insert, param);
  gtk_text_iter_set_line_offset (&insert, 0);

  ide_source_view_movements_select_range (self, &insert, &selection, extend_selection);
}

static void
ide_source_view_movements_nth_line (IdeSourceView         *self,
                                    IdeSourceViewMovement  movement,
                                    gboolean               extend_selection,
                                    gint                   param)
{
  GtkTextBuffer *buffer;
  GtkTextIter insert;
  GtkTextIter selection;

  ide_source_view_movements_get_selection (self, &insert, &selection);

  buffer = gtk_text_view_get_buffer (GTK_TEXT_VIEW (self));

  if (param < 1)
    gtk_text_buffer_get_end_iter (buffer, &insert);
  else
    gtk_text_iter_set_line (&insert, param - 1);

  gtk_text_iter_set_line_offset (&insert, 0);

  ide_source_view_movements_select_range (self, &insert, &selection, extend_selection);
}

static void
ide_source_view_movements_last_line (IdeSourceView         *self,
                                     IdeSourceViewMovement  movement,
                                     gboolean               extend_selection,
                                     gint                   param)
{
  GtkTextBuffer *buffer;
  GtkTextIter insert;
  GtkTextIter selection;

  ide_source_view_movements_get_selection (self, &insert, &selection);

  buffer = gtk_text_view_get_buffer (GTK_TEXT_VIEW (self));
  gtk_text_buffer_get_end_iter (buffer, &insert);
  gtk_text_iter_set_line_offset (&insert, 0);

  if (param)
    {
      gint line;

      line = gtk_text_iter_get_line (&insert) - param;
      gtk_text_iter_set_line (&insert, MAX (0, line));
    }

  ide_source_view_movements_select_range (self, &insert, &selection, extend_selection);
}

static void
ide_source_view_movements_next_line (IdeSourceView         *self,
                                     IdeSourceViewMovement  movement,
                                     gboolean               extend_selection,
                                     gint                   param)
{
  param = MAX (1, param);
  g_signal_emit_by_name (self,
                         "move-cursor",
                         GTK_MOVEMENT_DISPLAY_LINES,
                         param,
                         extend_selection);
}

static void
ide_source_view_movements_previous_line (IdeSourceView         *self,
                                         IdeSourceViewMovement  movement,
                                         gboolean               extend_selection,
                                         gint                   param)
{
  param = MAX (1, param);
  g_signal_emit_by_name (self,
                         "move-cursor",
                         GTK_MOVEMENT_DISPLAY_LINES,
                         -param,
                         extend_selection);
}

static void
ide_source_view_movements_screen_top (IdeSourceView         *self,
                                      IdeSourceViewMovement  movement,
                                      gboolean               extend_selection,
                                      gint                   param)
{
  GtkTextView *text_view = (GtkTextView *)self;
  GdkRectangle rect;
  GtkTextIter insert;
  GtkTextIter selection;

  ide_source_view_movements_get_selection (self, &insert, &selection);

  gtk_text_view_get_visible_rect (text_view, &rect);
  gtk_text_view_get_iter_at_location (text_view, &insert, rect.x, rect.y);
  gtk_text_iter_set_line_offset (&insert, 0);

  ide_source_view_movements_select_range (self, &insert, &selection, extend_selection);
}

static void
ide_source_view_movements_screen_middle (IdeSourceView         *self,
                                         IdeSourceViewMovement  movement,
                                         gboolean               extend_selection,
                                         gint                   param)
{
  GtkTextView *text_view = (GtkTextView *)self;
  GdkRectangle rect;
  GtkTextIter insert;
  GtkTextIter selection;

  ide_source_view_movements_get_selection (self, &insert, &selection);

  gtk_text_view_get_visible_rect (text_view, &rect);
  gtk_text_view_get_iter_at_location (text_view, &insert, rect.x, rect.y + (rect.height/2));
  gtk_text_iter_set_line_offset (&insert, 0);

  ide_source_view_movements_select_range (self, &insert, &selection, extend_selection);
}

static void
ide_source_view_movements_screen_bottom (IdeSourceView         *self,
                                         IdeSourceViewMovement  movement,
                                         gboolean               extend_selection,
                                         gint                   param)
{
  GtkTextView *text_view = (GtkTextView *)self;
  GdkRectangle rect;
  GtkTextIter insert;
  GtkTextIter selection;

  ide_source_view_movements_get_selection (self, &insert, &selection);

  gtk_text_view_get_visible_rect (text_view, &rect);
  gtk_text_view_get_iter_at_location (text_view, &insert, rect.x, rect.y + rect.height);
  gtk_text_iter_set_line_offset (&insert, 0);

  ide_source_view_movements_select_range (self, &insert, &selection, extend_selection);
}

static void
ide_source_view_movements_scroll_by_lines (IdeSourceView *self,
                                           gint           lines)
{
  GtkTextView *text_view = (GtkTextView *)self;
  GtkTextIter insert;
  GtkTextIter selection;
  GtkTextIter begin;
  GtkTextIter end;
  GtkAdjustment *vadj;
  GtkTextBuffer *buffer;
  GdkRectangle rect;
  gdouble amount;
  gdouble value;
  gdouble upper;

  if (lines == 0)
    return;

  vadj = gtk_scrollable_get_vadjustment (GTK_SCROLLABLE (self));
  buffer = gtk_text_view_get_buffer (text_view);

  gtk_text_buffer_get_bounds (buffer, &begin, &end);

  ide_source_view_movements_get_selection (self, &insert, &selection);

  if (lines > 0)
    {
      if (gtk_text_iter_get_line (&end) == gtk_text_iter_get_line (&insert))
        return;
    }
  else if (lines < 0)
    {
      if (gtk_text_iter_get_line (&begin) == gtk_text_iter_get_line (&insert))
        return;
    }
  else
    g_assert_not_reached ();

  gtk_text_view_get_iter_location (text_view, &insert, &rect);

  amount = lines * rect.height;

  value = gtk_adjustment_get_value (vadj);
  upper = gtk_adjustment_get_upper (vadj);
  gtk_adjustment_set_value (vadj, CLAMP (value + amount, 0, upper));
}

static void
ide_source_view_movements_scroll (IdeSourceView         *self,
                                  IdeSourceViewMovement  movement,
                                  gboolean               extend_selection,
                                  gint                   param,
                                  GtkDirectionType       dir)
{
  GtkTextView *text_view = (GtkTextView *)self;
  GtkTextBuffer *buffer;
  GtkTextMark *mark;
  GtkTextIter insert;
  GtkTextIter selection;

  param = MAX (1, param);

  ide_source_view_movements_get_selection (self, &insert, &selection);

  ide_source_view_movements_scroll_by_lines (self, (dir == GTK_DIR_UP) ? param : -param);

  buffer = gtk_text_view_get_buffer (GTK_TEXT_VIEW (self));
  mark = gtk_text_buffer_get_insert (buffer);
  gtk_text_view_move_mark_onscreen (text_view, mark);

  gtk_text_buffer_get_iter_at_mark (buffer, &insert, mark);

  ide_source_view_movements_select_range (self, &insert, &selection, extend_selection);
}

static void
ide_source_view_movements_move_page (IdeSourceView         *self,
                                     IdeSourceViewMovement  movement,
                                     gboolean               extend_selection,
                                     gint                   param)
{
  GtkTextView *text_view = (GtkTextView *)self;
  GtkTextBuffer *buffer;
  GdkRectangle rect;
  GtkTextIter iter_top;
  GtkTextIter iter_bottom;
  GtkTextIter iter_current;
  GtkTextIter insert;
  GtkTextIter selection;
  gint half_page;
  gint line_top;
  gint line_bottom;

  ide_source_view_movements_get_selection (self, &insert, &selection);

  gtk_text_view_get_visible_rect (text_view, &rect);
  gtk_text_view_get_iter_at_location (text_view, &iter_top, rect.x, rect.y);
  gtk_text_view_get_iter_at_location (text_view, &iter_bottom, rect.x, rect.y + rect.height);

  buffer = gtk_text_view_get_buffer (text_view);
  gtk_text_buffer_get_selection_bounds (buffer, &iter_current, NULL);

  line_top = gtk_text_iter_get_line (&iter_top);
  line_bottom = gtk_text_iter_get_line (&iter_bottom);

  half_page = (line_bottom - line_top) / 2;

  /*
   * TODO: Reintroduce scroll offset.
   */

  switch ((int)movement)
    {
    case IDE_SOURCE_VIEW_MOVEMENT_HALF_PAGE_UP:
      ide_source_view_movements_scroll_by_lines (self, -half_page);
      gtk_text_iter_backward_lines (&insert, half_page);
      ide_source_view_movements_select_range (self, &insert, &selection, extend_selection);
      break;

    case IDE_SOURCE_VIEW_MOVEMENT_HALF_PAGE_DOWN:
      ide_source_view_movements_scroll_by_lines (self, half_page);
      gtk_text_iter_forward_lines (&insert, half_page);
      ide_source_view_movements_select_range (self, &insert, &selection, extend_selection);
      break;

    case IDE_SOURCE_VIEW_MOVEMENT_PAGE_UP:
      gtk_text_buffer_get_iter_at_line (buffer, &insert, MAX (0, line_top - 1));
      ide_source_view_movements_select_range (self, &insert, &selection, extend_selection);
      gtk_text_view_scroll_to_iter (text_view, &insert, .0, TRUE, .0, 1.0);
      break;

    case IDE_SOURCE_VIEW_MOVEMENT_PAGE_DOWN:
      gtk_text_buffer_get_iter_at_line (buffer, &insert, line_bottom + 1);
      ide_source_view_movements_select_range (self, &insert, &selection, extend_selection);
      gtk_text_view_scroll_to_iter (text_view, &insert, .0, TRUE, .0, .0);
      break;

    default:
      g_assert_not_reached();
    }
}

static gboolean
bracket_predicate (gunichar ch,
                   gpointer user_data)
{
  MatchingBracketState *state = user_data;

  if (ch == state->jump_from)
    state->depth++;
  else if (ch == state->jump_to)
    state->depth--;

  return (state->depth == 0);
}

static void
ide_source_view_movements_match_special (IdeSourceView         *self,
                                         IdeSourceViewMovement  movement,
                                         gboolean               extend_selection,
                                         gint                   param)
{
  MatchingBracketState state;
  GtkTextIter insert;
  GtkTextIter selection;
  gboolean is_forward = FALSE;
  gboolean ret;

  ide_source_view_movements_get_selection (self, &insert, &selection);

  state.depth = 1;
  state.jump_from = gtk_text_iter_get_char (&insert);

  switch (state.jump_from)
    {
    case '{':
      state.jump_to = '}';
      is_forward = TRUE;
      break;

    case '[':
      state.jump_to = ']';
      is_forward = TRUE;
      break;

    case '(':
      state.jump_to = ')';
      is_forward = TRUE;
      break;

    case '}':
      state.jump_to = '{';
      is_forward = FALSE;
      break;

    case ']':
      state.jump_to = '[';
      is_forward = FALSE;
      break;

    case ')':
      state.jump_to = '(';
      is_forward = FALSE;
      break;

    default:
      return;
    }

  if (is_forward)
    ret = gtk_text_iter_forward_find_char (&insert, bracket_predicate, &state, NULL);
  else
    ret = gtk_text_iter_backward_find_char (&insert, bracket_predicate, &state, NULL);

  if (ret)
    ide_source_view_movements_select_range (self, &insert, &selection, extend_selection);
}

static void
ide_source_view_movements_scroll_center (IdeSourceView         *self,
                                         IdeSourceViewMovement  movement,
                                         gboolean               extend_selection,
                                         gint                   param)
{
  GtkTextView *text_view = (GtkTextView *)self;
  GtkTextIter insert;
  GtkTextIter selection;

  ide_source_view_movements_get_selection (self, &insert, &selection);

  switch ((int)movement)
    {
    case IDE_SOURCE_VIEW_MOVEMENT_SCROLL_SCREEN_BOTTOM:
      gtk_text_view_scroll_to_iter (text_view, &insert, 0.0, TRUE, 1.0, 1.0);
      break;

    case IDE_SOURCE_VIEW_MOVEMENT_SCROLL_SCREEN_TOP:
      gtk_text_view_scroll_to_iter (text_view, &insert, 0.0, TRUE, 1.0, 0.0);
      break;

    case IDE_SOURCE_VIEW_MOVEMENT_SCROLL_SCREEN_CENTER:
      gtk_text_view_scroll_to_iter (text_view, &insert, 0.0, TRUE, 1.0, 0.5);
      break;

    default:
      break;
    }
}

void
_ide_source_view_apply_movement (IdeSourceView         *self,
                                 IdeSourceViewMovement  movement,
                                 gboolean               extend_selection,
                                 gint                   param)
{
#ifndef IDE_DISABLE_TRACE
  {
    GEnumValue *enum_value;
    GEnumClass *enum_class;

    enum_class = g_type_class_peek (IDE_TYPE_SOURCE_VIEW_MOVEMENT);
    enum_value = g_enum_get_value (enum_class, movement);
    IDE_TRACE_MSG ("apply movement: %s", enum_value->value_nick);
  }
#endif

  switch (movement)
    {
    case IDE_SOURCE_VIEW_MOVEMENT_NTH_CHAR:
      ide_source_view_movements_nth_char (self, movement, extend_selection, param);
      break;

    case IDE_SOURCE_VIEW_MOVEMENT_PREVIOUS_CHAR:
      ide_source_view_movements_previous_char (self, movement, extend_selection, param);
      break;

    case IDE_SOURCE_VIEW_MOVEMENT_NEXT_CHAR:
      ide_source_view_movements_next_char (self, movement, extend_selection, param);
      break;

    case IDE_SOURCE_VIEW_MOVEMENT_FIRST_CHAR:
      ide_source_view_movements_first_char (self, movement, extend_selection, param);
      break;

    case IDE_SOURCE_VIEW_MOVEMENT_FIRST_NONSPACE_CHAR:
      ide_source_view_movements_first_nonspace_char (self, movement, extend_selection, param);
      break;

    case IDE_SOURCE_VIEW_MOVEMENT_MIDDLE_CHAR:
      ide_source_view_movements_middle_char (self, movement, extend_selection, param);
      break;

    case IDE_SOURCE_VIEW_MOVEMENT_LAST_CHAR:
      ide_source_view_movements_last_char (self, movement, extend_selection, param);
      break;

    case IDE_SOURCE_VIEW_MOVEMENT_PREVIOUS_WORD_START:
      break;

    case IDE_SOURCE_VIEW_MOVEMENT_NEXT_WORD_START:
      break;

    case IDE_SOURCE_VIEW_MOVEMENT_PREVIOUS_WORD_END:
      break;

    case IDE_SOURCE_VIEW_MOVEMENT_NEXT_WORD_END:
      break;

    case IDE_SOURCE_VIEW_MOVEMENT_SENTANCE_START:
      break;

    case IDE_SOURCE_VIEW_MOVEMENT_SENTANCE_END:
      break;

    case IDE_SOURCE_VIEW_MOVEMENT_PARAGRAPH_START:
      break;

    case IDE_SOURCE_VIEW_MOVEMENT_PARAGRAPH_END:
      break;

    case IDE_SOURCE_VIEW_MOVEMENT_PREVIOUS_LINE:
      ide_source_view_movements_previous_line (self, movement, extend_selection, param);
      break;

    case IDE_SOURCE_VIEW_MOVEMENT_NEXT_LINE:
      ide_source_view_movements_next_line (self, movement, extend_selection, param);
      break;

    case IDE_SOURCE_VIEW_MOVEMENT_FIRST_LINE:
      ide_source_view_movements_first_line (self, movement, extend_selection, param);
      break;

    case IDE_SOURCE_VIEW_MOVEMENT_NTH_LINE:
      ide_source_view_movements_nth_line (self, movement, extend_selection, param);
      break;

    case IDE_SOURCE_VIEW_MOVEMENT_LAST_LINE:
      ide_source_view_movements_last_line (self, movement, extend_selection, param);
      break;

    case IDE_SOURCE_VIEW_MOVEMENT_LINE_PERCENTAGE:
      break;

    case IDE_SOURCE_VIEW_MOVEMENT_HALF_PAGE_UP:
    case IDE_SOURCE_VIEW_MOVEMENT_HALF_PAGE_DOWN:
    case IDE_SOURCE_VIEW_MOVEMENT_PAGE_UP:
    case IDE_SOURCE_VIEW_MOVEMENT_PAGE_DOWN:
      ide_source_view_movements_move_page (self, movement, extend_selection, param);
      break;

    case IDE_SOURCE_VIEW_MOVEMENT_SCREEN_UP:
      ide_source_view_movements_scroll (self, movement, extend_selection, param, GTK_DIR_UP);
      break;

    case IDE_SOURCE_VIEW_MOVEMENT_SCREEN_DOWN:
      ide_source_view_movements_scroll (self, movement, extend_selection, param, GTK_DIR_DOWN);
      break;

    case IDE_SOURCE_VIEW_MOVEMENT_SCREEN_TOP:
      ide_source_view_movements_screen_top (self, movement, extend_selection, param);
      break;

    case IDE_SOURCE_VIEW_MOVEMENT_SCREEN_MIDDLE:
      ide_source_view_movements_screen_middle (self, movement, extend_selection, param);
      break;

    case IDE_SOURCE_VIEW_MOVEMENT_SCREEN_BOTTOM:
      ide_source_view_movements_screen_bottom (self, movement, extend_selection, param);
      break;

    case IDE_SOURCE_VIEW_MOVEMENT_MATCH_SPECIAL:
      ide_source_view_movements_match_special (self, movement, extend_selection, param);
      break;

    case IDE_SOURCE_VIEW_MOVEMENT_SCROLL_SCREEN_TOP:
    case IDE_SOURCE_VIEW_MOVEMENT_SCROLL_SCREEN_CENTER:
    case IDE_SOURCE_VIEW_MOVEMENT_SCROLL_SCREEN_BOTTOM:
      ide_source_view_movements_scroll_center (self, movement, extend_selection, param);
      break;

    default:
      g_return_if_reached ();
    }
}
