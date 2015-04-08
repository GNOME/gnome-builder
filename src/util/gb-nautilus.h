/* gb-nautilus.h
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

#ifndef GB_NAUTILUS_H
#define GB_NAUTILUS_H

#include <gio/gio.h>
#include <gtk/gtk.h>

G_BEGIN_DECLS

gboolean gb_nautilus_select_file (GtkWidget *widget,
                                  GFile     *file,
                                  guint32    user_time);

G_END_DECLS

#endif /* GB_NAUTILUS_H */
