/*
 * manuals-navigatable.h
 *
 * Copyright 2024 Christian Hergert <chergert@redhat.com>
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
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include <gio/gio.h>
#include <libdex.h>

G_BEGIN_DECLS

#define MANUALS_TYPE_NAVIGATABLE (manuals_navigatable_get_type())

G_DECLARE_FINAL_TYPE (ManualsNavigatable, manuals_navigatable, MANUALS, NAVIGATABLE, GObject)

ManualsNavigatable *manuals_navigatable_new              (void);
ManualsNavigatable *manuals_navigatable_new_for_resource (GObject            *resource);
GIcon              *manuals_navigatable_get_icon         (ManualsNavigatable *self);
void                manuals_navigatable_set_icon         (ManualsNavigatable *self,
                                                          GIcon              *icon);
const char         *manuals_navigatable_get_title        (ManualsNavigatable *self);
void                manuals_navigatable_set_title        (ManualsNavigatable *self,
                                                          const char         *title);
GIcon              *manuals_navigatable_get_menu_icon    (ManualsNavigatable *self);
void                manuals_navigatable_set_menu_icon    (ManualsNavigatable *self,
                                                          GIcon              *menu_icon);
const char         *manuals_navigatable_get_menu_title   (ManualsNavigatable *self);
void                manuals_navigatable_set_menu_title   (ManualsNavigatable *self,
                                                          const char         *menu_title);
const char         *manuals_navigatable_get_uri          (ManualsNavigatable *self);
void                manuals_navigatable_set_uri          (ManualsNavigatable *self,
                                                          const char         *uri);
gpointer            manuals_navigatable_get_item         (ManualsNavigatable *self);
void                manuals_navigatable_set_item         (ManualsNavigatable *self,
                                                          gpointer            item);
DexFuture          *manuals_navigatable_find_parent      (ManualsNavigatable *self);
DexFuture          *manuals_navigatable_find_children    (ManualsNavigatable *self);
DexFuture          *manuals_navigatable_find_peers       (ManualsNavigatable *self);

G_END_DECLS
