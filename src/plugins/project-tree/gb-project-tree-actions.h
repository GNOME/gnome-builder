/* gb-project-tree-actions.h
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

#include "gb-project-tree.h"

G_BEGIN_DECLS

void gb_project_tree_actions_init   (GbProjectTree *self);
void gb_project_tree_actions_update (GbProjectTree *self);

typedef gboolean (*gb_project_tree_action_enable_cb) (gboolean);

void register_tree_action_build_check(gb_project_tree_action_enable_cb cb);
void register_tree_action_rebuild_check(gb_project_tree_action_enable_cb cb);

G_END_DECLS
