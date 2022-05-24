/* ide-project-tree-addin.c
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

#define G_LOG_DOMAIN "ide-project-tree-addin"

#include "config.h"

#include "ide-project-tree-addin.h"

/**
 * SECTION:ide-project-tree-addin
 * @title: IdeProjectTreeAddin
 * @short_description: Addins to extend the project tree
 *
 * The #IdeProjectTreeAddin is used to extend the project tree. Plugins
 * can add new tree builders to the tree in the load virtual function. They
 * should remove the tree builders from the unload virtual function.
 */

G_DEFINE_INTERFACE (IdeProjectTreeAddin, ide_project_tree_addin, G_TYPE_OBJECT)

static void
ide_project_tree_addin_default_init (IdeProjectTreeAddinInterface *iface)
{
}

/**
 * ide_project_tree_addin_load:
 * @self: a #IdeProjectTreeAddin
 * @tree: a #IdeTree
 *
 * This function will call the IdeProjectTreeAddin::load vfunc of @self.
 *
 * This is used to initialize the project tree so that plugins can extend
 * the contents of the tree.
 *
 * Plugins should add a #IdeTreeBuilder to the tree when loading, and remove
 * them when unloading.
 *
 * See also: ide_project_tree_addin_unload()
 */
void
ide_project_tree_addin_load (IdeProjectTreeAddin *self,
                             IdeTree             *tree)
{
  g_return_if_fail (IDE_IS_PROJECT_TREE_ADDIN (self));
  g_return_if_fail (IDE_IS_TREE (tree));

  if (IDE_PROJECT_TREE_ADDIN_GET_IFACE (self)->load)
    IDE_PROJECT_TREE_ADDIN_GET_IFACE (self)->load (self, tree);
}

/**
 * ide_project_tree_addin_unload:
 * @self: a #IdeProjectTreeAddin
 * @tree: a #IdeTree
 *
 * This function will call the IdeProjectTreeAddin::unload vfunc of @self.
 *
 * This is used to unload the project tree so that plugins can clealy be
 * disabled by the user at runtime. Any changes to @tree done during load
 * should be undone here.
 *
 * See also: ide_project_tree_addin_load()
 */
void
ide_project_tree_addin_unload (IdeProjectTreeAddin *self,
                               IdeTree             *tree)
{
  g_return_if_fail (IDE_IS_PROJECT_TREE_ADDIN (self));
  g_return_if_fail (IDE_IS_TREE (tree));

  if (IDE_PROJECT_TREE_ADDIN_GET_IFACE (self)->unload)
    IDE_PROJECT_TREE_ADDIN_GET_IFACE (self)->unload (self, tree);
}
