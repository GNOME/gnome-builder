/* gb-about-window.c
 *
 * Copyright (C) 2014 Christian Hergert <christian@hergert.me>
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

#include <glib/gi18n.h>

#include "gb-about-window.h"

G_DEFINE_TYPE (GbAboutWindow, gb_about_window, GTK_TYPE_WINDOW)

static gboolean
gb_about_window_draw (GtkWidget *widget,
                      cairo_t   *cr)
{
  GtkAllocation alloc;

  g_return_val_if_fail (GB_IS_ABOUT_WINDOW (widget), FALSE);
  g_return_val_if_fail (cr, FALSE);

  gtk_widget_get_allocation (widget, &alloc);

  cairo_save (cr);
  cairo_set_operator (cr, CAIRO_OPERATOR_SOURCE);
  cairo_set_source_rgba (cr, 1, 1, 1, 0);
  gdk_cairo_rectangle (cr, &alloc);
  cairo_paint (cr);
  cairo_restore (cr);

  GTK_WIDGET_CLASS (gb_about_window_parent_class)->draw (widget, cr);

  return GDK_EVENT_PROPAGATE;
}

static void
gb_about_window_constructed (GObject *object)
{
  GdkVisual *visual;

  G_OBJECT_CLASS (gb_about_window_parent_class)->constructed (object);

  visual = gdk_screen_get_rgba_visual (gdk_screen_get_default ());
  gtk_widget_set_visual (GTK_WIDGET (object), visual);
}

static void
gb_about_window_class_init (GbAboutWindowClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  object_class->constructed = gb_about_window_constructed;

  widget_class->draw = gb_about_window_draw;

  gtk_widget_class_set_template_from_resource (widget_class,
                                               "/org/gnome/builder/ui/gb-about-window.ui");
}

static void
gb_about_window_init (GbAboutWindow *window)
{
  window->priv = gb_about_window_get_instance_private (window);

  gtk_widget_init_template (GTK_WIDGET (window));
}
