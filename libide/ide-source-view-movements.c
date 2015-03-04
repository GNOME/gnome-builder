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

  g_assert (IDE_IS_SOURCE_VIEW (self));

  buffer = gtk_text_view_get_buffer (GTK_TEXT_VIEW (self));

  if (extend_selection)
    gtk_text_buffer_select_range (buffer, insert, selection);
  else
    gtk_text_buffer_select_range (buffer, insert, insert);
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
      break;

    case IDE_SOURCE_VIEW_MOVEMENT_NEXT_LINE:
      break;

    case IDE_SOURCE_VIEW_MOVEMENT_FIRST_LINE:
      break;

    case IDE_SOURCE_VIEW_MOVEMENT_NTH_LINE:
      break;

    case IDE_SOURCE_VIEW_MOVEMENT_LAST_LINE:
      break;

    case IDE_SOURCE_VIEW_MOVEMENT_LINE_PERCENTAGE:
      break;

    case IDE_SOURCE_VIEW_MOVEMENT_HALF_PAGE_UP:
      break;

    case IDE_SOURCE_VIEW_MOVEMENT_HALF_PAGE_DOWN:
      break;

    case IDE_SOURCE_VIEW_MOVEMENT_PAGE_UP:
      break;

    case IDE_SOURCE_VIEW_MOVEMENT_PAGE_DOWN:
      break;

    case IDE_SOURCE_VIEW_MOVEMENT_SCREEN_UP:
      break;

    case IDE_SOURCE_VIEW_MOVEMENT_SCREEN_DOWN:
      break;

    case IDE_SOURCE_VIEW_MOVEMENT_SCREEN_TOP:
      break;

    case IDE_SOURCE_VIEW_MOVEMENT_SCREEN_MIDDLE:
      break;

    case IDE_SOURCE_VIEW_MOVEMENT_SCREEN_BOTTOM:
      break;

    case IDE_SOURCE_VIEW_MOVEMENT_MATCH_SPECIAL:
      break;

    default:
      g_return_if_reached ();
    }
}
