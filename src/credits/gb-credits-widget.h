/* gb-credits-widget.h
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

#ifndef GB_CREDITS_WIDGET_H
#define GB_CREDITS_WIDGET_H

#include <gtk/gtk.h>

G_BEGIN_DECLS

#define GB_TYPE_CREDITS_WIDGET            (gb_credits_widget_get_type())
#define GB_CREDITS_WIDGET(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), GB_TYPE_CREDITS_WIDGET, GbCreditsWidget))
#define GB_CREDITS_WIDGET_CONST(obj)      (G_TYPE_CHECK_INSTANCE_CAST ((obj), GB_TYPE_CREDITS_WIDGET, GbCreditsWidget const))
#define GB_CREDITS_WIDGET_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass),  GB_TYPE_CREDITS_WIDGET, GbCreditsWidgetClass))
#define GB_IS_CREDITS_WIDGET(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GB_TYPE_CREDITS_WIDGET))
#define GB_IS_CREDITS_WIDGET_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass),  GB_TYPE_CREDITS_WIDGET))
#define GB_CREDITS_WIDGET_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj),  GB_TYPE_CREDITS_WIDGET, GbCreditsWidgetClass))

typedef struct _GbCreditsWidget        GbCreditsWidget;
typedef struct _GbCreditsWidgetClass   GbCreditsWidgetClass;
typedef struct _GbCreditsWidgetPrivate GbCreditsWidgetPrivate;

struct _GbCreditsWidget
{
  GtkOverlay parent;

  /*< private >*/
  GbCreditsWidgetPrivate *priv;
};

struct _GbCreditsWidgetClass
{
  GtkOverlayClass parent;
};

GType      gb_credits_widget_get_type     (void);
GtkWidget *gb_credits_widget_new          (void);
void       gb_credits_widget_start        (GbCreditsWidget *widget);
void       gb_credits_widget_stop         (GbCreditsWidget *widget);
gboolean   gb_credits_widget_is_rolling   (GbCreditsWidget *widget);
guint      gb_credits_widget_get_duration (GbCreditsWidget *widget);
void       gb_credits_widget_set_duration (GbCreditsWidget *widget,
                                           guint            duration);
gdouble    gb_credits_widget_get_progress (GbCreditsWidget *widget);
void       gb_credits_widget_set_progress (GbCreditsWidget *widget,
                                           gdouble          progress);

G_END_DECLS

#endif /* GB_CREDITS_WIDGET_H */
