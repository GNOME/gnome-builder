/* ide-hover.c
 *
 * Copyright 2018-2019 Christian Hergert <chergert@redhat.com>
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

#define G_LOG_DOMAIN "ide-hover"

#include "config.h"

#include <dazzle.h>
#include <libide-code.h>
#include <libide-plugins.h>
#include <libpeas/peas.h>
#include <string.h>

#include "ide-hover-popover-private.h"
#include "ide-hover-private.h"
#include "ide-hover-provider.h"

#define GRACE_X 20
#define GRACE_Y 20
#define MOTION_SETTLE_TIMEOUT_MSEC 500

typedef enum
{
  IDE_HOVER_STATE_INITIAL,
  IDE_HOVER_STATE_DISPLAY,
  IDE_HOVER_STATE_IN_POPOVER,
} IdeHoverState;

struct _IdeHover
{
  GObject parent_instance;

  /*
   * Our signal group to handle the number of events on the textview so that
   * we can update the hover provider and associated content.
   */
  DzlSignalGroup *signals;

  /*
   * Our plugins that can populate our IdeHoverContext with content to be
   * displayed.
   */
  IdeExtensionSetAdapter *providers;

  /*
   * Our popover that will display content once the cursor has settled
   * somewhere of importance.
   */
  IdeHoverPopover *popover;

  /*
   * Our last motion position, used to calculate where we should find
   * our iter to display the popover.
   */
  gdouble motion_x;
  gdouble motion_y;

  /*
   * Our state so that we can handle events in a sane manner without
   * stomping all over things.
   */
  IdeHoverState state;

  /*
   * Our source which is continually delayed until the motion event has
   * settled somewhere we can potentially display a popover.
   */
  guint delay_display_source;

  /*
   * We need to introduce some delay when we get a leave-notify-event
   * because we might be entering the popover next.
   */
  guint dismiss_source;
};

static gboolean ide_hover_dismiss_cb (gpointer data);

G_DEFINE_TYPE (IdeHover, ide_hover, G_TYPE_OBJECT)

static void
ide_hover_queue_dismiss (IdeHover *self)
{
  g_assert (IDE_IS_HOVER (self));

  if (self->dismiss_source)
    g_source_remove (self->dismiss_source);

  /*
   * Give ourselves just enough time to get the crossing event
   * into the popover before we try to dismiss the popover.
   */
  self->dismiss_source =
    gdk_threads_add_timeout_full (G_PRIORITY_HIGH,
                                  10,
                                  ide_hover_dismiss_cb,
                                  self, NULL);
}

static void
ide_hover_popover_closed_cb (IdeHover        *self,
                             IdeHoverPopover *popover)
{
  g_assert (IDE_IS_HOVER (self));
  g_assert (IDE_IS_HOVER_POPOVER (popover));

  self->state = IDE_HOVER_STATE_INITIAL;
  gtk_widget_destroy (GTK_WIDGET (popover));
  dzl_clear_source (&self->dismiss_source);
  dzl_clear_source (&self->delay_display_source);

  g_assert (self->popover == NULL);
  g_assert (self->state == IDE_HOVER_STATE_INITIAL);
  g_assert (self->dismiss_source == 0);
  g_assert (self->delay_display_source == 0);
}

static gboolean
ide_hover_popover_enter_notify_event_cb (IdeHover               *self,
                                         const GdkEventCrossing *event,
                                         IdeHoverPopover        *popover)
{
  g_assert (IDE_IS_HOVER (self));
  g_assert (event != NULL);
  g_assert (IDE_IS_HOVER_POPOVER (popover));
  g_assert (self->state == IDE_HOVER_STATE_DISPLAY);

  self->state = IDE_HOVER_STATE_IN_POPOVER;

  dzl_clear_source (&self->dismiss_source);

  return GDK_EVENT_PROPAGATE;
}

static gboolean
ide_hover_popover_leave_notify_event_cb (IdeHover               *self,
                                         const GdkEventCrossing *event,
                                         IdeHoverPopover        *popover)
{
  GtkWidget *child;

  g_assert (IDE_IS_HOVER (self));
  g_assert (event != NULL);
  g_assert (IDE_IS_HOVER_POPOVER (popover));

  if (self->state == IDE_HOVER_STATE_IN_POPOVER)
    self->state = IDE_HOVER_STATE_DISPLAY;

  /* If the window that we are crossing into is not a descendant of our
   * popover window, then we want to dismiss. This is rather annoying to
   * track and suffers the same issue as with GtkNotebook tabs containing
   * buttons (where it's possible to break the prelight state tracking).
   *
   * In future Gtk releases, we may be able to use GtkEventControllerMotion.
   */

  if ((child = gtk_bin_get_child (GTK_BIN (popover))))
    {
      GdkRectangle point = { event->x, event->y, 1, 1 };
      GtkAllocation alloc;

      gtk_widget_get_allocation (child, &alloc);

      if (!dzl_cairo_rectangle_contains_rectangle (&alloc, &point))
        ide_hover_queue_dismiss (self);
    }

  return GDK_EVENT_PROPAGATE;
}

static void
ide_hover_providers_foreach_cb (IdeExtensionSetAdapter *set,
                                PeasPluginInfo         *plugin_info,
                                PeasExtension          *exten,
                                gpointer                user_data)
{
  IdeHoverPopover *popover = user_data;
  IdeHoverProvider *provider = (IdeHoverProvider *)exten;

  g_assert (IDE_IS_EXTENSION_SET_ADAPTER (set));
  g_assert (plugin_info != NULL);
  g_assert (IDE_IS_HOVER_PROVIDER (provider));
  g_assert (IDE_IS_HOVER_POPOVER (popover));

  _ide_hover_popover_add_provider (popover, provider);
}

static void
ide_hover_popover_destroy_cb (IdeHover        *self,
                              IdeHoverPopover *popover)
{
  g_assert (IDE_IS_HOVER (self));
  g_assert (IDE_IS_HOVER_POPOVER (popover));

  self->popover = NULL;
  self->state = IDE_HOVER_STATE_INITIAL;
}

static gboolean
ide_hover_get_bounds (IdeHover    *self,
                      GtkTextIter *begin,
                      GtkTextIter *end,
                      GtkTextIter *hover)
{
  GtkTextView *view;
  GtkTextIter iter;
  gint x, y;

  g_assert (IDE_IS_HOVER (self));
  g_assert (begin != NULL);
  g_assert (end != NULL);
  g_assert (hover != NULL);

  memset (begin, 0, sizeof *begin);
  memset (end, 0, sizeof *end);
  memset (hover, 0, sizeof *hover);

  if (!(view = dzl_signal_group_get_target (self->signals)))
    return FALSE;

  g_assert (GTK_IS_TEXT_VIEW (view));

  gtk_text_view_window_to_buffer_coords (view,
                                         GTK_TEXT_WINDOW_WIDGET,
                                         self->motion_x,
                                         self->motion_y,
                                         &x, &y);

  if (!gtk_text_view_get_iter_at_location (view, &iter, x, y))
    return FALSE;

  *hover = iter;

  if (!_ide_source_iter_inside_word (&iter))
    {
      *begin = iter;
      gtk_text_iter_set_line_offset (begin, 0);

      *end = *begin;
      gtk_text_iter_forward_to_line_end (end);

      return TRUE;
    }

  if (!_ide_source_iter_starts_full_word (&iter))
    _ide_source_iter_backward_full_word_start (&iter);

  *begin = iter;
  *end = iter;

  _ide_source_iter_forward_full_word_end (end);

  return TRUE;
}

static gboolean
ide_hover_motion_timeout_cb (gpointer data)
{
  IdeHover *self = data;
  IdeSourceView *view;
  GdkRectangle rect;
  GdkRectangle begin_rect;
  GdkRectangle end_rect;
  GdkRectangle hover_rect;
  GtkTextIter begin;
  GtkTextIter end;
  GtkTextIter hover;

  g_assert (IDE_IS_HOVER (self));

  self->delay_display_source = 0;

  if (!(view = dzl_signal_group_get_target (self->signals)))
    return G_SOURCE_REMOVE;

  /* Ignore signal if we're already processing */
  if (self->state != IDE_HOVER_STATE_INITIAL)
    return G_SOURCE_REMOVE;

  /* Make sure we're over text */
  if (!ide_hover_get_bounds (self, &begin, &end, &hover))
    return G_SOURCE_REMOVE;

  if (self->popover == NULL)
    {
      self->popover = g_object_new (IDE_TYPE_HOVER_POPOVER,
                                    "modal", FALSE,
                                    "position", GTK_POS_TOP,
                                    "relative-to", view,
                                    NULL);

      g_signal_connect_object (self->popover,
                               "destroy",
                               G_CALLBACK (ide_hover_popover_destroy_cb),
                               self,
                               G_CONNECT_SWAPPED);

      g_signal_connect_object (self->popover,
                               "closed",
                               G_CALLBACK (ide_hover_popover_closed_cb),
                               self,
                               G_CONNECT_SWAPPED);

      g_signal_connect_object (self->popover,
                               "enter-notify-event",
                               G_CALLBACK (ide_hover_popover_enter_notify_event_cb),
                               self,
                               G_CONNECT_SWAPPED);

      g_signal_connect_object (self->popover,
                               "leave-notify-event",
                               G_CALLBACK (ide_hover_popover_leave_notify_event_cb),
                               self,
                               G_CONNECT_SWAPPED);

      if (self->providers != NULL)
        ide_extension_set_adapter_foreach (self->providers,
                                           ide_hover_providers_foreach_cb,
                                           self->popover);
    }

  self->state = IDE_HOVER_STATE_DISPLAY;
  gtk_text_view_get_iter_location (GTK_TEXT_VIEW (view), &begin, &begin_rect);
  gtk_text_view_get_iter_location (GTK_TEXT_VIEW (view), &end, &end_rect);
  gtk_text_view_get_iter_location (GTK_TEXT_VIEW (view), &hover, &hover_rect);
  gdk_rectangle_union (&begin_rect, &end_rect, &rect);

  gtk_text_view_buffer_to_window_coords (GTK_TEXT_VIEW (view),
                                         GTK_TEXT_WINDOW_WIDGET,
                                         rect.x, rect.y, &rect.x, &rect.y);

  _ide_hover_popover_set_hovered_at (self->popover, &hover_rect);

  if (gtk_text_iter_equal (&begin, &end) &&
      gtk_text_iter_starts_line (&begin))
    {
      rect.width = 1;
      gtk_popover_set_pointing_to (GTK_POPOVER (self->popover), &rect);
      gtk_popover_set_position (GTK_POPOVER (self->popover), GTK_POS_RIGHT);
    }
  else
    {
      gtk_popover_set_pointing_to (GTK_POPOVER (self->popover), &rect);
      gtk_popover_set_position (GTK_POPOVER (self->popover), GTK_POS_TOP);
    }

  _ide_hover_popover_show (self->popover);

  return G_SOURCE_REMOVE;
}

static void
ide_hover_delay_display (IdeHover *self)
{
  g_assert (IDE_IS_HOVER (self));

  if (self->delay_display_source)
    g_source_remove (self->delay_display_source);

  self->delay_display_source =
    gdk_threads_add_timeout_full (G_PRIORITY_LOW,
                                  MOTION_SETTLE_TIMEOUT_MSEC,
                                  ide_hover_motion_timeout_cb,
                                  self, NULL);
}

void
_ide_hover_display (IdeHover          *self,
                    const GtkTextIter *iter)
{
  IdeSourceView *view;
  GdkRectangle rect;

  g_assert (IDE_IS_HOVER (self));
  g_assert (iter != NULL);

  if (self->state != IDE_HOVER_STATE_INITIAL)
    return;

  if (!(view = dzl_signal_group_get_target (self->signals)))
    return;

  g_assert (GTK_IS_TEXT_VIEW (view));

  dzl_clear_source (&self->delay_display_source);

  gtk_text_view_get_iter_location (GTK_TEXT_VIEW (view), iter, &rect);
  gtk_text_view_buffer_to_window_coords (GTK_TEXT_VIEW (view),
                                         GTK_TEXT_WINDOW_TEXT,
                                         rect.x, rect.y,
                                         &rect.x, &rect.y);

  self->motion_x = rect.x;
  self->motion_y = rect.y;

  ide_hover_motion_timeout_cb (self);
}

static inline gboolean
should_ignore_event (IdeSourceView  *view,
                     GdkWindow      *event_window)
{
  GdkWindow *text_window;
  GdkWindow *gutter_window;

  g_assert (IDE_IS_SOURCE_VIEW (view));

  text_window = gtk_text_view_get_window (GTK_TEXT_VIEW (view), GTK_TEXT_WINDOW_TEXT);
  gutter_window = gtk_text_view_get_window (GTK_TEXT_VIEW (view), GTK_TEXT_WINDOW_LEFT);

  return (event_window != text_window && event_window != gutter_window);
}

static gboolean
ide_hover_key_press_event_cb (IdeHover          *self,
                              const GdkEventKey *event,
                              IdeSourceView     *view)
{
  g_assert (IDE_IS_HOVER (self));
  g_assert (event != NULL);
  g_assert (IDE_IS_SOURCE_VIEW (view));

  if (self->popover != NULL)
    gtk_widget_destroy (GTK_WIDGET (self->popover));

  dzl_clear_source (&self->delay_display_source);
  dzl_clear_source (&self->dismiss_source);

  g_assert (self->popover == NULL);
  g_assert (self->state == IDE_HOVER_STATE_INITIAL);
  g_assert (self->delay_display_source == 0);
  g_assert (self->dismiss_source == 0);

  return GDK_EVENT_PROPAGATE;
}

static gboolean
ide_hover_enter_notify_event_cb (IdeHover               *self,
                                 const GdkEventCrossing *event,
                                 IdeSourceView          *view)
{
  g_assert (IDE_IS_HOVER (self));
  g_assert (event != NULL);
  g_assert (event->type == GDK_ENTER_NOTIFY);
  g_assert (IDE_IS_SOURCE_VIEW (view));

  if (should_ignore_event (view, event->window))
    return GDK_EVENT_PROPAGATE;

  dzl_clear_source (&self->dismiss_source);

  return GDK_EVENT_PROPAGATE;
}

static gboolean
ide_hover_dismiss_cb (gpointer data)
{
  IdeHover *self = data;

  g_assert (IDE_IS_HOVER (self));

  self->dismiss_source = 0;

  switch (self->state)
    {
    case IDE_HOVER_STATE_DISPLAY:
      g_assert (IDE_IS_HOVER_POPOVER (self->popover));

      _ide_hover_popover_hide (self->popover);

      g_assert (self->state == IDE_HOVER_STATE_INITIAL);
      g_assert (self->popover == NULL);

      break;

    case IDE_HOVER_STATE_INITIAL:
    case IDE_HOVER_STATE_IN_POPOVER:
    default:
      dzl_clear_source (&self->delay_display_source);
      break;
    }

  return G_SOURCE_REMOVE;
}

static gboolean
ide_hover_leave_notify_event_cb (IdeHover               *self,
                                 const GdkEventCrossing *event,
                                 IdeSourceView          *view)
{
  g_assert (IDE_IS_HOVER (self));
  g_assert (event != NULL);
  g_assert (event->type == GDK_LEAVE_NOTIFY);
  g_assert (IDE_IS_SOURCE_VIEW (view));

  if (should_ignore_event (view, event->window))
    return GDK_EVENT_PROPAGATE;

  ide_hover_queue_dismiss (self);

  return GDK_EVENT_PROPAGATE;
}

static gboolean
ide_hover_scroll_event_cb (IdeHover             *self,
                           const GdkEventScroll *event,
                           IdeSourceView        *view)
{
  g_assert (IDE_IS_HOVER (self));
  g_assert (event != NULL);
  g_assert (IDE_IS_SOURCE_VIEW (view));
  g_assert (!self->popover || IDE_IS_HOVER_POPOVER (self->popover));

  if (self->popover != NULL)
    gtk_widget_destroy (GTK_WIDGET (self->popover));

  return GDK_EVENT_PROPAGATE;
}

static gboolean
ide_hover_motion_notify_event_cb (IdeHover             *self,
                                  const GdkEventMotion *event,
                                  IdeSourceView        *view)
{
  GdkWindow *window;

  g_assert (IDE_IS_HOVER (self));
  g_assert (event != NULL);
  g_assert (event->type == GDK_MOTION_NOTIFY);
  g_assert (IDE_IS_SOURCE_VIEW (view));

  window = gtk_text_view_get_window (GTK_TEXT_VIEW (view), GTK_TEXT_WINDOW_LEFT);

  if (window != NULL)
    {
      gint left_width = gdk_window_get_width (window);

      self->motion_x = event->x + left_width;
      self->motion_y = event->y;
    }

  /*
   * If we have a popover displayed, get it's allocation so that
   * we can detect if our x/y coordinate is outside the threshold
   * of the rectangle + grace area. If so, we'll dismiss the popover
   * immediately.
   */

  if (self->popover != NULL)
    {
      GtkAllocation alloc;
      GdkRectangle pointing_to;

      gtk_widget_get_allocation (GTK_WIDGET (self->popover), &alloc);
      gtk_widget_translate_coordinates (GTK_WIDGET (self->popover),
                                        GTK_WIDGET (view),
                                        alloc.x, alloc.y,
                                        &alloc.x, &alloc.y);
      gtk_popover_get_pointing_to (GTK_POPOVER (self->popover), &pointing_to);

      alloc.x -= GRACE_X;
      alloc.width += GRACE_X * 2;
      alloc.y -= GRACE_Y;
      alloc.height += GRACE_Y * 2;

      gdk_rectangle_union (&alloc, &pointing_to, &alloc);

      if (event->x < alloc.x ||
          event->x > (alloc.x + alloc.width) ||
          event->y < alloc.y ||
          event->y > (alloc.y + alloc.height))
        {
          _ide_hover_popover_hide (self->popover);

          g_assert (self->popover == NULL);
          g_assert (self->state == IDE_HOVER_STATE_INITIAL);
        }
    }

  dzl_clear_source (&self->dismiss_source);

  ide_hover_delay_display (self);

  return GDK_EVENT_PROPAGATE;
}

static void
ide_hover_destroy_cb (IdeHover      *self,
                      IdeSourceView *view)
{
  g_assert (IDE_IS_HOVER (self));
  g_assert (IDE_IS_SOURCE_VIEW (view));
  g_assert (!self->popover || IDE_IS_HOVER_POPOVER (self->popover));

  dzl_clear_source (&self->delay_display_source);

  if (self->popover != NULL)
    gtk_widget_destroy (GTK_WIDGET (self->popover));

  g_assert (self->popover == NULL);
  g_assert (self->delay_display_source == 0);
}

static void
ide_hover_dispose (GObject *object)
{
  IdeHover *self = (IdeHover *)object;

  ide_clear_and_destroy_object (&self->providers);

  dzl_clear_source (&self->delay_display_source);
  dzl_clear_source (&self->dismiss_source);
  dzl_signal_group_set_target (self->signals, NULL);

  if (self->popover != NULL)
    gtk_widget_destroy (GTK_WIDGET (self->popover));

  G_OBJECT_CLASS (ide_hover_parent_class)->dispose (object);

  g_assert (self->popover == NULL);
  g_assert (self->delay_display_source == 0);
  g_assert (self->dismiss_source == 0);
}

static void
ide_hover_finalize (GObject *object)
{
  IdeHover *self = (IdeHover *)object;

  g_clear_object (&self->signals);

  g_assert (self->signals == NULL);
  g_assert (self->popover == NULL);
  g_assert (self->providers == NULL);

  g_assert (self->delay_display_source == 0);
  g_assert (self->dismiss_source == 0);

  G_OBJECT_CLASS (ide_hover_parent_class)->finalize (object);
}

static void
ide_hover_class_init (IdeHoverClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->dispose = ide_hover_dispose;
  object_class->finalize = ide_hover_finalize;
}

static void
ide_hover_init (IdeHover *self)
{
  self->signals = dzl_signal_group_new (IDE_TYPE_SOURCE_VIEW);

  dzl_signal_group_connect_object (self->signals,
                                   "key-press-event",
                                   G_CALLBACK (ide_hover_key_press_event_cb),
                                   self,
                                   G_CONNECT_SWAPPED);

  dzl_signal_group_connect_object (self->signals,
                                   "enter-notify-event",
                                   G_CALLBACK (ide_hover_enter_notify_event_cb),
                                   self,
                                   G_CONNECT_SWAPPED);

  dzl_signal_group_connect_object (self->signals,
                                   "leave-notify-event",
                                   G_CALLBACK (ide_hover_leave_notify_event_cb),
                                   self,
                                   G_CONNECT_SWAPPED);

  dzl_signal_group_connect_object (self->signals,
                                   "motion-notify-event",
                                   G_CALLBACK (ide_hover_motion_notify_event_cb),
                                   self,
                                   G_CONNECT_SWAPPED);

  dzl_signal_group_connect_object (self->signals,
                                   "scroll-event",
                                   G_CALLBACK (ide_hover_scroll_event_cb),
                                   self,
                                   G_CONNECT_SWAPPED);

  dzl_signal_group_connect_object (self->signals,
                                   "destroy",
                                   G_CALLBACK (ide_hover_destroy_cb),
                                   self,
                                   G_CONNECT_SWAPPED);
}

static void
ide_hover_extension_added_cb (IdeExtensionSetAdapter *set,
                              PeasPluginInfo         *plugin_info,
                              PeasExtension          *exten,
                              gpointer                user_data)
{
  IdeHover *self = user_data;
  IdeHoverProvider *provider = (IdeHoverProvider *)exten;
  IdeSourceView *view;

  g_assert (IDE_IS_EXTENSION_SET_ADAPTER (set));
  g_assert (IDE_IS_HOVER (self));
  g_assert (plugin_info != NULL);
  g_assert (IDE_IS_HOVER_PROVIDER (provider));

  view = dzl_signal_group_get_target (self->signals);
  ide_hover_provider_load (provider, view);
}

static void
ide_hover_extension_removed_cb (IdeExtensionSetAdapter *set,
                                PeasPluginInfo         *plugin_info,
                                PeasExtension          *exten,
                                gpointer                user_data)
{
  IdeHover *self = user_data;
  IdeHoverProvider *provider = (IdeHoverProvider *)exten;
  IdeSourceView *view;

  g_assert (IDE_IS_EXTENSION_SET_ADAPTER (set));
  g_assert (IDE_IS_HOVER (self));
  g_assert (plugin_info != NULL);
  g_assert (IDE_IS_HOVER_PROVIDER (provider));

  view = dzl_signal_group_get_target (self->signals);
  ide_hover_provider_unload (provider, view);
}

void
_ide_hover_set_context (IdeHover   *self,
                        IdeContext *context)
{
  g_return_if_fail (IDE_IS_HOVER (self));
  g_return_if_fail (IDE_IS_CONTEXT (context));

  if (self->providers != NULL)
    return;

  self->providers = ide_extension_set_adapter_new (IDE_OBJECT (context),
                                                   peas_engine_get_default (),
                                                   IDE_TYPE_HOVER_PROVIDER,
                                                   "Hover-Provider-Languages",
                                                   NULL);

  g_signal_connect_object (self->providers,
                           "extension-added",
                           G_CALLBACK (ide_hover_extension_added_cb),
                           self, 0);

  g_signal_connect_object (self->providers,
                           "extension-removed",
                           G_CALLBACK (ide_hover_extension_removed_cb),
                           self, 0);

  ide_extension_set_adapter_foreach (self->providers,
                                     ide_hover_extension_added_cb,
                                     self);
}

void
_ide_hover_set_language (IdeHover    *self,
                         const gchar *language)
{
  g_return_if_fail (IDE_IS_HOVER (self));

  if (self->providers != NULL)
    ide_extension_set_adapter_set_value (self->providers, language);
}

IdeHover *
_ide_hover_new (IdeSourceView *view)
{
  IdeHover *self;

  self = g_object_new (IDE_TYPE_HOVER, NULL);
  dzl_signal_group_set_target (self->signals, view);

  return self;
}
