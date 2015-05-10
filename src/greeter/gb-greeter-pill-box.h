/* gb-greeter-pill-box.h
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

#ifndef GB_GREETER_PILL_BOX_H
#define GB_GREETER_PILL_BOX_H

#include <gtk/gtk.h>

G_BEGIN_DECLS

#define GB_TYPE_GREETER_PILL_BOX (gb_greeter_pill_box_get_type())

G_DECLARE_FINAL_TYPE (GbGreeterPillBox, gb_greeter_pill_box, GB, GREETER_PILL_BOX, GtkEventBox)

GtkWidget   *gb_greeter_pill_box_new       (const gchar      *label);
const gchar *gb_greeter_pill_box_get_label (GbGreeterPillBox *self);
void         gb_greeter_pill_box_set_label (GbGreeterPillBox *self,
                                            const gchar      *label);

G_END_DECLS

#endif /* GB_GREETER_PILL_BOX_H */
