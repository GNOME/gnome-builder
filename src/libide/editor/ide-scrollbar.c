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
#define SCROLLBAR_H_MARGIN 6

struct _IdeScrollbar {
  GtkWidget          parent_instance;

  GtkScrollbar      *scrollbar;

  IdeSourceView     *view;
  IdeBuffer         *buffer;

  GSignalGroup      *buffer_signals;
  GSignalGroup      *monitor_signals;
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

G_DEFINE_TYPE_WITH_CODE(IdeScrollbar, ide_scrollbar,
                        GTK_TYPE_WIDGET, G_IMPLEMENT_INTERFACE(GTK_TYPE_BUILDABLE, NULL))

enum {
    PROP_0,
    PROP_VIEW,
    N_PROPERTIES
};

static GParamSpec *properties[N_PROPERTIES] = { NULL, };

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
    gdk_rgba_parse (&self->error_color, IDE_DIAGNOSTIC_FALLBACK_ERROR);
  if (!get_style_rgba (scheme, "def:error", &self->fatal_color))
    gdk_rgba_parse (&self->fatal_color, IDE_DIAGNOSTIC_FALLBACK_FATAL);
  if (!get_style_rgba (scheme, "def:warning", &self->warning_color))
    gdk_rgba_parse (&self->warning_color, IDE_DIAGNOSTIC_FALLBACK_WARNING);
  if (!get_style_rgba (scheme, "def:note", &self->deprecated_color))
    gdk_rgba_parse (&self->deprecated_color, IDE_DIAGNOSTIC_FALLBACK_DEPRECATED);

  gtk_widget_queue_draw(GTK_WIDGET(self));
}

static void
notify_style_scheme_cb (IdeBuffer             *buffer,
                        GParamSpec            *pspec,
                        IdeScrollbar *self)
{
  g_debug ("notify_style_scheme_cb");
  connect_style_scheme (self);
}

static void
notify_buffer_cb (GtkTextView *text_view,
                  GParamSpec  *pspec,
                  gpointer     user_data)
{
  IdeScrollbar *self = IDE_SCROLLBAR (user_data);

  self->buffer = IDE_BUFFER (gtk_text_view_get_buffer(text_view));

  g_signal_group_set_target (self->buffer_signals, self->buffer);

  g_object_bind_property (self->buffer, "change-monitor",
                          self->monitor_signals, "target",
                          G_BINDING_SYNC_CREATE);

  connect_style_scheme (self);
}

static void
ide_scrollbar_dispose(GObject *object)
{
  IdeScrollbar *self;

  self = IDE_SCROLLBAR (object);
  g_clear_object (&self->view);
  g_clear_object (&self->buffer);

  G_OBJECT_CLASS (ide_scrollbar_parent_class)->dispose(object);
}

static void
snapshot_changed_chunk (IdeScrollbar *self,
                        IdeBufferLineChange    change,
                        GtkSnapshot           *snapshot,
                        double                 start_y,
                        double                 end_y,
                        double                 width)
{
  const GdkRGBA *color = NULL;
  double height;

  if (change == 0)
    return;

  if (change & IDE_BUFFER_LINE_CHANGE_ADDED)
    color = &self->add_color;
  else if (change & IDE_BUFFER_LINE_CHANGE_CHANGED)
    color = &self->change_color;
  else if (change & IDE_BUFFER_LINE_CHANGE_DELETED)
    color = &self->remove_color;

  if (color && end_y > start_y)
    {
      height = (int)MAX (end_y - start_y, 1);
      gtk_snapshot_append_color (snapshot, color,
                                 &GRAPHENE_RECT_INIT(0, (int)start_y, width, height));
    }
}

static void
ide_scrollbar_snapshot (GtkWidget  *widget,
                        GtkSnapshot *snapshot)
{
  IdeScrollbar *self = IDE_SCROLLBAR(widget);
  IdeBufferChangeMonitor *monitor;
  IdeDiagnostics *diagnostics;
  int total_lines;
  int line;
  GtkAllocation allocation;
  double width;
  double height;
  double content_height;
  double view_height;
  double top_margin;
  double bottom_margin;
  double ratio;
  int cursor_position;
  int cursor_line;
  GtkTextIter cursor_iter;
  IdeBufferLineChange last_change = 0;
  double chunk_start_y = 0;
  double line_height;
  int quantized_line_h;
  gboolean first_line = TRUE;

  g_return_if_fail (self->buffer);

  monitor = ide_buffer_get_change_monitor (self->buffer);

  total_lines = gtk_text_buffer_get_line_count (GTK_TEXT_BUFFER(self->buffer));

  gtk_widget_get_allocation(widget, &allocation);
  height = allocation.height;
  width  = allocation.width;

  view_height = gtk_adjustment_get_upper (gtk_scrollable_get_vadjustment (GTK_SCROLLABLE (self->view)));
  ratio = height / view_height;
  top_margin = ratio * gtk_text_view_get_top_margin (GTK_TEXT_VIEW (self->view)) + SCROLLBAR_V_MARGIN;
  bottom_margin = ratio * gtk_text_view_get_bottom_margin (GTK_TEXT_VIEW (self->view)) + SCROLLBAR_V_MARGIN;
  content_height = height - top_margin - bottom_margin;

  line_height = content_height / total_lines;

  quantized_line_h = (int)MAX (line_height, 1);

  /* Draw cursor */
  g_object_get (self->buffer, "cursor-position", &cursor_position, NULL);
  gtk_text_buffer_get_iter_at_offset (GTK_TEXT_BUFFER (self->buffer), &cursor_iter, cursor_position);
  cursor_line = gtk_text_iter_get_line (&cursor_iter);

  if (cursor_line >= 0 && cursor_line < total_lines)
    {
      double cursor_y = top_margin + (double)cursor_line * line_height;

      gtk_snapshot_append_color (snapshot,
                                 &self->cursor_color,
                                 &GRAPHENE_RECT_INIT (0,
                                                      (int)cursor_y,
                                                      width,
                                                      quantized_line_h));
    }

  /* Draw changes */
  for (line = 0; line < total_lines; line++)
    {
      IdeBufferLineChange change;
      double line_y;

      change = ide_buffer_change_monitor_get_change (monitor, line);
      line_y = top_margin + (double)(line + 1) * line_height;

      if (first_line)
        {
          last_change = change;
          chunk_start_y = line_y;
          first_line = FALSE;
        }
      else if (change != last_change)
        {
          snapshot_changed_chunk (self,
                                  last_change,
                                  snapshot,
                                  chunk_start_y,
                                  line_y,
                                  SCROLLBAR_H_MARGIN);

          last_change = change;
          chunk_start_y = line_y;
        }
    }

  if (!first_line)
    {
      double final_y = top_margin + content_height;
      snapshot_changed_chunk (self,
                              last_change,
                              snapshot,
                              chunk_start_y,
                              final_y,
                              SCROLLBAR_H_MARGIN);
    }

  /* Draw diagnostics */
  if ((diagnostics = ide_buffer_get_diagnostics (IDE_BUFFER (self->buffer))))
    {
      const GdkRGBA *color = NULL;
      GFile *file;

      file = ide_buffer_get_file (IDE_BUFFER (self->buffer));

      for (line = 0; line < total_lines; line++)
        {
          IdeDiagnostic *diagnostic;
          IdeDiagnosticSeverity severity;
          double line_y;

          diagnostic = ide_diagnostics_get_diagnostic_at_line (diagnostics, file, line);

          if ((diagnostic == NULL) || !IDE_IS_DIAGNOSTIC (diagnostic))
            continue;

          severity = ide_diagnostic_get_severity (diagnostic);
          line_y = top_margin + (double)line * line_height;

          switch (severity)
            {
            case IDE_DIAGNOSTIC_WARNING:
              color = &self->warning_color;
              break;
            case IDE_DIAGNOSTIC_ERROR:
              color = &self->error_color;
              break;
            case IDE_DIAGNOSTIC_FATAL:
              color = &self->fatal_color;
              break;
            case IDE_DIAGNOSTIC_DEPRECATED:
            case IDE_DIAGNOSTIC_IGNORED:
            case IDE_DIAGNOSTIC_NOTE:
            case IDE_DIAGNOSTIC_UNUSED:
            default:
              break;
            }

          if (color != NULL)
            gtk_snapshot_append_color (snapshot,
                                       color,
                                       &GRAPHENE_RECT_INIT (width - SCROLLBAR_H_MARGIN,
                                                            (int)line_y,
                                                            SCROLLBAR_H_MARGIN,
                                                            quantized_line_h));
        }
    }

  gtk_widget_snapshot_child (GTK_WIDGET (self), GTK_WIDGET (self->scrollbar), snapshot);
}
void
ide_scrollbar_set_view (IdeScrollbar *self,
                        IdeSourceView         *view)
{
  GtkAdjustment *v_adjustment;

  g_return_if_fail (IDE_IS_SCROLLBAR (self));
  g_return_if_fail (GTK_SOURCE_IS_VIEW (view));

  g_set_object(&self->view, view);

  v_adjustment = gtk_scrollable_get_vadjustment (GTK_SCROLLABLE (self->view));
  gtk_scrollbar_set_adjustment (self->scrollbar, v_adjustment);

  self->buffer = IDE_BUFFER (gtk_text_view_get_buffer (GTK_TEXT_VIEW (view)));

  g_signal_connect_object (view,
                           "notify::buffer",
                           G_CALLBACK(notify_buffer_cb),
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

    switch (prop_id) {
        case PROP_VIEW:
            ide_scrollbar_set_view (self, g_value_get_object(value));
            break;
        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
    }
}

static void
ide_scrollbar_get_property (GObject    *object,
                            guint       prop_id,
                            GValue     *value,
                            GParamSpec *pspec)
{
    IdeScrollbar *self = IDE_SCROLLBAR (object);

    switch (prop_id) {
        case PROP_VIEW:
            g_value_set_object(value, self->view);
            break;
        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
    }
}

static void
ide_scrollbar_class_init(IdeScrollbarClass *klass)
{
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class = G_OBJECT_CLASS(klass);
  widget_class = GTK_WIDGET_CLASS(klass);

  object_class->set_property = ide_scrollbar_set_property;
  object_class->get_property = ide_scrollbar_get_property;
  object_class->dispose = ide_scrollbar_dispose;
  widget_class->snapshot = ide_scrollbar_snapshot;

  properties[PROP_VIEW] =
      g_param_spec_object("view", NULL, NULL,
                          IDE_TYPE_SOURCE_VIEW,
                          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);

  g_object_class_install_properties(object_class,
                                    N_PROPERTIES,
                                    properties);

  gtk_widget_class_set_css_name (widget_class, "IdeChangeScrollbar");
  gtk_widget_class_set_layout_manager_type (widget_class, GTK_TYPE_BIN_LAYOUT);
  gtk_widget_class_set_template_from_resource (widget_class, "/org/gnome/libide-editor/ide-scrollbar.ui");
  gtk_widget_class_bind_template_child (widget_class, IdeScrollbar, scrollbar);
}

static void
ide_scrollbar_init(IdeScrollbar *self)
{
  gtk_widget_set_hexpand(GTK_WIDGET(self), FALSE);
  gtk_widget_set_vexpand(GTK_WIDGET(self), TRUE);

  gtk_widget_init_template (GTK_WIDGET (self));

  self->monitor_signals = g_signal_group_new (IDE_TYPE_BUFFER_CHANGE_MONITOR);

  g_signal_group_connect_object (self->monitor_signals,
                                 "changed",
                                 G_CALLBACK(gtk_widget_queue_draw),
                                 self,
                                 G_CONNECT_SWAPPED);

  self->buffer_signals = g_signal_group_new (IDE_TYPE_BUFFER);

  g_signal_group_connect_object (self->buffer_signals,
                                 "cursor-moved",
                                 G_CALLBACK(gtk_widget_queue_draw),
                                 self,
                                 G_CONNECT_SWAPPED);

  g_signal_group_connect_object (self->buffer_signals,
                                 "notify::style-scheme",
                                 G_CALLBACK(notify_style_scheme_cb),
                                 self,
                                 0);

  self->view_signals = g_signal_group_new (IDE_TYPE_SOURCE_VIEW);

  g_signal_group_connect_object (self->view_signals,
                                 "notify::bottom-margin",
                                 G_CALLBACK(gtk_widget_queue_draw),
                                 self,
                                 G_CONNECT_SWAPPED);

  g_signal_group_connect_object (self->view_signals,
                                 "notify::top-margin",
                                 G_CALLBACK(gtk_widget_queue_draw),
                                 self,
                                 G_CONNECT_SWAPPED);
}
