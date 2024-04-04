/*
 * manuals-book.h
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

G_BEGIN_DECLS

#define MANUALS_TYPE_BOOK (manuals_book_get_type())

G_DECLARE_FINAL_TYPE (ManualsBook, manuals_book, MANUALS, BOOK, GomResource)

gint64      manuals_book_get_id          (ManualsBook *self);
void        manuals_book_set_id          (ManualsBook *self,
                                          gint64       id);
gint64      manuals_book_get_sdk_id      (ManualsBook *self);
void        manuals_book_set_sdk_id      (ManualsBook *self,
                                          gint64       sdk_id);
const char *manuals_book_get_etag        (ManualsBook *self);
void        manuals_book_set_etag        (ManualsBook *self,
                                          const char  *etag);
const char *manuals_book_get_title       (ManualsBook *self);
void        manuals_book_set_title       (ManualsBook *self,
                                          const char  *title);
const char *manuals_book_get_uri         (ManualsBook *self);
void        manuals_book_set_uri         (ManualsBook *self,
                                          const char  *uri);
const char *manuals_book_get_default_uri (ManualsBook *self);
void        manuals_book_set_default_uri (ManualsBook *self,
                                          const char  *default_uri);
const char *manuals_book_get_online_uri  (ManualsBook *self);
void        manuals_book_set_online_uri  (ManualsBook *self,
                                          const char  *online_uri);
const char *manuals_book_get_language    (ManualsBook *self);
void        manuals_book_set_language    (ManualsBook *self,
                                          const char  *language);
DexFuture  *manuals_book_list_headings   (ManualsBook *self);
DexFuture  *manuals_book_list_alternates (ManualsBook *self);
DexFuture  *manuals_book_find_sdk        (ManualsBook *self);

G_END_DECLS
