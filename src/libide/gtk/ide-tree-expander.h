/* ide-tree-expander.h
 *
 * Copyright 2022 Christian Hergert <chergert@redhat.com>
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

#include <gtk/gtk.h>

#include <libide-core.h>

G_BEGIN_DECLS

#define IDE_TYPE_TREE_EXPANDER (ide_tree_expander_get_type())

IDE_AVAILABLE_IN_ALL
G_DECLARE_FINAL_TYPE (IdeTreeExpander, ide_tree_expander, IDE, TREE_EXPANDER, GtkWidget)

IDE_AVAILABLE_IN_ALL
GtkWidget      *ide_tree_expander_new                    (void);
IDE_AVAILABLE_IN_ALL
GMenuModel     *ide_tree_expander_get_menu_model         (IdeTreeExpander *self);
IDE_AVAILABLE_IN_ALL
void            ide_tree_expander_set_menu_model         (IdeTreeExpander *self,
                                                          GMenuModel      *menu_model);
IDE_AVAILABLE_IN_ALL
GIcon          *ide_tree_expander_get_icon               (IdeTreeExpander *self);
IDE_AVAILABLE_IN_ALL
void            ide_tree_expander_set_icon               (IdeTreeExpander *self,
                                                          GIcon           *icon);
IDE_AVAILABLE_IN_ALL
void            ide_tree_expander_set_icon_name          (IdeTreeExpander *self,
                                                          const char      *icon_name);
IDE_AVAILABLE_IN_ALL
GIcon          *ide_tree_expander_get_expanded_icon      (IdeTreeExpander *self);
IDE_AVAILABLE_IN_ALL
void            ide_tree_expander_set_expanded_icon      (IdeTreeExpander *self,
                                                          GIcon           *icon);
IDE_AVAILABLE_IN_ALL
void            ide_tree_expander_set_expanded_icon_name (IdeTreeExpander *self,
                                                          const char      *expanded_icon_name);
IDE_AVAILABLE_IN_ALL
const char     *ide_tree_expander_get_title              (IdeTreeExpander *self);
IDE_AVAILABLE_IN_ALL
void            ide_tree_expander_set_title              (IdeTreeExpander *self,
                                                          const char      *title);
IDE_AVAILABLE_IN_46
gboolean        ide_tree_expander_get_ignored            (IdeTreeExpander *self);
IDE_AVAILABLE_IN_46
void            ide_tree_expander_set_ignored            (IdeTreeExpander *self,
                                                          gboolean         ignored);
IDE_AVAILABLE_IN_ALL
GtkWidget      *ide_tree_expander_get_suffix             (IdeTreeExpander *self);
IDE_AVAILABLE_IN_ALL
void            ide_tree_expander_set_suffix             (IdeTreeExpander *self,
                                                          GtkWidget       *suffix);
IDE_AVAILABLE_IN_ALL
GtkTreeListRow *ide_tree_expander_get_list_row           (IdeTreeExpander *self);
IDE_AVAILABLE_IN_ALL
void            ide_tree_expander_set_list_row           (IdeTreeExpander *self,
                                                          GtkTreeListRow  *list_row);
IDE_AVAILABLE_IN_ALL
gpointer        ide_tree_expander_get_item               (IdeTreeExpander *self);
IDE_AVAILABLE_IN_44
gboolean        ide_tree_expander_get_use_markup         (IdeTreeExpander *self);
IDE_AVAILABLE_IN_44
void            ide_tree_expander_set_use_markup         (IdeTreeExpander *self,
                                                          gboolean         use_markup);
IDE_AVAILABLE_IN_44
void            ide_tree_expander_show_popover           (IdeTreeExpander *self,
                                                          GtkPopover      *popover);

G_END_DECLS
