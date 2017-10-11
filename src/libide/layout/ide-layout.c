/* ide-layout.c
 *
 * Copyright © 2015 Christian Hergert <christian@hergert.me>
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
 */

#define G_LOG_DOMAIN "ide-layout"

#include "layout/ide-layout.h"
#include "layout/ide-layout-pane.h"
#include "layout/ide-layout-view.h"

G_DEFINE_TYPE (IdeLayout, ide_layout, DZL_TYPE_DOCK_BIN)

static GtkWidget *
ide_layout_create_edge (DzlDockBin      *dock,
                        GtkPositionType  edge)
{
  g_assert (IDE_IS_LAYOUT (dock));

  return g_object_new (IDE_TYPE_LAYOUT_PANE,
                       "edge", edge,
                       "visible", TRUE,
                       "reveal-child", FALSE,
                       NULL);
}

static void
ide_layout_class_init (IdeLayoutClass *klass)
{
  DzlDockBinClass *dock_bin_class = DZL_DOCK_BIN_CLASS (klass);

  dock_bin_class->create_edge = ide_layout_create_edge;
}

static void
ide_layout_init (IdeLayout *self)
{
}
