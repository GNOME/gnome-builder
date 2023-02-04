/* gbp-project-tree-private.h
 *
 * Copyright 2018-2019 Christian Hergert <chergert@redhat.com>
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

#include <libide-gui.h>
#include <libide-tree.h>

#include "gbp-project-tree-pane.h"

G_BEGIN_DECLS

struct _GbpProjectTreePane
{
  IdePane             parent_instance;

  GCancellable       *cancellable;

  IdeTree            *tree;
  GtkStack           *stack;
  GtkSearchEntry     *search;
  GtkSingleSelection *selection;
  GtkListView        *list;
  GActionGroup       *actions;

  guint               has_loaded : 1;
};

void _gbp_project_tree_pane_init_actions   (GbpProjectTreePane *self);
void _gbp_project_tree_pane_update_actions (GbpProjectTreePane *self);

G_END_DECLS
