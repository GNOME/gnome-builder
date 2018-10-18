/* gbp-glade-view.h
 *
 * Copyright 2018 Christian Hergert <chergert@redhat.com>
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

#include <gladeui/glade.h>
#include <ide.h>

G_BEGIN_DECLS

#define GBP_TYPE_GLADE_VIEW (gbp_glade_view_get_type())

G_DECLARE_FINAL_TYPE (GbpGladeView, gbp_glade_view, GBP, GLADE_VIEW, IdeLayoutView)

GbpGladeView *gbp_glade_view_new              (void);
void          gbp_glade_view_load_file_async  (GbpGladeView         *self,
                                               GFile                *file,
                                               GCancellable         *cancellable,
                                               GAsyncReadyCallback   callback,
                                               gpointer              user_data);
gboolean      gbp_glade_view_load_file_finish (GbpGladeView         *self,
                                               GAsyncResult         *result,
                                               GError              **error);
GladeProject *gbp_glade_view_get_project      (GbpGladeView         *self);

G_END_DECLS
