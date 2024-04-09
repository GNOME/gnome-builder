/*
 * manuals-tag.h
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

#include <gtk/gtk.h>

G_BEGIN_DECLS

#define MANUALS_TYPE_TAG (manuals_tag_get_type())

G_DECLARE_FINAL_TYPE (ManualsTag, manuals_tag, MANUALS, TAG, GtkWidget)

const char *manuals_tag_get_key   (ManualsTag *self);
void        manuals_tag_set_key   (ManualsTag *self,
                                   const char *key);
const char *manuals_tag_get_value (ManualsTag *self);
void        manuals_tag_set_value (ManualsTag *self,
                                   const char *value);

G_END_DECLS
