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

#include "ide-source-view-movements.h"

void
_ide_source_view_apply_movement (IdeSourceView         *self,
                                 IdeSourceViewMovement  movement,
                                 gboolean               extend_selection,
                                 gint                   param)
{
  g_assert (IDE_IS_SOURCE_VIEW (self));

  switch (movement)
    {
    case IDE_SOURCE_VIEW_MOVEMENT_NTH_CHAR:
      break;

    case IDE_SOURCE_VIEW_MOVEMENT_PREVIOUS_CHAR:
      break;

    case IDE_SOURCE_VIEW_MOVEMENT_NEXT_CHAR:
      break;

    case IDE_SOURCE_VIEW_MOVEMENT_FIRST_CHAR:
      break;

    case IDE_SOURCE_VIEW_MOVEMENT_FIRST_NONSPACE_CHAR:
      break;

    case IDE_SOURCE_VIEW_MOVEMENT_MIDDLE_CHAR:
      break;

    case IDE_SOURCE_VIEW_MOVEMENT_LAST_CHAR:
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
