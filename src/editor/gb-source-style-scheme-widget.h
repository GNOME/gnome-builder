/* gb-source-style-scheme-widget.h
 *
 * Copyright (C) 2014 Christian Hergert <christian@hergert.me>
 *
 * This file is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as
 * published by the Free Software Foundation; either version 3 of the
 * License, or (at your option) any later version.
 *
 * This file is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef GB_SOURCE_STYLE_SCHEME_WIDGET_H
#define GB_SOURCE_STYLE_SCHEME_WIDGET_H

#include <gtk/gtk.h>

G_BEGIN_DECLS

#define GB_TYPE_SOURCE_STYLE_SCHEME_WIDGET            (gb_source_style_scheme_widget_get_type())
#define GB_SOURCE_STYLE_SCHEME_WIDGET(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), GB_TYPE_SOURCE_STYLE_SCHEME_WIDGET, GbSourceStyleSchemeWidget))
#define GB_SOURCE_STYLE_SCHEME_WIDGET_CONST(obj)      (G_TYPE_CHECK_INSTANCE_CAST ((obj), GB_TYPE_SOURCE_STYLE_SCHEME_WIDGET, GbSourceStyleSchemeWidget const))
#define GB_SOURCE_STYLE_SCHEME_WIDGET_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass),  GB_TYPE_SOURCE_STYLE_SCHEME_WIDGET, GbSourceStyleSchemeWidgetClass))
#define GB_IS_SOURCE_STYLE_SCHEME_WIDGET(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GB_TYPE_SOURCE_STYLE_SCHEME_WIDGET))
#define GB_IS_SOURCE_STYLE_SCHEME_WIDGET_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass),  GB_TYPE_SOURCE_STYLE_SCHEME_WIDGET))
#define GB_SOURCE_STYLE_SCHEME_WIDGET_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj),  GB_TYPE_SOURCE_STYLE_SCHEME_WIDGET, GbSourceStyleSchemeWidgetClass))

typedef struct _GbSourceStyleSchemeWidget        GbSourceStyleSchemeWidget;
typedef struct _GbSourceStyleSchemeWidgetClass   GbSourceStyleSchemeWidgetClass;
typedef struct _GbSourceStyleSchemeWidgetPrivate GbSourceStyleSchemeWidgetPrivate;

struct _GbSourceStyleSchemeWidget
{
  GtkBin parent;

  /*< private >*/
  GbSourceStyleSchemeWidgetPrivate *priv;
};

struct _GbSourceStyleSchemeWidgetClass
{
  GtkBinClass parent;
};

GType        gb_source_style_scheme_widget_get_type              (void);
GtkWidget   *gb_source_style_scheme_widget_new                   (void);
const gchar *gb_source_style_scheme_widget_get_style_scheme_name (GbSourceStyleSchemeWidget *widget);
void         gb_source_style_scheme_widget_set_style_scheme_name (GbSourceStyleSchemeWidget *widget,
                                                                  const gchar               *style_scheme_name);

G_END_DECLS

#endif /* GB_SOURCE_STYLE_SCHEME_WIDGET_H */
