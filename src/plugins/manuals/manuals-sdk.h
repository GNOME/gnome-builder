/*
 * manuals-sdk.h
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

#define MANUALS_TYPE_SDK (manuals_sdk_get_type())

G_DECLARE_FINAL_TYPE (ManualsSdk, manuals_sdk, MANUALS, SDK, GomResource)

gint64      manuals_sdk_get_id         (ManualsSdk *self);
void        manuals_sdk_set_id         (ManualsSdk *self,
                                        gint64      id);
const char *manuals_sdk_get_kind       (ManualsSdk *self);
void        manuals_sdk_set_kind       (ManualsSdk *self,
                                        const char *kind);
const char *manuals_sdk_get_name       (ManualsSdk *self);
void        manuals_sdk_set_name       (ManualsSdk *self,
                                        const char *name);
const char *manuals_sdk_get_version    (ManualsSdk *self);
void        manuals_sdk_set_version    (ManualsSdk *self,
                                        const char *version);
const char *manuals_sdk_get_online_uri (ManualsSdk *self);
void        manuals_sdk_set_online_uri (ManualsSdk *self,
                                        const char *online_uri);
char       *manuals_sdk_dup_title      (ManualsSdk *self);
const char *manuals_sdk_get_uri        (ManualsSdk *self);
void        manuals_sdk_set_uri        (ManualsSdk *self,
                                        const char *uri);
const char *manuals_sdk_get_icon_name  (ManualsSdk *self);
void        manuals_sdk_set_icon_name  (ManualsSdk *self,
                                        const char *icon_name);
DexFuture  *manuals_sdk_list_books     (ManualsSdk *self);

G_END_DECLS
