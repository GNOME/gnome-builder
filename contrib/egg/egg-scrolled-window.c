/* egg-scrolled-window.c
 *
 * Copyright (C) 2016 Christian Hergert <chergert@redhat.com>
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

#define G_LOG_DOMAIN "egg-scrolled-window"

#include "egg-scrolled-window.h"

struct _EggScrolledWindow
{
  GtkScrolledWindow parent_instance;
};

G_DEFINE_TYPE (EggScrolledWindow, egg_scrolled_window, GTK_TYPE_SCROLLED_WINDOW)

static void
egg_scrolled_window_get_preferred_height_for_width (GtkWidget *widget,
                                                    gint       width,
                                                    gint      *min_height,
                                                    gint      *nat_height)
{
  EggScrolledWindow *self = (EggScrolledWindow *)widget;
  gint border_width;
  gint min_content_height;
  gint max_content_height;
  GtkWidget *child;

  g_assert (EGG_IS_SCROLLED_WINDOW (self));
  g_assert (min_height != NULL);
  g_assert (nat_height != NULL);

  min_content_height = gtk_scrolled_window_get_min_content_height (GTK_SCROLLED_WINDOW (self));
  max_content_height = gtk_scrolled_window_get_max_content_height (GTK_SCROLLED_WINDOW (self));
  border_width = gtk_container_get_border_width (GTK_CONTAINER (self));
  child = gtk_bin_get_child (GTK_BIN (self));

  if (child == NULL)
    {
      *min_height = 0;
      *nat_height = 0;
      return;
    }

  gtk_widget_get_preferred_height_for_width (child, width, min_height, nat_height);

  if (min_content_height > 0)
    *min_height = MAX (*min_height, min_content_height);
  else
    *min_height = 1;

  if (max_content_height > 0)
    *nat_height = MIN (*nat_height, max_content_height);

  *nat_height = MAX (*min_height, *nat_height);

  /*
   * Special case for our use. What we should probably do is have a "grow with child
   * range" but still fill into larger space with vexpand.
   *
   * This tries to enfoce at least a 5x3 ratio for the content, for asthetic reasons.
   */
  if (*nat_height > width && *min_height < (width / 5 * 3))
    *min_height = (width / 5 * 3);

  *min_height += border_width * 2;
  *nat_height += border_width * 2;
}

static GtkSizeRequestMode
egg_scrolled_window_get_request_mode (GtkWidget *widget)
{
  return GTK_SIZE_REQUEST_HEIGHT_FOR_WIDTH;
}

static void
egg_scrolled_window_class_init (EggScrolledWindowClass *klass)
{
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  widget_class->get_preferred_height_for_width = egg_scrolled_window_get_preferred_height_for_width;
  widget_class->get_request_mode = egg_scrolled_window_get_request_mode;
}

static void
egg_scrolled_window_init (EggScrolledWindow *self)
{
}
