/* rg-renderer.h
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

#ifndef RG_RENDERER_H
#define RG_RENDERER_H

#include <glib-object.h>

#include "rg-table.h"

G_BEGIN_DECLS

#define RG_TYPE_RENDERER (rg_renderer_get_type ())

G_DECLARE_INTERFACE (RgRenderer, rg_renderer, RG, RENDERER, GObject)

struct _RgRendererInterface
{
  GTypeInterface parent;

  void (*render) (RgRenderer                  *self,
                  RgTable                     *table,
                  gint64                       x_begin,
                  gint64                       x_end,
                  gdouble                      y_begin,
                  gdouble                      y_end,
                  cairo_t                     *cr,
                  const cairo_rectangle_int_t *area);
};

void rg_renderer_render (RgRenderer                  *self,
                         RgTable                     *table,
                         gint64                       x_begin,
                         gint64                       x_end,
                         gdouble                      y_begin,
                         gdouble                      y_end,
                         cairo_t                     *cr,
                         const cairo_rectangle_int_t *area);

G_END_DECLS

#endif /* RG_RENDERER_H */
