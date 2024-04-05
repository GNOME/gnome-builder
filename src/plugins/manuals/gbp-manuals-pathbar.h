/*
 * gbp-manuals-pathbar.h
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

#include "manuals-navigatable.h"

G_BEGIN_DECLS

#define GBP_TYPE_MANUALS_PATHBAR (gbp_manuals_pathbar_get_type())

G_DECLARE_FINAL_TYPE (GbpManualsPathbar, gbp_manuals_pathbar, GBP, MANUALS_PATHBAR, GtkWidget)

GbpManualsPathbar  *gbp_manuals_pathbar_new              (void);
ManualsNavigatable *gbp_manuals_pathbar_get_navigatable  (GbpManualsPathbar  *self);
void                gbp_manuals_pathbar_set_navigatable  (GbpManualsPathbar  *self,
                                                          ManualsNavigatable *navigatable);
void                gbp_manuals_pathbar_inhibit_scroll   (GbpManualsPathbar  *self);
void                gbp_manuals_pathbar_uninhibit_scroll (GbpManualsPathbar  *self);

G_END_DECLS
