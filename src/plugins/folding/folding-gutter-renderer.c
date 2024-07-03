/*
 * folding-gutter-renderer.c
 *
 * Copyright 2024 Christian Hergert <chergert@redhat.com>
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

#include "config.h"

#include <libide-code.h>

#include "folding-buffer-addin.h"
#include "folding-gutter-renderer.h"

#define ICON_SIZE 16
#define LOVERLAP  4
#define RPAD      1

struct _FoldingGutterRenderer
{
  GtkSourceGutterRenderer  parent_instance;
  GSignalGroup            *buffer_signals;
  FoldingBufferAddin      *buffer_addin;
  GdkRGBA                  current_line;
  GdkRGBA                  background;
  GdkRGBA                  foreground;
  guint                    hl_current_line : 1;
};

G_DEFINE_FINAL_TYPE (FoldingGutterRenderer, folding_gutter_renderer, GTK_SOURCE_TYPE_GUTTER_RENDERER)

static GQuark quark_starts_region;
static GQuark quark_in_region;
static GQuark quark_ends_region;
static GtkIconPaintable *collapse_paintable;
static GtkIconPaintable *expand_paintable;

static GtkIconPaintable *
load_icon (const char *resource_path)
{
  g_autofree char *uri = g_strdup_printf ("resource://%s", resource_path);
  g_autoptr(GFile) file = g_file_new_for_uri (uri);

  return gtk_icon_paintable_new_for_file (file, ICON_SIZE, 2);
}

static void
folding_gutter_renderer_snapshot (GtkWidget   *widget,
                                  GtkSnapshot *snapshot)
{
  FoldingGutterRenderer *self = FOLDING_GUTTER_RENDERER (widget);

  gtk_snapshot_append_color (snapshot,
                             &self->background,
                             &GRAPHENE_RECT_INIT (0, 0,
                                                  gtk_widget_get_width (widget),
                                                  gtk_widget_get_height (widget)));

  GTK_WIDGET_CLASS (folding_gutter_renderer_parent_class)->snapshot (widget, snapshot);
}

static void
folding_gutter_renderer_snapshot_line (GtkSourceGutterRenderer *gutter_renderer,
                                       GtkSnapshot             *snapshot,
                                       GtkSourceGutterLines    *lines,
                                       guint                    line)
{
  FoldingGutterRenderer *self = (FoldingGutterRenderer *)gutter_renderer;
  int y;
  int height;

  g_assert (FOLDING_IS_GUTTER_RENDERER (self));
  g_assert (GTK_IS_SNAPSHOT (snapshot));
  g_assert (lines != NULL);

  if (self->buffer_addin == NULL)
    return;

  gtk_source_gutter_lines_get_line_yrange (lines, line, GTK_SOURCE_GUTTER_RENDERER_ALIGNMENT_MODE_CELL, &y, &height);

  if (self->hl_current_line &&
      gtk_source_gutter_lines_is_cursor (lines, line))
    gtk_snapshot_append_color (snapshot,
                               &self->current_line,
                               &GRAPHENE_RECT_INIT (0, y, -LOVERLAP + ICON_SIZE + RPAD, height));

  /* TODO: Track expanded status */

  if (gtk_source_gutter_lines_has_qclass (lines, line, quark_starts_region))
    {
      gtk_snapshot_save (snapshot);
      gtk_snapshot_translate (snapshot, &GRAPHENE_POINT_INIT (-LOVERLAP, y + ((height - ICON_SIZE) / 2)));
      gdk_paintable_snapshot (GDK_PAINTABLE (collapse_paintable), snapshot, ICON_SIZE, ICON_SIZE);
      gtk_snapshot_restore (snapshot);
    }
  else if (gtk_source_gutter_lines_has_qclass (lines, line, quark_ends_region))
    {
      gtk_snapshot_append_color (snapshot,
                                 &self->foreground,
                                 &GRAPHENE_RECT_INIT (-LOVERLAP + (ICON_SIZE / 2),
                                                      y,
                                                      1,
                                                      height / 2));
      gtk_snapshot_append_color (snapshot,
                                 &self->foreground,
                                 &GRAPHENE_RECT_INIT (-LOVERLAP + (ICON_SIZE / 2),
                                                      y + (height / 2),
                                                      ICON_SIZE / 4,
                                                      1));
    }
  else if (gtk_source_gutter_lines_has_qclass (lines, line, quark_in_region))
    {
      gtk_snapshot_append_color (snapshot,
                                 &self->foreground,
                                 &GRAPHENE_RECT_INIT (-LOVERLAP + (ICON_SIZE / 2),
                                                      y,
                                                      1,
                                                      height));
    }
}

static void
folding_gutter_renderer_update_buffer_addin (FoldingGutterRenderer *self)
{
  GtkSourceBuffer *buffer;
  IdeBufferAddin *buffer_addin;

  g_assert (FOLDING_IS_GUTTER_RENDERER (self));

  if (self->buffer_addin)
    {
      g_signal_handlers_disconnect_by_func (self->buffer_addin,
                                            G_CALLBACK (gtk_widget_queue_draw),
                                            self);
      g_clear_object (&self->buffer_addin);
    }

  buffer = gtk_source_gutter_renderer_get_buffer (GTK_SOURCE_GUTTER_RENDERER (self));
  g_assert (!buffer || IDE_IS_BUFFER (buffer));

  g_signal_group_set_target (self->buffer_signals, buffer);

  if (buffer == NULL)
    return;

  buffer_addin = ide_buffer_addin_find_by_module_name (IDE_BUFFER (buffer), "folding");
  g_assert (!buffer_addin || FOLDING_IS_BUFFER_ADDIN (buffer_addin));

  if (buffer_addin == NULL)
    return;

  g_set_object (&self->buffer_addin, FOLDING_BUFFER_ADDIN (buffer_addin));

  g_signal_connect_object (buffer_addin,
                           "invalidated",
                           G_CALLBACK (gtk_widget_queue_draw),
                           self,
                           G_CONNECT_SWAPPED);
}

static void
folding_gutter_renderer_change_buffer (GtkSourceGutterRenderer *gutter_renderer,
                                       GtkSourceBuffer         *buffer)
{
  FoldingGutterRenderer *self = (FoldingGutterRenderer *)gutter_renderer;

  g_assert (FOLDING_IS_GUTTER_RENDERER (self));
  g_assert (!buffer || IDE_IS_BUFFER (buffer));

  folding_gutter_renderer_update_buffer_addin (self);
}

static void
folding_gutter_renderer_foreach_cb (guint              line,
                                    IdeFoldRegionFlags flags,
                                    gpointer           user_data)
{
  GtkSourceGutterLines *lines = user_data;

  if (flags & IDE_FOLD_REGION_FLAGS_STARTS_REGION)
    gtk_source_gutter_lines_add_qclass (lines, line, quark_starts_region);

  if (flags & IDE_FOLD_REGION_FLAGS_ENDS_REGION)
    gtk_source_gutter_lines_add_qclass (lines, line, quark_ends_region);

  if (flags & IDE_FOLD_REGION_FLAGS_IN_REGION)
    gtk_source_gutter_lines_add_qclass (lines, line, quark_in_region);
}

static void
folding_gutter_renderer_begin (GtkSourceGutterRenderer *gutter_renderer,
                               GtkSourceGutterLines    *lines)
{
  FoldingGutterRenderer *self = (FoldingGutterRenderer *)gutter_renderer;
  IdeFoldRegions *regions;
  GtkSourceView *view;

  g_assert (FOLDING_IS_GUTTER_RENDERER (self));
  g_assert (GTK_SOURCE_IS_GUTTER_LINES (lines));

  if (self->buffer_addin == NULL)
    return;

  if (!(regions = folding_buffer_addin_get_fold_regions (self->buffer_addin)))
    return;

  if (!(view = gtk_source_gutter_renderer_get_view (GTK_SOURCE_GUTTER_RENDERER (self))))
    return;

  self->hl_current_line = gtk_source_view_get_highlight_current_line (view);

  G_GNUC_BEGIN_IGNORE_DEPRECATIONS {
    GtkStyleContext *style_context;

    style_context = gtk_widget_get_style_context (GTK_WIDGET (self));
    gtk_style_context_get_color (style_context, &self->foreground);

    style_context = gtk_widget_get_style_context (GTK_WIDGET (self));
    gtk_style_context_lookup_color (style_context, "scheme_text_bg", &self->background);
    gtk_style_context_lookup_color (style_context, "scheme_current_line_bg", &self->current_line);
  } G_GNUC_END_IGNORE_DEPRECATIONS

  ide_fold_regions_foreach_in_range (regions,
                                     gtk_source_gutter_lines_get_first (lines),
                                     gtk_source_gutter_lines_get_last (lines),
                                     folding_gutter_renderer_foreach_cb,
                                     lines);

}

static void
folding_gutter_renderer_query_data (GtkSourceGutterRenderer *gutter_renderer,
                                    GtkSourceGutterLines    *lines,
                                    guint                    line)
{
}

static gboolean
folding_gutter_renderer_query_activatable (GtkSourceGutterRenderer *gutter_renderer,
                                           GtkTextIter             *iter,
                                           GdkRectangle            *rectangle)
{
  return TRUE;
}

static void
folding_gutter_renderer_activate (GtkSourceGutterRenderer *gutter_renderer,
                                  GtkTextIter             *iter,
                                  GdkRectangle            *area,
                                  guint                    button,
                                  GdkModifierType          state,
                                  gint                     n_presses)
{
  FoldingGutterRenderer *self = (FoldingGutterRenderer *)gutter_renderer;
  GtkSourceBuffer *buffer;
  guint line;

  g_assert (FOLDING_IS_GUTTER_RENDERER (self));
  g_assert (iter != NULL);

  if (n_presses != 1)
    return;

  if (!(buffer = gtk_source_gutter_renderer_get_buffer (gutter_renderer)))
    return;

  line = gtk_text_iter_get_line (iter);

  ide_buffer_toggle_fold_at_line (IDE_BUFFER (buffer), line);
}

static void
folding_gutter_renderer_root (GtkWidget *widget)
{
  FoldingGutterRenderer *self = (FoldingGutterRenderer *)widget;

  g_assert (FOLDING_IS_GUTTER_RENDERER (self));

  GTK_WIDGET_CLASS (folding_gutter_renderer_parent_class)->root (widget);

  folding_gutter_renderer_update_buffer_addin (self);
}

static void
folding_gutter_renderer_dispose (GObject *object)
{
  FoldingGutterRenderer *self = (FoldingGutterRenderer *)object;

  g_clear_object (&self->buffer_addin);
  g_clear_object (&self->buffer_signals);

  G_OBJECT_CLASS (folding_gutter_renderer_parent_class)->dispose (object);
}

static void
folding_gutter_renderer_class_init (FoldingGutterRendererClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);
  GtkSourceGutterRendererClass *gutter_renderer_class = GTK_SOURCE_GUTTER_RENDERER_CLASS (klass);

  object_class->dispose = folding_gutter_renderer_dispose;

  widget_class->root = folding_gutter_renderer_root;
  widget_class->snapshot = folding_gutter_renderer_snapshot;

  gutter_renderer_class->activate = folding_gutter_renderer_activate;
  gutter_renderer_class->begin = folding_gutter_renderer_begin;
  gutter_renderer_class->change_buffer = folding_gutter_renderer_change_buffer;
  gutter_renderer_class->query_activatable = folding_gutter_renderer_query_activatable;
  gutter_renderer_class->query_data = folding_gutter_renderer_query_data;
  gutter_renderer_class->snapshot_line = folding_gutter_renderer_snapshot_line;

  quark_starts_region = g_quark_from_static_string ("folding-starts-region");
  quark_ends_region = g_quark_from_static_string ("folding-ends-region");
  quark_in_region = g_quark_from_static_string ("folding-in-region");

  collapse_paintable = load_icon ("/plugins/folding/icons/folding-collapse-symbolic.svg");
  expand_paintable = load_icon ("/plugins/folding/icons/folding-expand-symbolic.svg");

  g_assert (collapse_paintable != NULL);
  g_assert (expand_paintable != NULL);
}

static void
folding_gutter_renderer_init (FoldingGutterRenderer *self)
{
  gtk_widget_set_size_request (GTK_WIDGET (self), -LOVERLAP + ICON_SIZE + RPAD, -1);
  gtk_widget_add_css_class (GTK_WIDGET (self), "folding");

  self->buffer_signals = g_signal_group_new (IDE_TYPE_BUFFER);
  g_signal_group_connect_object (self->buffer_signals,
                                 "cursor-moved",
                                 G_CALLBACK (gtk_widget_queue_draw),
                                 self,
                                 G_CONNECT_SWAPPED | G_CONNECT_AFTER);
  g_signal_group_connect_object (self->buffer_signals,
                                 "changed",
                                 G_CALLBACK (gtk_widget_queue_draw),
                                 self,
                                 G_CONNECT_SWAPPED | G_CONNECT_AFTER);
}

FoldingGutterRenderer *
folding_gutter_renderer_new (void)
{
  return g_object_new (FOLDING_TYPE_GUTTER_RENDERER, NULL);
}
