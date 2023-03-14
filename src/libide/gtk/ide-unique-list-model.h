/* ide-unique-list-model.h
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

#include <libide-core.h>

#include <gtk/gtk.h>

G_BEGIN_DECLS

#define IDE_TYPE_UNIQUE_LIST_MODEL (ide_unique_list_model_get_type())

IDE_AVAILABLE_IN_44
G_DECLARE_FINAL_TYPE (IdeUniqueListModel, ide_unique_list_model, IDE, UNIQUE_LIST_MODEL, GObject)

IDE_AVAILABLE_IN_44
IdeUniqueListModel *ide_unique_list_model_new             (GListModel         *model,
                                                           GtkSorter          *sorter);
IDE_AVAILABLE_IN_44
GListModel         *ide_unique_list_model_get_model       (IdeUniqueListModel *self);
IDE_AVAILABLE_IN_44
void                ide_unique_list_model_set_model       (IdeUniqueListModel *self,
                                                           GListModel         *model);
IDE_AVAILABLE_IN_44
GtkSorter          *ide_unique_list_model_get_sorter      (IdeUniqueListModel *self);
IDE_AVAILABLE_IN_44
void                ide_unique_list_model_set_sorter      (IdeUniqueListModel *self,
                                                           GtkSorter          *sorter);
IDE_AVAILABLE_IN_44
gboolean            ide_unique_list_model_get_incremental (IdeUniqueListModel *self);
IDE_AVAILABLE_IN_44
void                ide_unique_list_model_set_incremental (IdeUniqueListModel *self,
                                                           gboolean            incremental);

G_END_DECLS
