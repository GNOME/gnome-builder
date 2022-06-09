/* ide-gtk-init.c
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

#define G_LOG_DOMAIN "ide-gtk-init"

#include "config.h"

#include "ide-gtk-resources.h"

#include "ide-animation.h"
#include "ide-entry-popover.h"
#include "ide-file-chooser-entry.h"
#include "ide-gtk-private.h"
#include "ide-cell-renderer-fancy.h"
#include "ide-enum-object.h"
#include "ide-fancy-tree-view.h"
#include "ide-progress-icon.h"
#include "ide-radio-box.h"
#include "ide-search-entry.h"
#include "ide-three-grid.h"
#include "ide-truncate-model.h"

void
_ide_gtk_init (void)
{
  g_type_ensure (IDE_TYPE_ANIMATION);
  g_type_ensure (IDE_TYPE_CELL_RENDERER_FANCY);
  g_type_ensure (IDE_TYPE_ENUM_OBJECT);
  g_type_ensure (IDE_TYPE_ENTRY_POPOVER);
  g_type_ensure (IDE_TYPE_FANCY_TREE_VIEW);
  g_type_ensure (IDE_TYPE_FILE_CHOOSER_ENTRY);
  g_type_ensure (IDE_TYPE_PROGRESS_ICON);
  g_type_ensure (IDE_TYPE_RADIO_BOX);
  g_type_ensure (IDE_TYPE_SEARCH_ENTRY);
  g_type_ensure (IDE_TYPE_THREE_GRID);
  g_type_ensure (IDE_TYPE_TRUNCATE_MODEL);

  g_resources_register (ide_gtk_get_resource ());
}
