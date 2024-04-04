/* manuals-keyword.h
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

#include <gom/gom.h>
#include <libdex.h>

#include "manuals-repository.h"

G_BEGIN_DECLS

#define MANUALS_TYPE_KEYWORD (manuals_keyword_get_type())

G_DECLARE_FINAL_TYPE (ManualsKeyword, manuals_keyword, MANUALS, KEYWORD, GomResource)

DexFuture  *manuals_keyword_find_by_uri     (ManualsRepository *repository,
                                             const char        *uri);
DexFuture  *manuals_keyword_find_book       (ManualsKeyword    *self);
gint64      manuals_keyword_get_id          (ManualsKeyword    *self);
void        manuals_keyword_set_id          (ManualsKeyword    *self,
                                             gint64             id);
gint64      manuals_keyword_get_book_id     (ManualsKeyword    *self);
void        manuals_keyword_set_book_id     (ManualsKeyword    *self,
                                             gint64             book_id);
const char *manuals_keyword_get_kind        (ManualsKeyword    *self);
void        manuals_keyword_set_kind        (ManualsKeyword    *self,
                                             const char        *kind);
const char *manuals_keyword_get_since       (ManualsKeyword    *self);
void        manuals_keyword_set_since       (ManualsKeyword    *self,
                                             const char        *since);
const char *manuals_keyword_get_stability   (ManualsKeyword    *self);
void        manuals_keyword_set_stability   (ManualsKeyword    *self,
                                             const char        *stability);
const char *manuals_keyword_get_deprecated  (ManualsKeyword    *self);
void        manuals_keyword_set_deprecated  (ManualsKeyword    *self,
                                             const char        *deprecated);
const char *manuals_keyword_get_name        (ManualsKeyword    *self);
void        manuals_keyword_set_name        (ManualsKeyword    *self,
                                             const char        *name);
const char *manuals_keyword_get_uri         (ManualsKeyword    *self);
void        manuals_keyword_set_uri         (ManualsKeyword    *self,
                                             const char        *uri);
DexFuture  *manuals_keyword_list_alternates (ManualsKeyword    *self);

G_END_DECLS
