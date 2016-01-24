/* rg-renderer.c
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

#include <cairo.h>

#include "rg-renderer.h"

G_DEFINE_INTERFACE (RgRenderer, rg_renderer, G_TYPE_OBJECT)

static void
dummy_render (RgRenderer                  *renderer,
              RgTable                     *table,
              gint64                       x_begin,
              gint64                       x_end,
              gdouble                      y_begin,
              gdouble                      y_end,
              cairo_t                     *cr,
              const cairo_rectangle_int_t *area)
{
}

static void
rg_renderer_default_init (RgRendererInterface *iface)
{
  iface->render = dummy_render;
}

void
rg_renderer_render (RgRenderer                  *self,
                    RgTable                     *table,
                    gint64                       x_begin,
                    gint64                       x_end,
                    gdouble                      y_begin,
                    gdouble                      y_end,
                    cairo_t                     *cr,
                    const cairo_rectangle_int_t *area)
{
  g_return_if_fail (RG_IS_RENDERER (self));
  g_return_if_fail (cr != NULL);
  g_return_if_fail (area != NULL);

  RG_RENDERER_GET_IFACE (self)->render (self, table, x_begin, x_end, y_begin, y_end, cr, area);
}
