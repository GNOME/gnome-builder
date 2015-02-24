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

  GtkTextView   *text_view;
  guint          text_view_notify_buffer;

  GtkTextBuffer *buffer;
  guint          buffer_notify_style_scheme;

  GdkRGBA rgba_added;
  GdkRGBA rgba_changed;

  guint rgba_added_set : 1;
  guint rgba_changed_set : 1;
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
disconnect_style_scheme (IdeLineChangeGutterRenderer *self)
{
  self->rgba_added_set = 0;
  self->rgba_changed_set = 0;
}

static void
disconnect_buffer (IdeLineChangeGutterRenderer *self)
{
  disconnect_style_scheme (self);

  if (self->buffer && self->buffer_notify_style_scheme)
    {
      g_signal_handler_disconnect (self->buffer, self->buffer_notify_style_scheme);
      self->buffer_notify_style_scheme = 0;
      ide_clear_weak_pointer (&self->buffer);
    }
}

static void
disconnect_view (IdeLineChangeGutterRenderer *self)
{
  disconnect_buffer (self);

  if (self->text_view && self->text_view_notify_buffer)
    {
      g_signal_handler_disconnect (self->text_view, self->text_view_notify_buffer);
      self->text_view_notify_buffer = 0;
      ide_clear_weak_pointer (&self->text_view);
    }
}

static void
connect_style_scheme (IdeLineChangeGutterRenderer *self)
{
  GtkTextView *text_view;
  GtkTextBuffer *buffer;
  GtkSourceStyleScheme *style_scheme;

  text_view = gtk_source_gutter_renderer_get_view (GTK_SOURCE_GUTTER_RENDERER (self));
  buffer = gtk_text_view_get_buffer (text_view);

  if (!GTK_SOURCE_IS_BUFFER (buffer))
    return;

  style_scheme = gtk_source_buffer_get_style_scheme (GTK_SOURCE_BUFFER (buffer));

  if (style_scheme)
    {
      GtkSourceStyle *style;

      style = gtk_source_style_scheme_get_style (style_scheme, "diff:added-line");

      if (style)
        {
          g_autofree gchar *foreground = NULL;
          gboolean foreground_set = 0;

          g_object_get (style,
                        "foreground-set", &foreground_set,
                        "foreground", &foreground,
                        NULL);

          if (foreground_set)
            self->rgba_added_set = gdk_rgba_parse (&self->rgba_added, foreground);
        }

      style = gtk_source_style_scheme_get_style (style_scheme, "diff:changed-line");

      if (style)
        {
          g_autofree gchar *foreground = NULL;
          gboolean foreground_set = 0;

          g_object_get (style,
                        "foreground-set", &foreground_set,
                        "foreground", &foreground,
                        NULL);

          if (foreground_set)
            self->rgba_changed_set = gdk_rgba_parse (&self->rgba_changed, foreground);
        }
    }
}

static void
notify_style_scheme_cb (GtkTextBuffer               *buffer,
                        GParamSpec                  *pspec,
                        IdeLineChangeGutterRenderer *self)
{
  disconnect_style_scheme (self);
  connect_style_scheme (self);
}

static void
connect_buffer (IdeLineChangeGutterRenderer *self)
{
  GtkTextBuffer *buffer;

  buffer = gtk_text_view_get_buffer (self->text_view);

  if (buffer)
    {
      ide_set_weak_pointer (&self->buffer, buffer);
      self->buffer_notify_style_scheme = g_signal_connect (buffer,
                                                           "notify::style-scheme",
                                                           G_CALLBACK (notify_style_scheme_cb),
                                                           self);
      connect_style_scheme (self);
    }
}

static void
notify_buffer_cb (GtkTextView                 *text_view,
                  GParamSpec                  *pspec,
                  IdeLineChangeGutterRenderer *self)
{
  disconnect_buffer (self);
  connect_buffer (self);
}

static void
connect_view (IdeLineChangeGutterRenderer *self)
{
  GtkTextView *view;

  view = gtk_source_gutter_renderer_get_view (GTK_SOURCE_GUTTER_RENDERER (self));

  if (view)
    {
      ide_set_weak_pointer (&self->text_view, view);
      self->text_view_notify_buffer = g_signal_connect (self->text_view,
                                                        "notify::buffer",
                                                        G_CALLBACK (notify_buffer_cb),
                                                        self);
      connect_buffer (self);
    }
}

static void
ide_line_change_gutter_renderer_notify_view (IdeLineChangeGutterRenderer *self)
{
  disconnect_view (self);
  connect_view (self);
}

static void
ide_line_change_gutter_renderer_draw (GtkSourceGutterRenderer      *renderer,
                                      cairo_t                      *cr,
                                      GdkRectangle                 *bg_area,
                                      GdkRectangle                 *cell_area,
                                      GtkTextIter                  *begin,
                                      GtkTextIter                  *end,
                                      GtkSourceGutterRendererState  state)
{
  IdeLineChangeGutterRenderer *self = (IdeLineChangeGutterRenderer *)renderer;
  GtkTextBuffer *buffer;
  GdkRGBA *rgba = NULL;
  IdeBufferLineFlags flags;
  guint lineno;

  g_return_if_fail (IDE_IS_LINE_CHANGE_GUTTER_RENDERER (self));
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
    rgba = self->rgba_added_set ? &self->rgba_added : &gRgbaAdded;

  if ((flags & IDE_BUFFER_LINE_FLAGS_CHANGED) != 0)
    rgba = self->rgba_changed_set ? &self->rgba_changed : &gRgbaChanged;

  if (rgba)
    {
      gdk_cairo_rectangle (cr, cell_area);
      gdk_cairo_set_source_rgba (cr, rgba);
      cairo_fill (cr);
    }
}

static void
ide_line_change_gutter_renderer_dispose (GObject *object)
{
  disconnect_view (IDE_LINE_CHANGE_GUTTER_RENDERER (object));

  G_OBJECT_CLASS (ide_line_change_gutter_renderer_parent_class)->dispose (object);
}

static void
ide_line_change_gutter_renderer_class_init (IdeLineChangeGutterRendererClass *klass)
{
  GtkSourceGutterRendererClass *renderer_class = GTK_SOURCE_GUTTER_RENDERER_CLASS (klass);
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->dispose = ide_line_change_gutter_renderer_dispose;

  renderer_class->draw = ide_line_change_gutter_renderer_draw;

  gdk_rgba_parse (&gRgbaAdded, "#8ae234");
  gdk_rgba_parse (&gRgbaChanged, "#fcaf3e");
}

static void
ide_line_change_gutter_renderer_init (IdeLineChangeGutterRenderer *self)
{
  g_signal_connect (self,
                    "notify::view",
                    G_CALLBACK (ide_line_change_gutter_renderer_notify_view),
                    NULL);
}
