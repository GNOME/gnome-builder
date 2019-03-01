/* ide-source-view-movements.c
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

#define G_LOG_DOMAIN "ide-source-view-movements"

#include "config.h"

#include <dazzle.h>
#include <string.h>

#include <libide-code.h>

#include "ide-source-view-enums.h"
#include "ide-source-view-movements.h"
#include "ide-source-view-private.h"

#define ANCHOR_BEGIN   "SELECTION_ANCHOR_BEGIN"
#define ANCHOR_END     "SELECTION_ANCHOR_END"
#define JUMP_THRESHOLD 20

#define TRACE_ITER(iter) \
  IDE_TRACE_MSG("%d:%d", gtk_text_iter_get_line(iter), \
                gtk_text_iter_get_line_offset(iter))

typedef struct
{
  IdeSourceView         *self;
  /* The target_column contains the ideal character column (visual offset).
   * This can sometimes be further forward than designed when the line does not
   * have enough characters to get back to the original position.
   */
  guint                 *target_column;
  IdeSourceViewMovement  type;                        /* Type of movement */
  IdeSourceScrollAlign   scroll_align;                /* How to align the post-movement scroll */
  GtkTextIter            insert;                      /* Current insert cursor location */
  GtkTextIter            selection;                   /* Current selection cursor location */
  gint                   count;                       /* Repeat count for movement */
  GString               *command_str;                 /* Current command string */
  gunichar               command;                     /* Command that trigger some movements type. See , and ; in vim */
  gunichar               modifier;                    /* For forward/backward char search */
  gunichar               search_char;                 /* For forward/backward char search according to fFtT vim modifier */
  guint                  newline_stop : 1;            /* Stop the movement at newline chararacter
                                                       * currently used by [previous|next]_[word|full_word] functions
                                                       */
  guint                  extend_selection : 1;        /* If selection should be extended */
  guint                  exclusive : 1;               /* See ":help exclusive" in vim */
  guint                  ignore_select : 1;           /* Don't update selection after movement */
  guint                  ignore_target_column : 1;    /* Don't propagate new line column */
  guint                  ignore_scroll_to_insert : 1; /* Don't scroll to insert mark */
} Movement;

typedef struct
{
  gunichar         jump_to;
  gunichar         jump_from;
  GtkDirectionType direction;
  guint            depth;
  gboolean         string_mode;
} MatchingBracketState;

typedef enum
{
  HTML_TAG_KIND_ERROR,
  HTML_TAG_KIND_OPEN,
  HTML_TAG_KIND_CLOSE,
  HTML_TAG_KIND_EMPTY,
  HTML_TAG_KIND_STRAY_END,
  HTML_TAG_KIND_COMMENT
} HtmlTagKind;

typedef struct
{
  GtkTextIter  begin;
  GtkTextIter  end;
  gchar       *name;
  HtmlTagKind  kind;
} HtmlTag;

typedef struct
{
  HtmlTag *left;
  HtmlTag *right;
} HtmlElement;

typedef enum
{
  MACRO_COND_NONE,
  MACRO_COND_IF,
  MACRO_COND_IFDEF,
  MACRO_COND_IFNDEF,
  MACRO_COND_ELIF,
  MACRO_COND_ELSE,
  MACRO_COND_ENDIF
} MacroCond;

static gboolean
is_single_line_selection (const GtkTextIter *begin,
                          const GtkTextIter *end)
{
  if (gtk_text_iter_compare (begin, end) < 0)
    return ((gtk_text_iter_get_line_offset (begin) == 0) &&
            (gtk_text_iter_get_line_offset (end) == 0) &&
            ((gtk_text_iter_get_line (begin) + 1) ==
             gtk_text_iter_get_line (end)));
  else
    return ((gtk_text_iter_get_line_offset (begin) == 0) &&
            (gtk_text_iter_get_line_offset (end) == 0) &&
            ((gtk_text_iter_get_line (end) + 1) ==
             gtk_text_iter_get_line (begin)));
}

static gboolean
is_single_char_selection (const GtkTextIter *begin,
                          const GtkTextIter *end)
{
  GtkTextIter tmp;

  g_assert (begin);
  g_assert (end);

  tmp = *begin;
  if (gtk_text_iter_forward_char (&tmp) && gtk_text_iter_equal (&tmp, end))
    return TRUE;

  tmp = *end;
  if (gtk_text_iter_forward_char (&tmp) && gtk_text_iter_equal (&tmp, begin))
    return TRUE;

  return FALSE;
}

static gboolean
text_iter_forward_to_nonspace_captive (GtkTextIter *iter)
{
  while (!gtk_text_iter_ends_line (iter) && g_unichar_isspace (gtk_text_iter_get_char (iter)))
    if (!gtk_text_iter_forward_char (iter))
      return FALSE;

  return !g_unichar_isspace (gtk_text_iter_get_char (iter));
}

static void
select_range (Movement    *mv,
              GtkTextIter *insert_iter,
              GtkTextIter *selection_iter)
{
  GtkTextBuffer *buffer;
  GtkTextMark *insert;
  GtkTextMark *selection;
  gint insert_off;
  gint selection_off;

  g_assert (insert_iter);
  g_assert (selection_iter);

  buffer = gtk_text_view_get_buffer (GTK_TEXT_VIEW (mv->self));
  insert = gtk_text_buffer_get_insert (buffer);
  selection = gtk_text_buffer_get_selection_bound (buffer);

  mv->ignore_select = TRUE;

  /*
   * If the caller is requesting that we select a single character, we will
   * keep the iter before that character. This more closely matches the visual
   * mode in VIM.
   */
  insert_off = gtk_text_iter_get_offset (insert_iter);
  selection_off = gtk_text_iter_get_offset (selection_iter);
  if ((insert_off - selection_off) == 1)
    gtk_text_iter_order (insert_iter, selection_iter);

  gtk_text_buffer_move_mark (buffer, insert, insert_iter);
  gtk_text_buffer_move_mark (buffer, selection, selection_iter);
}

static void
ensure_anchor_selected (Movement *mv)
{
  GtkTextBuffer *buffer;
  GtkTextMark *selection_mark;
  GtkTextMark *insert_mark;
  GtkTextIter anchor_begin;
  GtkTextIter anchor_end;
  GtkTextIter insert_iter;
  GtkTextIter selection_iter;
  GtkTextMark *selection_anchor_begin;
  GtkTextMark *selection_anchor_end;

  buffer = gtk_text_view_get_buffer (GTK_TEXT_VIEW (mv->self));

  selection_anchor_begin = gtk_text_buffer_get_mark (buffer, ANCHOR_BEGIN);
  selection_anchor_end = gtk_text_buffer_get_mark (buffer, ANCHOR_END);

  if (!selection_anchor_begin || !selection_anchor_end)
    return;

  gtk_text_buffer_get_iter_at_mark (buffer, &anchor_begin, selection_anchor_begin);
  gtk_text_buffer_get_iter_at_mark (buffer, &anchor_end, selection_anchor_end);

  insert_mark = gtk_text_buffer_get_insert (buffer);
  gtk_text_buffer_get_iter_at_mark (buffer, &insert_iter, insert_mark);

  selection_mark = gtk_text_buffer_get_selection_bound (buffer);
  gtk_text_buffer_get_iter_at_mark (buffer, &selection_iter, selection_mark);

  if ((gtk_text_iter_compare (&selection_iter, &anchor_end) < 0) &&
      (gtk_text_iter_compare (&insert_iter, &anchor_end) < 0))
    {
      if (gtk_text_iter_compare (&insert_iter, &selection_iter) < 0)
        select_range (mv, &insert_iter, &anchor_end);
      else
        select_range (mv, &anchor_end, &selection_iter);
    }
  else if ((gtk_text_iter_compare (&selection_iter, &anchor_begin) > 0) &&
           (gtk_text_iter_compare (&insert_iter, &anchor_begin) > 0))
    {
      if (gtk_text_iter_compare (&insert_iter, &selection_iter) < 0)
        select_range (mv, &anchor_begin, &selection_iter);
      else
        select_range (mv, &insert_iter, &anchor_begin);
    }
}

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

  if (!mv->exclusive && !gtk_text_iter_ends_line (&mv->insert))
    gtk_text_iter_forward_char (&mv->insert);
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

  if (gtk_text_iter_get_line_offset (&mv->insert) != 0)
    gtk_text_iter_set_line_offset (&mv->insert, 0);

  while (!gtk_text_iter_ends_line (&mv->insert) &&
         (ch = gtk_text_iter_get_char (&mv->insert)) &&
         g_unichar_isspace (ch))
    gtk_text_iter_forward_char (&mv->insert);

  if (!mv->exclusive && !gtk_text_iter_ends_line (&mv->insert))
    gtk_text_iter_forward_char (&mv->insert);
}

static void
ide_source_view_movements_line_chars (Movement *mv)
{
  GtkTextIter orig = mv->insert;

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

      if (gtk_text_iter_ends_line (&mv->insert) ||
          (gtk_text_iter_compare (&orig, &mv->insert) <= 0))
        gtk_text_iter_set_line_offset (&mv->insert, 0);
    }

  if (!mv->exclusive)
    gtk_text_iter_forward_char (&mv->insert);
}

static void
ide_source_view_movements_line_end (Movement *mv)
{
  if (!gtk_text_iter_ends_line (&mv->insert))
    gtk_text_iter_forward_to_line_end (&mv->insert);

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
  gtk_text_iter_set_line (&mv->insert, 0);
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

  while (!gtk_text_iter_ends_line (&mv->insert) &&
         g_unichar_isspace (gtk_text_iter_get_char (&mv->insert)))
    if (!gtk_text_iter_forward_char (&mv->insert))
      break;
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

static gboolean
ide_source_view_movements_next_line (Movement *mv)
{
  GtkTextIter prev_insert = mv->insert;
  GtkTextIter prev_selection = mv->selection;
  GtkTextBuffer *buffer;
  gboolean has_selection;
  guint line;
  guint column = *mv->target_column;

  buffer = gtk_text_view_get_buffer (GTK_TEXT_VIEW (mv->self));

  /* check for linewise */
  has_selection = !gtk_text_iter_equal (&mv->insert, &mv->selection) || !mv->exclusive;

  line = gtk_text_iter_get_line (&mv->insert);

  /*
   * If we have a whole line selected (from say `V`), then we need to swap
   * the cursor and selection. This feels to me like a slight bit of a hack.
   * There may be cause to actually have a selection mode and know the type
   * of selection (line vs individual characters).
   */
  if (is_single_line_selection (&mv->insert, &mv->selection))
    {
      guint target_line;

      if (gtk_text_iter_compare (&mv->insert, &mv->selection) < 0)
        gtk_text_iter_order (&mv->selection, &mv->insert);

      target_line = gtk_text_iter_get_line (&mv->insert) + 1;
      gtk_text_iter_set_line (&mv->insert, target_line);

      if (target_line != (guint)gtk_text_iter_get_line (&mv->insert))
        {
          gtk_text_buffer_get_end_iter (buffer, &mv->insert);
          goto select_to_end;
        }

      select_range (mv, &mv->insert, &mv->selection);
      ensure_anchor_selected (mv);
      return TRUE;
    }

  if (is_single_char_selection (&mv->insert, &mv->selection))
    {
      if (gtk_text_iter_compare (&mv->insert, &mv->selection) < 0)
        *mv->target_column = ++column;
    }

  gtk_text_buffer_get_iter_at_line (buffer, &mv->insert, line + 1);
  if (gtk_text_iter_get_line (&mv->insert) == line + 1)
    ide_source_view_get_iter_at_visual_column (mv->self, *mv->target_column, &mv->insert);
  else
    gtk_text_buffer_get_end_iter (buffer, &mv->insert);

select_to_end:

  if (has_selection)
    {
      select_range (mv, &mv->insert, &mv->selection);
      ensure_anchor_selected (mv);
    }
  else
    gtk_text_buffer_select_range (buffer, &mv->insert, &mv->insert);

  /* make sure selection/insert are up to date */
  if (!gtk_text_buffer_get_has_selection (buffer))
    mv->selection = mv->insert;

  return (!gtk_text_iter_equal (&prev_selection, &mv->selection) ||
          !gtk_text_iter_equal (&prev_insert, &mv->insert));
}

static gboolean
ide_source_view_movements_previous_line (Movement *mv)
{
  GtkTextIter prev_insert = mv->insert;
  GtkTextIter prev_selection = mv->selection;
  GtkTextBuffer *buffer;
  gboolean has_selection;
  guint line;
  guint column = 0;

  buffer = gtk_text_view_get_buffer (GTK_TEXT_VIEW (mv->self));

  /* check for linewise */
  has_selection = !gtk_text_iter_equal (&mv->insert, &mv->selection) || !mv->exclusive;

  line = gtk_text_iter_get_line (&mv->insert);

  if ((*mv->target_column) > 0)
    column = *mv->target_column;

  if (line == 0)
    return FALSE;

  /*
   * If we have a whole line selected (from say `V`), then we need to swap the cursor and
   * selection. This feels to me like a slight bit of a hack.  There may be cause to actually have
   * a selection mode and know the type of selection (line vs individual characters).
   */
  if (is_single_line_selection (&mv->insert, &mv->selection))
    {
      if (gtk_text_iter_compare (&mv->insert, &mv->selection) > 0)
        gtk_text_iter_order (&mv->insert, &mv->selection);
      gtk_text_iter_set_line (&mv->insert, gtk_text_iter_get_line (&mv->insert) - 1);
      select_range (mv, &mv->insert, &mv->selection);
      ensure_anchor_selected (mv);
      return TRUE;
    }

  if (is_single_char_selection (&mv->insert, &mv->selection))
    {
      if (gtk_text_iter_compare (&mv->insert, &mv->selection) > 0)
        {
          if (column)
            --column;
          *mv->target_column = column;
        }
    }

  gtk_text_buffer_get_iter_at_line (buffer, &mv->insert, line - 1);
  if (line == ((guint)gtk_text_iter_get_line (&mv->insert) + 1))
    {
      ide_source_view_get_iter_at_visual_column (mv->self, column, &mv->insert);

      if (has_selection)
        {
          if (gtk_text_iter_equal (&mv->insert, &mv->selection))
            gtk_text_iter_backward_char (&mv->insert);
          select_range (mv, &mv->insert, &mv->selection);
          ensure_anchor_selected (mv);
        }
      else
        gtk_text_buffer_select_range (buffer, &mv->insert, &mv->insert);
    }

  /* make sure selection/insert are up to date */
  if (!gtk_text_buffer_get_has_selection (buffer))
    mv->selection = mv->insert;

  return (!gtk_text_iter_equal (&prev_selection, &mv->selection) ||
          !gtk_text_iter_equal (&prev_insert, &mv->insert));
}

static void
ide_source_view_movements_screen_top (Movement *mv)
{
  GtkTextView *text_view = (GtkTextView *)mv->self;
  GdkRectangle rect;

  ide_source_view_get_visible_rect (mv->self, &rect);
  gtk_text_view_get_iter_at_location (text_view, &mv->insert, rect.x, rect.y);
  gtk_text_iter_set_line_offset (&mv->insert, 0);

  mv->ignore_scroll_to_insert = TRUE;
}

static void
ide_source_view_movements_screen_middle (Movement *mv)
{
  GtkTextView *text_view = (GtkTextView *)mv->self;
  GdkRectangle rect;

  ide_source_view_get_visible_rect (mv->self, &rect);
  gtk_text_view_get_iter_at_location (text_view, &mv->insert, rect.x, rect.y + (rect.height / 2));
  gtk_text_iter_set_line_offset (&mv->insert, 0);

  mv->ignore_scroll_to_insert = TRUE;
}

static void
ide_source_view_movements_screen_bottom (Movement *mv)
{
  GtkTextView *text_view = (GtkTextView *)mv->self;
  GdkRectangle rect;

  ide_source_view_get_visible_rect (mv->self, &rect);
  gtk_text_view_get_iter_at_location (text_view, &mv->insert, rect.x, rect.y + rect.height - 1);
  gtk_text_iter_set_line_offset (&mv->insert, 0);

  mv->ignore_scroll_to_insert = TRUE;
}

static void
ide_source_view_movements_scroll_by_chars (Movement *mv,
                                           gint      chars)
{
  GtkTextView *text_view = (GtkTextView *)mv->self;
  GtkAdjustment *hadj;
  GdkRectangle rect;
  gdouble amount;
  gdouble value;
  gdouble new_value;
  gdouble upper;
  gdouble page_size;

  if (chars == 0)
    return;

  hadj = gtk_scrollable_get_hadjustment (GTK_SCROLLABLE (mv->self));

  gtk_text_view_get_iter_location (text_view, &mv->insert, &rect);

  amount = chars * rect.width;

  value = gtk_adjustment_get_value (hadj);
  upper = gtk_adjustment_get_upper (hadj);
  page_size = gtk_adjustment_get_page_size (hadj);

  if (chars < 0 && value <= 0)
    return;
  else if (chars > 0 && value >= upper - page_size)
    return;

  new_value = CLAMP (value + amount, 0, upper - page_size);
  if (new_value == value)
    return;

  gtk_adjustment_set_value (hadj, new_value);

  if (chars > 0 && (rect.x < (gint)new_value))
    gtk_text_view_get_iter_at_location (text_view, &mv->insert, new_value, rect.y);
  else if (dzl_cairo_rectangle_x2 (&rect) > (gint)(new_value + page_size))
    gtk_text_view_get_iter_at_location (text_view, &mv->insert, new_value + page_size - rect.width, rect.y);
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

  ide_source_view_place_cursor_onscreen (mv->self);
}

static void
ide_source_view_movements_scroll (Movement *mv)
{
  GtkTextBuffer *buffer;
  GtkTextMark *mark;
  gint count = MAX (1, mv->count);

  if (mv->type == IDE_SOURCE_VIEW_MOVEMENT_SCREEN_DOWN ||
      mv->type == IDE_SOURCE_VIEW_MOVEMENT_SCREEN_LEFT)
    count = -count;

  if (mv->type == IDE_SOURCE_VIEW_MOVEMENT_SCREEN_DOWN ||
      mv->type == IDE_SOURCE_VIEW_MOVEMENT_SCREEN_UP)
    {
      ide_source_view_movements_scroll_by_lines (mv, count);
      buffer = gtk_text_view_get_buffer (GTK_TEXT_VIEW (mv->self));
      mark = gtk_text_buffer_get_insert (buffer);
      gtk_text_buffer_get_iter_at_mark (buffer, &mv->insert, mark);
    }
  else
    ide_source_view_movements_scroll_by_chars (mv, count);

  mv->ignore_scroll_to_insert = TRUE;
}

static void
ide_source_view_movements_move_page (Movement *mv)
{
  GtkTextView *text_view = (GtkTextView *)mv->self;
  GtkTextBuffer *buffer;
  GtkAdjustment *hadj;
  GtkTextMark *mark;
  GdkRectangle rect;
  GtkTextIter iter_top;
  GtkTextIter iter_bottom;
  GtkTextIter scroll_iter;
  gint scrolloff;
  gint half_page_vertical;
  gint half_page_horizontal;
  gint line_top;
  gint line_bottom;

  gtk_text_view_get_visible_rect (text_view, &rect);
  gtk_text_view_get_iter_at_location (text_view, &iter_top, rect.x, rect.y);
  gtk_text_view_get_iter_at_location (text_view, &iter_bottom,
                                      rect.x + rect.width,
                                      rect.y + rect.height);

  buffer = gtk_text_view_get_buffer (text_view);

  line_top = gtk_text_iter_get_line (&iter_top);
  line_bottom = gtk_text_iter_get_line (&iter_bottom);

  half_page_vertical = MAX (1, (line_bottom - line_top) / 2);
  scrolloff = MIN (ide_source_view_get_scroll_offset (mv->self), (guint)half_page_vertical);

  hadj = gtk_scrollable_get_hadjustment (GTK_SCROLLABLE (mv->self));
  gtk_text_view_get_iter_location (text_view, &mv->insert, &rect);

  half_page_horizontal = gtk_adjustment_get_page_size (hadj) / (rect.width * 2.0);

  switch ((int)mv->type)
    {
    case IDE_SOURCE_VIEW_MOVEMENT_HALF_PAGE_UP:
      ide_source_view_movements_scroll_by_lines (mv, -half_page_vertical);
      gtk_text_iter_backward_lines (&mv->insert, half_page_vertical);
      break;

    case IDE_SOURCE_VIEW_MOVEMENT_HALF_PAGE_DOWN:
      ide_source_view_movements_scroll_by_lines (mv, half_page_vertical);
      gtk_text_iter_forward_lines (&mv->insert, half_page_vertical);
      break;

    case IDE_SOURCE_VIEW_MOVEMENT_HALF_PAGE_LEFT:
      ide_source_view_movements_scroll_by_chars (mv, -half_page_horizontal);
      break;

    case IDE_SOURCE_VIEW_MOVEMENT_HALF_PAGE_RIGHT:
      ide_source_view_movements_scroll_by_chars (mv, half_page_horizontal);
      break;

    case IDE_SOURCE_VIEW_MOVEMENT_PAGE_UP:
      gtk_text_buffer_get_iter_at_line (buffer, &mv->insert, MAX (0, line_top - scrolloff));
      text_iter_forward_to_nonspace_captive (&mv->insert);
      ide_source_view_movements_select_range (mv);

      mark = _ide_source_view_get_scroll_mark (mv->self);
      gtk_text_buffer_get_iter_at_line (buffer, &scroll_iter, line_top);
      gtk_text_buffer_move_mark (buffer, mark, &scroll_iter);
      gtk_text_view_scroll_to_mark (text_view, mark, 0.0, TRUE, 1.0, 1.0);

      mv->ignore_select = TRUE;
      mv->ignore_scroll_to_insert = TRUE;
      break;

    case IDE_SOURCE_VIEW_MOVEMENT_PAGE_UP_LINES:
      gtk_text_buffer_get_iter_at_line (buffer, &mv->insert, MAX (0, line_top - scrolloff));
      if (!gtk_text_iter_ends_line (&mv->insert))
        {
          if (gtk_text_iter_compare (&mv->insert, &mv->selection) < 0)
            gtk_text_iter_forward_line (&mv->insert);
          else
            gtk_text_iter_set_line_offset (&mv->insert, 0);
        }
      ide_source_view_movements_select_range (mv);

      mark = _ide_source_view_get_scroll_mark (mv->self);
      gtk_text_buffer_get_iter_at_line (buffer, &scroll_iter, line_top);
      gtk_text_buffer_move_mark (buffer, mark, &scroll_iter);
      gtk_text_view_scroll_to_mark (text_view, mark, 0.0, TRUE, 1.0, 1.0);

      mv->ignore_select = TRUE;
      mv->ignore_scroll_to_insert = TRUE;
      break;

    case IDE_SOURCE_VIEW_MOVEMENT_PAGE_DOWN_LINES:
      gtk_text_buffer_get_iter_at_line (buffer, &mv->insert, line_bottom + scrolloff);
      if (!gtk_text_iter_ends_line (&mv->insert))
        {
          if (gtk_text_iter_compare (&mv->insert, &mv->selection) < 0)
            gtk_text_iter_set_line_offset (&mv->insert, 0);
          else
            gtk_text_iter_forward_line (&mv->insert);
        }
      ide_source_view_movements_select_range (mv);

      mark = _ide_source_view_get_scroll_mark (mv->self);
      gtk_text_buffer_get_iter_at_line (buffer, &scroll_iter, line_bottom);
      gtk_text_buffer_move_mark (buffer, mark, &scroll_iter);
      gtk_text_view_scroll_to_mark (text_view, mark, 0.0, TRUE, 1.0, 0.0);

      mv->ignore_select = TRUE;
      mv->ignore_scroll_to_insert = TRUE;
      break;

    case IDE_SOURCE_VIEW_MOVEMENT_PAGE_DOWN:
      gtk_text_buffer_get_iter_at_line (buffer, &mv->insert, line_bottom + scrolloff);
      text_iter_forward_to_nonspace_captive (&mv->insert);
      ide_source_view_movements_select_range (mv);

      mark = _ide_source_view_get_scroll_mark (mv->self);
      gtk_text_buffer_get_iter_at_line (buffer, &scroll_iter, line_bottom);
      gtk_text_buffer_move_mark (buffer, mark, &scroll_iter);
      gtk_text_view_scroll_to_mark (text_view, mark, 0.0, TRUE, 1.0, 0.0);

      mv->ignore_select = TRUE;
      mv->ignore_scroll_to_insert = TRUE;
      break;

    default:
      g_assert_not_reached();
    }
}

static gboolean
bracket_predicate (GtkTextIter *iter,
                   gunichar     ch,
                   gpointer     user_data)
{
  MatchingBracketState *state = user_data;
  GtkTextIter near;

  if (ch == state->jump_from && state->string_mode)
    {
      near = *iter;

      if (!gtk_text_iter_starts_line (iter))
        {
          gtk_text_iter_backward_char (&near);
          return (gtk_text_iter_get_char (&near) != '\\');
        }

      if (state->direction == GTK_DIR_RIGHT)
        return FALSE;

      return TRUE;
    }

  if (ch == state->jump_from && !state->string_mode)
    state->depth += (state->direction == GTK_DIR_RIGHT) ? 1 : -1;
  else if (ch == state->jump_to)
    state->depth += (state->direction == GTK_DIR_RIGHT) ? -1 : 1;

  return (state->depth == 0);

}

/* find the matching char position in 'depth' outer levels */
static gboolean
match_char_with_depth (GtkTextIter      *iter,
                       gunichar          left_char,
                       gunichar          right_char,
                       GtkDirectionType  direction,
                       gint              depth,
                       gboolean          is_exclusive,
                       gboolean          string_mode)
{
  MatchingBracketState state;
  GtkTextIter limit;
  gboolean ret;

  g_return_val_if_fail (direction == GTK_DIR_LEFT || direction == GTK_DIR_RIGHT, FALSE);
  g_return_val_if_fail ((left_char == right_char && string_mode) ||
                        (left_char != right_char && !string_mode), FALSE);

  state.jump_from = left_char;
  state.jump_to = right_char;
  state.direction = direction;
  state.string_mode = string_mode;

  /* We can't yet distinguish nested objects where left and right bounds are the same */
  state.depth = (left_char == right_char) ? 1 : depth;

  limit = *iter;

  if (direction == GTK_DIR_LEFT)
    {
      /* We handle cases where we are just under the right bound or
       * at the right of the left bound or at the line start in string mode
       * with a quote under the cursor !
       */
      if (!gtk_text_iter_ends_line(iter))
          if (state.string_mode ? gtk_text_iter_starts_line (iter) : gtk_text_iter_get_char (iter) != right_char)
            gtk_text_iter_forward_char (iter);

      if (string_mode)
        {
          gtk_text_iter_set_line_offset (&limit, 0);
          ret = ide_text_iter_backward_find_char (iter, bracket_predicate, &state, &limit);
        }
      else
        ret = ide_text_iter_backward_find_char (iter, bracket_predicate, &state, NULL);
    }
  else
    {
      if (string_mode)
        {
          gtk_text_iter_forward_to_line_end (&limit);
          ret = ide_text_iter_forward_find_char (iter, bracket_predicate, &state, &limit);
        }
      else
        ret = ide_text_iter_forward_find_char (iter, bracket_predicate, &state, NULL);
    }

  if (ret && !is_exclusive)
    gtk_text_iter_forward_char (iter);

  return ret;
}

static gboolean
find_char_predicate (gunichar ch,
                     gpointer data)
{
  gunichar ch_searched = GPOINTER_TO_UINT (data);

  return (ch == ch_searched);
}

static gboolean
vim_percent_predicate (GtkTextIter *iter,
                       gunichar     ch,
                       gpointer     user_data)
{
  GtkTextIter near;

  if (ch == '(' || ch == ')' ||
      ch == '[' || ch == ']' ||
      ch == '{' || ch == '}' ||
      ch == '/' || ch == '*')
    {
      if (!gtk_text_iter_starts_line (iter))
        {
          near = *iter;
          gtk_text_iter_backward_char (&near);

          return (gtk_text_iter_get_char (&near) != '\\');
        }

      return TRUE;
    }

  return FALSE;
}

static MacroCond
macro_conditionals_qualify_iter (GtkTextIter *insert,
                                 GtkTextIter *cond_start,
                                 GtkTextIter *cond_end,
                                 gboolean     include_str_bounds)
{
  if (ide_text_iter_in_string (insert, "#ifdef", cond_start, cond_end, include_str_bounds))
    return MACRO_COND_IFDEF;
  else if (ide_text_iter_in_string (insert, "#ifndef", cond_start, cond_end, include_str_bounds))
    return MACRO_COND_IFNDEF;
  else if (ide_text_iter_in_string (insert, "#if", cond_start, cond_end, include_str_bounds))
    return MACRO_COND_IF;
  else if (ide_text_iter_in_string (insert, "#elif", cond_start, cond_end, include_str_bounds))
    return MACRO_COND_ELIF;
  else if (ide_text_iter_in_string (insert, "#else", cond_start, cond_end, include_str_bounds))
    return MACRO_COND_ELSE;
  else if (ide_text_iter_in_string (insert, "#endif", cond_start, cond_end, include_str_bounds))
    return MACRO_COND_ENDIF;
  else
    return MACRO_COND_NONE;
}

static MacroCond
find_macro_conditionals_backward (GtkTextIter *insert,
                                  GtkTextIter *cond_end)
{
  MacroCond cond;

  while (gtk_text_iter_backward_find_char (insert, find_char_predicate, GUINT_TO_POINTER ('#'), NULL))
    {
      cond = macro_conditionals_qualify_iter (insert, NULL, cond_end, TRUE);
      if (cond != MACRO_COND_NONE)
        return cond;
    }

  return MACRO_COND_NONE;
}

static MacroCond
find_macro_conditionals_forward (GtkTextIter *insert,
                                 GtkTextIter *cond_end)
{
  MacroCond cond;

  while (gtk_text_iter_forward_find_char (insert, find_char_predicate, GUINT_TO_POINTER ('#'), NULL))
    {
      cond = macro_conditionals_qualify_iter (insert, NULL, cond_end, TRUE);
      if (cond == MACRO_COND_NONE)
        gtk_text_iter_forward_char (insert);
      else
        return cond;
    }

  return MACRO_COND_NONE;
}

/* Skip a whole macro conditional block backward and
 * setup insert to the previous macro conditional directive.
 */
static MacroCond
macro_conditionals_skip_block_backward (GtkTextIter *insert)
{
  GtkTextIter insert_copy;
  MacroCond cond;
  guint depth = 0;

  insert_copy = *insert;

  while ((cond = find_macro_conditionals_backward (insert, NULL)))
    {
      if (cond == MACRO_COND_ENDIF)
        depth++;
      else if (cond == MACRO_COND_IFDEF || cond == MACRO_COND_IFNDEF || cond == MACRO_COND_IF)
        {
          if (depth == 0)
            return cond;
          else
            --depth;
        }
      else if (cond == MACRO_COND_ELIF || cond == MACRO_COND_ELSE)
        {
          if (depth == 0)
            return cond;
        }
      else
        g_assert_not_reached ();
    }

  *insert = insert_copy;

  return MACRO_COND_NONE;
}

/* Skip a whole macro conditional block forward and
 * setup insert to the next macro conditional directive.
 */
static MacroCond
macro_conditionals_skip_block_forward (GtkTextIter *insert)
{
  GtkTextIter insert_copy;
  GtkTextIter cond_end;
  MacroCond cond;
  guint depth = 0;

  insert_copy = *insert;

  while ((cond = find_macro_conditionals_forward (insert, &cond_end)))
    {
      if (cond == MACRO_COND_IFDEF || cond == MACRO_COND_IFNDEF || cond == MACRO_COND_IF)
        depth++;
      else if (cond == MACRO_COND_ENDIF)
        {
          if (depth == 0)
            return cond;
          else
            --depth;
        }
      else if (cond == MACRO_COND_ELIF || cond == MACRO_COND_ELSE)
        {
          if (depth == 0)
            return cond;
        }
      else
        g_assert_not_reached ();

      *insert = cond_end;
    }

  *insert = insert_copy;

  return MACRO_COND_NONE;
}

static gboolean
match_macro_conditionals (GtkTextIter *insert)
{
  GtkTextIter cursor;
  GtkTextIter cond_start;
  GtkTextIter cond_end;
  MacroCond next_cond;
  MacroCond cond;

  cond = macro_conditionals_qualify_iter (insert, &cond_start, &cond_end, TRUE);
  if (cond == MACRO_COND_NONE)
    return FALSE;

  if (cond == MACRO_COND_ENDIF)
    {
      cursor = cond_start;
      while ((next_cond = macro_conditionals_skip_block_backward (&cursor)))
        {
          if (next_cond == MACRO_COND_IFDEF || next_cond == MACRO_COND_IFNDEF || next_cond == MACRO_COND_IF)
            {
              *insert = cursor;

              return TRUE;
            }
        }
    }
  else
    {
      cursor = cond_end;
      if (macro_conditionals_skip_block_forward (&cursor))
        {
          *insert = cursor;

          return TRUE;
        }

    }

  return FALSE;
}

static gboolean
match_comments (GtkTextIter *insert,
                gunichar     ch)
{
  GtkTextIter cursor;
  GtkTextIter cursor_before;
  GtkTextIter cursor_after;
  gunichar ch_before = 0;
  gunichar ch_after = 0;
  gboolean comment_start;

  cursor_after = *insert;
  if (gtk_text_iter_forward_char (&cursor_after))
    ch_after = gtk_text_iter_get_char (&cursor_after);

  cursor_before = *insert;
  if (gtk_text_iter_backward_char (&cursor_before))
    ch_before = gtk_text_iter_get_char (&cursor_before);

  if ((ch == '/' && ch_before == '*' && ch_after == '*') ||
      (ch == '*' && ch_before == '/' && ch_after == '/'))
    {
      *insert = cursor_after;
      return FALSE;
    }

  if (ch == '/' && ch_after == '*')
    {
      gtk_text_iter_forward_char (&cursor_after);
      *insert = cursor = cursor_after;
      comment_start = TRUE;
    }
  else if (ch_before == '/' && ch == '*' && ch_after != 0)
    {
      *insert = cursor = cursor_after;
      comment_start = TRUE;
    }
  else if (ch == '*' && ch_after == '/' && ch_before != 0)
    {
      cursor = *insert;
      *insert = cursor_after;
      gtk_text_iter_forward_char (insert);
      comment_start = FALSE;
    }
  else if (ch_before == '*' && ch == '/')
    {
      cursor = cursor_before;
      *insert = cursor_after;
      comment_start = FALSE;
    }
  else
    {
      *insert = cursor_after;
      return FALSE;
    }

  if (comment_start && !gtk_text_iter_is_end (&cursor))
    {
      if (ide_text_iter_find_chars_forward (&cursor, NULL, NULL, "*/", FALSE))
        {
          gtk_text_iter_forward_char (&cursor);
          *insert = cursor;

          return TRUE;
        }
    }
  else if (!comment_start && !gtk_text_iter_is_start (&cursor))
    {
      if (ide_text_iter_find_chars_backward (&cursor, NULL, NULL, "/*", FALSE))
        {
          *insert = cursor;

          return TRUE;
        }
    }

  return FALSE;
}

static void
ide_source_view_movements_match_special (Movement *mv)
{
  gunichar start_char;
  GtkTextIter copy;
  GtkTextIter limit;
  GtkTextIter cond_end;
  gboolean ret = FALSE;

  copy = mv->insert;
  gtk_text_iter_set_line_offset (&mv->insert, 0);

  while (!gtk_text_iter_ends_line (&mv->insert) &&
         (start_char = gtk_text_iter_get_char (&mv->insert)) &&
         g_unichar_isspace (start_char))
    gtk_text_iter_forward_char (&mv->insert);

  start_char = gtk_text_iter_get_char (&mv->insert);

  if (start_char == '#' &&
      macro_conditionals_qualify_iter (&mv->insert, NULL, &cond_end, TRUE) &&
      gtk_text_iter_compare (&copy, &cond_end) < 0)
    {
      mv->insert = cond_end;
      if (match_macro_conditionals (&mv->insert))
        return;
    }

  limit = mv->insert = copy;
  if (!gtk_text_iter_ends_line(&limit))
    gtk_text_iter_forward_to_line_end (&limit);

  start_char = gtk_text_iter_get_char (&mv->insert);
  if (!vim_percent_predicate (&mv->insert, start_char, NULL))
    {
loop:
      if (ide_text_iter_forward_find_char (&mv->insert, vim_percent_predicate, NULL, &limit))
        start_char = gtk_text_iter_get_char (&mv->insert);
      else
        {
          mv->insert = copy;
          return;
        }
    }

  if (start_char == '/' || start_char == '*')
    {
      if (match_comments (&mv->insert, start_char))
        return;
      else
        goto loop;
    }

  switch (start_char)
  {
  case '{':
    ret = match_char_with_depth (&mv->insert, '{', '}', GTK_DIR_RIGHT, 1, mv->exclusive, 0);
    break;

  case '[':
    ret = match_char_with_depth (&mv->insert, '[', ']', GTK_DIR_RIGHT, 1, mv->exclusive, 0);
    break;

  case '(':
    ret = match_char_with_depth (&mv->insert, '(', ')', GTK_DIR_RIGHT, 1, mv->exclusive, 0);
    break;

  case '}':
    ret = match_char_with_depth (&mv->insert, '{', '}', GTK_DIR_LEFT, 1, mv->exclusive, 0);
    break;

  case ']':
    ret = match_char_with_depth (&mv->insert, '[', ']', GTK_DIR_LEFT, 1, mv->exclusive, 0);
    break;

  case ')':
    ret = match_char_with_depth (&mv->insert, '(', ')', GTK_DIR_LEFT, 1, mv->exclusive, 0);
    break;

  default:
    return;
  }

  if (!ret)
    mv->insert = copy;
}

static void
ide_source_view_movements_scroll_to_horizontal_bounds (Movement *mv)
{
  GtkTextView *text_view = (GtkTextView *)mv->self;
  GtkTextMark *insert;
  GtkTextBuffer *buffer;
  GtkAdjustment *hadj;
  GtkTextIter insert_iter;
  GdkRectangle screen_rect;
  GdkRectangle insert_rect;
  gdouble value;
  gdouble offset = 0.0;

  buffer = gtk_text_view_get_buffer (text_view);
  insert = gtk_text_buffer_get_insert (buffer);
  hadj = gtk_scrollable_get_hadjustment (GTK_SCROLLABLE (mv->self));

  ide_source_view_get_visible_rect (mv->self, &screen_rect);
  gtk_text_buffer_get_iter_at_mark (buffer, &insert_iter, insert);
  gtk_text_view_get_iter_location (text_view, &insert_iter, &insert_rect);
  value = gtk_adjustment_get_value (hadj);

  switch ((int)mv->type)
    {
    case IDE_SOURCE_VIEW_MOVEMENT_SCROLL_SCREEN_LEFT:
      offset = screen_rect.x - insert_rect.x;
      break;

    case IDE_SOURCE_VIEW_MOVEMENT_SCROLL_SCREEN_RIGHT:
      offset = dzl_cairo_rectangle_x2 (&screen_rect) - dzl_cairo_rectangle_x2 (&insert_rect);
      break;

    default:
      break;
    }

  gtk_adjustment_set_value (hadj, value - offset);

  mv->ignore_scroll_to_insert = TRUE;
}

static void
ide_source_view_movements_scroll_center (Movement *mv)
{
  GtkTextView *text_view = (GtkTextView *)mv->self;
  GtkTextMark *insert;
  GtkTextBuffer *buffer;
  gint line_count;
  gint x_offset;
  gint line_len;

  buffer = gtk_text_view_get_buffer (text_view);
  insert = gtk_text_buffer_get_insert (buffer);

  if (mv->count > 0)
    {
      line_count = gtk_text_buffer_get_line_count (buffer);
      if (mv->count > line_count)
        return;

      x_offset = gtk_text_iter_get_line_offset (&mv->insert);

      gtk_text_iter_set_line (&mv->insert, mv->count - 1);
      line_len = gtk_text_iter_get_chars_in_line (&mv->insert);
      x_offset = MIN (x_offset, line_len -1);
      gtk_text_iter_set_line_offset (&mv->insert, x_offset);

      gtk_text_buffer_move_mark (buffer, insert, &mv->insert);
    }

  switch ((int)mv->type)
    {
    case IDE_SOURCE_VIEW_MOVEMENT_SCROLL_SCREEN_BOTTOM:
      ide_source_view_scroll_to_mark (mv->self, insert, 0.0, TRUE, 1.0, 1.0, TRUE);
      break;

    case IDE_SOURCE_VIEW_MOVEMENT_SCROLL_SCREEN_TOP:
      ide_source_view_scroll_to_mark (mv->self, insert, 0.0, TRUE, 1.0, 0.0, TRUE);
      break;

    case IDE_SOURCE_VIEW_MOVEMENT_SCROLL_SCREEN_CENTER:
      ide_source_view_scroll_to_mark (mv->self, insert, 0.0, TRUE, 1.0, 0.5, TRUE);
      break;

    default:
      break;
    }

  if (g_str_has_suffix (mv->command_str->str, "-") ||
      g_str_has_suffix (mv->command_str->str, ".") ||
      g_str_has_suffix (mv->command_str->str, "[Return]") ||
      g_str_has_suffix (mv->command_str->str, "[KP_Enter]"))
    ide_source_view_movements_first_nonspace_char (mv);

  mv->ignore_scroll_to_insert = TRUE;
}

static void
ide_source_view_movements_next_word_end (Movement *mv)
{
  GtkTextIter copy;

  copy = mv->insert;

  ide_text_iter_forward_word_end (&mv->insert, mv->newline_stop);

  /* prefer an empty line before word */
  text_iter_forward_to_empty_line (&copy, &mv->insert);
  if (gtk_text_iter_compare (&copy, &mv->insert) < 0)
    mv->insert = copy;

  if (!mv->exclusive && !gtk_text_iter_ends_line (&mv->insert))
    gtk_text_iter_forward_char (&mv->insert);
}

static void
ide_source_view_movements_next_full_word_end (Movement *mv)
{
  GtkTextIter copy;

  copy = mv->insert;

  ide_text_iter_forward_WORD_end (&mv->insert, mv->newline_stop);

  /* prefer an empty line before word */
  text_iter_forward_to_empty_line (&copy, &mv->insert);
  if (gtk_text_iter_compare (&copy, &mv->insert) < 0)
    mv->insert = copy;

  if (!mv->exclusive && !gtk_text_iter_ends_line (&mv->insert))
    gtk_text_iter_forward_char (&mv->insert);
}

static void
ide_source_view_movements_next_word_start (Movement *mv)
{
  GtkTextIter copy;

  copy = mv->insert;

  ide_text_iter_forward_word_start (&mv->insert, mv->newline_stop);

  /* prefer an empty line before word */
  text_iter_forward_to_empty_line (&copy, &mv->insert);
  if (gtk_text_iter_compare (&copy, &mv->insert) < 0)
    mv->insert = copy;

  if (!mv->exclusive && !gtk_text_iter_ends_line (&mv->insert))
    gtk_text_iter_forward_char (&mv->insert);
}

static void
ide_source_view_movements_next_full_word_start (Movement *mv)
{
  GtkTextIter copy;

  copy = mv->insert;

  ide_text_iter_forward_WORD_start (&mv->insert, mv->newline_stop);

  /* prefer an empty line before word */
  text_iter_forward_to_empty_line (&copy, &mv->insert);
  if (gtk_text_iter_compare (&copy, &mv->insert) < 0)
    mv->insert = copy;

  if (!mv->exclusive && !gtk_text_iter_ends_line (&mv->insert))
    gtk_text_iter_forward_char (&mv->insert);
}

static void
ide_source_view_movements_previous_word_start (Movement *mv)
{
  GtkTextIter copy;

  copy = mv->insert;

  ide_text_iter_backward_word_start (&mv->insert, mv->newline_stop);

  /*
   * Vim treats an empty line as a word.
   */
  if (gtk_text_iter_backward_char (&copy))
    if (gtk_text_iter_get_char (&copy) == '\n')
      mv->insert = copy;

  if (!mv->exclusive && !gtk_text_iter_ends_line (&mv->insert))
    gtk_text_iter_forward_char (&mv->insert);
}

static void
ide_source_view_movements_previous_full_word_start (Movement *mv)
{
  GtkTextIter copy;

  copy = mv->insert;

  ide_text_iter_backward_WORD_start (&mv->insert, mv->newline_stop);

  /*
   * Vim treats an empty line as a word.
   */
  if (gtk_text_iter_backward_char (&copy))
    if (gtk_text_iter_get_char (&copy) == '\n')
      mv->insert = copy;

  if (!mv->exclusive && !gtk_text_iter_ends_line (&mv->insert))
    gtk_text_iter_forward_char (&mv->insert);
}

static void
ide_source_view_movements_previous_word_end (Movement *mv)
{
  GtkTextIter copy;

  copy = mv->insert;

  ide_text_iter_backward_word_end (&mv->insert, mv->newline_stop);

  /*
   * Vim treats an empty line as a word.
   */
  while ((gtk_text_iter_compare (&copy, &mv->insert) > 0) &&
         gtk_text_iter_backward_char (&copy))
    {
      if (gtk_text_iter_starts_line (&copy) &&
          gtk_text_iter_ends_line (&copy))
        mv->insert = copy;
    }

  if (!mv->exclusive && !gtk_text_iter_ends_line (&mv->insert))
    gtk_text_iter_forward_char (&mv->insert);
}

static void
ide_source_view_movements_previous_full_word_end (Movement *mv)
{
  GtkTextIter copy;

  copy = mv->insert;

  ide_text_iter_backward_WORD_end (&mv->insert, mv->newline_stop);

  /*
   * Vim treats an empty line as a word.
   */
  while ((gtk_text_iter_compare (&copy, &mv->insert) > 0) &&
         gtk_text_iter_backward_char (&copy))
    {
      if (gtk_text_iter_starts_line (&copy) &&
          gtk_text_iter_ends_line (&copy))
        mv->insert = copy;
    }

  if (!mv->exclusive && !gtk_text_iter_ends_line (&mv->insert))
    gtk_text_iter_forward_char (&mv->insert);
}

static void
ide_source_view_movements_paragraph_start (Movement *mv)
{
  ide_text_iter_backward_paragraph_start (&mv->insert);

  if (mv->exclusive)
    {
      while (g_unichar_isspace (gtk_text_iter_get_char (&mv->insert)))
        {
          if (!gtk_text_iter_forward_char (&mv->insert))
            break;
        }
    }
}

static void
ide_source_view_movements_paragraph_end (Movement *mv)
{
  ide_text_iter_forward_paragraph_end (&mv->insert);

  if (mv->exclusive)
    {
      gboolean adjust = FALSE;

      while (g_unichar_isspace (gtk_text_iter_get_char (&mv->insert)))
        {
          adjust = TRUE;
          if (!gtk_text_iter_backward_char (&mv->insert))
            break;
        }

      if (adjust)
        gtk_text_iter_forward_char (&mv->insert);
    }
}

static void
ide_source_view_movements_sentence_start (Movement *mv)
{
  ide_text_iter_backward_sentence_start (&mv->insert);
}

static void
ide_source_view_movements_sentence_end (Movement *mv)
{
  ide_text_iter_forward_sentence_end (&mv->insert);
}

static void
ide_source_view_movements_line_percentage (Movement *mv)
{
  GtkTextBuffer *buffer;
  GtkTextIter end;
  guint end_line;

  buffer = gtk_text_view_get_buffer (GTK_TEXT_VIEW (mv->self));
  gtk_text_buffer_get_end_iter (buffer, &end);
  end_line = gtk_text_iter_get_line (&end);

  if (!mv->count)
    {
      gtk_text_iter_set_line (&mv->insert, 0);
    }
  else
    {
      guint line;

      mv->count = MAX (1, mv->count);
      line = (float)end_line * (mv->count / 100.0);
      gtk_text_iter_set_line (&mv->insert, line);
    }

  mv->count = 0;

  ide_source_view_movements_first_nonspace_char (mv);
}

static void
ide_source_view_movements_previous_unmatched (Movement *mv,
                                              gunichar  target,
                                              gunichar  opposite)
{
  GtkTextIter copy;
  guint count = 1;

  g_assert (mv);
  g_assert (target);
  g_assert (opposite);

  copy = mv->insert;

  do
    {
      gunichar ch;

      if (!gtk_text_iter_backward_char (&mv->insert))
        {
          mv->insert = copy;
          return;
        }

      ch = gtk_text_iter_get_char (&mv->insert);

      if (ch == target)
        count--;
      else if (ch == opposite)
        count++;

      if (!count)
        {
          if (!mv->exclusive)
            gtk_text_iter_forward_char (&mv->insert);
          return;
        }
    }
  while (TRUE);

  g_assert_not_reached ();
}

static void
ide_source_view_movements_next_unmatched (Movement *mv,
                                          gunichar  target,
                                          gunichar  opposite)
{
  GtkTextIter copy;
  guint count = 1;

  g_assert (mv);
  g_assert (target);
  g_assert (opposite);

  copy = mv->insert;

  do
    {
      gunichar ch;

      if (!gtk_text_iter_forward_char (&mv->insert))
        {
          mv->insert = copy;
          return;
        }

      ch = gtk_text_iter_get_char (&mv->insert);

      if (ch == target)
        count--;
      else if (ch == opposite)
        count++;

      if (!count)
        {
          if (!mv->exclusive)
            gtk_text_iter_forward_char (&mv->insert);
          return;
        }
    }
  while (TRUE);

  g_assert_not_reached ();
}

static gboolean
find_match (gunichar ch,
            gpointer data)
{
  Movement *mv = data;

  return (mv->modifier == ch);
}

static void
ide_source_view_movements_next_match_modifier (Movement *mv)
{
  GtkTextIter insert;
  GtkTextIter bounds;

  bounds = insert = mv->insert;
  gtk_text_iter_forward_to_line_end (&bounds);

  if (gtk_text_iter_forward_find_char (&insert, find_match, mv, &bounds))
    {
      if (!mv->exclusive)
        gtk_text_iter_forward_char (&insert);
      mv->insert = insert;
    }
}

static void
ide_source_view_movements_previous_match_modifier (Movement *mv)
{
  GtkTextIter insert;
  GtkTextIter bounds;

  bounds = insert = mv->insert;
  gtk_text_iter_set_line_offset (&bounds, 0);

  if (gtk_text_iter_backward_find_char (&insert, find_match, mv, &bounds))
    {
      if (!mv->exclusive)
        gtk_text_iter_forward_char (&insert);
      mv->insert = insert;
    }
}

static void
ide_source_view_movement_match_search_char (Movement *mv,
                                            gboolean  is_next_direction)
{
  GtkTextIter insert;
  GtkTextIter limit;
  gboolean is_forward;
  gboolean is_till;
  gboolean is_inclusive_mode;
  gboolean is_selection_positive;
  const gchar *mode_name;

  limit = insert = mv->insert;
  is_forward = (mv->command == 'f' || mv->command == 't');
  is_till = (mv->command == 't' || mv->command == 'T');

  mode_name = ide_source_view_get_mode_name (mv->self);
  is_inclusive_mode = (g_str_has_prefix (mode_name, "vim-visual") ||
                       g_str_has_prefix (mode_name, "vim-normal-c") ||
                       g_str_has_prefix (mode_name, "vim-normal-d"));

  is_selection_positive = (gtk_text_iter_compare (&insert, &mv->selection) >= 0);

  if (mv->modifier == 0)
    return;

  if ((is_forward && is_next_direction) || (!is_forward && !is_next_direction))
    {
      /* We search to the right */
      gtk_text_iter_forward_to_line_end (&limit);

      if (is_till)
        gtk_text_iter_forward_char (&insert);

      if (is_inclusive_mode && is_selection_positive)
        gtk_text_iter_backward_char (&insert);

      if (gtk_text_iter_forward_find_char (&insert, find_char_predicate, GUINT_TO_POINTER (mv->modifier), &limit))
        {
          if (is_till)
            gtk_text_iter_backward_char (&insert);

          is_selection_positive = (gtk_text_iter_compare (&insert, &mv->selection) >= 0);
          if (is_inclusive_mode && is_selection_positive)
            gtk_text_iter_forward_char (&insert);

          mv->insert = insert;
        }
    }
  else
    {
      /* We search to the left */
      gtk_text_iter_set_line_offset (&limit, 0);

      if (is_till)
        gtk_text_iter_backward_char (&insert);

      if (is_inclusive_mode && is_selection_positive)
        gtk_text_iter_backward_char (&insert);

      if (gtk_text_iter_backward_find_char (&insert, find_char_predicate, GUINT_TO_POINTER (mv->modifier), &limit))
        {
          if (is_till)
            gtk_text_iter_forward_char (&insert);

          is_selection_positive = (gtk_text_iter_compare (&insert, &mv->selection) >= 0);
          if (is_inclusive_mode && is_selection_positive)
            gtk_text_iter_forward_char (&insert);

          mv->insert = insert;
        }
    }
}

static void
ide_source_view_movements_smart_home (Movement                  *mv,
                                      GtkSourceSmartHomeEndType  mode)
{
  GtkTextIter iter;

  g_assert (mv != NULL);

  iter = mv->insert;

  switch (mode)
    {
    case GTK_SOURCE_SMART_HOME_END_BEFORE:
      ide_source_view_movements_first_nonspace_char (mv);
      if (gtk_text_iter_equal (&iter, &mv->insert))
        gtk_text_iter_set_line_offset (&mv->insert, 0);
      return;

    case GTK_SOURCE_SMART_HOME_END_AFTER:
      ide_source_view_movements_first_char (mv);
      if (gtk_text_iter_equal (&iter, &mv->insert))
        ide_source_view_movements_first_nonspace_char (mv);
      return;

    case GTK_SOURCE_SMART_HOME_END_ALWAYS:
      ide_source_view_movements_first_nonspace_char (mv);
      return;

    case GTK_SOURCE_SMART_HOME_END_DISABLED:
    default:
      ide_source_view_movements_first_char (mv);
      return;
    }
}

void
_ide_source_view_apply_movement (IdeSourceView         *self,
                                 IdeSourceViewMovement  movement,
                                 gboolean               extend_selection,
                                 gboolean               exclusive,
                                 gint                   count,
                                 GString               *command_str,
                                 gunichar               command,
                                 gunichar               modifier,
                                 gunichar               search_char,
                                 guint                 *target_column)
{
  Movement mv = { 0 };
  GtkTextBuffer *buffer;
  GtkTextMark *insert;
  GtkTextIter end_iter;
  GtkTextIter before_insert;
  GtkTextIter after_insert;
  gdouble xalign = 0.5;
  gint min_count = 1;
  gint end_line;
  gint distance;
  gsize i;
  guint line;

  g_return_if_fail (IDE_IS_SOURCE_VIEW (self));

#ifdef IDE_ENABLE_TRACE
  {
    GEnumValue *enum_value;
    GEnumClass *enum_class, *enum_class_unref = NULL;

    if (!(enum_class = g_type_class_peek (IDE_TYPE_SOURCE_VIEW_MOVEMENT)))
      enum_class = enum_class_unref = g_type_class_ref (IDE_TYPE_SOURCE_VIEW_MOVEMENT);
    enum_value = g_enum_get_value (enum_class, movement);
    IDE_TRACE_MSG ("movement(%s, extend_selection=%s, exclusive=%s, count=%u)",
                   enum_value->value_nick,
                   extend_selection ? "YES" : "NO",
                   exclusive ? "YES" : "NO",
                   count);
    g_clear_pointer (&enum_class_unref, g_type_class_unref);
  }
#endif

  buffer = gtk_text_view_get_buffer (GTK_TEXT_VIEW (self));
  insert = gtk_text_buffer_get_insert (buffer);

  gtk_text_buffer_get_iter_at_mark (buffer, &before_insert, insert);

  /* specific processing for underscore motion */
  if (g_str_has_suffix (command_str->str, "_"))
    {
      min_count = 0;
      if (count > 0)
        --count;
      else
        count = 0;
    }

  gtk_text_buffer_get_end_iter (buffer, &end_iter);
  end_line = gtk_text_iter_get_line (&end_iter);

  mv.self = self;
  mv.target_column = target_column;
  mv.type = movement;
  mv.scroll_align = IDE_SOURCE_SCROLL_BOTH;
  mv.extend_selection = extend_selection;
  mv.exclusive = exclusive;
  mv.count = count;
  mv.ignore_select = FALSE;
  mv.ignore_target_column = FALSE;
  mv.command_str = command_str;
  mv.command = command;
  mv.modifier = modifier;

  ide_source_view_movements_get_selection (&mv);

  switch (movement)
    {
    case IDE_SOURCE_VIEW_MOVEMENT_PREVIOUS_OFFSET:
      gtk_text_iter_backward_chars (&mv.insert, MAX (1, mv.count));
      break;

    case IDE_SOURCE_VIEW_MOVEMENT_NEXT_OFFSET:
      gtk_text_iter_forward_chars (&mv.insert, MAX (1, mv.count));
      break;

    case IDE_SOURCE_VIEW_MOVEMENT_NTH_CHAR:
      mv.count = MAX (1, mv.count);
      ide_source_view_movements_nth_char (&mv);
      break;

    case IDE_SOURCE_VIEW_MOVEMENT_PREVIOUS_CHAR:
      mv.count = MAX (1, mv.count);
      ide_source_view_movements_previous_char (&mv);
      break;

    case IDE_SOURCE_VIEW_MOVEMENT_NEXT_CHAR:
      mv.count = MAX (1, mv.count);
      ide_source_view_movements_next_char (&mv);
      break;

    case IDE_SOURCE_VIEW_MOVEMENT_FIRST_CHAR:
      mv.count = MAX (1, mv.count);
      mv.scroll_align = IDE_SOURCE_SCROLL_X;
      ide_source_view_movements_first_char (&mv);
      xalign = 1.0;
      break;

    case IDE_SOURCE_VIEW_MOVEMENT_FIRST_NONSPACE_CHAR:
      mv.count = MAX (1, mv.count);
      mv.scroll_align = IDE_SOURCE_SCROLL_X;
      ide_source_view_movements_first_nonspace_char (&mv);
      xalign = 1.0;
      break;

    case IDE_SOURCE_VIEW_MOVEMENT_MIDDLE_CHAR:
      mv.count = MAX (1, mv.count);
      mv.scroll_align = IDE_SOURCE_SCROLL_X;
      ide_source_view_movements_middle_char (&mv);
      break;

    case IDE_SOURCE_VIEW_MOVEMENT_LAST_CHAR:
      mv.count = MAX (1, mv.count);
      mv.scroll_align = IDE_SOURCE_SCROLL_X;
      ide_source_view_movements_last_char (&mv);
      break;

    case IDE_SOURCE_VIEW_MOVEMENT_PREVIOUS_FULL_WORD_START:
      for (i = MAX (1, mv.count); i > 0; i--)
        ide_source_view_movements_previous_full_word_start (&mv);
      break;

    case IDE_SOURCE_VIEW_MOVEMENT_PREVIOUS_FULL_WORD_START_NEWLINE_STOP:
      mv.newline_stop = TRUE;
      for (i = MAX (1, mv.count); i > 0; i--)
        ide_source_view_movements_previous_full_word_start (&mv);
      break;

    case IDE_SOURCE_VIEW_MOVEMENT_NEXT_FULL_WORD_START:
      for (i = MAX (1, mv.count); i > 0; i--)
        ide_source_view_movements_next_full_word_start (&mv);
      break;

    case IDE_SOURCE_VIEW_MOVEMENT_NEXT_FULL_WORD_START_NEWLINE_STOP:
      mv.newline_stop = TRUE;
      for (i = MAX (1, mv.count); i > 0; i--)
        ide_source_view_movements_next_full_word_start (&mv);
      break;

    case IDE_SOURCE_VIEW_MOVEMENT_PREVIOUS_FULL_WORD_END:
      for (i = MAX (1, mv.count); i > 0; i--)
        ide_source_view_movements_previous_full_word_end (&mv);
      break;

    case IDE_SOURCE_VIEW_MOVEMENT_PREVIOUS_FULL_WORD_END_NEWLINE_STOP:
      mv.newline_stop = TRUE;
      for (i = MAX (1, mv.count); i > 0; i--)
        ide_source_view_movements_previous_full_word_end (&mv);
      break;

    case IDE_SOURCE_VIEW_MOVEMENT_NEXT_FULL_WORD_END:
      for (i = MAX (1, mv.count); i > 0; i--)
        ide_source_view_movements_next_full_word_end (&mv);
      break;

    case IDE_SOURCE_VIEW_MOVEMENT_NEXT_FULL_WORD_END_NEWLINE_STOP:
      mv.newline_stop = TRUE;
      for (i = MAX (1, mv.count); i > 0; i--)
        ide_source_view_movements_next_full_word_end (&mv);
      break;

    case IDE_SOURCE_VIEW_MOVEMENT_PREVIOUS_SUB_WORD_START:
      gtk_text_iter_backward_visible_word_starts (&mv.insert, MAX (1, mv.count));
      break;

    case IDE_SOURCE_VIEW_MOVEMENT_NEXT_SUB_WORD_START:
      if (!gtk_text_iter_forward_visible_word_ends (&mv.insert, MAX (1, mv.count)))
        gtk_text_iter_forward_to_line_end (&mv.insert);
      break;

    case IDE_SOURCE_VIEW_MOVEMENT_PREVIOUS_WORD_START:
      for (i = MAX (1, mv.count); i > 0; i--)
        ide_source_view_movements_previous_word_start (&mv);
      break;

    case IDE_SOURCE_VIEW_MOVEMENT_PREVIOUS_WORD_START_NEWLINE_STOP:
      mv.newline_stop = TRUE;
      for (i = MAX (1, mv.count); i > 0; i--)
        ide_source_view_movements_previous_word_start (&mv);
      break;

    case IDE_SOURCE_VIEW_MOVEMENT_NEXT_WORD_START:
      for (i = MAX (1, mv.count); i > 0; i--)
        ide_source_view_movements_next_word_start (&mv);
      break;

    case IDE_SOURCE_VIEW_MOVEMENT_NEXT_WORD_START_NEWLINE_STOP:
      mv.newline_stop = TRUE;
      for (i = MAX (1, mv.count); i > 0; i--)
        ide_source_view_movements_next_word_start (&mv);
      break;

    case IDE_SOURCE_VIEW_MOVEMENT_PREVIOUS_WORD_END:
      for (i = MAX (1, mv.count); i > 0; i--)
        ide_source_view_movements_previous_word_end (&mv);
      break;

    case IDE_SOURCE_VIEW_MOVEMENT_PREVIOUS_WORD_END_NEWLINE_STOP:
      mv.newline_stop = TRUE;
      for (i = MAX (1, mv.count); i > 0; i--)
        ide_source_view_movements_previous_word_end (&mv);
      break;

    case IDE_SOURCE_VIEW_MOVEMENT_NEXT_WORD_END:
      for (i = MAX (1, mv.count); i > 0; i--)
        ide_source_view_movements_next_word_end (&mv);
      break;

    case IDE_SOURCE_VIEW_MOVEMENT_NEXT_WORD_END_NEWLINE_STOP:
      mv.newline_stop = TRUE;
      for (i = MAX (1, mv.count); i > 0; i--)
        ide_source_view_movements_next_word_end (&mv);
      break;

    case IDE_SOURCE_VIEW_MOVEMENT_SENTENCE_START:
      for (i = MAX (1, mv.count); i > 0; i--)
        ide_source_view_movements_sentence_start (&mv);
      break;

    case IDE_SOURCE_VIEW_MOVEMENT_SENTENCE_END:
      for (i = MAX (1, mv.count); i > 0; i--)
        ide_source_view_movements_sentence_end (&mv);
      break;

    case IDE_SOURCE_VIEW_MOVEMENT_PARAGRAPH_START:
      for (i = MAX (1, mv.count); i > 0; i--)
        {
          mv.exclusive = exclusive && i == 1;
          ide_source_view_movements_paragraph_start (&mv);
        }
      break;

    case IDE_SOURCE_VIEW_MOVEMENT_PARAGRAPH_END:
      for (i = MAX (1, mv.count); i > 0; i--)
        {
          mv.exclusive = exclusive && i == 1;
          ide_source_view_movements_paragraph_end (&mv);
        }
      break;

    case IDE_SOURCE_VIEW_MOVEMENT_PREVIOUS_LINE:
      mv.ignore_target_column = TRUE;
      mv.ignore_select = TRUE;
      mv.count = MIN (mv.count, end_line);
      mv.scroll_align = IDE_SOURCE_SCROLL_X;
      /*
       * It would be nice to do this as one large movement, but
       * ide_source_view_movements_previous_line() needs to be
       * split up into movements for different line-wise options.
       */
      for (i = MAX (1, mv.count); i > 0; i--)
        if (!ide_source_view_movements_previous_line (&mv))
          break;
      break;

    case IDE_SOURCE_VIEW_MOVEMENT_NEXT_LINE:
      mv.ignore_target_column = TRUE;
      mv.ignore_select = TRUE;
      mv.count = MIN (mv.count, end_line);
      mv.scroll_align = IDE_SOURCE_SCROLL_X;
      /*
       * It would be nice to do this as one large movement, but
       * ide_source_view_movements_next_line() needs to be
       * split up into movements for different line-wise options.
       */
      for (i = MAX (min_count, mv.count); i > 0; i--)
        if (!ide_source_view_movements_next_line (&mv))
          break;
      break;

    case IDE_SOURCE_VIEW_MOVEMENT_FIRST_LINE:
      ide_source_view_movements_first_line (&mv);
      break;

    case IDE_SOURCE_VIEW_MOVEMENT_NTH_LINE:
      ide_source_view_movements_nth_line (&mv);
      break;

    case IDE_SOURCE_VIEW_MOVEMENT_LAST_LINE:
      mv.count = MAX (1, mv.count);
      ide_source_view_movements_last_line (&mv);
      break;

    case IDE_SOURCE_VIEW_MOVEMENT_LINE_PERCENTAGE:
      mv.count = MAX (1, mv.count);
      ide_source_view_movements_line_percentage (&mv);
      break;

    case IDE_SOURCE_VIEW_MOVEMENT_LINE_CHARS:
      mv.count = MAX (1, mv.count);
      ide_source_view_movements_line_chars (&mv);
      break;

    case IDE_SOURCE_VIEW_MOVEMENT_LINE_END:
      mv.count = MAX (1, mv.count);
      mv.scroll_align = IDE_SOURCE_SCROLL_X;
      ide_source_view_movements_line_end (&mv);
      break;

    case IDE_SOURCE_VIEW_MOVEMENT_HALF_PAGE_UP:
    case IDE_SOURCE_VIEW_MOVEMENT_HALF_PAGE_DOWN:
    case IDE_SOURCE_VIEW_MOVEMENT_HALF_PAGE_LEFT:
    case IDE_SOURCE_VIEW_MOVEMENT_HALF_PAGE_RIGHT:
    case IDE_SOURCE_VIEW_MOVEMENT_PAGE_UP:
    case IDE_SOURCE_VIEW_MOVEMENT_PAGE_UP_LINES:
    case IDE_SOURCE_VIEW_MOVEMENT_PAGE_DOWN:
    case IDE_SOURCE_VIEW_MOVEMENT_PAGE_DOWN_LINES:
      for (i = MAX (1, mv.count); i > 0; i--)
        ide_source_view_movements_move_page (&mv);
      break;

    case IDE_SOURCE_VIEW_MOVEMENT_SCREEN_DOWN:
    case IDE_SOURCE_VIEW_MOVEMENT_SCREEN_UP:
    case IDE_SOURCE_VIEW_MOVEMENT_SCREEN_LEFT:
    case IDE_SOURCE_VIEW_MOVEMENT_SCREEN_RIGHT:
      ide_source_view_movements_scroll (&mv);
      break;

    case IDE_SOURCE_VIEW_MOVEMENT_SCREEN_TOP:
      mv.count = MAX (1, mv.count);
      ide_source_view_movements_screen_top (&mv);
      break;

    case IDE_SOURCE_VIEW_MOVEMENT_SCREEN_MIDDLE:
      mv.count = MAX (1, mv.count);
      ide_source_view_movements_screen_middle (&mv);
      break;

    case IDE_SOURCE_VIEW_MOVEMENT_SCREEN_BOTTOM:
      mv.count = MAX (1, mv.count);
      ide_source_view_movements_screen_bottom (&mv);
      break;

    case IDE_SOURCE_VIEW_MOVEMENT_MATCH_SPECIAL:
      mv.count = MAX (1, mv.count);
      ide_source_view_movements_match_special (&mv);
      break;

    case IDE_SOURCE_VIEW_MOVEMENT_SCROLL_SCREEN_TOP:
    case IDE_SOURCE_VIEW_MOVEMENT_SCROLL_SCREEN_CENTER:
    case IDE_SOURCE_VIEW_MOVEMENT_SCROLL_SCREEN_BOTTOM:
      ide_source_view_movements_scroll_center (&mv);
      break;

    case IDE_SOURCE_VIEW_MOVEMENT_SCROLL_SCREEN_LEFT:
    case IDE_SOURCE_VIEW_MOVEMENT_SCROLL_SCREEN_RIGHT:
      ide_source_view_movements_scroll_to_horizontal_bounds (&mv);
      break;

    case IDE_SOURCE_VIEW_MOVEMENT_PREVIOUS_UNMATCHED_BRACE:
      for (i = MAX (1, mv.count); i > 0; i--)
        ide_source_view_movements_previous_unmatched (&mv, '{', '}');
      break;

    case IDE_SOURCE_VIEW_MOVEMENT_NEXT_UNMATCHED_BRACE:
      for (i = MAX (1, mv.count); i > 0; i--)
        ide_source_view_movements_next_unmatched (&mv, '}', '{');
      break;

    case IDE_SOURCE_VIEW_MOVEMENT_PREVIOUS_UNMATCHED_PAREN:
      for (i = MAX (1, mv.count); i > 0; i--)
        ide_source_view_movements_previous_unmatched (&mv, '(', ')');
      break;

    case IDE_SOURCE_VIEW_MOVEMENT_NEXT_UNMATCHED_PAREN:
      for (i = MAX (1, mv.count); i > 0; i--)
        ide_source_view_movements_next_unmatched (&mv, ')', '(');
      break;

    case IDE_SOURCE_VIEW_MOVEMENT_NEXT_MATCH_MODIFIER:
      for (i = MAX (1, mv.count); i > 0; i--)
        ide_source_view_movements_next_match_modifier (&mv);
      break;

    case IDE_SOURCE_VIEW_MOVEMENT_PREVIOUS_MATCH_MODIFIER:
      for (i = MAX (1, mv.count); i > 0; i--)
        ide_source_view_movements_previous_match_modifier (&mv);
      break;

    case IDE_SOURCE_VIEW_MOVEMENT_PREVIOUS_MATCH_SEARCH_CHAR:
      mv.modifier = search_char;
      for (i = MAX (1, mv.count); i > 0; i--)
        ide_source_view_movement_match_search_char (&mv, FALSE);
      break;

    case IDE_SOURCE_VIEW_MOVEMENT_NEXT_MATCH_SEARCH_CHAR:
      mv.modifier = search_char;
      for (i = MAX (1, mv.count); i > 0; i--)
        ide_source_view_movement_match_search_char (&mv, TRUE);
      break;

    case IDE_SOURCE_VIEW_MOVEMENT_SMART_HOME:
      {
        GtkSourceSmartHomeEndType smart_home;

        mv.count = 1;
        mv.scroll_align = IDE_SOURCE_SCROLL_X;

        smart_home = gtk_source_view_get_smart_home_end (GTK_SOURCE_VIEW (self));
        ide_source_view_movements_smart_home (&mv, smart_home);
      }
      break;

    default:
      g_return_if_reached ();
    }

  if (!mv.ignore_select)
    ide_source_view_movements_select_range (&mv);

  if (!mv.ignore_target_column)
    ide_source_view_get_visual_position (mv.self, &line, target_column);

  if (!mv.ignore_scroll_to_insert)
    ide_source_view_scroll_mark_onscreen (self, insert, mv.scroll_align, xalign, 0.5);

  /* Emit a jump if we moved more than JUMP_THRESHOLD lines */
  gtk_text_buffer_get_iter_at_mark (buffer, &after_insert, insert);
  distance = gtk_text_iter_get_line (&before_insert) -
             gtk_text_iter_get_line (&after_insert);
  if (ABS (distance) > JUMP_THRESHOLD)
    {
      /* We push both jumps and can rely on the receivers to
       * chain the locations.
       */
      ide_source_view_jump (self, &before_insert, &after_insert);
    }
}

void
_ide_source_view_select_inner (IdeSourceView *self,
                               gunichar       inner_left,
                               gunichar       inner_right,
                               gint           count,
                               gboolean       exclusive,
                               gboolean       string_mode)
{
  GtkTextBuffer *buffer;
  GtkTextMark *insert;
  GtkTextMark *selection;
  GtkTextIter start;
  GtkTextIter end;
  GtkTextIter selection_iter;
  gboolean ret;

  g_return_if_fail (IDE_IS_SOURCE_VIEW (self));

  buffer = gtk_text_view_get_buffer (GTK_TEXT_VIEW (self));
  insert = gtk_text_buffer_get_insert (buffer);
  gtk_text_buffer_get_iter_at_mark (buffer, &start, insert);
  selection = gtk_text_buffer_get_selection_bound (buffer);
  gtk_text_buffer_get_iter_at_mark (buffer, &selection_iter, selection);

  /* Visual mode start with a selection length of 1. We use the left bound in this case */
  if ((gtk_text_iter_get_offset (&start) - gtk_text_iter_get_offset (&selection_iter)) == 1)
    gtk_text_iter_backward_char (&start);

  if (string_mode)
    {
      if (gtk_text_iter_ends_line (&start))
        return;

      count = 1;
      inner_right = inner_left;
    }
  else
    {
      count = MAX (1, count);
    }

  ret = match_char_with_depth (&start, inner_left, inner_right, GTK_DIR_LEFT, count, !exclusive, string_mode);
  if (!ret && string_mode)
    ret = match_char_with_depth (&start, inner_left, inner_right, GTK_DIR_RIGHT, count, !exclusive, string_mode);

  if (ret)
    {
      end = start;
      if (exclusive)
        gtk_text_iter_backward_char (&end);

      if (match_char_with_depth (&end, inner_left, inner_right, GTK_DIR_RIGHT, 1, exclusive, string_mode))
        {
          gtk_text_buffer_select_range (buffer, &start, &end);
          gtk_text_view_scroll_mark_onscreen (GTK_TEXT_VIEW (self), insert);
        }

    }
}

static gboolean
html_tag_predicate (GtkTextIter *iter,
                    gunichar     ch,
                    gpointer     user_data)
{
  GtkTextIter near;
  gunichar bound = GPOINTER_TO_UINT (user_data);

  if (ch == bound)
    {
      if (!gtk_text_iter_starts_line (iter))
        {
          near = *iter;
          gtk_text_iter_backward_char (&near);

          return (gtk_text_iter_get_char (&near) != '\\');
        }

      return TRUE;
    }

  return FALSE;
}

/* Iter need to be at the start of the name */
static gchar *
get_html_tag_name (GtkTextIter *iter)
{
  GtkTextIter start = *iter;
  gunichar ch;

  do
    {
      ch = gtk_text_iter_get_char (iter);
      if (!(g_unichar_isalnum (ch) || ch == '-' || ch == '_' || ch == '.'))
        break;
    } while (gtk_text_iter_forward_char(iter));

  return gtk_text_iter_get_text (&start, iter);
}

static gboolean
find_tag_end (GtkTextIter *cursor)
{
  gunichar ch;
  gunichar previous = 0;

  while ((ch = gtk_text_iter_get_char (cursor)))
    {
      if (previous == '\\')
        {
          previous = 0;
          gtk_text_iter_forward_char (cursor);
          continue;
        }

      if (ch == '>')
        return TRUE;
      else if (ch == '<')
        return FALSE;

      previous = ch;
      gtk_text_iter_forward_char (cursor);
    }

  return FALSE;
}

/* iter is updated to the left of the tag for a GTK_DIR_LEFT direction or in case
 * of error in the tag, and to the right of the tag for a GTK_DIR_RIGHT direction.
 * If no tag can be found, NULL is returned and iter equal the corresponding buffer bound.
 */
static HtmlTag *
find_html_tag (GtkTextIter      *iter,
               GtkDirectionType  direction)
{
  GtkTextIter cursor;
  GtkTextIter end;
  HtmlTag *tag;
  gchar *name;
  gunichar ch;
  gboolean ret;

  g_return_val_if_fail (direction == GTK_DIR_LEFT || direction == GTK_DIR_RIGHT, NULL);

  if (direction == GTK_DIR_LEFT)
    ret = ide_text_iter_backward_find_char (iter, html_tag_predicate, GUINT_TO_POINTER ('<'), NULL);
  else
    ret = (gtk_text_iter_get_char (iter) == '<') ||
          ide_text_iter_forward_find_char (iter, html_tag_predicate, GUINT_TO_POINTER ('<'), NULL);

  if (!ret)
    return NULL;

  tag = g_new0 (HtmlTag, 1);
  tag->kind = HTML_TAG_KIND_OPEN;
  cursor = tag->begin = tag->end = *iter;

  gtk_text_iter_forward_char (&cursor);
  if (gtk_text_iter_is_end (&cursor))
    {
      tag->kind = HTML_TAG_KIND_ERROR;
      tag->end = cursor;

      return tag;
    }

  ch = gtk_text_iter_get_char (&cursor);
  if (ch == '/')
    {
      tag->kind = HTML_TAG_KIND_CLOSE;
      gtk_text_iter_forward_char (&cursor);
    }
  else if (ch == '>')
    {
      tag->kind = HTML_TAG_KIND_EMPTY;
      gtk_text_iter_forward_char (&cursor);
      if (direction == GTK_DIR_RIGHT)
        *iter = cursor;

      tag->end = cursor;

      return tag;
    }
  else if (ide_text_iter_find_chars_forward (&cursor, NULL, &end, "!--", TRUE))
    {
      tag->kind = HTML_TAG_KIND_COMMENT;
      cursor = end;
      if (ide_text_iter_find_chars_forward (&cursor, NULL, &end, "-->", FALSE))
        {
          tag->end = end;
          if (direction == GTK_DIR_RIGHT)
            *iter = tag->end;
        }
      else
        {
          tag->kind = HTML_TAG_KIND_ERROR;
          tag->end = cursor;
        }

      return tag;
    }

  name = get_html_tag_name (&cursor);
  if (dzl_str_empty0 (name))
    {
      g_free (name);
      tag->kind = HTML_TAG_KIND_ERROR;
      tag->end = cursor;

      return tag;
    }
  else
    {
      tag->name = g_utf8_casefold (name, -1);
      g_free (name);
    }

  if (!find_tag_end (&cursor))
    {
      tag->kind = HTML_TAG_KIND_ERROR;
      tag->end = cursor;

      return tag;
    }

  tag->end = cursor;
  gtk_text_iter_forward_char (&tag->end);

  gtk_text_iter_backward_char (&cursor);
  if (gtk_text_iter_get_char (&cursor) == '/' && tag->kind != HTML_TAG_KIND_CLOSE)
    tag->kind = HTML_TAG_KIND_STRAY_END;

  if (direction == GTK_DIR_RIGHT)
    *iter = tag->end;

  return tag;
}

static void
free_html_tag (gpointer data)
{
  HtmlTag *tag = (HtmlTag *)data;

  if (tag != NULL)
    {
      g_free (tag->name);
      g_free (tag);
    }
}

/* cursor should be at the left of the block cursor */
static HtmlTag *
find_non_matching_html_tag_at_left (GtkTextIter *cursor,
                                    gboolean     block_cursor)
{
  GtkTextIter cursor_right;
  HtmlTag *last_closing_tag = NULL;
  HtmlTag *tag = NULL;
  GQueue *stack = NULL;

  stack = g_queue_new ();

  cursor_right = *cursor;
  if (block_cursor)
    gtk_text_iter_forward_char (&cursor_right);

  while ((tag = find_html_tag (&cursor_right, GTK_DIR_LEFT)))
    {
      if (tag->kind == HTML_TAG_KIND_CLOSE)
        {
          if (gtk_text_iter_compare (cursor, &tag->end) >= 0)
            {
              g_queue_push_head (stack, tag);
              continue;
            }
          else
            cursor_right = tag->begin;
        }
      else if (tag->kind == HTML_TAG_KIND_OPEN)
        {
          last_closing_tag = g_queue_peek_head (stack);
          if (last_closing_tag != NULL)
            {
              if (dzl_str_equal0 (tag->name, last_closing_tag->name))
                {
                  g_queue_pop_head (stack);
                  free_html_tag (last_closing_tag);
                }
            }
          else
            {
              *cursor = tag->begin;
              break;
            }
        }

      free_html_tag (tag);
    }

  g_queue_free_full (stack, free_html_tag);

  return tag;
}

/* cursor should be at the left of the block cursor */
static HtmlTag *
find_non_matching_html_tag_at_right (GtkTextIter *cursor,
                                     gboolean     block_cursor)
{
  GQueue *stack;
  HtmlTag *tag;
  HtmlTag *last_closing_tag;
  GtkTextIter cursor_left;
  GtkTextIter cursor_right;

  stack = g_queue_new ();
  cursor_left = cursor_right = *cursor;

  if (block_cursor)
    gtk_text_iter_forward_char (&cursor_right);

  tag = find_html_tag (&cursor_right, GTK_DIR_LEFT);
  if (tag != NULL && gtk_text_iter_compare (cursor, &tag->end) < 0)
    {
      if (tag->kind == HTML_TAG_KIND_CLOSE)
        cursor_left = tag->begin;
      else if (tag->kind == HTML_TAG_KIND_OPEN)
        cursor_left = tag->end;
    }

  while ((tag = find_html_tag (&cursor_left, GTK_DIR_RIGHT)))
    {
      if (tag->kind == HTML_TAG_KIND_OPEN)
        {
          g_queue_push_head (stack, tag);
          continue;
        }
      else if (tag->kind == HTML_TAG_KIND_CLOSE)
        {
          while ((last_closing_tag = g_queue_pop_head (stack)))
            {
              gboolean is_names_equal = dzl_str_equal0 (tag->name, last_closing_tag->name);

              free_html_tag (last_closing_tag);
              if (is_names_equal)
                break;
            }

          if (last_closing_tag == NULL)
            {
              *cursor = tag->begin;
              break;
            }
        }
      else if (tag->kind == HTML_TAG_KIND_ERROR)
        gtk_text_iter_forward_char (&cursor_left);

      g_clear_pointer (&tag, free_html_tag);
    }

  g_queue_free_full (stack, free_html_tag);
  g_clear_pointer (&tag, free_html_tag);

  return tag;
}

static void
free_html_element (gpointer data)
{
  HtmlElement *element = (HtmlElement *)data;

  if (element != NULL)
    {
      free_html_tag (element->left);
      free_html_tag (element->right);

      g_free (element);
    }
}

static HtmlElement *
get_html_element (GtkTextIter cursor_left,
                  gboolean    block_cursor)
{
  HtmlElement *element = NULL;
  HtmlTag *left_tag;
  HtmlTag *right_tag;

  right_tag = find_non_matching_html_tag_at_right (&cursor_left, block_cursor);
  if (right_tag != NULL)
    {
      while ((left_tag = find_non_matching_html_tag_at_left (&cursor_left, block_cursor)))
        {
          if (!dzl_str_equal0 (left_tag->name, right_tag->name))
            {
              cursor_left = left_tag->begin;
              free_html_tag (left_tag);
              left_tag = NULL;

              if (block_cursor && !gtk_text_iter_backward_char (&cursor_left))
                break;

            }
          else
            break;
        };

      if (left_tag != NULL)
        {
          element = g_new0 (HtmlElement, 1);
          element->left = left_tag;
          element->right = right_tag;
        }
      else
        free_html_tag (right_tag);
    }

  return element;
}

static HtmlElement *
get_html_element_parent (HtmlElement *element)
{
  g_return_val_if_fail (element != NULL, NULL);

  return get_html_element (element->right->end, FALSE);
}

void
_ide_source_view_select_tag (IdeSourceView *self,
                             gint           count,
                             gboolean       exclusive)
{
  GtkTextBuffer *buffer;
  GtkTextMark *insert_mark;
  GtkTextMark *selection_mark;
  GtkTextIter insert;
  GtkTextIter selection;
  GtkTextIter selection_left;
  HtmlElement *element;
  HtmlElement *element_parent;

  g_return_if_fail (IDE_IS_SOURCE_VIEW (self));

  buffer = gtk_text_view_get_buffer (GTK_TEXT_VIEW (self));
  insert_mark = gtk_text_buffer_get_insert (buffer);
  gtk_text_buffer_get_iter_at_mark (buffer, &insert, insert_mark);
  selection_mark = gtk_text_buffer_get_selection_bound (buffer);
  gtk_text_buffer_get_iter_at_mark (buffer, &selection, selection_mark);

  selection_left = selection;
  if (gtk_text_buffer_get_has_selection (buffer))
    {
      /* fix for visual mode selection and fake block cursor */
      gtk_text_iter_order (&insert, &selection_left);
      gtk_text_iter_backward_char (&selection_left);
    }

  element = get_html_element (selection_left, TRUE);
  while (element != NULL &&
         (gtk_text_iter_compare (&insert, &element->left->begin) < 0 ||
          gtk_text_iter_compare (&selection, &element->right->end) > 0))
    {
      element_parent = get_html_element_parent (element);
      free_html_element (element);
      element = element_parent;
    }

  if (element != NULL)
    {
      if (exclusive)
        gtk_text_buffer_select_range (buffer, &element->left->end, &element->right->begin);
      else
        gtk_text_buffer_select_range (buffer, &element->left->begin, &element->right->end);

      free_html_element (element);
    }
}
