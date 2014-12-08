/* gb-preferences-window.h
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

#ifndef GB_PREFERENCES_WINDOW_H
#define GB_PREFERENCES_WINDOW_H

#include <gtk/gtk.h>

G_BEGIN_DECLS

#define GB_TYPE_PREFERENCES_WINDOW            (gb_preferences_window_get_type())
#define GB_PREFERENCES_WINDOW(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), GB_TYPE_PREFERENCES_WINDOW, GbPreferencesWindow))
#define GB_PREFERENCES_WINDOW_CONST(obj)      (G_TYPE_CHECK_INSTANCE_CAST ((obj), GB_TYPE_PREFERENCES_WINDOW, GbPreferencesWindow const))
#define GB_PREFERENCES_WINDOW_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass),  GB_TYPE_PREFERENCES_WINDOW, GbPreferencesWindowClass))
#define GB_IS_PREFERENCES_WINDOW(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GB_TYPE_PREFERENCES_WINDOW))
#define GB_IS_PREFERENCES_WINDOW_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass),  GB_TYPE_PREFERENCES_WINDOW))
#define GB_PREFERENCES_WINDOW_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj),  GB_TYPE_PREFERENCES_WINDOW, GbPreferencesWindowClass))

typedef struct _GbPreferencesWindow        GbPreferencesWindow;
typedef struct _GbPreferencesWindowClass   GbPreferencesWindowClass;
typedef struct _GbPreferencesWindowPrivate GbPreferencesWindowPrivate;

struct _GbPreferencesWindow
{
  GtkWindow parent;

  /*< private >*/
  GbPreferencesWindowPrivate *priv;
};

struct _GbPreferencesWindowClass
{
  GtkWindowClass parent;

  void (*close) (GbPreferencesWindow *window);
};

GType      gb_preferences_window_get_type (void) G_GNUC_CONST;
GtkWidget *gb_preferences_window_new      (void);

G_END_DECLS

#endif /* GB_PREFERENCES_WINDOW_H */
