/* rg-line-renderer.h
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

#ifndef RG_LINE_RENDERER_H
#define RG_LINE_RENDERER_H

#include <gdk/gdk.h>

#include "rg-renderer.h"

G_BEGIN_DECLS

#define RG_TYPE_LINE_RENDERER (rg_line_renderer_get_type())

G_DECLARE_FINAL_TYPE (RgLineRenderer, rg_line_renderer, RG, LINE_RENDERER, GObject)

RgLineRenderer *rg_line_renderer_new (void);
void            rg_line_renderer_set_stroke_color      (RgLineRenderer *self,
                                                        const gchar    *stroke_color);
void            rg_line_renderer_set_stroke_color_rgba (RgLineRenderer *self,
                                                        const GdkRGBA  *stroke_color_rgba);
const GdkRGBA  *rg_line_renderer_get_stroke_color_rgba (RgLineRenderer *self);

G_END_DECLS

#endif /* RG_LINE_RENDERER_H */
