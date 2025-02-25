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

  GtkSourceBuffer        *buffer;
  gulong                  buffer_notify_style_scheme;

  GSignalGroup           *change_monitor_signals;

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

static GQuark added_quark;
static GQuark changed_quark;
static GQuark deleted_quark;

G_DEFINE_FINAL_TYPE (IdeLineChangeGutterRenderer, ide_line_change_gutter_renderer, GTK_SOURCE_TYPE_GUTTER_RENDERER)

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

  g_signal_group_set_target (self->change_monitor_signals, NULL);

  if (self->buffer)
    {
      g_clear_signal_handler (&self->buffer_notify_style_scheme, self->buffer);
      g_clear_weak_pointer (&self->buffer);
    }
}

static void
connect_style_scheme (IdeLineChangeGutterRenderer *self)
{
  GtkSourceStyleScheme *scheme;
  GtkTextBuffer *buffer;
  GtkSourceView *view;

  if (!(view = gtk_source_gutter_renderer_get_view (GTK_SOURCE_GUTTER_RENDERER (self))) ||
      !(buffer = gtk_text_view_get_buffer (GTK_TEXT_VIEW (view))) ||
      !GTK_SOURCE_IS_BUFFER (buffer))
    return;

  scheme = gtk_source_buffer_get_style_scheme (GTK_SOURCE_BUFFER (buffer));

  if (!get_style_rgba (scheme, "-Builder:added-line", FOREGROUND, &self->changes.add))
    gdk_rgba_parse (&self->changes.add, IDE_LINE_CHANGES_FALLBACK_ADDED);

  if (!get_style_rgba (scheme, "-Builder:changed-line", FOREGROUND, &self->changes.change))
    gdk_rgba_parse (&self->changes.change, IDE_LINE_CHANGES_FALLBACK_CHANGED);

  if (!get_style_rgba (scheme, "-Builder:removed-line", FOREGROUND, &self->changes.remove))
    gdk_rgba_parse (&self->changes.remove, IDE_LINE_CHANGES_FALLBACK_REMOVED);
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
  GtkSourceBuffer *buffer;

  g_assert (IDE_IS_LINE_CHANGE_GUTTER_RENDERER (self));

  buffer = gtk_source_gutter_renderer_get_buffer (GTK_SOURCE_GUTTER_RENDERER (self));
  if (!IDE_IS_BUFFER (buffer))
    return;

  g_set_weak_pointer (&self->buffer, buffer);
  self->buffer_notify_style_scheme =
    g_signal_connect (buffer,
                      "notify::style-scheme",
                      G_CALLBACK (notify_style_scheme_cb),
                      self);

  g_object_bind_property (buffer, "change-monitor",
                          self->change_monitor_signals, "target",
                          G_BINDING_SYNC_CREATE);

  connect_style_scheme (self);
}

static void
ide_line_change_gutter_renderer_change_buffer (GtkSourceGutterRenderer *renderer,
                                               GtkSourceBuffer         *old_buffer)
{
  disconnect_buffer (IDE_LINE_CHANGE_GUTTER_RENDERER (renderer));
  connect_buffer (IDE_LINE_CHANGE_GUTTER_RENDERER (renderer));
}

static void
populate_changes_cb (guint               line,
                     IdeBufferLineChange change,
                     gpointer            user_data)
{
  GtkSourceGutterLines *lines = user_data;

  if (line < gtk_source_gutter_lines_get_first (lines) ||
      line > gtk_source_gutter_lines_get_last (lines))
    return;

  if (change & IDE_BUFFER_LINE_CHANGE_ADDED)
    gtk_source_gutter_lines_add_qclass (lines, line, added_quark);

  if (change & IDE_BUFFER_LINE_CHANGE_CHANGED)
    gtk_source_gutter_lines_add_qclass (lines, line, changed_quark);

  if (change & IDE_BUFFER_LINE_CHANGE_DELETED)
    gtk_source_gutter_lines_add_qclass (lines, line, deleted_quark);
}

static void
ide_line_change_gutter_renderer_begin (GtkSourceGutterRenderer *renderer,
                                       GtkSourceGutterLines    *lines)
{
  IdeLineChangeGutterRenderer *self = (IdeLineChangeGutterRenderer *)renderer;
  g_autoptr(IdeBufferChangeMonitor) change_monitor = NULL;
  guint first;
  guint last;

  g_assert (IDE_IS_LINE_CHANGE_GUTTER_RENDERER (self));
  g_assert (lines != NULL);

  if (!(change_monitor = g_signal_group_dup_target (self->change_monitor_signals)))
    return;

  first = gtk_source_gutter_lines_get_first (lines);
  last = gtk_source_gutter_lines_get_last (lines);

  ide_buffer_change_monitor_foreach_change (change_monitor,
                                            first,
                                            last,
                                            populate_changes_cb,
                                            lines);
}

static void
ide_line_change_gutter_renderer_snapshot_line (GtkSourceGutterRenderer *renderer,
                                               GtkSnapshot             *snapshot,
                                               GtkSourceGutterLines    *lines,
                                               guint                    line)
{
  IdeLineChangeGutterRenderer *self = (IdeLineChangeGutterRenderer *)renderer;
  gboolean is_add;
  gboolean is_change;
  gboolean is_delete;
  double line_y;
  double line_height;
  int width;

  if (!gtk_source_gutter_lines_has_any_class (lines, line))
    return;

  is_add = gtk_source_gutter_lines_has_qclass (lines, line, added_quark);
  is_change = gtk_source_gutter_lines_has_qclass (lines, line, changed_quark);
  is_delete = gtk_source_gutter_lines_has_qclass (lines, line, deleted_quark);

  if (!is_add && !is_change && !is_delete)
    return;

  gtk_source_gutter_lines_get_line_extent (lines, line, GTK_SOURCE_GUTTER_RENDERER_ALIGNMENT_MODE_CELL, &line_y, &line_height);
  width = gtk_widget_get_width (GTK_WIDGET (renderer));

  if (is_add || is_change)
    gtk_snapshot_append_color (snapshot,
                               is_add ? &self->changes.add : &self->changes.change,
                               &GRAPHENE_RECT_INIT (0, line_y, width, line_height));

  if (!is_delete &&
      line < gtk_source_gutter_lines_get_last (lines) &&
      gtk_source_gutter_lines_has_qclass (lines, line+1, deleted_quark))
    gtk_snapshot_append_color (snapshot,
                               &self->changes.remove,
                               &GRAPHENE_RECT_INIT (0, line_y+line_height/2., width, line_height/2.));

  if (is_delete && line > 0 && gtk_source_gutter_lines_has_qclass (lines, line-1, deleted_quark))
    gtk_snapshot_append_color (snapshot,
                               &self->changes.remove,
                               &GRAPHENE_RECT_INIT (0, line_y, width, line_height/2.));
}

static void
ide_line_change_gutter_renderer_dispose (GObject *object)
{
  disconnect_buffer (IDE_LINE_CHANGE_GUTTER_RENDERER (object));

  G_OBJECT_CLASS (ide_line_change_gutter_renderer_parent_class)->dispose (object);
}

static void
ide_line_change_gutter_renderer_finalize (GObject *object)
{
  IdeLineChangeGutterRenderer *self = (IdeLineChangeGutterRenderer *)object;

  g_clear_object (&self->change_monitor_signals);

  G_OBJECT_CLASS (ide_line_change_gutter_renderer_parent_class)->finalize (object);
}

static void
ide_line_change_gutter_renderer_class_init (IdeLineChangeGutterRendererClass *klass)
{
  GtkSourceGutterRendererClass *renderer_class = GTK_SOURCE_GUTTER_RENDERER_CLASS (klass);
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->dispose = ide_line_change_gutter_renderer_dispose;
  object_class->finalize = ide_line_change_gutter_renderer_finalize;

  renderer_class->begin = ide_line_change_gutter_renderer_begin;
  renderer_class->snapshot_line = ide_line_change_gutter_renderer_snapshot_line;
  renderer_class->change_buffer = ide_line_change_gutter_renderer_change_buffer;
  renderer_class->query_data = NULL; /* opt out */

  added_quark = g_quark_from_static_string ("added");
  changed_quark = g_quark_from_static_string ("changed");
  deleted_quark = g_quark_from_static_string ("deleted");
}

static void
ide_line_change_gutter_renderer_init (IdeLineChangeGutterRenderer *self)
{
  self->change_monitor_signals = g_signal_group_new (IDE_TYPE_BUFFER_CHANGE_MONITOR);

  g_signal_group_connect_object (self->change_monitor_signals,
                                 "changed",
                                 G_CALLBACK (gtk_widget_queue_draw),
                                 self,
                                 G_CONNECT_SWAPPED);
}
