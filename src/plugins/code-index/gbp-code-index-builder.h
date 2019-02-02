/* gbp-code-index-builder.h
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

#include <libide-core.h>

#include "gbp-code-index-plan.h"

G_BEGIN_DECLS

#define GBP_TYPE_CODE_INDEX_BUILDER (gbp_code_index_builder_get_type())

G_DECLARE_FINAL_TYPE (GbpCodeIndexBuilder, gbp_code_index_builder, GBP, CODE_INDEX_BUILDER, IdeObject)

GbpCodeIndexBuilder *gbp_code_index_builder_new         (GFile                       *source_dir,
                                                         GFile                       *index_dir);
void                 gbp_code_index_builder_add_item    (GbpCodeIndexBuilder         *self,
                                                         const GbpCodeIndexPlanItem  *item);
void                 gbp_code_index_builder_run_async   (GbpCodeIndexBuilder         *self,
                                                         GCancellable                *cancellable,
                                                         GAsyncReadyCallback          callback,
                                                         gpointer                     user_data);
gboolean             gbp_code_index_builder_run_finish  (GbpCodeIndexBuilder         *self,
                                                         GAsyncResult                *result,
                                                         GError                     **error);


G_END_DECLS
