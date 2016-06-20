/* ide-perspective-menu-button.h
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

#ifndef IDE_PERSPECTIVE_MENU_BUTTON_H
#define IDE_PERSPECTIVE_MENU_BUTTON_H

#include <gtk/gtk.h>

G_BEGIN_DECLS

G_DECLARE_FINAL_TYPE (IdePerspectiveMenuButton, ide_perspective_menu_button, IDE, PERSPECTIVE_MENU_BUTTON, GtkMenuButton)

GtkWidget *ide_perspective_menu_button_get_stack (IdePerspectiveMenuButton *self);
void       ide_perspective_menu_button_set_stack (IdePerspectiveMenuButton *self,
                                                  GtkWidget                *stack);

G_END_DECLS

#endif /* IDE_PERSPECTIVE_MENU_BUTTON_H */
