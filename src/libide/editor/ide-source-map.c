/*
 * ide-source-map.c
 *
 * Copyright 2024 Christian Hergert <chergert@redhat.com>
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

#include "config.h"

#include "ide-source-map.h"

struct _IdeSourceMap
{
  GtkSourceMap parent_instance;
};

G_DEFINE_FINAL_TYPE (IdeSourceMap, ide_source_map, GTK_SOURCE_TYPE_MAP)

static void
ide_source_map_snapshot (GtkWidget   *widget,
                         GtkSnapshot *snapshot)
{
  GdkSurface *surface;
  GtkRoot *root;

  g_assert (IDE_IS_SOURCE_MAP (widget));
  g_assert (GTK_IS_SNAPSHOT (snapshot));

  /* Try to clip the outer edge on fractional scaling so that we don't
   * end up damaging the shadows causing additonal draw/blends.
   *
   * Ideally, GTK would handle this for us by having shadows cached
   * but that is not the case (yet) on NGL/Vulkan.
   *
   * This helps avoid a number of frame dropouts.
   */

  if ((root = gtk_widget_get_root (widget)) &&
      GTK_IS_NATIVE (root) &&
      (surface = gtk_native_get_surface (GTK_NATIVE (root))) &&
      (double)gdk_surface_get_scale (surface) != gdk_surface_get_scale_factor (surface))
    {
      graphene_rect_t rect;
      int w = gtk_widget_get_width (widget);
      int h = gtk_widget_get_height (widget);

      if (gtk_widget_get_direction (widget) == GTK_TEXT_DIR_RTL)
        rect = GRAPHENE_RECT_INIT (1, 1, w-1, h-2);
      else
        rect = GRAPHENE_RECT_INIT (0, 1, w-1, h-2);

      gtk_snapshot_push_clip (snapshot, &rect);
      GTK_WIDGET_CLASS (ide_source_map_parent_class)->snapshot (widget, snapshot);
      gtk_snapshot_pop (snapshot);
    }
  else
    {
      GTK_WIDGET_CLASS (ide_source_map_parent_class)->snapshot (widget, snapshot);
    }
}

static void
ide_source_map_class_init (IdeSourceMapClass *klass)
{
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  widget_class->snapshot = ide_source_map_snapshot;
}

static void
ide_source_map_init (IdeSourceMap *self)
{
}
