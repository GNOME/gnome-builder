/*
 * Copyright © 2018 Benjamin Otte
 * Copyright © 2023 Christian Hergert
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library. If not, see <http://www.gnu.org/licenses/>.
 *
 * Authors: Benjamin Otte <otte@gnome.org>
 *          Christian Hergert <chergert@gnome.org>
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
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
IDE_AVAILABLE_IN_44
guint               ide_unique_list_model_get_pending     (IdeUniqueListModel *self);

G_END_DECLS
