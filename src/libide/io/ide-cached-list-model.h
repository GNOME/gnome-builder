/* ide-cached-list-model.h
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

#pragma once

#include <libide-core.h>

G_BEGIN_DECLS

#define IDE_TYPE_CACHED_LIST_MODEL (ide_cached_list_model_get_type())

IDE_AVAILABLE_IN_ALL
G_DECLARE_FINAL_TYPE (IdeCachedListModel, ide_cached_list_model, IDE, CACHED_LIST_MODEL, GObject)

IDE_AVAILABLE_IN_ALL
IdeCachedListModel *ide_cached_list_model_new       (GListModel         *model);
IDE_AVAILABLE_IN_ALL
GListModel         *ide_cached_list_model_get_model (IdeCachedListModel *self);
IDE_AVAILABLE_IN_ALL
void                ide_cached_list_model_set_model (IdeCachedListModel *self,
                                                     GListModel         *model);

G_END_DECLS
