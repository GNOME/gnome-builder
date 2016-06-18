/* ide-workbench-header-bar-private.h
 *
 * Copyright (C) 2016 Christian Hergert <chergert@redhat.com>
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

#ifndef IDE_WORKBENCH_HEADER_BAR_PRIVATE_H
#define IDE_WORKBENCH_HEADER_BAR_PRIVATE_H

#include "workbench/ide-workbench-header-bar.h"
#include "workbench/ide-perspective.h"

G_BEGIN_DECLS

void _ide_workbench_header_bar_set_perspectives (IdeWorkbenchHeaderBar *self,
                                                 GListModel            *model)
  G_GNUC_INTERNAL;

void _ide_workbench_header_bar_set_perspective (IdeWorkbenchHeaderBar *self,
                                                IdePerspective        *perspective)
  G_GNUC_INTERNAL;

G_END_DECLS

#endif /* IDE_WORKBENCH_HEADER_BAR_PRIVATE_H */
