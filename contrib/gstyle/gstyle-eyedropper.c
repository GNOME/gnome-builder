/* gstyle-eyedropper.c
 *
 * Copyright (C) 2016 sebastien lafargue <slafargue@gnome.org>
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

/* This source code is first based on the now deprecated
 * gtk eyedropper source code available at:
 * https://git.gnome.org/browse/gtk+/tree/gtk/deprecated/gtkcolorsel.c#n1705
 */

#define G_LOG_DOMAIN "gstyle-eyedropper"

#include <gdk/gdk.h>

#include "gstyle-color.h"
#include "gstyle-eyedropper.h"

struct _GstyleEyedropper
{
  GtkWindow   parent_instance;

  GtkWidget  *source;
  GtkWidget  *window;
  GdkCursor  *cursor;
  GdkSeat    *seat;

  gulong      key_handler_id;
  gulong      grab_broken_handler_id;
  gulong      motion_notify_handler_id;
  gulong      pointer_pressed_handler_id;
  gulong      pointer_released_handler_id;
};

G_DEFINE_TYPE (GstyleEyedropper, gstyle_eyedropper, GTK_TYPE_WINDOW)

enum {
  COLOR_PICKED,
  GRAB_RELEASED,
  LAST_SIGNAL
};

enum {
  PROP_0,
  PROP_SOURCE_EVENT,
  N_PROPS
};

static guint signals [LAST_SIGNAL];
static GParamSpec *properties [N_PROPS];

static void
get_rgba_at_cursor (GstyleEyedropper *self,
                    GdkScreen        *screen,
                    GdkDevice        *device,
                    gint              x,
                    gint              y,
                    GdkRGBA          *rgba)
{
  GdkWindow *window;
  GdkPixbuf *pixbuf;
  guchar *pixels;

  g_assert (GSTYLE_IS_EYEDROPPER (self));
  g_assert (GDK_IS_SCREEN (screen));
  g_assert (GDK_IS_DEVICE (device));

  window = gdk_screen_get_root_window (screen);
  pixbuf = gdk_pixbuf_get_from_window (window, x, y, 1, 1);
  if (!pixbuf)
    {
      window = gdk_device_get_window_at_position (device, &x, &y);
      if (!window)
        return;

      pixbuf = gdk_pixbuf_get_from_window (window, x, y, 1, 1);
      if (!pixbuf)
        return;
    }

  g_assert (gdk_pixbuf_get_colorspace (pixbuf) == GDK_COLORSPACE_RGB);
  g_assert (gdk_pixbuf_get_bits_per_sample (pixbuf) == 8);

  pixels = gdk_pixbuf_get_pixels (pixbuf);
  rgba->red = pixels[0] / 255.0;
  rgba->green = pixels[1] / 255.0;
  rgba->blue = pixels[2] /255.0;

  g_object_unref (pixbuf);
}

static void
release_grab (GstyleEyedropper *self)
{
  g_assert (GSTYLE_IS_EYEDROPPER (self));

  if (self->key_handler_id)
    {
      g_signal_handler_disconnect (self->window, self->key_handler_id);
      self->key_handler_id = 0;
    }

  if (self->grab_broken_handler_id)
    {
      g_signal_handler_disconnect (self->window, self->grab_broken_handler_id);
      self->grab_broken_handler_id = 0;
    }

  if (self->motion_notify_handler_id)
    {
      g_signal_handler_disconnect (self->window, self->motion_notify_handler_id);
      self->motion_notify_handler_id = 0;
    }

  if (self->pointer_released_handler_id)
    {
      g_signal_handler_disconnect (self->window, self->pointer_released_handler_id);
      self->pointer_released_handler_id = 0;
    }

  gtk_grab_remove (self->window);
  gdk_seat_ungrab (self->seat);

  g_signal_emit (self, signals [GRAB_RELEASED], 0);
}

static void
gstyle_eyedropper_pointer_motion_notify_cb (GstyleEyedropper *self,
                                            GdkEventMotion   *event,
                                            GtkWindow        *window)
{
  GdkRGBA rgba;

  g_assert (GSTYLE_IS_EYEDROPPER (self));
  g_assert (event != NULL);
  g_assert (GTK_IS_WINDOW (window));

  get_rgba_at_cursor (self,
                      gdk_event_get_screen ((GdkEvent *) event),
                      gdk_event_get_device ((GdkEvent *) event),
                      event->x_root, event->y_root, &rgba);

  g_signal_emit (self, signals [COLOR_PICKED], 0, &rgba);
}

static gboolean
gstyle_eyedropper_pointer_released_cb (GstyleEyedropper *self,
                                       GdkEventButton   *event,
                                       GtkWindow        *window)
{
  g_assert (GSTYLE_IS_EYEDROPPER (self));
  g_assert (event != NULL);
  g_assert (GTK_IS_WINDOW (window));

  release_grab (self);

  return GDK_EVENT_STOP;
}

static gboolean
gstyle_eyedropper_pointer_pressed_cb (GstyleEyedropper *self,
                                      GdkEventButton   *event,
                                      GtkWindow        *window)
{
  GdkRGBA rgba;

  g_assert (GSTYLE_IS_EYEDROPPER (self));
  g_assert (event != NULL);
  g_assert (GTK_IS_WINDOW (window));

  if (event->type == GDK_BUTTON_PRESS && event->button == GDK_BUTTON_PRIMARY)
    {
      self->motion_notify_handler_id =
        g_signal_connect_swapped (window, "motion-notify-event",
                                  G_CALLBACK (gstyle_eyedropper_pointer_motion_notify_cb), self);

      self->pointer_released_handler_id =
        g_signal_connect_swapped (window, "button-release-event",
                                  G_CALLBACK (gstyle_eyedropper_pointer_released_cb), self);

      g_signal_handler_disconnect (self->window, self->pointer_pressed_handler_id);
      self->pointer_pressed_handler_id = 0;

      get_rgba_at_cursor (self,
                          gdk_event_get_screen ((GdkEvent *) event),
                          gdk_event_get_device ((GdkEvent *) event),
                          event->x_root, event->y_root, &rgba);

      g_signal_emit (self, signals [COLOR_PICKED], 0, &rgba);

      return GDK_EVENT_STOP;
    }

  return GDK_EVENT_PROPAGATE;
}

static gboolean
gstyle_eyedropper_key_pressed_cb (GstyleEyedropper *self,
                                  GdkEventKey      *event,
                                  GtkWindow        *window)
{
  g_assert (GSTYLE_IS_EYEDROPPER (self));
  g_assert (event != NULL);
  g_assert (GTK_IS_WINDOW (window));

  /* TODO: handle cursor and picker trigger keys */
  switch (event->keyval)
    {
    case GDK_KEY_Escape:
      release_grab (self);
      break;

    default:
      break;
    }

  return GDK_EVENT_STOP;
}

static gboolean
gstyle_eyedropper_grab_broken_cb (GstyleEyedropper *self,
                                  GdkEventKey      *event,
                                  GtkWidget        *window)
{
  g_assert (GSTYLE_IS_EYEDROPPER (self));
  g_assert (event != NULL);
  g_assert (GTK_IS_WINDOW (window));

  release_grab (self);

  return GDK_EVENT_STOP;
}

void
gstyle_eyedropper_set_source_event (GstyleEyedropper *self,
                                    GdkEvent         *event)
{
  GdkScreen *screen;
  GtkWidget *source;
  GdkGrabStatus status;

  g_return_if_fail (GSTYLE_IS_EYEDROPPER (self));
  g_return_if_fail (event != NULL);

  self->seat = g_object_ref (gdk_event_get_seat (event));
  source = gtk_get_event_widget (event);
  screen = gdk_event_get_screen (event);

  g_clear_object (&self->window);
  g_clear_object (&self->cursor);

  self->window = gtk_window_new (GTK_WINDOW_POPUP);
  gtk_window_set_screen (GTK_WINDOW (self->window), screen);
  gtk_window_resize (GTK_WINDOW (self->window), 1, 1);
  gtk_window_move (GTK_WINDOW (self->window), -1, -1);
  gtk_widget_show (self->window);

  gtk_widget_add_events (self->window,
                         GDK_BUTTON_RELEASE_MASK | GDK_BUTTON_PRESS_MASK | GDK_POINTER_MOTION_MASK);

  self->cursor = gdk_cursor_new_from_name (gdk_screen_get_display (screen), "cell");
  gtk_grab_add (self->window);
  status = gdk_seat_grab (self->seat,
                          gtk_widget_get_window (source),
                          GDK_SEAT_CAPABILITY_ALL,
                          FALSE,
                          self->cursor,
                          event,
                          NULL, NULL);

  if (status != GDK_GRAB_SUCCESS)
    {
      g_warning ("grab failed status:%i\n", status);
      return;
    }

  self->pointer_pressed_handler_id =
    g_signal_connect_swapped (self->window,
                              "button-press-event",
                              G_CALLBACK (gstyle_eyedropper_pointer_pressed_cb),
                              self);

  self->key_handler_id =
    g_signal_connect_swapped (self->window,
                              "key-press-event",
                              G_CALLBACK (gstyle_eyedropper_key_pressed_cb),
                              self);

  self->grab_broken_handler_id =
    g_signal_connect_swapped (self->window,
                              "grab-broken-event",
                              G_CALLBACK (gstyle_eyedropper_grab_broken_cb),
                              self);
}

GstyleEyedropper *
gstyle_eyedropper_new (GdkEvent *event)
{
  return g_object_new (GSTYLE_TYPE_EYEDROPPER,
                       "source-event", event,
                       NULL);
}

static void
gstyle_eyedropper_finalize (GObject *object)
{
  GstyleEyedropper *self = GSTYLE_EYEDROPPER (object);

  g_clear_object (&self->window);
  g_clear_object (&self->cursor);
  gdk_seat_ungrab (self->seat);

  G_OBJECT_CLASS (gstyle_eyedropper_parent_class)->finalize (object);
}

static void
gstyle_eyedropper_set_property (GObject      *object,
                                guint         prop_id,
                                const GValue *value,
                                GParamSpec   *pspec)
{
  GstyleEyedropper *self = GSTYLE_EYEDROPPER (object);
  GdkEvent *event;

  switch (prop_id)
    {
    case PROP_SOURCE_EVENT:
      event = g_value_get_boxed (value);
      gstyle_eyedropper_set_source_event (self, event);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
gstyle_eyedropper_class_init (GstyleEyedropperClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = gstyle_eyedropper_finalize;
  object_class->set_property = gstyle_eyedropper_set_property;

  /**
   * GstyleEyedropper::color-picked:
   * @self: A #GstyleEyedropper.
   * @rgba: a #GdkRGBA color.
   *
   * This signal is emitted when you click to pick a color.
   */
  signals [COLOR_PICKED] = g_signal_new ("color-picked",
                                         G_TYPE_FROM_CLASS (klass),
                                         G_SIGNAL_RUN_LAST,
                                         0,
                                         NULL, NULL, NULL,
                                         G_TYPE_NONE,
                                         1,
                                         GDK_TYPE_RGBA);

  /**
   * GstyleEyedropper::grab-released:
   * @self: A #GstyleEyedropper.
   *
   * This signal is emitted when you release the grab by hitting 'Esc'.
   */
  signals [GRAB_RELEASED] = g_signal_new ("grab-released",
                                          G_TYPE_FROM_CLASS (klass),
                                          G_SIGNAL_RUN_LAST,
                                          0,
                                          NULL, NULL, NULL,
                                          G_TYPE_NONE,
                                          0);

  properties [PROP_SOURCE_EVENT] =
    g_param_spec_boxed ("source-event",
                        "source-event",
                        "the event generated when triggering the picker widget",
                        GDK_TYPE_EVENT,
                        (G_PARAM_CONSTRUCT_ONLY | G_PARAM_WRITABLE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, N_PROPS, properties);
}

static void
gstyle_eyedropper_init (GstyleEyedropper *self)
{
}
