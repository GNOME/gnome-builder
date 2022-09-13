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

/**
 * ide_panel_position_get_area:
 * @self: a #PanelPosition
 * @area: (out) (nullable): a location for the area
 *
 * Returns: %TRUE if the area was set
 */
gboolean
ide_panel_position_get_area (PanelPosition *self,
                             PanelArea     *area)
{
  g_return_val_if_fail (PANEL_IS_POSITION (self), FALSE);

  if (area != NULL)
    *area = panel_position_get_area (self);

  return panel_position_get_area_set (self);
}

/**
 * ide_panel_position_get_column:
 * @self: a #PanelPosition
 * @column: (out) (nullable): a location for a column
 *
 * Returns: %TRUE if the column was set
 */
gboolean
ide_panel_position_get_column (PanelPosition *self,
                               guint         *column)
{
  g_return_val_if_fail (self != NULL, FALSE);

  if (column != NULL)
    *column = panel_position_get_column (self);

  return panel_position_get_column_set (self);
}

/**
 * ide_panel_position_get_row:
 * @self: a #PanelPosition
 * @row: (out) (nullable): a location for the row
 *
 * Returns: %TRUE if the row was set
 */
gboolean
ide_panel_position_get_row (PanelPosition *self,
                            guint         *row)
{
  g_return_val_if_fail (self != NULL, FALSE);

  if (row != NULL)
    *row = panel_position_get_row (self);

  return panel_position_get_row_set (self);
}

/**
 * ide_panel_position_get_depth:
 * @self: a #PanelPosition
 * @depth: (out) (nullable): a location for the depth
 *
 * Returns: %TRUE if the depth was set
 */
gboolean
ide_panel_position_get_depth (PanelPosition *self,
                              guint         *depth)
{
  g_return_val_if_fail (self != NULL, FALSE);

  if (depth != NULL)
    *depth = panel_position_get_depth (self);

  return panel_position_get_depth_set (self);
}
