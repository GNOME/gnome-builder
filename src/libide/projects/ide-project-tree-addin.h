/* ide-project-tree-addin.h
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

#include <libide-tree.h>

G_BEGIN_DECLS

#define IDE_TYPE_PROJECT_TREE_ADDIN (ide_project_tree_addin_get_type())

IDE_AVAILABLE_IN_ALL
G_DECLARE_INTERFACE (IdeProjectTreeAddin, ide_project_tree_addin, IDE, PROJECT_TREE_ADDIN, GObject)

struct _IdeProjectTreeAddinInterface
{
  GTypeInterface parent_iface;

  void (*load)   (IdeProjectTreeAddin *self,
                  IdeTree             *tree);
  void (*unload) (IdeProjectTreeAddin *self,
                  IdeTree             *tree);
};

IDE_AVAILABLE_IN_ALL
void ide_project_tree_addin_load   (IdeProjectTreeAddin *self,
                                    IdeTree             *tree);
IDE_AVAILABLE_IN_ALL
void ide_project_tree_addin_unload (IdeProjectTreeAddin *self,
                                    IdeTree             *tree);

G_END_DECLS
