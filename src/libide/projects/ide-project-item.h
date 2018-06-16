/* ide-project-item.h
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

#include "ide-version-macros.h"

#include "ide-object.h"

G_BEGIN_DECLS

#define IDE_TYPE_PROJECT_ITEM (ide_project_item_get_type())

IDE_AVAILABLE_IN_ALL
G_DECLARE_DERIVABLE_TYPE (IdeProjectItem, ide_project_item, IDE, PROJECT_ITEM, IdeObject)

struct _IdeProjectItemClass
{
  IdeObjectClass parent_class;
};

IDE_AVAILABLE_IN_ALL
IdeProjectItem *ide_project_item_new          (IdeProjectItem *parent);
IDE_AVAILABLE_IN_ALL
IdeProjectItem *ide_project_item_get_parent   (IdeProjectItem *item);
IDE_AVAILABLE_IN_ALL
void            ide_project_item_append       (IdeProjectItem *item,
                                               IdeProjectItem *child);
IDE_AVAILABLE_IN_ALL
void            ide_project_item_remove       (IdeProjectItem *item,
                                               IdeProjectItem *child);
IDE_AVAILABLE_IN_ALL
GSequence      *ide_project_item_get_children (IdeProjectItem *item);

G_END_DECLS
