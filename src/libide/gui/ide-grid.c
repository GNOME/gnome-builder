/* ide-grid.c
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
  return PANEL_FRAME (ide_frame_new ());
}

static void
ide_grid_class_init (IdeGridClass *klass)
{
  PanelGridClass *grid_class = PANEL_GRID_CLASS (klass);

  grid_class->create_frame = ide_grid_real_create_frame;
}

static void
ide_grid_init (IdeGrid *self)
{
}

GtkWidget *
ide_grid_new (void)
{
  return g_object_new (IDE_TYPE_GRID, NULL);
}

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
      callback (info->page, info->column, info->row, info->depth, user_data);
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
