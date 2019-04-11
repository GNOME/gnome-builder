/* dspy-name.h
 *
 * Copyright 2019 Christian Hergert <chergert@redhat.com>
 *
 * This file is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as
 * published by the Free Software Foundation; either version 3 of the
 * License, or (at your option) any later version.
 *
 * This file is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#pragma once

#include <gio/gio.h>
#include <gtk/gtk.h>

#include "dspy-connection.h"

G_BEGIN_DECLS

#define DSPY_TYPE_NAME (dspy_name_get_type())

G_DECLARE_FINAL_TYPE (DspyName, dspy_name, DSPY, NAME, GObject)

DspyName       *dspy_name_new               (DspyConnection       *connection,
                                             const gchar          *name,
                                             gboolean              activatable);
DspyConnection *dspy_name_get_connection    (DspyName             *self);
gboolean        dspy_name_get_activatable   (DspyName             *self);
GPid            dspy_name_get_pid           (DspyName             *self);
const gchar    *dspy_name_get_name          (DspyName             *self);
const gchar    *dspy_name_get_owner         (DspyName             *self);
const gchar    *dspy_name_get_search_text   (DspyName             *self);
gint            dspy_name_compare           (gconstpointer         a,
                                             gconstpointer         b);
void            dspy_name_introspect_async  (DspyName             *self,
                                             GCancellable         *cancellable,
                                             GAsyncReadyCallback   callback,
                                             gpointer              user_data);
GtkTreeModel   *dspy_name_introspect_finish (DspyName             *self,
                                             GAsyncResult         *result,
                                             GError              **error);

G_END_DECLS
