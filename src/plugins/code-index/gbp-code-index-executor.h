/* gbp-code-index-executor.h
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

#define GBP_TYPE_CODE_INDEX_EXECUTOR (gbp_code_index_executor_get_type())

G_DECLARE_FINAL_TYPE (GbpCodeIndexExecutor, gbp_code_index_executor, GBP, CODE_INDEX_EXECUTOR, IdeObject)

GbpCodeIndexExecutor *gbp_code_index_executor_new            (GbpCodeIndexPlan      *plan);
void                  gbp_code_index_executor_execute_async  (GbpCodeIndexExecutor  *self,
                                                              IdeNotification       *notif,
                                                              GCancellable          *cancellable,
                                                              GAsyncReadyCallback    callback,
                                                              gpointer               user_data);
gboolean              gbp_code_index_executor_execute_finish (GbpCodeIndexExecutor  *self,
                                                              GAsyncResult          *result,
                                                              GError               **error);

G_END_DECLS
