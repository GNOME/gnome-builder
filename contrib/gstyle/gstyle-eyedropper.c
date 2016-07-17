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
#include <math.h>

#include "gstyle-color.h"
#include "gstyle-color-widget.h"
#include "gstyle-css-provider.h"

#include "gstyle-eyedropper.h"

#define ZOOM_AREA_WIDTH 100
#define ZOOM_AREA_HEIGHT 100

/* The spot coords is the oriented distance between the window and the cursor
 * that mean the cursor is never inside the window, this also mean that the cursor
 * can only be in one of the four window corners area.
 */
#define ZOOM_AREA_SPOT_X -20
#define ZOOM_AREA_SPOT_Y -20

#define DEFAULT_ZOOM_FACTOR 2
#define MIN_ZOOM_FACTOR 1
#define MAX_ZOOM_FACTOR 20

struct _GstyleEyedropper
{
  GtkWindow          parent_instance;

  GstyleCssProvider *default_provider;
  GtkWidget         *source;
  GtkWidget         *window;
  GdkScreen         *screen;
  GtkWidget         *zoom_area;
  GdkCursor         *cursor;
  GdkSeat           *seat;
  GdkPixbuf         *pixbuf;
  GstyleColor       *color;

  gulong             key_handler_id;
  gulong             grab_broken_handler_id;
  gulong             motion_notify_handler_id;
  gulong             pointer_pressed_handler_id;
  gulong             pointer_released_handler_id;
  gulong             pointer_wheel_handler_id;
  gulong             screen_size_changed_handler_id;

  gdouble            zoom_factor;
  gint               screen_width;
  gint               screen_height;

  guint              button_pressed : 1;
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
  rgba->alpha = 1.0;

  g_object_unref (pixbuf);
}

static void
release_grab (GstyleEyedropper *self)
{
  gboolean has_grab = FALSE;

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

  if (self->pointer_pressed_handler_id)
    {
      g_signal_handler_disconnect (self->window, self->pointer_pressed_handler_id);
      self->pointer_pressed_handler_id = 0;
    }

  if (self->pointer_released_handler_id)
    {
      g_signal_handler_disconnect (self->window, self->pointer_released_handler_id);
      self->pointer_released_handler_id = 0;
    }

  if (self->screen_size_changed_handler_id)
    {
      g_signal_handler_disconnect (self->screen, self->screen_size_changed_handler_id);
      self->screen_size_changed_handler_id = 0;
    }

  if (self->window != NULL)
    if (gtk_widget_has_grab (self->window))
      {
        has_grab = TRUE;
        gtk_grab_remove (self->window);
      }

  if (self->seat != NULL)
    gdk_seat_ungrab (self->seat);

  g_clear_object (&self->default_provider);
  g_clear_object (&self->seat);
  g_clear_object (&self->cursor);

  if (self->window != NULL)
    {
      gtk_widget_destroy (self->window);
      self->window = NULL;
    }

  if (has_grab)
    g_signal_emit (self, signals [GRAB_RELEASED], 0);
}

static void
gstyle_eyedropper_event_get_root_coords (GstyleEyedropper *self,
                                         GdkEvent         *event,
                                         gint             *x_root,
                                         gint             *y_root)
{
  GdkSeat *seat;
  GdkDevice *pointer;

  seat = gdk_event_get_seat (event);
  pointer = gdk_seat_get_pointer (seat);
  gdk_device_get_position (pointer, NULL, x_root, y_root);
}

static void
gstyle_eyedropper_calculate_window_position (GstyleEyedropper *self,
                                             GtkWindow        *window,
                                             gint              cursor_root_x,
                                             gint              cursor_root_y,
                                             gint             *x,
                                             gint             *y)
{
  GtkAllocation alloc;
  gint spot_x = ZOOM_AREA_SPOT_X;
  gint spot_y = ZOOM_AREA_SPOT_Y;

  g_assert (GSTYLE_IS_EYEDROPPER (self));
  g_assert (GTK_IS_WINDOW (window));

  gtk_widget_get_allocated_size (GTK_WIDGET (window), &alloc, NULL);

  if ((spot_x < 0 && cursor_root_x > self->screen_width - alloc.width + spot_x * 2) ||
      (spot_x > 0 && cursor_root_x < alloc.width + spot_x * 2))
    spot_x = -spot_x;

  if (spot_x > 0)
    *x = cursor_root_x - alloc.width - spot_x;
  else
    *x = cursor_root_x - spot_x;

  if ((spot_y < 0 && cursor_root_y > self->screen_height - alloc.height + spot_y * 2) ||
      (spot_y > 0 && cursor_root_y < alloc.height + spot_y + 2))
    spot_y = -spot_y;

  if (spot_y > 0)
    *y = cursor_root_y - alloc.height - spot_y;
  else
    *y = cursor_root_y - spot_y;
}

static void
gstyle_eyedropper_draw_zoom_area (GstyleEyedropper *self,
                                  gint              cursor_x,
                                  gint              cursor_y)
{
  GdkWindow *window;
  GdkPixbuf *root_pixbuf;
  gint src_width;
  gint src_height;
  gint start_x;
  gint start_y;

  g_assert (GSTYLE_IS_EYEDROPPER (self));

  src_width = ZOOM_AREA_WIDTH / self->zoom_factor;
  src_height = ZOOM_AREA_HEIGHT / self->zoom_factor;
  window = gdk_screen_get_root_window (self->screen);

  start_x = MAX (cursor_x - src_width / 2, 0);
  if (start_x + src_width > self->screen_width)
    start_x = self->screen_width - src_width;

  start_y = MAX (cursor_y - src_height / 2, 0);
  if (start_y + src_height > self->screen_height)
    start_y = self->screen_height - src_height;

  root_pixbuf = gdk_pixbuf_get_from_window (window, start_x, start_y, src_width, src_height);
  g_clear_object (&self->pixbuf);
  self->pixbuf = gdk_pixbuf_scale_simple (root_pixbuf, ZOOM_AREA_WIDTH, ZOOM_AREA_HEIGHT, GDK_INTERP_NEAREST);
  g_object_unref (root_pixbuf);

  gtk_widget_queue_draw (self->zoom_area);
}

static void
gstyle_eyedropper_pointer_motion_notify_cb (GstyleEyedropper *self,
                                            GdkEventMotion   *event,
                                            GtkWindow        *window)
{
  GdkRGBA rgba;
  gint x_root, y_root;
  gint x, y;

  g_assert (GSTYLE_IS_EYEDROPPER (self));
  g_assert (GTK_IS_WINDOW (window));
  g_assert (event != NULL);
  g_assert (self->screen == gdk_event_get_screen ((GdkEvent *) event));

  gstyle_eyedropper_event_get_root_coords (self, (GdkEvent *)event, &x_root, &y_root);
  gstyle_eyedropper_calculate_window_position (self, GTK_WINDOW (self->window), event->x_root, event->y_root, &x, &y);
  gtk_window_move (GTK_WINDOW (self->window), x, y);

  gstyle_eyedropper_draw_zoom_area (self, event->x_root, event->y_root);
  get_rgba_at_cursor (self,
                      self->screen,
                      gdk_event_get_device ((GdkEvent *) event),
                      event->x_root, event->y_root, &rgba);

  gstyle_color_set_rgba (self->color, &rgba);
  if (self->button_pressed)
    g_signal_emit (self, signals [COLOR_PICKED], 0, &rgba);
}

static gboolean
gstyle_eyedropper_pointer_released_cb (GstyleEyedropper *self,
                                       GdkEventButton   *event,
                                       GtkWindow        *window)
{
  GdkRGBA rgba;

  g_assert (GSTYLE_IS_EYEDROPPER (self));
  g_assert (event != NULL);
  g_assert (GTK_IS_WINDOW (window));
  g_assert (self->screen == gdk_event_get_screen ((GdkEvent *) event));

  get_rgba_at_cursor (self,
                      self->screen,
                      gdk_event_get_device ((GdkEvent *) event),
                      event->x_root, event->y_root, &rgba);

  gstyle_color_set_rgba (self->color, &rgba);
  g_signal_emit (self, signals [COLOR_PICKED], 0, &rgba);

  release_grab (self);
  self->button_pressed = FALSE;

  return GDK_EVENT_STOP;
}

static gboolean
gstyle_eyedropper_pointer_pressed_cb (GstyleEyedropper *self,
                                      GdkEventButton   *event,
                                      GtkWindow        *window)
{
  g_assert (GSTYLE_IS_EYEDROPPER (self));
  g_assert (event != NULL);
  g_assert (GTK_IS_WINDOW (window));

  if (event->type == GDK_BUTTON_PRESS)
    {
      if (!self->button_pressed && event->button == GDK_BUTTON_PRIMARY)
        {
          self->button_pressed = TRUE;
          self->pointer_released_handler_id =
            g_signal_connect_object (window, "button-release-event",
                                     G_CALLBACK (gstyle_eyedropper_pointer_released_cb),
                                     self,
                                     G_CONNECT_SWAPPED);

          return GDK_EVENT_STOP;
        }
    }

  return GDK_EVENT_PROPAGATE;
}

static gboolean
gstyle_eyedropper_pointer_wheel_cb (GstyleEyedropper *self,
                                    GdkEventScroll   *event,
                                    GtkWindow        *window)
{
  g_assert (GSTYLE_IS_EYEDROPPER (self));
  g_assert (event != NULL);
  g_assert (GTK_IS_WINDOW (window));
  g_assert (self->screen == gdk_event_get_screen ((GdkEvent *) event));

  if (event->type == GDK_SCROLL)
    {
      if (event->direction == GDK_SCROLL_UP)
        self->zoom_factor = CLAMP (self->zoom_factor + 0.5, MIN_ZOOM_FACTOR, MAX_ZOOM_FACTOR);
      else if (event->direction == GDK_SCROLL_DOWN)
        self->zoom_factor = CLAMP (self->zoom_factor - 0.5, MIN_ZOOM_FACTOR, MAX_ZOOM_FACTOR);
      else
        return GDK_EVENT_PROPAGATE;
    }
  else
    return GDK_EVENT_PROPAGATE;

  gstyle_eyedropper_draw_zoom_area (self, event->x_root, event->y_root);
  return GDK_EVENT_STOP;
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

static void
gstyle_eyedropper_screen_size_changed_cb (GstyleEyedropper *self,
                                          GdkScreen        *screen)
{
  GdkDevice *pointer;
  gint x;
  gint y;

  g_assert (GSTYLE_IS_EYEDROPPER (self));
  g_assert (GDK_IS_SCREEN (screen));

  self->screen_width = gdk_screen_get_width (screen);
  self->screen_height = gdk_screen_get_height (screen);

  pointer = gdk_seat_get_pointer (self->seat);
  gdk_device_get_position (pointer, NULL, &x, &y);
  gstyle_eyedropper_draw_zoom_area (self, x, y);
}

static gint
gstyle_eyedropper_zoom_area_draw_cb (GstyleEyedropper *self,
                                     cairo_t          *cr,
                                     GtkWidget        *widget)
{
  g_assert (GSTYLE_IS_EYEDROPPER (self));

  if (self->pixbuf != NULL)
    {
      gdk_cairo_set_source_pixbuf (cr, self->pixbuf, 0, 0);
      cairo_paint (cr);
    }

  return TRUE;
}

void
gstyle_eyedropper_set_source_event (GstyleEyedropper *self,
                                    GdkEvent         *event)
{
  GtkWidget *source;
  GtkStyleContext *context;
  GtkWidget *box;
  GtkWidget *swatch;
  GdkGrabStatus status;
  gint x_root, y_root;
  gint x, y;

  g_return_if_fail (GSTYLE_IS_EYEDROPPER (self));
  g_return_if_fail (event != NULL);

  self->seat = g_object_ref (gdk_event_get_seat (event));
  source = gtk_get_event_widget (event);
  self->screen = gdk_event_get_screen (event);
  g_signal_connect_swapped (self->screen,
                            "size-changed",
                            G_CALLBACK (gstyle_eyedropper_screen_size_changed_cb),
                            self);

  self->window = g_object_ref_sink (gtk_window_new (GTK_WINDOW_POPUP));
  gtk_window_set_screen (GTK_WINDOW (self->window),self->screen);
  gtk_widget_set_name (self->window, "gstyleeyedropper");
  context = gtk_widget_get_style_context (self->window);
  self->default_provider = gstyle_css_provider_init_default (gtk_style_context_get_screen (context));

  box = gtk_box_new (GTK_ORIENTATION_VERTICAL, 6);
  gtk_container_add (GTK_CONTAINER (self->window), box);

  self->zoom_area = gtk_drawing_area_new ();
  gtk_widget_set_size_request (self->zoom_area, ZOOM_AREA_WIDTH, ZOOM_AREA_HEIGHT);
  gtk_container_add (GTK_CONTAINER (box), self->zoom_area);

  swatch = g_object_new (GSTYLE_TYPE_COLOR_WIDGET,
                         "fallback-name-kind", GSTYLE_COLOR_KIND_RGB_HEX6,
                         "fallback-name-visible", TRUE,
                         "color", self->color,
                         NULL);
  gtk_container_add (GTK_CONTAINER (box), swatch);

  g_signal_connect_object (self->zoom_area,
                           "draw",
                           G_CALLBACK (gstyle_eyedropper_zoom_area_draw_cb),
                           self,
                           G_CONNECT_SWAPPED);

  self->screen_width = gdk_screen_get_width (self->screen);
  self->screen_height = gdk_screen_get_height (self->screen);
  gstyle_eyedropper_event_get_root_coords (self, event, &x_root, &y_root);
  gstyle_eyedropper_calculate_window_position (self, GTK_WINDOW (self->window), x_root, y_root, &x, &y);
  gtk_window_move (GTK_WINDOW (self->window), x, y);
  gtk_widget_show_all (self->window);

  gtk_widget_add_events (self->window,
                         GDK_BUTTON_RELEASE_MASK | GDK_BUTTON_PRESS_MASK | GDK_POINTER_MOTION_MASK);

  self->cursor = gdk_cursor_new_from_name (gdk_screen_get_display (self->screen), "cell");
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

  self->motion_notify_handler_id =
    g_signal_connect_swapped (self->window, "motion-notify-event",
                              G_CALLBACK (gstyle_eyedropper_pointer_motion_notify_cb),
                              self);

  self->pointer_pressed_handler_id =
    g_signal_connect_swapped (self->window,
                              "button-press-event",
                              G_CALLBACK (gstyle_eyedropper_pointer_pressed_cb),
                              self);

  self->pointer_wheel_handler_id =
    g_signal_connect_swapped (self->window,
                              "scroll-event",
                              G_CALLBACK (gstyle_eyedropper_pointer_wheel_cb),
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

  release_grab (self);
  g_clear_object (&self->color);

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
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

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
  gtk_widget_class_set_css_name (widget_class, "gstyleeyedropper");
}

static void
gstyle_eyedropper_init (GstyleEyedropper *self)
{
  self->zoom_factor = DEFAULT_ZOOM_FACTOR;
  self->color = gstyle_color_new ("", GSTYLE_COLOR_KIND_RGB_HEX6, 0.0, 0.0, 0.0, 1.0);
}
