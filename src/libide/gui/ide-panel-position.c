/* ide-panel-position.c
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

#define G_LOG_DOMAIN "ide-panel-position"

#include "config.h"

#include "ide-panel-position.h"

struct _IdePanelPosition
{
  guint column : 8;
  guint row : 8;
  guint depth : 9;
  PanelDockPosition edge : 3;
  guint column_set : 1;
  guint row_set : 1;
  guint depth_set : 1;
  guint edge_set : 1;
};

G_DEFINE_BOXED_TYPE (IdePanelPosition, ide_panel_position, ide_panel_position_ref, ide_panel_position_unref)

IdePanelPosition *
ide_panel_position_new (void)
{
  return g_rc_box_alloc0 (sizeof (IdePanelPosition));
}

IdePanelPosition *
ide_panel_position_ref (IdePanelPosition *self)
{
  return g_rc_box_acquire (self);
}

void
ide_panel_position_unref (IdePanelPosition *self)
{
  g_rc_box_release (self);
}

gboolean
ide_panel_position_get_edge (IdePanelPosition  *self,
                             PanelDockPosition *edge)
{
  g_return_val_if_fail (self != NULL, FALSE);

  if (edge != NULL)
    *edge = self->edge;

  return self->edge_set;
}

void
ide_panel_position_set_edge (IdePanelPosition  *self,
                             PanelDockPosition  edge)
{
  g_return_if_fail (self != NULL);

  self->edge = edge;
  self->edge_set = TRUE;
}

gboolean
ide_panel_position_get_column (IdePanelPosition *self,
                               guint            *column)
{
  g_return_val_if_fail (self != NULL, FALSE);

  if (column != NULL)
    *column = self->column;

  return self->column_set;
}

void
ide_panel_position_set_column (IdePanelPosition *self,
                               guint             column)
{
  g_return_if_fail (self != NULL);

  self->column = column;
  self->column_set = TRUE;
}

gboolean
ide_panel_position_get_row (IdePanelPosition *self,
                            guint            *row)
{
  g_return_val_if_fail (self != NULL, FALSE);

  if (row != NULL)
    *row = self->row;

  return self->row_set;
}

void
ide_panel_position_set_row (IdePanelPosition *self,
                            guint             row)
{
  g_return_if_fail (self != NULL);

  self->row = row;
  self->row_set = TRUE;
}

gboolean
ide_panel_position_get_depth (IdePanelPosition *self,
                              guint            *depth)
{
  g_return_val_if_fail (self != NULL, FALSE);

  if (depth != NULL)
    *depth = self->depth;

  return self->depth_set;
}

void
ide_panel_position_set_depth (IdePanelPosition *self,
                              guint             depth)
{
  g_return_if_fail (self != NULL);

  self->depth = depth;
  self->depth_set = TRUE;
}
