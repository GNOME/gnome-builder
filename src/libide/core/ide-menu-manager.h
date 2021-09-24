/* ide-menu-manager.h
 *
 * Copyright (C) 2015 Christian Hergert <chergert@redhat.com>
 *
 * This file is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This file is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef IDE_MENU_MANAGER_H
#define IDE_MENU_MANAGER_H

#include <gtk/gtk.h>

#include "ide-version-macros.h"

G_BEGIN_DECLS

#define IDE_TYPE_MENU_MANAGER (ide_menu_manager_get_type())

IDE_AVAILABLE_IN_ALL
G_DECLARE_FINAL_TYPE (IdeMenuManager, ide_menu_manager, IDE, MENU_MANAGER, GObject)

IDE_AVAILABLE_IN_ALL
IdeMenuManager *ide_menu_manager_new            (void);
IDE_AVAILABLE_IN_ALL
guint           ide_menu_manager_add_filename   (IdeMenuManager  *self,
                                                 const gchar     *filename,
                                                 GError         **error);
IDE_AVAILABLE_IN_ALL
guint           ide_menu_manager_add_resource   (IdeMenuManager  *self,
                                                 const gchar     *resource,
                                                 GError         **error);
IDE_AVAILABLE_IN_ALL
guint           ide_menu_manager_merge          (IdeMenuManager  *self,
                                                 const gchar     *menu_id,
                                                 GMenuModel      *model);
IDE_AVAILABLE_IN_ALL
void            ide_menu_manager_remove         (IdeMenuManager  *self,
                                                 guint            merge_id);
IDE_AVAILABLE_IN_ALL
GMenu          *ide_menu_manager_get_menu_by_id (IdeMenuManager  *self,
                                                 const gchar     *menu_id);

G_END_DECLS

#endif /* IDE_MENU_MANAGER_H */
