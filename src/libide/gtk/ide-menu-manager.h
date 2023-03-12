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

#pragma once

#if !defined (IDE_GTK_INSIDE) && !defined (IDE_GTK_COMPILATION)
# error "Only <libide-gtk.h> can be included directly."
#endif

#include <gtk/gtk.h>

#include <libide-core.h>

G_BEGIN_DECLS

#define IDE_TYPE_MENU_MANAGER (ide_menu_manager_get_type())

IDE_AVAILABLE_IN_ALL
G_DECLARE_FINAL_TYPE (IdeMenuManager, ide_menu_manager, IDE, MENU_MANAGER, GObject)

IDE_AVAILABLE_IN_ALL
IdeMenuManager     *ide_menu_manager_new                  (void);
IDE_AVAILABLE_IN_ALL
guint               ide_menu_manager_add_filename         (IdeMenuManager  *self,
                                                           const char      *filename,
                                                           GError         **error);
IDE_AVAILABLE_IN_ALL
guint               ide_menu_manager_add_resource         (IdeMenuManager  *self,
                                                           const char      *resource,
                                                           GError         **error);
IDE_AVAILABLE_IN_ALL
guint               ide_menu_manager_merge                (IdeMenuManager  *self,
                                                           const char      *menu_id,
                                                           GMenuModel      *model);
IDE_AVAILABLE_IN_ALL
void                ide_menu_manager_remove               (IdeMenuManager  *self,
                                                           guint            merge_id);
IDE_AVAILABLE_IN_ALL
GMenu              *ide_menu_manager_get_menu_by_id       (IdeMenuManager  *self,
                                                           const char      *menu_id);
IDE_AVAILABLE_IN_ALL
const char * const *ide_menu_manager_get_menu_ids         (IdeMenuManager  *self);
IDE_AVAILABLE_IN_44
void                ide_menu_manager_set_attribute_string (IdeMenuManager  *self,
                                                           GMenu           *menu,
                                                           guint            position,
                                                           const char      *attribute,
                                                           const char      *value);
IDE_AVAILABLE_IN_44
GMenu              *ide_menu_manager_find_item_by_id      (IdeMenuManager  *self,
                                                           const char      *id,
                                                           guint           *position);

G_END_DECLS
