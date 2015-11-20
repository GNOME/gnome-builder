/*
 * ide-menu-extension.h
 * This file is part of gb
 *
 * Copyright (C) 2014 - Ignacio Casal Quinteiro
 *
 * gb is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * gb is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with gb. If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef __IDE_MENU_EXTENSION_H__
#define __IDE_MENU_EXTENSION_H__

#include <glib-object.h>
#include <gio/gio.h>

G_BEGIN_DECLS

#define IDE_TYPE_MENU_EXTENSION (ide_menu_extension_get_type ())

G_DECLARE_FINAL_TYPE (IdeMenuExtension, ide_menu_extension, IDE, MENU_EXTENSION, GObject)

IdeMenuExtension       *ide_menu_extension_new                 (GMenu           *menu);
IdeMenuExtension       *ide_menu_extension_new_for_section     (GMenu           *menu,
                                                              const gchar     *section);

void                   ide_menu_extension_append_menu_item    (IdeMenuExtension *menu,
                                                              GMenuItem       *item);

void                   ide_menu_extension_prepend_menu_item   (IdeMenuExtension *menu,
                                                              GMenuItem       *item);

void                   ide_menu_extension_remove_items        (IdeMenuExtension *menu);

G_END_DECLS

#endif /* __IDE_MENU_EXTENSION_H__ */

/* ex:set ts=8 noet: */

