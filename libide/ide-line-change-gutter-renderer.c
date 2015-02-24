/* ide-line-change-gutter-renderer.c
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

#include "ide-buffer.h"
#include "ide-context.h"
#include "ide-file.h"
#include "ide-line-change-gutter-renderer.h"
#include "ide-vcs.h"

struct _IdeLineChangeGutterRenderer
{
  GtkSourceGutterRenderer parent_instance;
};

struct _IdeLineChangeGutterRendererClass
{
  GtkSourceGutterRendererClass parent;
};

G_DEFINE_TYPE (IdeLineChangeGutterRenderer,
               ide_line_change_gutter_renderer,
               GTK_SOURCE_TYPE_GUTTER_RENDERER)

static GdkRGBA gRgbaAdded;
static GdkRGBA gRgbaChanged;

static void
ide_line_change_gutter_renderer_draw (GtkSourceGutterRenderer      *renderer,
                                      cairo_t                      *cr,
                                      GdkRectangle                 *bg_area,
                                      GdkRectangle                 *cell_area,
                                      GtkTextIter                  *begin,
                                      GtkTextIter                  *end,
                                      GtkSourceGutterRendererState  state)
{
  GtkTextBuffer *buffer;
  GdkRGBA *rgba = NULL;
  IdeBufferLineFlags flags;
  guint lineno;

  g_return_if_fail (IDE_IS_LINE_CHANGE_GUTTER_RENDERER (renderer));
  g_return_if_fail (cr);
  g_return_if_fail (bg_area);
  g_return_if_fail (cell_area);
  g_return_if_fail (begin);
  g_return_if_fail (end);

  GTK_SOURCE_GUTTER_RENDERER_CLASS (ide_line_change_gutter_renderer_parent_class)->
    draw (renderer, cr, bg_area, cell_area, begin, end, state);

  buffer = gtk_text_iter_get_buffer (begin);

  if (!IDE_IS_BUFFER (buffer))
    return;

  lineno = gtk_text_iter_get_line (begin);
  flags = ide_buffer_get_line_flags (IDE_BUFFER (buffer), lineno);

  if ((flags & IDE_BUFFER_LINE_FLAGS_ADDED) != 0)
    rgba = &gRgbaAdded;

  if ((flags & IDE_BUFFER_LINE_FLAGS_CHANGED) != 0)
    rgba = &gRgbaChanged;

  if (rgba)
    {
      gdk_cairo_rectangle (cr, cell_area);
      gdk_cairo_set_source_rgba (cr, rgba);
      cairo_fill (cr);
    }
}

static void
ide_line_change_gutter_renderer_class_init (IdeLineChangeGutterRendererClass *klass)
{
  GtkSourceGutterRendererClass *renderer_class = GTK_SOURCE_GUTTER_RENDERER_CLASS (klass);

  renderer_class->draw = ide_line_change_gutter_renderer_draw;

  gdk_rgba_parse (&gRgbaAdded, "#8ae234");
  gdk_rgba_parse (&gRgbaChanged, "#fcaf3e");
}

static void
ide_line_change_gutter_renderer_init (IdeLineChangeGutterRenderer *self)
{
}
