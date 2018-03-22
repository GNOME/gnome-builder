/* gb-project-tree.h
 *
 * Copyright 2015 Christian Hergert <christian@hergert.me>
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

#pragma once

#include <ide.h>

G_BEGIN_DECLS

#define GB_TYPE_PROJECT_TREE (gb_project_tree_get_type())

G_DECLARE_FINAL_TYPE (GbProjectTree, gb_project_tree, GB, PROJECT_TREE, DzlTree)

GtkWidget  *gb_project_tree_new                    (void);
void        gb_project_tree_set_context            (GbProjectTree *self,
                                                    IdeContext    *context);
IdeContext *gb_project_tree_get_context            (GbProjectTree *self);
gboolean    gb_project_tree_get_show_ignored_files (GbProjectTree *self);
void        gb_project_tree_set_show_ignored_files (GbProjectTree *self,
                                                    gboolean       show_ignored_files);
void        gb_project_tree_reveal                 (GbProjectTree *self,
                                                    GFile         *file,
                                                    gboolean       focus_tree_view,
                                                    gboolean       expand_folder);
DzlTreeNode *
            gb_project_tree_find_file_node         (GbProjectTree *self,
                                                    GFile         *file);

G_END_DECLS
