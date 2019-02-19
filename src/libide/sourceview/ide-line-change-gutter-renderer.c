/* ide-line-change-gutter-renderer.c
 *
 * Copyright 2015-2019 Christian Hergert <christian@hergert.me>
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

#define G_LOG_DOMAIN "ide-line-change-gutter-renderer"

#include "config.h"

#include <libide-code.h>

#include "ide-line-change-gutter-renderer.h"

#define DELETE_WIDTH  5.0
#define DELETE_HEIGHT 8.0

#define IS_LINE_CHANGE(i) ((i)->is_add || (i)->is_change || \
                           (i)->is_delete || (i)->is_next_delete || (i)->is_prev_delete)

struct _IdeLineChangeGutterRenderer
{
  GtkSourceGutterRenderer parent_instance;

  GtkTextView            *text_view;
  gulong                  text_view_notify_buffer;

  GtkTextBuffer          *buffer;
  gulong                  buffer_notify_style_scheme;

  GArray                 *lines;
  guint                   begin_line;

  struct {
    GdkRGBA add;
    GdkRGBA remove;
    GdkRGBA change;
  } changes;

  guint                   rgba_added_set : 1;
  guint                   rgba_changed_set : 1;
  guint                   rgba_removed_set : 1;
};

typedef struct
{
  /* The line is an addition to the buffer */
  guint is_add : 1;

  /* The line has changed in the buffer */
  guint is_change : 1;

  /* The line is part of a deleted range in the buffer */
  guint is_delete : 1;

  /* The previous line was a delete */
  guint is_prev_delete : 1;

  /* The next line is a delete */
  guint is_next_delete : 1;
} LineInfo;

enum {
  FOREGROUND,
  BACKGROUND,
};

G_DEFINE_TYPE (IdeLineChangeGutterRenderer, ide_line_change_gutter_renderer, GTK_SOURCE_TYPE_GUTTER_RENDERER)

static gboolean
get_style_rgba (GtkSourceStyleScheme *scheme,
                const gchar          *style_name,
                int                   type,
                GdkRGBA              *rgba)
{
  GtkSourceStyle *style;

  g_assert (!scheme || GTK_SOURCE_IS_STYLE_SCHEME (scheme));
  g_assert (style_name != NULL);
  g_assert (type == FOREGROUND || type == BACKGROUND);
  g_assert (rgba != NULL);

  memset (rgba, 0, sizeof *rgba);

  if (scheme == NULL)
    return FALSE;

  if (NULL != (style = gtk_source_style_scheme_get_style (scheme, style_name)))
    {
      g_autofree gchar *str = NULL;
      gboolean set = FALSE;

      g_object_get (style,
                    type ? "background" : "foreground", &str,
                    type ? "background-set" : "foreground-set", &set,
                    NULL);

      if (str != NULL)
        gdk_rgba_parse (rgba, str);

      return set;
    }

  return FALSE;
}

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
  GtkSourceStyleScheme *scheme;
  GtkTextBuffer *buffer;
  GtkTextView *view;

  if (!(view = gtk_source_gutter_renderer_get_view (GTK_SOURCE_GUTTER_RENDERER (self))) ||
      !(buffer = gtk_text_view_get_buffer (view)) ||
      !GTK_SOURCE_IS_BUFFER (buffer))
    return;

  scheme = gtk_source_buffer_get_style_scheme (GTK_SOURCE_BUFFER (buffer));

  if (!get_style_rgba (scheme, "gutter::added-line", FOREGROUND, &self->changes.add))
    gdk_rgba_parse (&self->changes.add, "#8ae234");

  if (!get_style_rgba (scheme, "gutter::changed-line", FOREGROUND, &self->changes.change))
    gdk_rgba_parse (&self->changes.change, "#fcaf3e");

  if (!get_style_rgba (scheme, "gutter::removed-line", FOREGROUND, &self->changes.remove))
    gdk_rgba_parse (&self->changes.remove, "#ef2929");
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
populate_changes_cb (guint               line,
                     IdeBufferLineChange change,
                     gpointer            user_data)
{
  LineInfo *info;
  struct {
    GArray *lines;
    guint   begin_line;
    guint   end_line;
  } *state = user_data;
  guint pos;

  g_assert (line >= state->begin_line);
  g_assert (line <= state->end_line);

  pos = line - state->begin_line;

  info = &g_array_index (state->lines, LineInfo, pos);
  info->is_add = !!(change & IDE_BUFFER_LINE_CHANGE_ADDED);
  info->is_change = !!(change & IDE_BUFFER_LINE_CHANGE_CHANGED);
  info->is_delete = !!(change & IDE_BUFFER_LINE_CHANGE_DELETED);

  if (pos > 0)
    {
      LineInfo *last = &g_array_index (state->lines, LineInfo, pos - 1);

      info->is_prev_delete = last->is_delete;
      last->is_next_delete = info->is_delete;
    }
}

static void
ide_line_change_gutter_renderer_begin (GtkSourceGutterRenderer *renderer,
                                       cairo_t                 *cr,
                                       GdkRectangle            *bg_area,
                                       GdkRectangle            *cell_area,
                                       GtkTextIter             *begin,
                                       GtkTextIter             *end)
{
  IdeLineChangeGutterRenderer *self = (IdeLineChangeGutterRenderer *)renderer;
  IdeBufferChangeMonitor *monitor;
  GtkTextBuffer *buffer;
  GtkTextView *view;
  struct {
    GArray *lines;
    guint   begin_line;
    guint   end_line;
  } state;

  g_assert (IDE_IS_LINE_CHANGE_GUTTER_RENDERER (self));
  g_assert (cr != NULL);
  g_assert (bg_area != NULL);
  g_assert (cell_area != NULL);
  g_assert (begin != NULL);
  g_assert (end != NULL);

  if (!(view = gtk_source_gutter_renderer_get_view (renderer)) ||
      !(buffer = gtk_text_view_get_buffer (view)) ||
      !IDE_IS_BUFFER (buffer) ||
      !(monitor = ide_buffer_get_change_monitor (IDE_BUFFER (buffer))))
    return;

  self->begin_line = state.begin_line = gtk_text_iter_get_line (begin);
  state.end_line = gtk_text_iter_get_line (end);
  state.lines = g_array_new (FALSE, TRUE, sizeof (LineInfo));
  g_array_set_size (state.lines, state.end_line - state.begin_line + 1);

  ide_buffer_change_monitor_foreach_change (monitor,
                                            state.begin_line,
                                            state.end_line,
                                            populate_changes_cb,
                                            &state);

  g_clear_pointer (&self->lines, g_array_unref);
  self->lines = g_steal_pointer (&state.lines);
}

static void
draw_line_change (IdeLineChangeGutterRenderer  *self,
                  cairo_t                      *cr,
                  GdkRectangle                 *area,
                  LineInfo                     *info,
                  GtkSourceGutterRendererState  state)
{
  g_assert (IDE_IS_LINE_CHANGE_GUTTER_RENDERER (self));
  g_assert (cr != NULL);
  g_assert (area != NULL);

  /*
   * Draw a simple line with the appropriate color from the style scheme
   * based on the type of change we have.
   */

  if (info->is_add || info->is_change)
    {
      gdk_cairo_rectangle (cr, area);

      if (info->is_add)
        gdk_cairo_set_source_rgba (cr, &self->changes.add);
      else
        gdk_cairo_set_source_rgba (cr, &self->changes.change);

      cairo_fill (cr);
    }

  if (info->is_next_delete && !info->is_delete)
    {
      cairo_rectangle (cr,
                       area->x,
                       area->y + area->width / 2.0,
                       area->width,
                       area->height / 2.0);
      gdk_cairo_set_source_rgba (cr, &self->changes.remove);
      cairo_fill (cr);
    }

  if (info->is_delete && !info->is_prev_delete)
    {
      cairo_rectangle (cr,
                       area->x,
                       area->y,
                       area->width,
                       area->height / 2.0);
      gdk_cairo_set_source_rgba (cr, &self->changes.remove);
      cairo_fill (cr);
    }
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
  guint line;

  g_assert (IDE_IS_LINE_CHANGE_GUTTER_RENDERER (self));
  g_assert (cr != NULL);
  g_assert (bg_area != NULL);
  g_assert (cell_area != NULL);
  g_assert (begin != NULL);
  g_assert (end != NULL);

  if (self->lines == NULL)
    return;

  line = gtk_text_iter_get_line (begin);

  if ((line - self->begin_line) < self->lines->len)
    {
      LineInfo *info = &g_array_index (self->lines, LineInfo, line - self->begin_line);

      if (IS_LINE_CHANGE (info))
        draw_line_change (self, cr, cell_area, info, state);
    }
}

static void
ide_line_change_gutter_renderer_dispose (GObject *object)
{
  IdeLineChangeGutterRenderer *self = (IdeLineChangeGutterRenderer *)object;

  disconnect_view (IDE_LINE_CHANGE_GUTTER_RENDERER (object));

  g_clear_pointer (&self->lines, g_array_unref);

  G_OBJECT_CLASS (ide_line_change_gutter_renderer_parent_class)->dispose (object);
}

static void
ide_line_change_gutter_renderer_class_init (IdeLineChangeGutterRendererClass *klass)
{
  GtkSourceGutterRendererClass *renderer_class = GTK_SOURCE_GUTTER_RENDERER_CLASS (klass);
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->dispose = ide_line_change_gutter_renderer_dispose;

  renderer_class->begin = ide_line_change_gutter_renderer_begin;
  renderer_class->draw = ide_line_change_gutter_renderer_draw;
}

static void
ide_line_change_gutter_renderer_init (IdeLineChangeGutterRenderer *self)
{
  g_signal_connect (self,
                    "notify::view",
                    G_CALLBACK (ide_line_change_gutter_renderer_notify_view),
                    NULL);
}
