/* gb-tab-grid.h
 *
 * Copyright (C) 2011 Christian Hergert <chris@dronelabs.com>
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

#ifndef GB_TAB_GRID_H
#define GB_TAB_GRID_H

#include <gtk/gtk.h>

#include "gb-tab.h"

G_BEGIN_DECLS

#define GB_TYPE_TAB_GRID            (gb_tab_grid_get_type())
#define GB_TAB_GRID(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), GB_TYPE_TAB_GRID, GbTabGrid))
#define GB_TAB_GRID_CONST(obj)      (G_TYPE_CHECK_INSTANCE_CAST ((obj), GB_TYPE_TAB_GRID, GbTabGrid const))
#define GB_TAB_GRID_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass),  GB_TYPE_TAB_GRID, GbTabGridClass))
#define GB_IS_TAB_GRID(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GB_TYPE_TAB_GRID))
#define GB_IS_TAB_GRID_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass),  GB_TYPE_TAB_GRID))
#define GB_TAB_GRID_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj),  GB_TYPE_TAB_GRID, GbTabGridClass))

typedef struct _GbTabGrid        GbTabGrid;
typedef struct _GbTabGridClass   GbTabGridClass;
typedef struct _GbTabGridPrivate GbTabGridPrivate;

struct _GbTabGrid
{
   GtkBin parent;

   /*< private >*/
   GbTabGridPrivate *priv;
};

struct _GbTabGridClass
{
   GtkBinClass parent_class;
};

GtkWidget    *gb_tab_grid_new                (void);
GType         gb_tab_grid_get_type           (void) G_GNUC_CONST;
GbTab        *gb_tab_grid_get_active         (GbTabGrid    *grid);
void          gb_tab_grid_focus_tab          (GbTabGrid    *grid,
                                              GbTab        *tab);
void          gb_tab_grid_move_tab_left      (GbTabGrid    *grid,
                                              GbTab        *tab);
void          gb_tab_grid_move_tab_right     (GbTabGrid    *grid,
                                              GbTab        *tab);
void          gb_tab_grid_focus_next_tab     (GbTabGrid    *grid,
                                              GbTab        *tab);
void          gb_tab_grid_focus_previous_tab (GbTabGrid    *grid,
                                              GbTab        *tab);
GbTab        *gb_tab_grid_find_tab_typed     (GbTabGrid    *grid,
                                              GType         type);
GtkTreeModel *gb_tab_grid_get_model          (GbTabGrid    *grid);
void          gb_tab_grid_set_model          (GbTabGrid    *grid,
                                              GtkTreeModel *model);

G_END_DECLS

#endif /* GB_TAB_GRID_H */
