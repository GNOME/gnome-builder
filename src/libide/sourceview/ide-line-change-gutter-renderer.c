/* ide-line-change-gutter-renderer.c
 *
 * Copyright 2015 Christian Hergert <christian@hergert.me>
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

#define G_LOG_DOMAIN "ide-line-change-gutter-renderer"

#include "config.h"

#include <dazzle.h>

#include "ide-context.h"

#include "buffers/ide-buffer.h"
#include "files/ide-file.h"
#include "sourceview/ide-line-change-gutter-renderer.h"
#include "vcs/ide-vcs.h"

#define DELETE_WIDTH  5.0
#define DELETE_HEIGHT 8.0

#if 0
# define ARROW_TOWARDS_GUTTER
#endif

struct _IdeLineChangeGutterRenderer
{
  GtkSourceGutterRenderer parent_instance;

  GtkTextView            *text_view;
  gulong                  text_view_notify_buffer;

  GtkTextBuffer          *buffer;
  gulong                  buffer_notify_style_scheme;

  GdkRGBA                 rgba_added;
  GdkRGBA                 rgba_changed;
  GdkRGBA                 rgba_removed;

  guint                   show_line_deletions : 1;

  guint                   rgba_added_set : 1;
  guint                   rgba_changed_set : 1;
  guint                   rgba_removed_set : 1;
};


G_DEFINE_TYPE (IdeLineChangeGutterRenderer,
               ide_line_change_gutter_renderer,
               GTK_SOURCE_TYPE_GUTTER_RENDERER)

static GdkRGBA rgbaAdded;
static GdkRGBA rgbaChanged;
static GdkRGBA rgbaRemoved;

enum {
  PROP_0,
  PROP_SHOW_LINE_DELETIONS,
  LAST_PROP
};

static GParamSpec *properties [LAST_PROP];

static void
disconnect_style_scheme (IdeLineChangeGutterRenderer *self)
{
  self->rgba_added_set = 0;
  self->rgba_changed_set = 0;
  self->rgba_removed_set = 0;
}

static void
disconnect_buffer (IdeLineChangeGutterRenderer *self)
{
  disconnect_style_scheme (self);

  if (self->buffer && self->buffer_notify_style_scheme)
    {
      g_signal_handler_disconnect (self->buffer, self->buffer_notify_style_scheme);
      self->buffer_notify_style_scheme = 0;
      g_clear_weak_pointer (&self->buffer);
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
      g_clear_weak_pointer (&self->text_view);
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

      style = gtk_source_style_scheme_get_style (style_scheme, "gutter:added-line");

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

      style = gtk_source_style_scheme_get_style (style_scheme, "gutter:changed-line");

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

      style = gtk_source_style_scheme_get_style (style_scheme, "gutter:removed-line");

      if (style)
        {
          g_autofree gchar *foreground = NULL;
          gboolean foreground_set = 0;

          g_object_get (style,
                        "foreground-set", &foreground_set,
                        "foreground", &foreground,
                        NULL);

          if (foreground_set)
            self->rgba_removed_set = gdk_rgba_parse (&self->rgba_removed, foreground);
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
      g_set_weak_pointer (&self->buffer, buffer);
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
      g_set_weak_pointer (&self->text_view, view);
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
  GdkRectangle cell_area_copy;
  GtkTextBuffer *buffer;
  GdkRGBA *rgba = NULL;
  IdeBufferLineFlags flags;
  IdeBufferLineFlags prev_flags = 0;
  IdeBufferLineFlags next_flags;
  guint lineno;
  gint xpad;

  g_return_if_fail (IDE_IS_LINE_CHANGE_GUTTER_RENDERER (self));
  g_return_if_fail (cr);
  g_return_if_fail (bg_area);
  g_return_if_fail (cell_area);
  g_return_if_fail (begin);
  g_return_if_fail (end);

  GTK_SOURCE_GUTTER_RENDERER_CLASS (ide_line_change_gutter_renderer_parent_class)->draw (renderer, cr, bg_area, cell_area, begin, end, state);

  buffer = gtk_text_iter_get_buffer (begin);

  if (!IDE_IS_BUFFER (buffer))
    return;

  lineno = gtk_text_iter_get_line (begin);

  flags = ide_buffer_get_line_flags (IDE_BUFFER (buffer), lineno);
  next_flags = ide_buffer_get_line_flags (IDE_BUFFER (buffer), lineno + 1);
  if (lineno > 0)
    prev_flags = ide_buffer_get_line_flags (IDE_BUFFER (buffer), lineno - 1);

  if ((flags & IDE_BUFFER_LINE_FLAGS_ADDED) != 0)
    rgba = self->rgba_added_set ? &self->rgba_added : &rgbaAdded;

  if ((flags & IDE_BUFFER_LINE_FLAGS_CHANGED) != 0)
    rgba = self->rgba_changed_set ? &self->rgba_changed : &rgbaChanged;

  if (rgba)
    {
      gdk_cairo_rectangle (cr, cell_area);
      gdk_cairo_set_source_rgba (cr, rgba);
      cairo_fill (cr);
    }

  if (!self->show_line_deletions)
    return;

  /*
   * If we have xpad, we want to draw over it. So we'll just mutate
   * the cell_area here.
   */
  g_object_get (self, "xpad", &xpad, NULL);
  cell_area_copy = *cell_area;
  cell_area->x += xpad;

  /*
   * If the next line is a deletion, but we were not a deletion, then
   * draw our half the deletion mark.
   */
  if (((next_flags & IDE_BUFFER_LINE_FLAGS_DELETED) != 0) &&
      ((flags & IDE_BUFFER_LINE_FLAGS_DELETED) == 0))
    {
      rgba = self->rgba_removed_set ? &self->rgba_removed : &rgbaRemoved;
      gdk_cairo_set_source_rgba (cr, rgba);

#ifdef ARROW_TOWARDS_GUTTER
      cairo_move_to (cr,
                     cell_area->x + cell_area->width,
                     cell_area->y + cell_area->height);
      cairo_line_to (cr,
                     cell_area->x + cell_area->width - DELETE_WIDTH,
                     cell_area->y + cell_area->height);
      cairo_line_to (cr,
                     cell_area->x + cell_area->width,
                     cell_area->y + cell_area->height - (DELETE_HEIGHT / 2));
      cairo_line_to (cr,
                     cell_area->x + cell_area->width,
                     cell_area->y + cell_area->height);
#else
      cairo_move_to (cr,
                     cell_area->x + cell_area->width,
                     cell_area->y + cell_area->height);
      cairo_line_to (cr,
                     cell_area->x + cell_area->width - DELETE_WIDTH,
                     cell_area->y + cell_area->height);
      cairo_line_to (cr,
                     cell_area->x + cell_area->width - DELETE_WIDTH,
                     cell_area->y + cell_area->height - (DELETE_HEIGHT / 2));
      cairo_line_to (cr,
                     cell_area->x + cell_area->width,
                     cell_area->y + cell_area->height);
#endif

      cairo_fill (cr);
    }

  /*
   * If the previous line was not a deletion, and we have a deletion, then
   * draw our half the deletion mark.
   */
  if (((prev_flags & IDE_BUFFER_LINE_FLAGS_DELETED) == 0) &&
      ((flags & IDE_BUFFER_LINE_FLAGS_DELETED) != 0))
    {
      rgba = self->rgba_removed_set ? &self->rgba_removed : &rgbaRemoved;
      gdk_cairo_set_source_rgba (cr, rgba);

#ifdef ARROW_TOWARDS_GUTTER
      cairo_move_to (cr,
                     cell_area->x + cell_area->width,
                     cell_area->y);
      cairo_line_to (cr,
                     cell_area->x + cell_area->width,
                     cell_area->y + (DELETE_HEIGHT / 2));
      cairo_line_to (cr,
                     cell_area->x + cell_area->width - DELETE_WIDTH,
                     cell_area->y);
      cairo_line_to (cr,
                     cell_area->x + cell_area->width,
                     cell_area->y);
#else
      cairo_move_to (cr,
                     cell_area->x + cell_area->width,
                     cell_area->y);
      cairo_line_to (cr,
                     cell_area->x + cell_area->width - DELETE_WIDTH,
                     cell_area->y);
      cairo_line_to (cr,
                     cell_area->x + cell_area->width - DELETE_WIDTH,
                     cell_area->y + (DELETE_HEIGHT / 2));
      cairo_line_to (cr,
                     cell_area->x + cell_area->width,
                     cell_area->y);
#endif

      cairo_fill (cr);
    }

  *cell_area = cell_area_copy;
}

static void
ide_line_change_gutter_renderer_dispose (GObject *object)
{
  disconnect_view (IDE_LINE_CHANGE_GUTTER_RENDERER (object));

  G_OBJECT_CLASS (ide_line_change_gutter_renderer_parent_class)->dispose (object);
}

static void
ide_line_change_gutter_renderer_get_property (GObject    *object,
                                              guint       prop_id,
                                              GValue     *value,
                                              GParamSpec *pspec)
{
  IdeLineChangeGutterRenderer *self = IDE_LINE_CHANGE_GUTTER_RENDERER(object);

  switch (prop_id)
    {
    case PROP_SHOW_LINE_DELETIONS:
      g_value_set_boolean (value, self->show_line_deletions);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
    }
}

static void
ide_line_change_gutter_renderer_set_property (GObject      *object,
                                              guint         prop_id,
                                              const GValue *value,
                                              GParamSpec   *pspec)
{
  IdeLineChangeGutterRenderer *self = IDE_LINE_CHANGE_GUTTER_RENDERER(object);

  switch (prop_id)
    {
    case PROP_SHOW_LINE_DELETIONS:
      self->show_line_deletions = g_value_get_boolean (value);
      gtk_source_gutter_renderer_queue_draw (GTK_SOURCE_GUTTER_RENDERER (self));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
    }
}

static void
ide_line_change_gutter_renderer_class_init (IdeLineChangeGutterRendererClass *klass)
{
  GtkSourceGutterRendererClass *renderer_class = GTK_SOURCE_GUTTER_RENDERER_CLASS (klass);
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->dispose = ide_line_change_gutter_renderer_dispose;
  object_class->get_property = ide_line_change_gutter_renderer_get_property;
  object_class->set_property = ide_line_change_gutter_renderer_set_property;

  renderer_class->draw = ide_line_change_gutter_renderer_draw;

  properties [PROP_SHOW_LINE_DELETIONS] =
    g_param_spec_boolean ("show-line-deletions",
                          "Show Line Deletions",
                          "If the deletion mark should be shown for deleted lines",
                          FALSE,
                          (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, LAST_PROP, properties);

  gdk_rgba_parse (&rgbaAdded, "#8ae234");
  gdk_rgba_parse (&rgbaChanged, "#fcaf3e");
  gdk_rgba_parse (&rgbaRemoved, "#ff0000");
}

static void
ide_line_change_gutter_renderer_init (IdeLineChangeGutterRenderer *self)
{
  g_signal_connect (self,
                    "notify::view",
                    G_CALLBACK (ide_line_change_gutter_renderer_notify_view),
                    NULL);
}
