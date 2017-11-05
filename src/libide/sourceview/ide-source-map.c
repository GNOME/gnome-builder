/* ide-source-map.c
 *
 * Copyright © 2015 Christian Hergert <christian@hergert.me>
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

#define G_LOG_DOMAIN "ide-source-map"

#include <dazzle.h>
#include <glib/gi18n.h>

#include "ide-macros.h"

#include "buffers/ide-buffer.h"
#include "sourceview/ide-line-change-gutter-renderer.h"
#include "sourceview/ide-source-map.h"
#include "sourceview/ide-source-view.h"

/**
 * SECTION:ide-source-map
 * @title: IdeSourceMap
 * @short_description: Source code overview minimap
 *
 * The #IdeSourceMap widget provides a minimap that displays a zoomed
 * out overview of the file in a scrollbar like interface.
 *
 * This widget was eventually merged upstream into #GtkSourceView, but
 * Builder retains a few changes which focus on the ability to auto-hide
 * the map and font rendering.
 *
 * Builder contains a custom font called "BuilderBlocks" which is used by
 * the #IdeSourceMap to render content in a simplified, blocky, style.
 *
 * Since: 3.18
 */

#define CONCEAL_TIMEOUT 2000

struct _IdeSourceMap
{
  GtkSourceMap               parent_instance;

  DzlSignalGroup            *view_signals;
  DzlSignalGroup            *buffer_signals;
  GtkSourceGutterRenderer   *line_renderer;
  guint                      delayed_conceal_timeout;
  guint                      show_map : 1;

  guint                      in_map : 1;
  guint                      in_view : 1;
};

G_DEFINE_TYPE (IdeSourceMap, ide_source_map, GTK_SOURCE_TYPE_MAP)


enum {
  SHOW_MAP,
  HIDE_MAP,
  LAST_SIGNAL
};

static guint signals [LAST_SIGNAL];

static gboolean
ide_source_map_do_conceal (gpointer data)
{
  IdeSourceMap *self = data;

  g_assert (IDE_IS_SOURCE_MAP (self));

  self->delayed_conceal_timeout = 0;

  if (self->show_map == TRUE)
    {
      self->show_map = FALSE;
      g_signal_emit (self, signals [HIDE_MAP], 0);
    }

  return G_SOURCE_REMOVE;
}

static gboolean
ide_source_map__enter_notify_event (IdeSourceMap     *self,
                                    GdkEventCrossing *event,
                                    GtkWidget        *widget)
{
  g_assert (IDE_IS_SOURCE_MAP (self));
  g_assert (event != NULL);
  g_assert (GTK_IS_WIDGET (widget));

  /* We use this same method for both the view and self,
   * so if we are hovering our map, keep track of it.
   */

  if (IDE_IS_SOURCE_MAP (widget))
    self->in_map = TRUE;

  if (IDE_IS_SOURCE_VIEW (widget))
    self->in_view = TRUE;

  if (self->show_map == FALSE)
    {
      self->show_map = TRUE;
      g_signal_emit (self, signals [SHOW_MAP], 0);
    }

  return GDK_EVENT_PROPAGATE;
}

static void
ide_source_map_show_map_and_queue_fade (IdeSourceMap *self)
{
  g_assert (IDE_IS_SOURCE_MAP (self));

  ide_clear_source (&self->delayed_conceal_timeout);

  if (self->in_map == FALSE)
    self->delayed_conceal_timeout = g_timeout_add (CONCEAL_TIMEOUT,
                                                   ide_source_map_do_conceal,
                                                   self);

  if (self->show_map == FALSE)
    {
      self->show_map = TRUE;
      g_signal_emit (self, signals [SHOW_MAP], 0);
    }
}

static gboolean
ide_source_map__leave_notify_event (IdeSourceMap     *self,
                                    GdkEventCrossing *event,
                                    GtkWidget        *widget)
{
  g_assert (IDE_IS_SOURCE_MAP (self));
  g_assert (event != NULL);
  g_assert (GTK_IS_WIDGET (widget));

  /* We use this same method for both the view and self,
   * so if we are hovering our map, keep track of it.
   */

  if (IDE_IS_SOURCE_MAP (widget))
    self->in_map = FALSE;

  if (IDE_IS_SOURCE_VIEW (widget))
    self->in_view = FALSE;

  ide_source_map_show_map_and_queue_fade (self);

  return GDK_EVENT_PROPAGATE;
}

static gboolean
ide_source_map__motion_notify_event (IdeSourceMap   *self,
                                     GdkEventMotion *motion,
                                     GtkWidget      *widget)
{
  g_assert (IDE_IS_SOURCE_MAP (self));
  g_assert (motion != NULL);
  g_assert (GTK_IS_WIDGET (widget));

  ide_source_map_show_map_and_queue_fade (self);

  return GDK_EVENT_PROPAGATE;
}

static gboolean
ide_source_map__scroll_event (IdeSourceMap   *self,
                              GdkEventScroll *scroll,
                              GtkWidget      *widget)
{
  g_assert (IDE_IS_SOURCE_MAP (self));
  g_assert (scroll != NULL);
  g_assert (GTK_IS_WIDGET (widget));

  ide_source_map_show_map_and_queue_fade (self);

  return GDK_EVENT_PROPAGATE;
}

static void
ide_source_map__buffer_line_flags_changed (IdeSourceMap *self,
                                           IdeBuffer    *buffer)
{
  g_assert (IDE_IS_SOURCE_MAP (self));
  g_assert (IDE_IS_BUFFER (buffer));

  gtk_source_gutter_renderer_queue_draw (self->line_renderer);
}

static void
ide_source_map__view_notify_buffer (IdeSourceMap  *self,
                                    GParamSpec    *pspec,
                                    GtkSourceView *view)
{
  GtkTextBuffer *buffer;

  g_assert (IDE_IS_SOURCE_MAP (self));
  g_assert (GTK_SOURCE_IS_VIEW (view));

  buffer = gtk_text_view_get_buffer (GTK_TEXT_VIEW (view));
  if (IDE_IS_BUFFER (buffer))
    dzl_signal_group_set_target (self->buffer_signals, buffer);
}

static gboolean
shrink_font (GBinding     *binding,
             const GValue *value,
             GValue       *to_value,
             gpointer      user_data)
{
  PangoFontDescription *font_desc;

  g_assert (G_VALUE_HOLDS (value, PANGO_TYPE_FONT_DESCRIPTION));

  if ((font_desc = g_value_dup_boxed (value)))
    {
      pango_font_description_set_size (font_desc, 1 * PANGO_SCALE);
      g_value_take_boxed (to_value, font_desc);
    }

  return TRUE;
}

static void
ide_source_map__view_changed (IdeSourceMap *self,
                              GParamSpec   *psepct,
                              gpointer      data)
{
  GtkSourceView *view;

  g_return_if_fail (IDE_IS_SOURCE_MAP (self));

  view = gtk_source_map_get_view (GTK_SOURCE_MAP (self));

  g_object_bind_property_full (view, "font-desc", self, "font-desc", G_BINDING_SYNC_CREATE,
                               shrink_font, NULL, NULL, NULL);

  dzl_signal_group_set_target (self->view_signals, view);
}

static void
ide_source_map_destroy (GtkWidget *widget)
{
  IdeSourceMap *self = (IdeSourceMap *)widget;

  if (self->delayed_conceal_timeout)
    {
      g_source_remove (self->delayed_conceal_timeout);
      self->delayed_conceal_timeout = 0;
    }

  self->line_renderer = NULL;

  g_clear_object (&self->view_signals);
  g_clear_object (&self->buffer_signals);

  GTK_WIDGET_CLASS (ide_source_map_parent_class)->destroy (widget);
}


static void
ide_source_map_class_init (IdeSourceMapClass *klass)
{
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  widget_class->destroy = ide_source_map_destroy;

  /**
   * IdeSourceMap::hide-map:
   *
   * The "hide-map" signal is emitted when the source map should be
   * hidden to the user. This is determined by focus tracking of the
   * the user's mouse pointer.
   *
   * Since: 3.18
   */
  signals [HIDE_MAP] =
    g_signal_new ("hide-map",
                  G_TYPE_FROM_CLASS (klass),
                  G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION,
                  0,
                  NULL, NULL,
                  NULL,
                  G_TYPE_NONE,
                  0);

  /**
   * IdeSourceMap::show-map:
   *
   * The "show-map" signal is emitted when the source map should be
   * shown to the user. This is determined by focus tracking of the
   * the user's mouse pointer.
   *
   * Since: 3.18
   */
  signals [SHOW_MAP] =
    g_signal_new ("show-map",
                  G_TYPE_FROM_CLASS (klass),
                  G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION,
                  0,
                  NULL, NULL,
                  NULL,
                  G_TYPE_NONE,
                  0);
}

static void
ide_source_map_init (IdeSourceMap *self)
{
  GtkSourceGutter *gutter;

  gtk_widget_add_events (GTK_WIDGET (self), GDK_ENTER_NOTIFY_MASK | GDK_LEAVE_NOTIFY_MASK);

  /* Buffer */
  self->buffer_signals = dzl_signal_group_new (IDE_TYPE_BUFFER);
  dzl_signal_group_connect_object (self->buffer_signals,
                                   "line-flags-changed",
                                   G_CALLBACK (ide_source_map__buffer_line_flags_changed),
                                   self,
                                   G_CONNECT_SWAPPED);

  /* View */
  self->view_signals = dzl_signal_group_new (GTK_SOURCE_TYPE_VIEW);

  dzl_signal_group_connect_object (self->view_signals,
                                   "notify::buffer",
                                   G_CALLBACK (ide_source_map__view_notify_buffer),
                                   self,
                                   G_CONNECT_SWAPPED);

  dzl_signal_group_connect_object (self->view_signals,
                                   "enter-notify-event",
                                   G_CALLBACK (ide_source_map__enter_notify_event),
                                   self,
                                   G_CONNECT_SWAPPED);

  dzl_signal_group_connect_object (self->view_signals,
                                   "leave-notify-event",
                                   G_CALLBACK (ide_source_map__leave_notify_event),
                                   self,
                                   G_CONNECT_SWAPPED);

  dzl_signal_group_connect_object (self->view_signals,
                                   "motion-notify-event",
                                   G_CALLBACK (ide_source_map__motion_notify_event),
                                   self,
                                   G_CONNECT_SWAPPED);

  dzl_signal_group_connect_object (self->view_signals,
                                   "scroll-event",
                                   G_CALLBACK (ide_source_map__scroll_event),
                                   self,
                                   G_CONNECT_SWAPPED);

  g_signal_connect_object (self,
                           "notify::view",
                           G_CALLBACK (ide_source_map__view_changed),
                           self,
                           G_CONNECT_SWAPPED);

  gutter = gtk_source_view_get_gutter (GTK_SOURCE_VIEW (self), GTK_TEXT_WINDOW_LEFT);
  self->line_renderer = g_object_new (IDE_TYPE_LINE_CHANGE_GUTTER_RENDERER,
                                      "size", 2,
                                      "visible", TRUE,
                                      NULL);
  gtk_source_gutter_insert (gutter, self->line_renderer, 0);

  g_signal_connect_object (self,
                           "enter-notify-event",
                           G_CALLBACK (ide_source_map__enter_notify_event),
                           self,
                           G_CONNECT_SWAPPED);

  g_signal_connect_object (self,
                           "leave-notify-event",
                           G_CALLBACK (ide_source_map__leave_notify_event),
                           self,
                           G_CONNECT_SWAPPED);

  g_signal_connect_object (self,
                           "motion-notify-event",
                           G_CALLBACK (ide_source_map__motion_notify_event),
                           self,
                           G_CONNECT_SWAPPED);

  g_signal_connect_object (self,
                           "scroll-event",
                           G_CALLBACK (ide_source_map__scroll_event),
                           self,
                           G_CONNECT_SWAPPED);
}
