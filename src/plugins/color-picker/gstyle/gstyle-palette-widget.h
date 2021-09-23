/* gstyle-palette-widget.h
 *
 * Copyright 2016 sebastien lafargue <slafargue@gnome.org>
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

#pragma once

#include <glib-object.h>
#include <gtk/gtk.h>

#include "gstyle-palette.h"
#include "gstyle-types.h"

G_BEGIN_DECLS

#define GSTYLE_TYPE_PALETTE_WIDGET (gstyle_palette_widget_get_type())
#define GSTYLE_TYPE_PALETTE_WIDGET_DND_LOCK_FLAGS (gstyle_palette_widget_dnd_lock_flags_get_type ())
#define GSTYLE_TYPE_PALETTE_WIDGET_VIEW_MODE (gstyle_palette_widget_view_mode_get_type())
#define GSTYLE_TYPE_PALETTE_WIDGET_SORT_MODE (gstyle_palette_widget_sort_mode_get_type())

G_DECLARE_FINAL_TYPE (GstylePaletteWidget, gstyle_palette_widget, GSTYLE, PALETTE_WIDGET, GtkBin)

typedef enum
{
  GSTYLE_PALETTE_WIDGET_DND_LOCK_FLAGS_NONE = 0,
  GSTYLE_PALETTE_WIDGET_DND_LOCK_FLAGS_DRAG = 1 << 0,
  GSTYLE_PALETTE_WIDGET_DND_LOCK_FLAGS_DROP = 2 << 0,
  GSTYLE_PALETTE_WIDGET_DND_LOCK_FLAGS_ALL  =
    (GSTYLE_PALETTE_WIDGET_DND_LOCK_FLAGS_DRAG |
     GSTYLE_PALETTE_WIDGET_DND_LOCK_FLAGS_DROP)
} GstylePaletteWidgetDndLockFlags;

typedef enum
{
  GSTYLE_PALETTE_WIDGET_VIEW_MODE_LIST,
  GSTYLE_PALETTE_WIDGET_VIEW_MODE_SWATCHS
} GstylePaletteWidgetViewMode;

typedef enum
{
  GSTYLE_PALETTE_WIDGET_SORT_MODE_ORIGINAL,
  GSTYLE_PALETTE_WIDGET_SORT_MODE_LIGHT,
  GSTYLE_PALETTE_WIDGET_SORT_MODE_APPROCHING
} GstylePaletteWidgetSortMode;

GType                            gstyle_palette_widget_dnd_lock_flags_get_type   (void);
GType                            gstyle_palette_widget_view_mode_get_type        (void);
GType                            gstyle_palette_widget_sort_mode_get_type        (void);

gboolean                         gstyle_palette_widget_add                       (GstylePaletteWidget             *self,
                                                                                  GstylePalette                   *palette);
GPtrArray                       *gstyle_palette_widget_fuzzy_parse_color_string  (GstylePaletteWidget             *self,
                                                                                  const gchar                     *color_string);
GstylePaletteWidgetDndLockFlags  gstyle_palette_widget_get_dnd_lock              (GstylePaletteWidget             *self);
GList                           *gstyle_palette_widget_get_list                  (GstylePaletteWidget             *self);
gint                             gstyle_palette_widget_get_n_palettes            (GstylePaletteWidget             *self);
GstylePalette                   *gstyle_palette_widget_get_palette_at_index      (GstylePaletteWidget             *self,
                                                                                  guint                            index);
GstylePalette                   *gstyle_palette_widget_get_palette_by_id         (GstylePaletteWidget             *self,
                                                                                  const gchar                     *id);
GtkWidget                       *gstyle_palette_widget_get_placeholder           (GstylePaletteWidget             *self);
GstylePalette                   *gstyle_palette_widget_get_selected_palette      (GstylePaletteWidget             *self);
GstylePaletteWidgetSortMode      gstyle_palette_widget_get_sort_mode             (GstylePaletteWidget             *self);
GListStore                      *gstyle_palette_widget_get_store                 (GstylePaletteWidget             *self);
GstylePaletteWidgetViewMode      gstyle_palette_widget_get_view_mode             (GstylePaletteWidget             *self);
gboolean                         gstyle_palette_widget_remove                    (GstylePaletteWidget             *self,
                                                                                  GstylePalette                   *palette);
void                             gstyle_palette_widget_remove_all                (GstylePaletteWidget             *self);
gboolean                         gstyle_palette_widget_remove_by_id              (GstylePaletteWidget             *self,
                                                                                  const gchar                     *id);
void                             gstyle_palette_widget_set_dnd_lock              (GstylePaletteWidget             *self,
                                                                                  GstylePaletteWidgetDndLockFlags  flags);
void                             gstyle_palette_widget_set_placeholder           (GstylePaletteWidget             *self,
                                                                                  GtkWidget                       *placeholder);
void                             gstyle_palette_widget_set_sort_mode             (GstylePaletteWidget             *self,
                                                                                  GstylePaletteWidgetSortMode      mode);
void                             gstyle_palette_widget_set_view_mode             (GstylePaletteWidget             *self,
                                                                                  GstylePaletteWidgetViewMode      mode);
gboolean                         gstyle_palette_widget_show_palette              (GstylePaletteWidget             *self,
                                                                                  GstylePalette                   *palette);
GstylePaletteWidget             *gstyle_palette_widget_new                       (void);

G_END_DECLS
