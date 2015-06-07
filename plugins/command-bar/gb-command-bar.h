/* gb-command-bar.h
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

#ifndef GB_COMMAND_BAR_H
#define GB_COMMAND_BAR_H

#include <gtk/gtk.h>

G_BEGIN_DECLS

#define GB_TYPE_COMMAND_BAR (gb_command_bar_get_type())

G_DECLARE_FINAL_TYPE (GbCommandBar, gb_command_bar, GB, COMMAND_BAR, GtkRevealer)

GtkWidget *gb_command_bar_new  (void);
void       gb_command_bar_show (GbCommandBar *bar);
void       gb_command_bar_hide (GbCommandBar *bar);

G_END_DECLS

#endif /* GB_COMMAND_BAR_H */
