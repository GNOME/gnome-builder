/*
 * ide-scrollbar.c
 *
 * Copyright 2025 Nokse <nokse@posteo.com>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this library; if not, see <http://www.gnu.org/licenses/>.
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#define G_LOG_DOMAIN "ide-scrollbar"

#include "ide-scrollbar.h"
#include <libide-code.h>
#include <libide-sourceview.h>
#include <gtksourceview/gtksource.h>
#include <math.h>

#define SCROLLBAR_V_MARGIN 6
#define SCROLLBAR_H_MARGIN 7
#define SLIDER_W 3

#define FALLBACK_ERROR      "#ff4444"
#define FALLBACK_FATAL      "#cc0000"
#define FALLBACK_WARNING    "#ffaa00"
#define FALLBACK_DEPRECATED "#8888ff"

typedef enum {
  LINE_CHANGED,
  LINE_ADDED,
  LINE_DELETED,
  LINE_ERROR,
  LINE_FATAL,
  LINE_WARNING,
  LINE_DEPRECATED
} ChunkType;

typedef struct {
  guint     start_line;
  guint     end_line;
  ChunkType line_type;
} LinesChunk;

struct _IdeScrollbar {
  GtkWidget          parent_instance;

  GtkScrollbar      *scrollbar;

  GArray            *chunks;

  IdeSourceView     *view;
  IdeBuffer         *buffer;

  GSignalGroup      *monitor_signals;
  GSignalGroup      *buffer_signals;
  GSignalGroup      *view_signals;

  GdkRGBA            add_color;
  GdkRGBA            change_color;
  GdkRGBA            remove_color;
  GdkRGBA            cursor_color;
  GdkRGBA            error_color;
  GdkRGBA            fatal_color;
  GdkRGBA            warning_color;
  GdkRGBA            deprecated_color;
};

G_DEFINE_TYPE_WITH_CODE (IdeScrollbar, ide_scrollbar, GTK_TYPE_WIDGET,
                         G_IMPLEMENT_INTERFACE (GTK_TYPE_BUILDABLE, NULL))

enum {
  PROP_0,
  PROP_VIEW,
  N_PROPS
};

static GParamSpec *properties [N_PROPS];

static gboolean
get_style_rgba (GtkSourceStyleScheme *scheme,
                const gchar          *style_name,
                GdkRGBA              *rgba)
{
  GtkSourceStyle *style;

  g_assert (!scheme || GTK_SOURCE_IS_STYLE_SCHEME (scheme));
  g_assert (style_name != NULL);
  g_assert (rgba != NULL);

  memset (rgba, 0, sizeof *rgba);

  if (scheme == NULL)
    return FALSE;

  if (NULL != (style = gtk_source_style_scheme_get_style (scheme, style_name)))
    {
      g_autofree gchar *str = NULL;
      gboolean set = FALSE;

      g_object_get (style,
                    "foreground", &str,
                    "foreground-set", &set,
                    NULL);

      if (str != NULL)
        gdk_rgba_parse (rgba, str);

      return set;
    }

  return FALSE;
}

static void
connect_style_scheme (IdeScrollbar *self)
{
  GtkSourceStyleScheme *scheme;
  if ((self->buffer == NULL) ||
      !GTK_SOURCE_IS_BUFFER (self->buffer))
    return;
  scheme = gtk_source_buffer_get_style_scheme (GTK_SOURCE_BUFFER (self->buffer));

  if (!get_style_rgba (scheme, "diff:added-line", &self->add_color))
    gdk_rgba_parse (&self->add_color, IDE_LINE_CHANGES_FALLBACK_ADDED);
  if (!get_style_rgba (scheme, "diff:changed-line", &self->change_color))
    gdk_rgba_parse (&self->change_color, IDE_LINE_CHANGES_FALLBACK_CHANGED);
  if (!get_style_rgba (scheme, "diff:removed-line", &self->remove_color))
    gdk_rgba_parse (&self->remove_color, IDE_LINE_CHANGES_FALLBACK_REMOVED);
  if (!get_style_rgba (scheme, "cursor", &self->cursor_color))
    gdk_rgba_parse (&self->cursor_color, IDE_LINE_CHANGES_FALLBACK_REMOVED);

  if (!get_style_rgba (scheme, "def:error", &self->error_color))
    gdk_rgba_parse (&self->error_color, FALLBACK_ERROR);
  if (!get_style_rgba (scheme, "def:error", &self->fatal_color))
    gdk_rgba_parse (&self->fatal_color, FALLBACK_FATAL);
  if (!get_style_rgba (scheme, "def:warning", &self->warning_color))
    gdk_rgba_parse (&self->warning_color, FALLBACK_WARNING);
  if (!get_style_rgba (scheme, "def:note", &self->deprecated_color))
    gdk_rgba_parse (&self->deprecated_color, FALLBACK_DEPRECATED);

  gtk_widget_queue_draw (GTK_WIDGET (self));
}

static void
notify_style_scheme_cb (IdeBuffer    *buffer,
                        GParamSpec   *pspec,
                        IdeScrollbar *self)
{
  connect_style_scheme (self);
}

static void
notify_buffer_cb (GtkTextView *text_view,
                  GParamSpec  *pspec,
                  gpointer     user_data)
{
  IdeScrollbar *self = IDE_SCROLLBAR (user_data);

  self->buffer = IDE_BUFFER (gtk_text_view_get_buffer (text_view));

  g_signal_group_set_target (self->buffer_signals, self->buffer);

  g_object_bind_property (self->buffer, "change-monitor",
                          self->monitor_signals, "target",
                          G_BINDING_SYNC_CREATE);

  connect_style_scheme (self);
}

static void
ide_scrollbar_dispose (GObject *object)
{
  IdeScrollbar *self = (IdeScrollbar *)object;

  g_clear_object (&self->view);
  g_clear_object (&self->buffer);
  g_clear_object (&self->monitor_signals);
  g_clear_object (&self->buffer_signals);
  g_clear_object (&self->view_signals);
  g_clear_pointer (&self->chunks, g_array_unref);

  g_clear_pointer ((GtkWidget **)&self->scrollbar, gtk_widget_unparent);

  G_OBJECT_CLASS (ide_scrollbar_parent_class)->dispose (object);
}

static void
snapshot_chunk (IdeScrollbar *self,
                ChunkType     chunk_type,
                int           start_y,
                int           end_y,
                double        width,
                GtkSnapshot  *snapshot)
{
  const GdkRGBA *color = NULL;
  gboolean is_change = TRUE;

  switch (chunk_type)
    {
    case LINE_ADDED:
      color = &self->add_color;
      break;

    case LINE_DELETED:
      color = &self->remove_color;
      break;

    case LINE_CHANGED:
      color = &self->change_color;
      break;

    case LINE_ERROR:
      color = &self->error_color;
      is_change = FALSE;
      break;

    case LINE_FATAL:
      color = &self->fatal_color;
      is_change = FALSE;
      break;

    case LINE_WARNING:
      color = &self->warning_color;
      is_change = FALSE;
      break;

    case LINE_DEPRECATED:
      color = &self->deprecated_color;
      is_change = FALSE;
      break;

    default:
      return;
    }

  if (color && end_y > start_y)
    {
      int height = MAX (end_y - start_y, 2);
      int chunk_width = MIN (SCROLLBAR_H_MARGIN, (width - SLIDER_W) / 2);

      gtk_snapshot_append_color (snapshot, color,
                                 &GRAPHENE_RECT_INIT (is_change ? 0 : width - chunk_width,
                                                      start_y,
                                                      chunk_width,
                                                      height));
    }
}

static void
diagnostic_line_cb (guint                 line,
                    IdeDiagnosticSeverity severity,
                    gpointer              user_data)
{
  IdeScrollbar *self = user_data;
  LinesChunk chunk = {0};

  chunk.start_line = line;
  chunk.end_line = line + 1;

  switch (severity)
    {
    case IDE_DIAGNOSTIC_WARNING:
      chunk.line_type = LINE_WARNING;
      break;

    case IDE_DIAGNOSTIC_ERROR:
      chunk.line_type = LINE_ERROR;
      break;

    case IDE_DIAGNOSTIC_FATAL:
      chunk.line_type = LINE_FATAL;
      break;

    case IDE_DIAGNOSTIC_DEPRECATED:
      chunk.line_type = LINE_DEPRECATED;
      break;

    case IDE_DIAGNOSTIC_IGNORED:
    case IDE_DIAGNOSTIC_NOTE:
    case IDE_DIAGNOSTIC_UNUSED:
    default:
      return;
    }

  g_array_append_val (self->chunks, chunk);
}

static void
ide_scrollbar_update_chunks (IdeScrollbar *self)
{
  IdeBufferChangeMonitor *monitor;
  IdeDiagnostics *diagnostics;
  int total_lines;

  g_assert (GTK_IS_TEXT_BUFFER (self->buffer));
  g_assert (self->chunks != NULL);

  /* Truncate without freeing allocation */
  self->chunks->len = 0;

  total_lines = gtk_text_buffer_get_line_count (GTK_TEXT_BUFFER (self->buffer));

  /* Update lines changes */
  if ((monitor = ide_buffer_get_change_monitor (self->buffer)))
    {
      IdeBufferLineChange last_change;
      int chunk_start_line;
      gboolean first_line = TRUE;
      guint line;

      for (line = 0; line <= total_lines; line++)
        {
          IdeBufferLineChange change;

          if (line == total_lines)
            change = IDE_BUFFER_LINE_CHANGE_NONE;
          else
            change = ide_buffer_change_monitor_get_change (monitor, line);

          if (first_line)
            {
              last_change = change;
              chunk_start_line = line;
              first_line = FALSE;
            }
          else if (change != last_change)
            {
              LinesChunk chunk = {0};
              chunk.start_line = chunk_start_line;
              chunk.end_line = line + 1;

              switch (last_change)
                {
                case IDE_BUFFER_LINE_CHANGE_ADDED:
                  chunk.line_type = LINE_ADDED;
                  break;

                case IDE_BUFFER_LINE_CHANGE_CHANGED:
                  chunk.line_type = LINE_CHANGED;
                  break;

                case IDE_BUFFER_LINE_CHANGE_DELETED:
                  chunk.line_type = LINE_DELETED;
                  break;

                case IDE_BUFFER_LINE_CHANGE_NONE:
                case IDE_BUFFER_LINE_CHANGE_PREVIOUS_DELETED:
                default:
                  last_change = change;
                  chunk_start_line = line + 1;
                  continue;
                }

              g_array_append_val (self->chunks, chunk);

              last_change = change;
              chunk_start_line = line + 1;
            }
        }
    }

  /* Update diagnostics */
  if ((diagnostics = ide_buffer_get_diagnostics (IDE_BUFFER (self->buffer))))
    {
      GFile *file;

      file = ide_buffer_get_file (IDE_BUFFER (self->buffer));

      ide_diagnostics_foreach_line_in_range (diagnostics,
                                             file,
                                             0,
                                             total_lines,
                                             diagnostic_line_cb,
                                             self);
    }

  gtk_widget_queue_draw (GTK_WIDGET (self));
}

static void
ide_scrollbar_snapshot (GtkWidget  *widget,
                        GtkSnapshot *snapshot)
{
  IdeScrollbar *self = IDE_SCROLLBAR (widget);
  GtkAllocation allocation;
  GtkTextIter cursor_iter;
  double bottom_margin;
  double view_height;
  double line_height;
  double top_margin;
  double height;
  double width;
  double ratio;
  int cursor_position;
  int total_lines;
  int cursor_line;

  g_assert (IDE_IS_SCROLLBAR (self));
  g_assert (GTK_IS_SNAPSHOT (snapshot));

  g_return_if_fail (self->buffer);

  total_lines = gtk_text_buffer_get_line_count (GTK_TEXT_BUFFER (self->buffer));

  gtk_widget_get_allocation (widget, &allocation);
  height = allocation.height;
  width  = allocation.width;

  view_height = gtk_adjustment_get_upper (gtk_scrollable_get_vadjustment (GTK_SCROLLABLE (self->view)));
  ratio = height / view_height;
  top_margin = ratio * gtk_text_view_get_top_margin (GTK_TEXT_VIEW (self->view)) + SCROLLBAR_V_MARGIN;
  bottom_margin = ratio * gtk_text_view_get_bottom_margin (GTK_TEXT_VIEW (self->view)) + SCROLLBAR_V_MARGIN;

  line_height = (height - top_margin - bottom_margin) / total_lines;

  /* Draw cursor */
  g_object_get (self->buffer, "cursor-position", &cursor_position, NULL);
  gtk_text_buffer_get_iter_at_offset (GTK_TEXT_BUFFER (self->buffer), &cursor_iter, cursor_position);
  cursor_line = gtk_text_iter_get_line (&cursor_iter);

  if (cursor_line >= 0 && cursor_line < total_lines)
    {
      int cursor_y = top_margin + cursor_line * line_height;

      gtk_snapshot_append_color (snapshot,
                                 &self->cursor_color,
                                 &GRAPHENE_RECT_INIT (0,
                                                      cursor_y,
                                                      width,
                                                      MAX ((int)line_height, 2)));
    }

  /* Draw changes and diagnostics */
  for (guint i = 0; i < self->chunks->len; i++)
    {
      const LinesChunk *chunk = &g_array_index (self->chunks, LinesChunk, i);

      int chunk_start_y = top_margin + chunk->start_line * line_height;
      int chunk_end_y   = top_margin + chunk->end_line * line_height;

      snapshot_chunk (self,
                      chunk->line_type,
                      chunk_start_y,
                      chunk_end_y,
                      width,
                      snapshot);
    }

  gtk_widget_snapshot_child (GTK_WIDGET (self), GTK_WIDGET (self->scrollbar), snapshot);
}

void
ide_scrollbar_set_view (IdeScrollbar  *self,
                        IdeSourceView *view)
{
  GtkAdjustment *v_adjustment;

  g_return_if_fail (IDE_IS_SCROLLBAR (self));
  g_return_if_fail (GTK_SOURCE_IS_VIEW (view));

  g_set_object (&self->view, view);

  v_adjustment = gtk_scrollable_get_vadjustment (GTK_SCROLLABLE (self->view));
  gtk_scrollbar_set_adjustment (self->scrollbar, v_adjustment);

  self->buffer = IDE_BUFFER (gtk_text_view_get_buffer (GTK_TEXT_VIEW (view)));

  g_signal_connect_object (view,
                           "notify::buffer",
                           G_CALLBACK (notify_buffer_cb),
                           self,
                           0);

  g_signal_group_set_target (self->view_signals, self->view);

  connect_style_scheme (self);
}

static void
ide_scrollbar_set_property (GObject      *object,
                            guint         prop_id,
                            const GValue *value,
                            GParamSpec   *pspec)
{
  IdeScrollbar *self = IDE_SCROLLBAR (object);

  switch (prop_id)
    {
    case PROP_VIEW:
      ide_scrollbar_set_view (self, g_value_get_object (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
ide_scrollbar_get_property (GObject    *object,
                            guint       prop_id,
                            GValue     *value,
                            GParamSpec *pspec)
{
  IdeScrollbar *self = IDE_SCROLLBAR (object);

  switch (prop_id)
    {
    case PROP_VIEW:
      g_value_set_object (value, self->view);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
on_motion_enter (IdeScrollbar *self)
{
  gtk_widget_add_css_class (GTK_WIDGET (self->scrollbar), "hovering");
}

static void on_motion_leave (IdeScrollbar *self)
{
  gtk_widget_remove_css_class (GTK_WIDGET (self->scrollbar), "hovering");
}

static void
ide_scrollbar_class_init (IdeScrollbarClass *klass)
{
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class = G_OBJECT_CLASS (klass);
  widget_class = GTK_WIDGET_CLASS (klass);

  object_class->set_property = ide_scrollbar_set_property;
  object_class->get_property = ide_scrollbar_get_property;
  object_class->dispose = ide_scrollbar_dispose;
  widget_class->snapshot = ide_scrollbar_snapshot;

  properties[PROP_VIEW] =
      g_param_spec_object ("view", NULL, NULL,
                           IDE_TYPE_SOURCE_VIEW,
                           G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);

  g_object_class_install_properties (object_class, N_PROPS, properties);

  gtk_widget_class_set_css_name (widget_class, "IdeScrollbar");
  gtk_widget_class_set_layout_manager_type (widget_class, GTK_TYPE_BIN_LAYOUT);
  gtk_widget_class_set_template_from_resource (widget_class, "/org/gnome/libide-editor/ide-scrollbar.ui");
  gtk_widget_class_bind_template_child (widget_class, IdeScrollbar, scrollbar);
  gtk_widget_class_bind_template_callback (widget_class, on_motion_enter);
  gtk_widget_class_bind_template_callback (widget_class, on_motion_leave);
}

static void
ide_scrollbar_init (IdeScrollbar *self)
{
  gtk_widget_init_template (GTK_WIDGET (self));

  self->chunks = g_array_new (FALSE, FALSE, sizeof (LinesChunk));

  self->monitor_signals = g_signal_group_new (IDE_TYPE_BUFFER_CHANGE_MONITOR);

  g_signal_group_connect_object (self->monitor_signals,
                                 "changed",
                                 G_CALLBACK (ide_scrollbar_update_chunks),
                                 self,
                                 G_CONNECT_SWAPPED);

  self->buffer_signals = g_signal_group_new (IDE_TYPE_BUFFER);

  g_signal_group_connect_object (self->buffer_signals,
                                 "cursor-moved",
                                 G_CALLBACK (gtk_widget_queue_draw),
                                 self,
                                 G_CONNECT_SWAPPED);

  g_signal_group_connect_object (self->buffer_signals,
                                 "notify::has-diagnostics",
                                 G_CALLBACK (ide_scrollbar_update_chunks),
                                 self,
                                 G_CONNECT_SWAPPED);

  g_signal_group_connect_object (self->buffer_signals,
                                 "notify::style-scheme",
                                 G_CALLBACK (notify_style_scheme_cb),
                                 self,
                                 0);

  self->view_signals = g_signal_group_new (IDE_TYPE_SOURCE_VIEW);

  g_signal_group_connect_object (self->view_signals,
                                 "notify::bottom-margin",
                                 G_CALLBACK (gtk_widget_queue_draw),
                                 self,
                                 G_CONNECT_SWAPPED);

  g_signal_group_connect_object (self->view_signals,
                                 "notify::top-margin",
                                 G_CALLBACK (gtk_widget_queue_draw),
                                 self,
                                 G_CONNECT_SWAPPED);
}
