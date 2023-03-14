/* ide-grid.c
 *
 * Copyright 2022-2023 Christian Hergert <chergert@redhat.com>
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

#define G_LOG_DOMAIN "ide-grid"

#include "config.h"

#include "ide-frame.h"
#include "ide-grid.h"

struct _IdeGrid
{
  PanelGrid parent_instance;
};

typedef struct
{
  IdePage *page;
  guint column;
  guint row;
  guint depth;
} PageInfo;

G_DEFINE_TYPE (IdeGrid, ide_grid, PANEL_TYPE_GRID)

static PanelFrame *
ide_grid_real_create_frame (PanelGrid *grid)
{
  g_assert (PANEL_IS_GRID (grid));

  return PANEL_FRAME (ide_frame_new ());
}

static gboolean
ide_grid_grab_focus (GtkWidget *widget)
{
  IdeGrid *self = IDE_GRID (widget);
  PanelFrame *frame;

  g_assert (IDE_IS_GRID (self));

  if ((frame = panel_grid_get_most_recent_frame (PANEL_GRID (self))))
    return gtk_widget_grab_focus (GTK_WIDGET (frame));

  return FALSE;
}

static void
ide_grid_class_init (IdeGridClass *klass)
{
  PanelGridClass *grid_class = PANEL_GRID_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  widget_class->grab_focus = ide_grid_grab_focus;

  grid_class->create_frame = ide_grid_real_create_frame;
}

static void
ide_grid_init (IdeGrid *self)
{
  PanelGridColumn *column;
  PanelFrame *row;

  column = panel_grid_get_column (PANEL_GRID (self), 0);
  row = panel_grid_column_get_row (column, 0);

  (void)row;
}

GtkWidget *
ide_grid_new (void)
{
  return g_object_new (IDE_TYPE_GRID, NULL);
}

/**
 * ide_grid_foreach_page:
 * @self: a #IdeGrid
 * @callback: (scope call): callback to execute for each page found
 * @user_data: closure data for @callback
 *
 * Calls @callback for each #IdePage found in the grid.
 */
void
ide_grid_foreach_page (IdeGrid         *self,
                       IdePageCallback  callback,
                       gpointer         user_data)
{
  g_autoptr(GArray) pages = NULL;
  guint n_columns;

  g_return_if_fail (IDE_IS_GRID (self));
  g_return_if_fail (callback != NULL);

  pages = g_array_new (FALSE, FALSE, sizeof (PageInfo));
  n_columns = panel_grid_get_n_columns (PANEL_GRID (self));

  for (guint i = 0; i < n_columns; i++)
    {
      PanelGridColumn *column = panel_grid_get_column (PANEL_GRID (self), i);
      guint n_rows = panel_grid_column_get_n_rows (column);

      for (guint j = 0; j < n_rows; j++)
        {
          PanelFrame *frame = panel_grid_column_get_row (column, j);
          guint n_pages = panel_frame_get_n_pages (frame);

          for (guint k = 0; k < n_pages; k++)
            {
              PanelWidget *widget = panel_frame_get_page (frame, k);

              if (IDE_IS_PAGE (widget))
                {
                  PageInfo info = { IDE_PAGE (widget), i, j, k };
                  g_array_append_val (pages, info);
                }
            }
        }
    }

  for (guint i = 0; i < pages->len; i++)
    {
      const PageInfo *info = &g_array_index (pages, PageInfo, i);
      callback (info->page, user_data);
    }
}

guint
ide_grid_count_pages (IdeGrid *self)
{
  guint count = 0;
  guint n_columns;

  g_return_val_if_fail (IDE_IS_GRID (self), 0);

  n_columns = panel_grid_get_n_columns (PANEL_GRID (self));

  for (guint i = 0; i < n_columns; i++)
    {
      PanelGridColumn *column = panel_grid_get_column (PANEL_GRID (self), i);
      guint n_rows = panel_grid_column_get_n_rows (column);

      for (guint j = 0; j < n_rows; j++)
        {
          PanelFrame *frame = panel_grid_column_get_row (column, j);
          guint n_pages = panel_frame_get_n_pages (frame);

          count += n_pages;
        }
    }

  return count;
}

void
ide_grid_get_page_position (IdeGrid *self,
                            IdePage *page,
                            guint   *out_column,
                            guint   *out_row,
                            guint   *out_depth)
{
  GtkWidget *frame;
  GtkWidget *column;
  GtkWidget *grid;
  guint n_pages;
  guint n_rows;
  guint n_columns;

  g_return_if_fail (IDE_IS_GRID (self));
  g_return_if_fail (IDE_IS_PAGE (self));

  *out_column = 0;
  *out_row = 0;
  *out_depth = 0;

  if (!(frame = gtk_widget_get_ancestor (GTK_WIDGET (page), PANEL_TYPE_FRAME)) ||
      !(column = gtk_widget_get_ancestor (GTK_WIDGET (frame), PANEL_TYPE_GRID_COLUMN)) ||
      !(grid = gtk_widget_get_ancestor (GTK_WIDGET (column), PANEL_TYPE_GRID)))
    return;

  n_pages = panel_frame_get_n_pages (PANEL_FRAME (frame));
  n_rows = panel_grid_column_get_n_rows (PANEL_GRID_COLUMN (column));
  n_columns = panel_grid_get_n_columns (PANEL_GRID (grid));

  for (guint i = 0; i < n_pages; i++)
    {
      PanelWidget *widget = panel_frame_get_page (PANEL_FRAME (frame), i);

      if (widget == PANEL_WIDGET (page))
        {
          *out_depth = i;
          break;
        }
    }

  for (guint i = 0; i < n_rows; i++)
    {
      if (PANEL_FRAME (frame) == panel_grid_column_get_row (PANEL_GRID_COLUMN (column), i))
        {
          *out_row = i;
          break;
        }
    }

  for (guint i = 0; i < n_columns; i++)
    {
      if (PANEL_GRID_COLUMN (column) == panel_grid_get_column (PANEL_GRID (grid), i))
        {
          *out_column = i;
          break;
        }
    }
}

/**
 * ide_grid_make_frame:
 * @self: a #IdeGrid
 * @column: the grid column index
 * @row: the grid column row index
 *
 * Retrieves or creates a frame at the column/row position.
 *
 * Returns: (transfer none): an #IdeFrame
 */
IdeFrame *
ide_grid_make_frame (IdeGrid *self,
                     guint    column,
                     guint    row)
{
  PanelGridColumn *grid_column;
  PanelFrame *frame;

  g_return_val_if_fail (IDE_IS_GRID (self), NULL);

  grid_column = panel_grid_get_column (PANEL_GRID (self), column);
  g_assert (PANEL_IS_GRID_COLUMN (grid_column));

  frame = panel_grid_column_get_row (grid_column, row);
  g_assert (IDE_IS_FRAME (frame));

  return IDE_FRAME (frame);
}
