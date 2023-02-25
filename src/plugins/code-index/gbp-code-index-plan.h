/* gbp-code-index-plan.h
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

#include <glib-object.h>

G_BEGIN_DECLS

#define GBP_TYPE_CODE_INDEX_PLAN (gbp_code_index_plan_get_type())

G_DECLARE_FINAL_TYPE (GbpCodeIndexPlan, gbp_code_index_plan, GBP, CODE_INDEX_PLAN, GObject)

typedef struct
{
  GFileInfo    *file_info;
  gchar       **build_flags;
  const gchar  *indexer_module_name;
} GbpCodeIndexPlanItem;

typedef enum
{
  GBP_CODE_INDEX_REASON_INITIAL = 1,
  GBP_CODE_INDEX_REASON_EXPIRED,
  GBP_CODE_INDEX_REASON_CHANGES,
  GBP_CODE_INDEX_REASON_REMOVE_INDEX,
} GbpCodeIndexReason;

/**
 * GbpCodeIndexPlanForeach:
 * @directory: the directory containing the items
 * @plan_items: (element-type GbpCodeIndexPlanItem): array of #GbpCodeIndexPlanItem
 * @reason: the reason for the (re)indexing
 *
 * Returns: %TRUE to remove item from plan
 */
typedef gboolean (*GbpCodeIndexPlanForeach) (GFile              *directory,
                                             GPtrArray          *plan_items,
                                             GbpCodeIndexReason  reason,
                                             gpointer            user_data);

GbpCodeIndexPlan     *gbp_code_index_plan_new                 (void);
void                  gbp_code_index_plan_populate_async      (GbpCodeIndexPlan            *self,
                                                               IdeContext                  *context,
                                                               GCancellable                *cancellable,
                                                               GAsyncReadyCallback          callback,
                                                               gpointer                     user_data);
gboolean              gbp_code_index_plan_populate_finish     (GbpCodeIndexPlan            *self,
                                                               GAsyncResult                *result,
                                                               GError                     **error);
void                  gbp_code_index_plan_cull_indexed_async  (GbpCodeIndexPlan            *self,
                                                               IdeContext                  *context,
                                                               GCancellable                *cancellable,
                                                               GAsyncReadyCallback          callback,
                                                               gpointer                     user_data);
gboolean              gbp_code_index_plan_cull_indexed_finish (GbpCodeIndexPlan            *self,
                                                               GAsyncResult                *result,
                                                               GError                     **error);
void                  gbp_code_index_plan_load_flags_async    (GbpCodeIndexPlan            *self,
                                                               IdeContext                  *context,
                                                               GCancellable                *cancellable,
                                                               GAsyncReadyCallback          callback,
                                                               gpointer                     user_data);
gboolean              gbp_code_index_plan_load_flags_finish   (GbpCodeIndexPlan            *self,
                                                               GAsyncResult                *result,
                                                               GError                     **error);
void                  gbp_code_index_plan_foreach             (GbpCodeIndexPlan            *self,
                                                               GbpCodeIndexPlanForeach      foreach_func,
                                                               gpointer                     foreach_data);
GbpCodeIndexPlanItem *gbp_code_index_plan_item_copy           (const GbpCodeIndexPlanItem  *item);
void                  gbp_code_index_plan_item_unref          (GbpCodeIndexPlanItem        *item);

G_END_DECLS
