/* dspy-connection-row.h
 *
 * Copyright 2019 Christian Hergert <chergert@redhat.com>
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

#define DSPY_TYPE_CONNECTION_ROW (dspy_connection_row_get_type())

G_DECLARE_FINAL_TYPE (DspyConnectionRow, dspy_connection_row, DSPY, CONNECTION_ROW, GtkListBoxRow)

DspyConnectionRow *dspy_connection_row_new          (void);
const gchar       *dspy_connection_row_get_address  (DspyConnectionRow *self);
void               dspy_connection_row_set_address  (DspyConnectionRow *self,
                                                     const gchar       *address);
GBusType           dspy_connection_row_get_bus_type (DspyConnectionRow *self);
void               dspy_connection_row_set_bus_type (DspyConnectionRow *self,
                                                     GBusType           bus_type);
void               dspy_connection_row_set_title    (DspyConnectionRow *self,
                                                     const gchar       *title);

G_END_DECLS
