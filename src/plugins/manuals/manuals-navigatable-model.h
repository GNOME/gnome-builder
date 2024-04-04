/*
 * manuals-navigatable-model.h
 *
 * Copyright 2024 Christian Hergert <chergert@redhat.com>
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

#include <gio/gio.h>

#include "manuals-navigatable.h"

G_BEGIN_DECLS

#define MANUALS_TYPE_NAVIGATABLE_MODEL (manuals_navigatable_model_get_type())

G_DECLARE_FINAL_TYPE (ManualsNavigatableModel, manuals_navigatable_model, MANUALS, NAVIGATABLE_MODEL, GObject)

ManualsNavigatableModel *manuals_navigatable_model_new           (ManualsNavigatable      *navigatable);
GListModel              *manuals_navigatable_model_list_children (ManualsNavigatableModel *self);

G_END_DECLS
