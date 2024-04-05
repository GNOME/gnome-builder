/*
 * manuals-path-element.h
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

G_BEGIN_DECLS

#define MANUALS_TYPE_PATH_ELEMENT (manuals_path_element_get_type())

G_DECLARE_FINAL_TYPE (ManualsPathElement, manuals_path_element, MANUALS, PATH_ELEMENT, GObject)

struct _ManualsPathElement
{
  GObject parent_instance;
  GObject *item;
  GIcon *icon;
  char *title;
  guint is_root : 1;
  guint is_leaf : 1;
};

ManualsPathElement *manuals_path_element_new           (void);
gpointer            manuals_path_element_get_item      (ManualsPathElement *self);
void                manuals_path_element_set_item      (ManualsPathElement *self,
                                                        gpointer            item);
const char         *manuals_path_element_get_title     (ManualsPathElement *self);
void                manuals_path_element_set_title     (ManualsPathElement *self,
                                                        const char         *title);
GIcon              *manuals_path_element_get_icon      (ManualsPathElement *self);
void                manuals_path_element_set_icon      (ManualsPathElement *self,
                                                        GIcon              *icon);
gboolean            manuals_path_element_get_show_icon (ManualsPathElement *self);

G_END_DECLS
