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
#include "ide-source-iter.h"
#include "ide-source-view-movements.h"
#include "ide-vim-iter.h"

typedef struct
{
  IdeSourceView         *self;
  IdeSourceViewMovement  type;                 /* Type of movement */
  GtkTextIter            insert;               /* Current insert cursor location */
  GtkTextIter            selection;            /* Current selection cursor location */
  gint                   count;                /* Repeat count for movement */
  guint                  extend_selection : 1; /* If selection should be extended */
  guint                  exclusive : 1;        /* See ":help exclusive" in vim */
  guint                  ignore_select : 1;    /* Don't update selection after movement */
} Movement;

typedef struct
{
  gunichar jump_to;
  gunichar jump_from;
  guint    depth;
} MatchingBracketState;

static gboolean
text_iter_forward_to_empty_line (GtkTextIter *iter,
                                 GtkTextIter *bounds)
{
  if (!gtk_text_iter_forward_char (iter))
    return FALSE;

  while (gtk_text_iter_compare (iter, bounds) < 0)
    {
      if (gtk_text_iter_starts_line (iter) && gtk_text_iter_ends_line (iter))
        return TRUE;
      if (!gtk_text_iter_forward_char (iter))
        return FALSE;
    }

  return FALSE;
}

static gboolean
is_single_line_selection (const GtkTextIter *insert,
                          const GtkTextIter *selection)
{
  return (gtk_text_iter_get_line (insert) == gtk_text_iter_get_line (selection)) &&
         (gtk_text_iter_starts_line (insert) || gtk_text_iter_starts_line (selection)) &&
         (gtk_text_iter_ends_line (insert) || gtk_text_iter_ends_line (selection));
}

static gboolean
is_line_selection (const GtkTextIter *insert,
                   const GtkTextIter *selection)
{
  gboolean ret = FALSE;

  if (is_single_line_selection (insert, selection))
    ret = TRUE;
  else if (gtk_text_iter_starts_line (selection) &&
      gtk_text_iter_ends_line (insert) &&
      !gtk_text_iter_equal (selection, insert))
    ret = TRUE;

  IDE_TRACE_MSG ("is_line_selection=%s", ret ? "TRUE" : "FALSE");

  return ret;
}

static void
ide_source_view_movements_get_selection (Movement *mv)
{
  GtkTextBuffer *buffer;
  GtkTextMark *mark;

  g_assert (mv);
  g_assert (IDE_IS_SOURCE_VIEW (mv->self));

  buffer = gtk_text_view_get_buffer (GTK_TEXT_VIEW (mv->self));

  mark = gtk_text_buffer_get_insert (buffer);
  gtk_text_buffer_get_iter_at_mark (buffer, &mv->insert, mark);

  mark = gtk_text_buffer_get_selection_bound (buffer);
  gtk_text_buffer_get_iter_at_mark (buffer, &mv->selection, mark);
}

static void
ide_source_view_movements_select_range (Movement *mv)
{
  GtkTextBuffer *buffer;
  GtkTextMark *mark;

  g_assert (mv);
  g_assert (IDE_IS_SOURCE_VIEW (mv->self));

  buffer = gtk_text_view_get_buffer (GTK_TEXT_VIEW (mv->self));

  if (mv->extend_selection)
    gtk_text_buffer_select_range (buffer, &mv->insert, &mv->selection);
  else
    gtk_text_buffer_select_range (buffer, &mv->insert, &mv->insert);

  mark = gtk_text_buffer_get_insert (buffer);
  gtk_text_view_scroll_mark_onscreen (GTK_TEXT_VIEW (mv->self), mark);
}

static void
ide_source_view_movements_nth_char (Movement *mv)
{
  gtk_text_iter_set_line_offset (&mv->insert, 0);

  for (; mv->count > 0; mv->count--)
    {
      if (gtk_text_iter_ends_line (&mv->insert))
        break;
      gtk_text_iter_forward_char (&mv->insert);
    }

  if (!mv->exclusive)
    gtk_text_iter_forward_char (&mv->insert);
}

static void
ide_source_view_movements_previous_char (Movement *mv)
{
  mv->count = MAX (1, mv->count);

  for (; mv->count; mv->count--)
    {
      if (gtk_text_iter_starts_line (&mv->insert))
        break;
      gtk_text_iter_backward_char (&mv->insert);
    }

  if (!mv->exclusive)
    gtk_text_iter_forward_char (&mv->insert);
}

static void
ide_source_view_movements_next_char (Movement *mv)
{
  mv->count = MAX (1, mv->count);

  for (; mv->count; mv->count--)
    {
      if (gtk_text_iter_ends_line (&mv->insert))
        break;
      gtk_text_iter_forward_char (&mv->insert);
    }
}

static void
ide_source_view_movements_first_char (Movement *mv)
{
  gtk_text_iter_set_line_offset (&mv->insert, 0);
}

static void
ide_source_view_movements_first_nonspace_char (Movement *mv)
{
  gunichar ch;

  gtk_text_iter_set_line_offset (&mv->insert, 0);

  while (!gtk_text_iter_ends_line (&mv->insert) &&
         (ch = gtk_text_iter_get_char (&mv->insert)) &&
         g_unichar_isspace (ch))
    gtk_text_iter_forward_char (&mv->insert);
}

static void
ide_source_view_movements_line_chars (Movement *mv)
{
  /*
   * Selects the current position up to the first nonspace character.
   * If the cursor is at the line start, we will select the newline.
   * If only whitespace exists, we will select line offset of 0.
   */

  if (gtk_text_iter_starts_line (&mv->insert))
    {
      gtk_text_iter_backward_char (&mv->insert);
    }
  else
    {
      gunichar ch;

      gtk_text_iter_set_line_offset (&mv->insert, 0);

      while (!gtk_text_iter_ends_line (&mv->insert) &&
             (ch = gtk_text_iter_get_char (&mv->insert)) &&
             g_unichar_isspace (ch))
        gtk_text_iter_forward_char (&mv->insert);

      if (gtk_text_iter_ends_line (&mv->insert))
        gtk_text_iter_set_line_offset (&mv->insert, 0);
    }

  if (!mv->exclusive)
    gtk_text_iter_forward_char (&mv->insert);
}

static void
ide_source_view_movements_middle_char (Movement *mv)
{
  GtkTextView *text_view = GTK_TEXT_VIEW (mv->self);
  GdkWindow *window;
  GdkRectangle rect;
  guint line_offset;
  int width;
  int chars_in_line;

  gtk_text_view_get_iter_location (text_view, &mv->insert, &rect);
  window = gtk_text_view_get_window (text_view, GTK_TEXT_WINDOW_TEXT);

  width = gdk_window_get_width (window);
  if (rect.width <= 0)
    return;

  chars_in_line = width / rect.width;
  if (chars_in_line == 0)
    return;

  gtk_text_iter_set_line_offset (&mv->insert, 0);

  for (line_offset = chars_in_line / 2; line_offset; line_offset--)
    if (!gtk_text_iter_forward_char (&mv->insert))
      break;

  if (!mv->exclusive)
    if (!gtk_text_iter_ends_line (&mv->insert))
      gtk_text_iter_forward_char (&mv->insert);
}

static void
ide_source_view_movements_last_char (Movement *mv)
{
  if (!gtk_text_iter_ends_line (&mv->insert))
    {
      gtk_text_iter_forward_to_line_end (&mv->insert);
      if (mv->exclusive && !gtk_text_iter_starts_line (&mv->insert))
        gtk_text_iter_backward_char (&mv->insert);
    }
}

static void
ide_source_view_movements_first_line (Movement *mv)
{
  gtk_text_iter_set_line (&mv->insert, mv->count);
  gtk_text_iter_set_line_offset (&mv->insert, 0);
}

static void
ide_source_view_movements_nth_line (Movement *mv)
{
  GtkTextBuffer *buffer;

  buffer = gtk_text_view_get_buffer (GTK_TEXT_VIEW (mv->self));

  if (mv->count < 1)
    gtk_text_buffer_get_end_iter (buffer, &mv->insert);
  else
    gtk_text_iter_set_line (&mv->insert, mv->count - 1);

  gtk_text_iter_set_line_offset (&mv->insert, 0);
}

static void
ide_source_view_movements_last_line (Movement *mv)
{
  GtkTextBuffer *buffer;

  buffer = gtk_text_view_get_buffer (GTK_TEXT_VIEW (mv->self));
  gtk_text_buffer_get_end_iter (buffer, &mv->insert);
  gtk_text_iter_set_line_offset (&mv->insert, 0);

  if (mv->count)
    {
      gint line;

      line = gtk_text_iter_get_line (&mv->insert) - mv->count;
      gtk_text_iter_set_line (&mv->insert, MAX (0, line));
    }
}

static void
ide_source_view_movements_next_line (Movement *mv)
{
  /*
   * Try to use the normal move-cursor helpers if this is a simple movement.
   */
  if (!mv->extend_selection || !is_line_selection (&mv->insert, &mv->selection))
    {
      IDE_TRACE_MSG ("next-line simple");

      mv->count = MAX (1, mv->count);
      mv->ignore_select = TRUE;
      g_signal_emit_by_name (mv->self,
                             "move-cursor",
                             GTK_MOVEMENT_DISPLAY_LINES,
                             mv->count,
                             mv->extend_selection);
      return;
    }

  IDE_TRACE_MSG ("next-line with line-selection");

  g_assert (mv->extend_selection);
  g_assert (is_line_selection (&mv->insert, &mv->selection));

  if (gtk_text_iter_is_end (&mv->insert) || gtk_text_iter_is_end (&mv->selection))
    return;

  if (is_single_line_selection (&mv->insert, &mv->selection))
    gtk_text_iter_order (&mv->selection, &mv->insert);

  gtk_text_iter_forward_line (&mv->insert);
  if (!gtk_text_iter_ends_line (&mv->insert))
    gtk_text_iter_forward_to_line_end (&mv->insert);
}

static void
ide_source_view_movements_previous_line (Movement *mv)
{
  /*
   * Try to use the normal move-cursor helpers if this is a simple movement.
   */
  if (!mv->extend_selection || !is_line_selection (&mv->insert, &mv->selection))
    {
      IDE_TRACE_MSG ("previous-line simple");

      mv->count = MAX (1, mv->count);
      mv->ignore_select = TRUE;
      g_signal_emit_by_name (mv->self,
                             "move-cursor",
                             GTK_MOVEMENT_DISPLAY_LINES,
                             -mv->count,
                             mv->extend_selection);
      return;
    }

  IDE_TRACE_MSG ("previous-line with line-selection");

  g_assert (mv->extend_selection);
  g_assert (is_line_selection (&mv->insert, &mv->selection));

  if (gtk_text_iter_is_start (&mv->insert) || gtk_text_iter_is_start (&mv->selection))
    return;

  /*
   * if the current line is selected
   */
  if (is_single_line_selection (&mv->insert, &mv->selection))
    {
      gtk_text_iter_order (&mv->insert, &mv->selection);
      gtk_text_iter_backward_line (&mv->insert);
      gtk_text_iter_set_line_offset (&mv->insert, 0);
    }
  else
    {
      gtk_text_iter_backward_line (&mv->insert);
      if (!gtk_text_iter_ends_line (&mv->insert))
        gtk_text_iter_forward_to_line_end (&mv->insert);
    }
}

static void
ide_source_view_movements_screen_top (Movement *mv)
{
  GtkTextView *text_view = (GtkTextView *)mv->self;
  GdkRectangle rect;

  gtk_text_view_get_visible_rect (text_view, &rect);
  gtk_text_view_get_iter_at_location (text_view, &mv->insert, rect.x, rect.y);
  gtk_text_iter_set_line_offset (&mv->insert, 0);
}

static void
ide_source_view_movements_screen_middle (Movement *mv)
{
  GtkTextView *text_view = (GtkTextView *)mv->self;
  GdkRectangle rect;

  gtk_text_view_get_visible_rect (text_view, &rect);
  gtk_text_view_get_iter_at_location (text_view, &mv->insert, rect.x, rect.y + (rect.height/2));
  gtk_text_iter_set_line_offset (&mv->insert, 0);
}

static void
ide_source_view_movements_screen_bottom (Movement *mv)
{
  GtkTextView *text_view = (GtkTextView *)mv->self;
  GdkRectangle rect;

  gtk_text_view_get_visible_rect (text_view, &rect);
  gtk_text_view_get_iter_at_location (text_view, &mv->insert, rect.x, rect.y + rect.height);
  gtk_text_iter_set_line_offset (&mv->insert, 0);
}

static void
ide_source_view_movements_scroll_by_lines (Movement *mv,
                                           gint      lines)
{
  GtkTextView *text_view = (GtkTextView *)mv->self;
  GtkAdjustment *vadj;
  GtkTextBuffer *buffer;
  GtkTextIter begin;
  GtkTextIter end;
  GdkRectangle rect;
  gdouble amount;
  gdouble value;
  gdouble upper;

  if (lines == 0)
    return;

  vadj = gtk_scrollable_get_vadjustment (GTK_SCROLLABLE (mv->self));
  buffer = gtk_text_view_get_buffer (text_view);

  gtk_text_buffer_get_bounds (buffer, &begin, &end);

  if (lines > 0)
    {
      if (gtk_text_iter_get_line (&end) == gtk_text_iter_get_line (&mv->insert))
        return;
    }
  else if (lines < 0)
    {
      if (gtk_text_iter_get_line (&begin) == gtk_text_iter_get_line (&mv->insert))
        return;
    }
  else
    g_assert_not_reached ();

  gtk_text_view_get_iter_location (text_view, &mv->insert, &rect);

  amount = lines * rect.height;

  value = gtk_adjustment_get_value (vadj);
  upper = gtk_adjustment_get_upper (vadj);
  gtk_adjustment_set_value (vadj, CLAMP (value + amount, 0, upper));
}

static void
ide_source_view_movements_scroll (Movement *mv)
{
  GtkTextView *text_view = (GtkTextView *)mv->self;
  GtkTextBuffer *buffer;
  GtkTextMark *mark;
  gint count = MAX (1, mv->count);

  if (mv->type == IDE_SOURCE_VIEW_MOVEMENT_SCREEN_DOWN)
    count = -count;

  ide_source_view_movements_scroll_by_lines (mv, count);

  buffer = gtk_text_view_get_buffer (GTK_TEXT_VIEW (mv->self));
  mark = gtk_text_buffer_get_insert (buffer);
  gtk_text_view_move_mark_onscreen (text_view, mark);
  gtk_text_buffer_get_iter_at_mark (buffer, &mv->insert, mark);
}

static void
ide_source_view_movements_move_page (Movement *mv)
{
  GtkTextView *text_view = (GtkTextView *)mv->self;
  GtkTextBuffer *buffer;
  GdkRectangle rect;
  GtkTextIter iter_top;
  GtkTextIter iter_bottom;
  GtkTextIter iter_current;
  gint half_page;
  gint line_top;
  gint line_bottom;

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

  switch ((int)mv->type)
    {
    case IDE_SOURCE_VIEW_MOVEMENT_HALF_PAGE_UP:
      ide_source_view_movements_scroll_by_lines (mv, -half_page);
      gtk_text_iter_backward_lines (&mv->insert, half_page);
      break;

    case IDE_SOURCE_VIEW_MOVEMENT_HALF_PAGE_DOWN:
      ide_source_view_movements_scroll_by_lines (mv, half_page);
      gtk_text_iter_forward_lines (&mv->insert, half_page);
      break;

    case IDE_SOURCE_VIEW_MOVEMENT_PAGE_UP:
      gtk_text_buffer_get_iter_at_line (buffer, &mv->insert, MAX (0, line_top - 1));
      ide_source_view_movements_select_range (mv);
      gtk_text_view_scroll_to_iter (text_view, &mv->insert, .0, TRUE, .0, 1.0);
      mv->ignore_select = TRUE;
      break;

    case IDE_SOURCE_VIEW_MOVEMENT_PAGE_DOWN:
      gtk_text_buffer_get_iter_at_line (buffer, &mv->insert, line_bottom + 1);
      ide_source_view_movements_select_range (mv);
      gtk_text_view_scroll_to_iter (text_view, &mv->insert, .0, TRUE, .0, .0);
      mv->ignore_select = TRUE;
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
ide_source_view_movements_match_special (Movement *mv)
{
  MatchingBracketState state;
  GtkTextIter copy;
  gboolean is_forward = FALSE;
  gboolean ret;

  copy = mv->insert;

  state.depth = 1;
  state.jump_from = gtk_text_iter_get_char (&mv->insert);

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
    ret = gtk_text_iter_forward_find_char (&mv->insert, bracket_predicate, &state, NULL);
  else
    ret = gtk_text_iter_backward_find_char (&mv->insert, bracket_predicate, &state, NULL);

  if (!ret)
    mv->insert = copy;
  else if (!mv->exclusive)
    gtk_text_iter_forward_char (&mv->insert);
}

static void
ide_source_view_movements_scroll_center (Movement *mv)
{
  GtkTextView *text_view = (GtkTextView *)mv->self;

  switch ((int)mv->type)
    {
    case IDE_SOURCE_VIEW_MOVEMENT_SCROLL_SCREEN_BOTTOM:
      gtk_text_view_scroll_to_iter (text_view, &mv->insert, 0.0, TRUE, 1.0, 1.0);
      break;

    case IDE_SOURCE_VIEW_MOVEMENT_SCROLL_SCREEN_TOP:
      gtk_text_view_scroll_to_iter (text_view, &mv->insert, 0.0, TRUE, 1.0, 0.0);
      break;

    case IDE_SOURCE_VIEW_MOVEMENT_SCROLL_SCREEN_CENTER:
      gtk_text_view_scroll_to_iter (text_view, &mv->insert, 0.0, TRUE, 1.0, 0.5);
      break;

    default:
      break;
    }
}

static void
ide_source_view_movements_next_word_end (Movement *mv)
{
  GtkTextIter copy;

  copy = mv->insert;

  _ide_vim_iter_forward_word_end (&mv->insert);

  /* prefer an empty line before word */
  text_iter_forward_to_empty_line (&copy, &mv->insert);
  if (gtk_text_iter_compare (&copy, &mv->insert) < 0)
    mv->insert = copy;
  else if (!mv->exclusive && !gtk_text_iter_ends_line (&mv->insert))
    gtk_text_iter_forward_char (&mv->insert);
}

static void
ide_source_view_movements_next_full_word_end (Movement *mv)
{
  GtkTextIter copy;

  copy = mv->insert;

  _ide_vim_iter_forward_WORD_end (&mv->insert);

  /* prefer an empty line before word */
  text_iter_forward_to_empty_line (&copy, &mv->insert);
  if (gtk_text_iter_compare (&copy, &mv->insert) < 0)
    mv->insert = copy;
}

static void
ide_source_view_movements_next_word_start (Movement *mv)
{
  GtkTextIter copy;

  copy = mv->insert;

  _ide_vim_iter_forward_word_start (&mv->insert);

  /* prefer an empty line before word */
  text_iter_forward_to_empty_line (&copy, &mv->insert);
  if (gtk_text_iter_compare (&copy, &mv->insert) < 0)
    mv->insert = copy;
}

static void
ide_source_view_movements_next_full_word_start (Movement *mv)
{
  GtkTextIter copy;

  copy = mv->insert;

  _ide_vim_iter_forward_WORD_start (&mv->insert);

  /* prefer an empty line before word */
  text_iter_forward_to_empty_line (&copy, &mv->insert);
  if (gtk_text_iter_compare (&copy, &mv->insert) < 0)
    mv->insert = copy;
}

static void
ide_source_view_movements_previous_word_start (Movement *mv)
{
  GtkTextIter copy;

  copy = mv->insert;

  _ide_source_iter_backward_visible_word_start (&mv->insert);

  /*
   * Vim treats an empty line as a word.
   */
  if (gtk_text_iter_backward_char (&copy))
    if (gtk_text_iter_get_char (&copy) == '\n')
      mv->insert = copy;
}

static void
ide_source_view_movements_previous_full_word_start (Movement *mv)
{
  GtkTextIter copy;

  copy = mv->insert;

  _ide_source_iter_backward_full_word_start (&mv->insert);

  /*
   * Vim treats an empty line as a word.
   */
  if (gtk_text_iter_backward_char (&copy))
    if (gtk_text_iter_get_char (&copy) == '\n')
      mv->insert = copy;
}

static void
ide_source_view_movements_previous_word_end (Movement *mv)
{
  GtkTextIter copy;

  copy = mv->insert;

  _ide_source_iter_backward_visible_word_starts (&mv->insert, 2);
  _ide_source_iter_forward_visible_word_end (&mv->insert);

  /*
   * Vim treats an empty line as a word.
   */
  if (gtk_text_iter_backward_char (&copy))
    if (gtk_text_iter_get_char (&copy) == '\n')
      mv->insert = copy;

  /*
   * Ensure we are strictly before our previous position.
   */
  if (gtk_text_iter_compare (&mv->insert, &copy) > 0)
    gtk_text_buffer_get_start_iter (gtk_text_iter_get_buffer (&mv->insert), &mv->insert);
}

static void
ide_source_view_movements_previous_full_word_end (Movement *mv)
{
  if (!_ide_source_iter_starts_full_word (&mv->insert))
    _ide_source_iter_backward_full_word_start (&mv->insert);
  _ide_source_iter_backward_full_word_start (&mv->insert);
  _ide_source_iter_forward_full_word_end (&mv->insert);
}

static void
ide_source_view_movements_paragraph_start (Movement *mv)
{
  _ide_vim_iter_backward_paragraph_start (&mv->insert);
}

static void
ide_source_view_movements_paragraph_end (Movement *mv)
{

  _ide_vim_iter_forward_paragraph_end (&mv->insert);
}

static void
ide_source_view_movements_sentence_start (Movement *mv)
{
  _ide_vim_iter_backward_sentence_start (&mv->insert);
}

static void
ide_source_view_movements_sentence_end (Movement *mv)
{
  _ide_vim_iter_forward_sentence_end (&mv->insert);
}

void
_ide_source_view_apply_movement (IdeSourceView         *self,
                                 IdeSourceViewMovement  movement,
                                 gboolean               extend_selection,
                                 gboolean               exclusive,
                                 guint                  count)
{
  Movement mv = { 0 };

  g_return_if_fail (IDE_IS_SOURCE_VIEW (self));

#ifndef IDE_DISABLE_TRACE
  {
    GEnumValue *enum_value;
    GEnumClass *enum_class;

    enum_class = g_type_class_peek (IDE_TYPE_SOURCE_VIEW_MOVEMENT);
    enum_value = g_enum_get_value (enum_class, movement);
    IDE_TRACE_MSG ("movement(%s, extend_selection=%s, exclusive=%s, count=%u)",
                   enum_value->value_nick,
                   extend_selection ? "YES" : "NO",
                   exclusive ? "YES" : "NO",
                   count);
  }
#endif

  mv.self = self;
  mv.type = movement;
  mv.extend_selection = extend_selection;
  mv.exclusive = exclusive;
  mv.count = count;
  mv.ignore_select = FALSE;

  ide_source_view_movements_get_selection (&mv);

  switch (movement)
    {
    case IDE_SOURCE_VIEW_MOVEMENT_NTH_CHAR:
      ide_source_view_movements_nth_char (&mv);
      break;

    case IDE_SOURCE_VIEW_MOVEMENT_PREVIOUS_CHAR:
      ide_source_view_movements_previous_char (&mv);
      break;

    case IDE_SOURCE_VIEW_MOVEMENT_NEXT_CHAR:
      ide_source_view_movements_next_char (&mv);
      break;

    case IDE_SOURCE_VIEW_MOVEMENT_FIRST_CHAR:
      ide_source_view_movements_first_char (&mv);
      break;

    case IDE_SOURCE_VIEW_MOVEMENT_FIRST_NONSPACE_CHAR:
      ide_source_view_movements_first_nonspace_char (&mv);
      break;

    case IDE_SOURCE_VIEW_MOVEMENT_MIDDLE_CHAR:
      ide_source_view_movements_middle_char (&mv);
      break;

    case IDE_SOURCE_VIEW_MOVEMENT_LAST_CHAR:
      ide_source_view_movements_last_char (&mv);
      break;

    case IDE_SOURCE_VIEW_MOVEMENT_PREVIOUS_FULL_WORD_START:
      ide_source_view_movements_previous_full_word_start (&mv);
      break;

    case IDE_SOURCE_VIEW_MOVEMENT_NEXT_FULL_WORD_START:
      ide_source_view_movements_next_full_word_start (&mv);
      break;

    case IDE_SOURCE_VIEW_MOVEMENT_PREVIOUS_FULL_WORD_END:
      ide_source_view_movements_previous_full_word_end (&mv);
      break;

    case IDE_SOURCE_VIEW_MOVEMENT_NEXT_FULL_WORD_END:
      ide_source_view_movements_next_full_word_end (&mv);
      break;

    case IDE_SOURCE_VIEW_MOVEMENT_PREVIOUS_WORD_START:
      ide_source_view_movements_previous_word_start (&mv);
      break;

    case IDE_SOURCE_VIEW_MOVEMENT_NEXT_WORD_START:
      ide_source_view_movements_next_word_start (&mv);
      break;

    case IDE_SOURCE_VIEW_MOVEMENT_PREVIOUS_WORD_END:
      ide_source_view_movements_previous_word_end (&mv);
      break;

    case IDE_SOURCE_VIEW_MOVEMENT_NEXT_WORD_END:
      ide_source_view_movements_next_word_end (&mv);
      break;

    case IDE_SOURCE_VIEW_MOVEMENT_SENTENCE_START:
      ide_source_view_movements_sentence_start (&mv);
      break;

    case IDE_SOURCE_VIEW_MOVEMENT_SENTENCE_END:
      ide_source_view_movements_sentence_end (&mv);
      break;

    case IDE_SOURCE_VIEW_MOVEMENT_PARAGRAPH_START:
      ide_source_view_movements_paragraph_start (&mv);
      break;

    case IDE_SOURCE_VIEW_MOVEMENT_PARAGRAPH_END:
      ide_source_view_movements_paragraph_end (&mv);
      break;

    case IDE_SOURCE_VIEW_MOVEMENT_PREVIOUS_LINE:
      ide_source_view_movements_previous_line (&mv);
      break;

    case IDE_SOURCE_VIEW_MOVEMENT_NEXT_LINE:
      ide_source_view_movements_next_line (&mv);
      break;

    case IDE_SOURCE_VIEW_MOVEMENT_FIRST_LINE:
      ide_source_view_movements_first_line (&mv);
      break;

    case IDE_SOURCE_VIEW_MOVEMENT_NTH_LINE:
      ide_source_view_movements_nth_line (&mv);
      break;

    case IDE_SOURCE_VIEW_MOVEMENT_LAST_LINE:
      ide_source_view_movements_last_line (&mv);
      break;

    case IDE_SOURCE_VIEW_MOVEMENT_LINE_PERCENTAGE:
      break;

    case IDE_SOURCE_VIEW_MOVEMENT_LINE_CHARS:
      ide_source_view_movements_line_chars (&mv);
      break;

    case IDE_SOURCE_VIEW_MOVEMENT_LINE_END:
      //ide_source_view_movements_line_end (&mv);
      break;

    case IDE_SOURCE_VIEW_MOVEMENT_HALF_PAGE_UP:
    case IDE_SOURCE_VIEW_MOVEMENT_HALF_PAGE_DOWN:
    case IDE_SOURCE_VIEW_MOVEMENT_PAGE_UP:
    case IDE_SOURCE_VIEW_MOVEMENT_PAGE_DOWN:
      ide_source_view_movements_move_page (&mv);
      break;

    case IDE_SOURCE_VIEW_MOVEMENT_SCREEN_DOWN:
    case IDE_SOURCE_VIEW_MOVEMENT_SCREEN_UP:
      ide_source_view_movements_scroll (&mv);
      break;

    case IDE_SOURCE_VIEW_MOVEMENT_SCREEN_TOP:
      ide_source_view_movements_screen_top (&mv);
      break;

    case IDE_SOURCE_VIEW_MOVEMENT_SCREEN_MIDDLE:
      ide_source_view_movements_screen_middle (&mv);
      break;

    case IDE_SOURCE_VIEW_MOVEMENT_SCREEN_BOTTOM:
      ide_source_view_movements_screen_bottom (&mv);
      break;

    case IDE_SOURCE_VIEW_MOVEMENT_MATCH_SPECIAL:
      ide_source_view_movements_match_special (&mv);
      break;

    case IDE_SOURCE_VIEW_MOVEMENT_SCROLL_SCREEN_TOP:
    case IDE_SOURCE_VIEW_MOVEMENT_SCROLL_SCREEN_CENTER:
    case IDE_SOURCE_VIEW_MOVEMENT_SCROLL_SCREEN_BOTTOM:
      ide_source_view_movements_scroll_center (&mv);
      break;

    default:
      g_return_if_reached ();
    }

  if (!mv.ignore_select)
    ide_source_view_movements_select_range (&mv);
}
