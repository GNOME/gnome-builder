/* egg-menu-manager.h
 *
 * Copyright (C) 2015 Christian Hergert <chergert@redhat.com>
 *
 * This file is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This file is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef EGG_MENU_MANAGER_H
#define EGG_MENU_MANAGER_H

#include <gtk/gtk.h>

G_BEGIN_DECLS

#define EGG_TYPE_MENU_MANAGER (egg_menu_manager_get_type())

G_DECLARE_FINAL_TYPE (EggMenuManager, egg_menu_manager, EGG, MENU_MANAGER, GObject)

EggMenuManager *egg_menu_manager_new            (void);
guint           egg_menu_manager_add_filename   (EggMenuManager  *self,
                                                 const gchar     *filename,
                                                 GError         **error);
guint           egg_menu_manager_add_resource   (EggMenuManager  *self,
                                                 const gchar     *resource,
                                                 GError         **error);
void            egg_menu_manager_remove         (EggMenuManager  *self,
                                                 guint            merge_id);
GMenu          *egg_menu_manager_get_menu_by_id (EggMenuManager  *self,
                                                 const gchar     *menu_id);

G_END_DECLS

#endif /* EGG_MENU_MANAGER_H */
