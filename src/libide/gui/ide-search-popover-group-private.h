/* ide-search-popover-group-private.h
 *
 * Copyright 2023 Christian Hergert <chergert@redhat.com>
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

#include <libide-search.h>

G_BEGIN_DECLS

#define IDE_TYPE_SEARCH_POPOVER_GROUP (ide_search_popover_group_get_type())

G_DECLARE_FINAL_TYPE (IdeSearchPopoverGroup, ide_search_popover_group, IDE, SEARCH_POPOVER_GROUP, GObject)

const char        *ide_search_popover_group_get_title     (IdeSearchPopoverGroup *self);
const char        *ide_search_popover_group_get_icon_name (IdeSearchPopoverGroup *self);
IdeSearchCategory  ide_search_popover_group_get_category  (IdeSearchPopoverGroup *self);

G_END_DECLS
