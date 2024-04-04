/*
 * manuals-search-result.h
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

#include <glib-object.h>

G_BEGIN_DECLS

#define MANUALS_TYPE_SEARCH_RESULT (manuals_search_result_get_type())

G_DECLARE_FINAL_TYPE (ManualsSearchResult, manuals_search_result, MANUALS, SEARCH_RESULT, GObject)

ManualsSearchResult *manuals_search_result_new          (guint                position);
guint                manuals_search_result_get_position (ManualsSearchResult *self);
gpointer             manuals_search_result_get_item     (ManualsSearchResult *self);
void                 manuals_search_result_set_item     (ManualsSearchResult *self,
                                                         gpointer             item);

G_END_DECLS
