/* ide-hover.c
 *
 * Copyright 2018 Christian Hergert <chergert@redhat.com>
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

#include "config.h"

#define G_LOG_DOMAIN "ide-hover"

#include <dazzle.h>
#include <libpeas/peas.h>
#include <string.h>

#include "hover/ide-hover-popover-private.h"
#include "hover/ide-hover-private.h"
#include "hover/ide-hover-provider.h"
#include "plugins/ide-extension-set-adapter.h"
#include "sourceview/ide-source-iter.h"
#include "util/ide-gtk.h"

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

G_DEFINE_TYPE (IdeHover, ide_hover, G_TYPE_OBJECT)

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
  g_assert (IDE_IS_HOVER (self));
  g_assert (event != NULL);
  g_assert (IDE_IS_HOVER_POPOVER (popover));

  if (self->state == IDE_HOVER_STATE_IN_POPOVER)
    self->state = IDE_HOVER_STATE_DISPLAY;

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

static void
ide_hover_get_bounds (IdeHover    *self,
                      GtkTextIter *begin,
                      GtkTextIter *end)
{
  GtkTextView *view;
  GtkTextIter iter;
  gint x, y;

  g_assert (IDE_IS_HOVER (self));
  g_assert (begin != NULL);
  g_assert (end != NULL);

  if (!(view = dzl_signal_group_get_target (self->signals)))
    {
      memset (begin, 0, sizeof *begin);
      memset (end, 0, sizeof *end);
      return;
    }

  g_assert (GTK_IS_TEXT_VIEW (view));

  gtk_text_view_window_to_buffer_coords (view,
                                         GTK_TEXT_WINDOW_WIDGET,
                                         self->motion_x,
                                         self->motion_y,
                                         &x, &y);

  gtk_text_view_get_iter_at_location (view, &iter, x, y);

  if (!_ide_source_iter_inside_word (&iter))
    {
      *begin = iter;
      gtk_text_iter_set_line_offset (begin, 0);

      *end = *begin;
      gtk_text_iter_forward_to_line_end (end);

      return;
    }

  if (!_ide_source_iter_starts_full_word (&iter))
    _ide_source_iter_backward_full_word_start (&iter);

  *begin = iter;
  *end = iter;

  _ide_source_iter_forward_full_word_end (end);
}

static gboolean
ide_hover_motion_timeout_cb (gpointer data)
{
  IdeHover *self = data;
  IdeSourceView *view;
  GdkRectangle rect;
  GdkRectangle begin_rect;
  GdkRectangle end_rect;
  GtkTextIter begin;
  GtkTextIter end;

  g_assert (IDE_IS_HOVER (self));

  self->delay_display_source = 0;

  if (!(view = dzl_signal_group_get_target (self->signals)))
    return G_SOURCE_REMOVE;

  /* Ignore signal if we're already processing */
  if (self->state != IDE_HOVER_STATE_INITIAL)
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

  ide_hover_get_bounds (self, &begin, &end);
  gtk_text_view_get_iter_location (GTK_TEXT_VIEW (view), &begin, &begin_rect);
  gtk_text_view_get_iter_location (GTK_TEXT_VIEW (view), &end, &end_rect);
  gdk_rectangle_union (&begin_rect, &end_rect, &rect);

  gtk_text_view_buffer_to_window_coords (GTK_TEXT_VIEW (view),
                                         GTK_TEXT_WINDOW_WIDGET,
                                         rect.x, rect.y, &rect.x, &rect.y);

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

static inline gboolean
should_ignore_event (IdeSourceView  *view,
                     GdkWindow      *event_window)
{
  GdkWindow *window;

  g_assert (IDE_IS_SOURCE_VIEW (view));

  window = gtk_text_view_get_window (GTK_TEXT_VIEW (view), GTK_TEXT_WINDOW_WIDGET);
  return window != event_window;
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

  if (self->dismiss_source)
    g_source_remove (self->dismiss_source);

  /*
   * Give ourselves just enough time to get the crossing event
   * into the popover before we try to dismiss the popover.
   */
  self->dismiss_source =
    gdk_threads_add_timeout_full (G_PRIORITY_HIGH,
                                  1,
                                  ide_hover_dismiss_cb,
                                  self, NULL);

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
  gint width;

  g_assert (IDE_IS_HOVER (self));
  g_assert (event != NULL);
  g_assert (event->type == GDK_MOTION_NOTIFY);
  g_assert (IDE_IS_SOURCE_VIEW (view));

  window = gtk_text_view_get_window (GTK_TEXT_VIEW (view), GTK_TEXT_WINDOW_LEFT);
  width = gdk_window_get_width (window);

  self->motion_x = event->x + width;
  self->motion_y = event->y;

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

  g_clear_object (&self->providers);

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

IdeHover *
_ide_hover_new (IdeSourceView *view)
{
  IdeHover *self;

  self = g_object_new (IDE_TYPE_HOVER, NULL);
  dzl_signal_group_set_target (self->signals, view);

  return self;
}

void
_ide_hover_set_context (IdeHover   *self,
                        IdeContext *context)
{
  g_return_if_fail (IDE_IS_HOVER (self));
  g_return_if_fail (IDE_IS_CONTEXT (context));

  if (self->providers != NULL)
    return;

  self->providers = ide_extension_set_adapter_new (context,
                                                   peas_engine_get_default (),
                                                   IDE_TYPE_HOVER_PROVIDER,
                                                   "Hover-Provider-Languages",
                                                   NULL);
}

void
_ide_hover_set_language (IdeHover    *self,
                         const gchar *language)
{
  g_return_if_fail (IDE_IS_HOVER (self));

  if (self->providers != NULL)
    ide_extension_set_adapter_set_value (self->providers, language);
}
