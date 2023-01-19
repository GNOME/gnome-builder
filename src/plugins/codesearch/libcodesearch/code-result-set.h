/*
 * code-result-set.h
 *
 * Copyright 2023 Christian Hergert <chergert@redhat.com>
 *
 * This file is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or (at
 * your option) any later version.
 *
 * This file is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE. See the GNU Lesser General Public
 * License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#pragma once

#include <libdex.h>

#include "code-index.h"
#include "code-query.h"

G_BEGIN_DECLS

#define CODE_TYPE_RESULT_SET (code_result_set_get_type())

G_DECLARE_FINAL_TYPE (CodeResultSet, code_result_set, CODE, RESULT_SET, GObject)

CodeResultSet *code_result_set_new             (CodeQuery            *query,
                                                CodeIndex * const    *indexes,
                                                guint                 n_indexes);
void           code_result_set_cancel          (CodeResultSet        *self);
DexFuture     *code_result_set_populate        (CodeResultSet        *self,
                                                DexScheduler         *scheduler);
void           code_result_set_populate_async  (CodeResultSet        *self,
                                                DexScheduler         *scheduler,
                                                GCancellable         *cancellable,
                                                GAsyncReadyCallback   callback,
                                                gpointer              user_data);
gboolean       code_result_set_populate_finish (CodeResultSet        *self,
                                                GAsyncResult         *result,
                                                GError              **error);

G_END_DECLS
