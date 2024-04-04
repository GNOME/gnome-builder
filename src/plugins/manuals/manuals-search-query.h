/*
 * manuals-search-query.h
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

#include <libdex.h>

#include "manuals-repository.h"

G_BEGIN_DECLS

#define MANUALS_TYPE_SEARCH_QUERY (manuals_search_query_get_type())

G_DECLARE_FINAL_TYPE (ManualsSearchQuery, manuals_search_query, MANUALS, SEARCH_QUERY, GObject)

ManualsSearchQuery *manuals_search_query_new      (void);
const char         *manuals_search_query_get_text (ManualsSearchQuery *self);
void                manuals_search_query_set_text (ManualsSearchQuery *self,
                                                   const char         *text);
DexFuture          *manuals_search_query_execute  (ManualsSearchQuery *query,
                                                   ManualsRepository  *repository);

G_END_DECLS
