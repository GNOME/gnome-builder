/* ide-source-map.c
 *
 * Copyright (C) 2015 Christian Hergert <christian@hergert.me>
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

#include <glib/gi18n.h>

#include "egg-signal-group.h"
#include "ide-buffer.h"
#include "ide-line-change-gutter-renderer.h"
#include "ide-source-map.h"
#include "ide-source-view.h"

#define CONCEAL_TIMEOUT      2000

struct _IdeSourceMap
{
  GtkSourceMap               parent_instance;

  EggSignalGroup            *signal_group;
  GtkTextBuffer             *buffer;
  GtkSourceGutterRenderer   *line_renderer;
  guint                      delayed_conceal_timeout;
  guint                      show_map : 1;
};

G_DEFINE_TYPE (IdeSourceMap, ide_source_map, GTK_SOURCE_TYPE_MAP)


enum {
  SHOW_MAP,
  HIDE_MAP,
  LAST_SIGNAL
};

static guint gSignals [LAST_SIGNAL];

static gboolean
ide_source_map_do_conceal (gpointer data)
{
  IdeSourceMap *self = data;

  g_assert (IDE_IS_SOURCE_MAP (self));

  self->delayed_conceal_timeout = 0;

  if (self->show_map == TRUE)
    {
      self->show_map = FALSE;
      g_signal_emit (self, gSignals [HIDE_MAP], 0);
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

  if (self->show_map == FALSE)
    {
      self->show_map = TRUE;
      g_signal_emit (self, gSignals [SHOW_MAP], 0);
    }

  return GDK_EVENT_PROPAGATE;
}

static void
ide_source_map_show_map_and_queue_fade (IdeSourceMap *self)
{
  g_assert (IDE_IS_SOURCE_MAP (self));

  if (self->delayed_conceal_timeout != 0)
    g_source_remove (self->delayed_conceal_timeout);

  self->delayed_conceal_timeout = g_timeout_add (CONCEAL_TIMEOUT,
                                                 ide_source_map_do_conceal,
                                                 self);

  if (self->show_map == FALSE)
    {
      self->show_map = TRUE;
      g_signal_emit (self, gSignals [SHOW_MAP], 0);
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
  if (self->buffer != buffer)
    {
      if (self->buffer != NULL)
        {
          g_signal_handlers_disconnect_by_func (self->buffer,
                                                G_CALLBACK (ide_source_map__buffer_line_flags_changed),
                                                self);
          ide_clear_weak_pointer (&self->buffer);
        }

      if (IDE_IS_BUFFER (buffer))
        {
          ide_set_weak_pointer (&self->buffer, buffer);
          g_signal_connect_object (buffer,
                                   "line-flags-changed",
                                   G_CALLBACK (ide_source_map__buffer_line_flags_changed),
                                   self,
                                   G_CONNECT_SWAPPED);
        }
    }
}

static void
ide_source_map__view_changed (IdeSourceMap *self,
                              GParamSpec   *psepct,
                              gpointer      data)
{
  g_return_if_fail (IDE_IS_SOURCE_MAP (self));

  egg_signal_group_set_target (self->signal_group,
                               gtk_source_map_get_view (GTK_SOURCE_MAP (self)));
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

  ide_clear_weak_pointer (&self->buffer);
  g_clear_object (&self->signal_group);

  GTK_WIDGET_CLASS (ide_source_map_parent_class)->destroy (widget);
}


static void
ide_source_map_class_init (IdeSourceMapClass *klass)
{
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  widget_class->destroy = ide_source_map_destroy;

  gSignals [HIDE_MAP] =
    g_signal_new ("hide-map",
                  G_TYPE_FROM_CLASS (klass),
                  G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION,
                  0,
                  NULL, NULL,
                  NULL,
                  G_TYPE_NONE,
                  0);

  gSignals [SHOW_MAP] =
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
  GtkSourceView *child_view = gtk_source_map_get_child_view (GTK_SOURCE_MAP (self));

  gutter = gtk_source_view_get_gutter (child_view, GTK_TEXT_WINDOW_LEFT);
  self->line_renderer = g_object_new (IDE_TYPE_LINE_CHANGE_GUTTER_RENDERER,
                                      "size", 2,
                                      "visible", TRUE,
                                      NULL);
  gtk_source_gutter_insert (gutter, self->line_renderer, 0);

  /* View */
  self->signal_group = egg_signal_group_new (GTK_SOURCE_TYPE_VIEW);

  g_signal_connect_object (self,
                           "notify::view",
                           G_CALLBACK (ide_source_map__view_changed),
                           self,
                           G_CONNECT_SWAPPED);

  egg_signal_group_connect_object (self->signal_group,
                                   "notify::buffer",
                                   G_CALLBACK (ide_source_map__view_notify_buffer),
                                   self,
                                   G_CONNECT_SWAPPED);

  egg_signal_group_connect_object (self->signal_group,
                                   "enter-notify-event",
                                   G_CALLBACK (ide_source_map__enter_notify_event),
                                   self,
                                   G_CONNECT_SWAPPED);

  egg_signal_group_connect_object (self->signal_group,
                                   "leave-notify-event",
                                   G_CALLBACK (ide_source_map__leave_notify_event),
                                   self,
                                   G_CONNECT_SWAPPED);

  egg_signal_group_connect_object (self->signal_group,
                                   "motion-notify-event",
                                   G_CALLBACK (ide_source_map__motion_notify_event),
                                   self,
                                   G_CONNECT_SWAPPED);

  egg_signal_group_connect_object (self->signal_group,
                                   "scroll-event",
                                   G_CALLBACK (ide_source_map__scroll_event),
                                   self,
                                   G_CONNECT_SWAPPED);

  /* Child view */
  g_signal_connect_object (child_view,
                           "enter-notify-event",
                           G_CALLBACK (ide_source_map__enter_notify_event),
                           self,
                           G_CONNECT_SWAPPED);

  g_signal_connect_object (child_view,
                           "leave-notify-event",
                           G_CALLBACK (ide_source_map__leave_notify_event),
                           self,
                           G_CONNECT_SWAPPED);

  g_signal_connect_object (child_view,
                           "motion-notify-event",
                           G_CALLBACK (ide_source_map__motion_notify_event),
                           self,
                           G_CONNECT_SWAPPED);

  g_signal_connect_object (child_view,
                           "scroll-event",
                           G_CALLBACK (ide_source_map__scroll_event),
                           self,
                           G_CONNECT_SWAPPED);
}
