/*
 * manuals-heading.h
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
#include "manuals-sdk.h"

G_BEGIN_DECLS

#define MANUALS_TYPE_HEADING (manuals_heading_get_type())

G_DECLARE_FINAL_TYPE (ManualsHeading, manuals_heading, MANUALS, HEADING, GomResource)

DexFuture  *manuals_heading_find_by_uri     (ManualsRepository *repository,
                                             const char        *uri);
gint64      manuals_heading_get_id          (ManualsHeading    *self);
void        manuals_heading_set_id          (ManualsHeading    *self,
                                             gint64             id);
gint64      manuals_heading_get_book_id     (ManualsHeading    *self);
void        manuals_heading_set_book_id     (ManualsHeading    *self,
                                             gint64             book_id);
gint64      manuals_heading_get_parent_id   (ManualsHeading    *self);
void        manuals_heading_set_parent_id   (ManualsHeading    *self,
                                             gint64             parent_id);
const char *manuals_heading_get_title       (ManualsHeading    *self);
void        manuals_heading_set_title       (ManualsHeading    *self,
                                             const char        *title);
const char *manuals_heading_get_uri         (ManualsHeading    *self);
void        manuals_heading_set_uri         (ManualsHeading    *self,
                                             const char        *uri);
DexFuture  *manuals_heading_find_parent     (ManualsHeading    *self);
DexFuture  *manuals_heading_find_sdk        (ManualsHeading    *self);
DexFuture  *manuals_heading_find_book       (ManualsHeading    *self);
DexFuture  *manuals_heading_list_headings   (ManualsHeading    *self);
DexFuture  *manuals_heading_list_alternates (ManualsHeading    *self);
DexFuture  *manuals_heading_has_children    (ManualsHeading    *self);

G_END_DECLS
