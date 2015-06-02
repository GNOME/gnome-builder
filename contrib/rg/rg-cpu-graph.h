/* rg-cpu-graph.h
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

#ifndef RG_CPU_GRAPH_H
#define RG_CPU_GRAPH_H

#include "rg-graph.h"

G_BEGIN_DECLS

#define RG_TYPE_CPU_GRAPH (rg_cpu_graph_get_type())

G_DECLARE_FINAL_TYPE (RgCpuGraph, rg_cpu_graph, RG, CPU_GRAPH, RgGraph)

GtkWidget *rg_cpu_graph_new (void);

G_END_DECLS

#endif /* RG_CPU_GRAPH_H */
