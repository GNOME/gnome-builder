/* ide-joined-menu-private.h
 *
 * Copyright 2017-2021 Christian Hergert <chergert@redhat.com>
 *
 * This file is free software; you can redistribute it and/or modify it under
 * the terms of the GNU Lesser General Public License as published by the Free
 * Software Foundation; either version 3 of the License, or (at your option)
 * any later version.
 *
 * This file is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU Lesser General Public
 * License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#pragma once

#if !defined (IDE_GTK_INSIDE) && !defined (IDE_GTK_COMPILATION)
# error "Only <libide-gtk.h> can be included directly."
#endif

#include <libide-core.h>

G_BEGIN_DECLS

#define IDE_TYPE_JOINED_MENU (ide_joined_menu_get_type())

IDE_AVAILABLE_IN_ALL
G_DECLARE_FINAL_TYPE (IdeJoinedMenu, ide_joined_menu, IDE, JOINED_MENU, GMenuModel)

IDE_AVAILABLE_IN_ALL
IdeJoinedMenu *ide_joined_menu_new          (void);
IDE_AVAILABLE_IN_ALL
guint          ide_joined_menu_get_n_joined (IdeJoinedMenu *self);
IDE_AVAILABLE_IN_ALL
void           ide_joined_menu_append_menu  (IdeJoinedMenu *self,
                                             GMenuModel    *model);
IDE_AVAILABLE_IN_ALL
void           ide_joined_menu_prepend_menu (IdeJoinedMenu *self,
                                             GMenuModel    *model);
IDE_AVAILABLE_IN_ALL
void           ide_joined_menu_remove_menu  (IdeJoinedMenu *self,
                                             GMenuModel    *model);
IDE_AVAILABLE_IN_ALL
void           ide_joined_menu_remove_index (IdeJoinedMenu *self,
                                             guint          index);

G_END_DECLS
