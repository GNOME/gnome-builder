/* dspy-name-view.h
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

#include "dspy-name.h"

G_BEGIN_DECLS

#define DSPY_TYPE_NAME_VIEW (dspy_name_view_get_type())

G_DECLARE_FINAL_TYPE (DspyNameView, dspy_name_view, DSPY, NAME_VIEW, GtkBin)

DspyNameView *dspy_name_view_new      (void);
void          dspy_name_view_set_name (DspyNameView    *self,
                                       GDBusConnection *connection,
                                       GBusType         bus_type,
                                       const gchar     *address,
                                       DspyName        *name);

G_END_DECLS
