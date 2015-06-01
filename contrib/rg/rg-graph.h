/* rg-graph.h
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

#ifndef RG_GRAPH_H
#define RG_GRAPH_H

#include <gtk/gtk.h>

#include "rg-table.h"
#include "rg-renderer.h"

G_BEGIN_DECLS

#define RG_TYPE_GRAPH (rg_graph_get_type())

G_DECLARE_DERIVABLE_TYPE (RgGraph, rg_graph, RG, GRAPH, GtkDrawingArea)

struct _RgGraphClass
{
  GtkDrawingAreaClass parent_class;
  gpointer padding[8];
};

GtkWidget *rg_graph_new          (void);
void       rg_graph_set_table    (RgGraph *self,
                                  RgTable *table);
RgTable   *rg_graph_get_table    (RgGraph *self);
void       rg_graph_add_renderer (RgGraph    *self,
                                  RgRenderer *renderer);

G_END_DECLS

#endif /* RG_GRAPH_H */
