/* gbp-meson-introspection.h
 *
 * Copyright 2022 Christian Hergert <chergert@redhat.com>
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

#include <libide-foundry.h>

G_BEGIN_DECLS

#define GBP_TYPE_MESON_INTROSPECTION (gbp_meson_introspection_get_type())

G_DECLARE_FINAL_TYPE (GbpMesonIntrospection, gbp_meson_introspection, GBP, MESON_INTROSPECTION, IdePipelineStage)

GbpMesonIntrospection *gbp_meson_introspection_new                      (IdePipeline            *pipeline);
void                   gbp_meson_introspection_list_run_commands_async  (GbpMesonIntrospection  *self,
                                                                         GCancellable           *cancelalble,
                                                                         GAsyncReadyCallback     callback,
                                                                         gpointer                user_data);
GListModel            *gbp_meson_introspection_list_run_commands_finish (GbpMesonIntrospection  *self,
                                                                         GAsyncResult           *result,
                                                                         GError                **error);

G_END_DECLS
