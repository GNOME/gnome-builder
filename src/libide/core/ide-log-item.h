/* ide-log-item.h
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

#include <glib-object.h>

#include "ide-version-macros.h"

G_BEGIN_DECLS

#define IDE_TYPE_LOG_ITEM (ide_log_item_get_type())

IDE_AVAILABLE_IN_44
G_DECLARE_FINAL_TYPE (IdeLogItem, ide_log_item, IDE, LOG_ITEM, GObject)

IDE_AVAILABLE_IN_44
const char     *ide_log_item_get_domain     (IdeLogItem *self);
IDE_AVAILABLE_IN_44
const char     *ide_log_item_get_message    (IdeLogItem *self);
IDE_AVAILABLE_IN_44
GDateTime      *ide_log_item_get_created_at (IdeLogItem *self);
IDE_AVAILABLE_IN_44
GLogLevelFlags  ide_log_item_get_severity   (IdeLogItem *self);

G_END_DECLS
