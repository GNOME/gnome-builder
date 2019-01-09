/* cpack-completion-results.h
 *
 * Copyright 2018-2019 Christian Hergert <chergert@redhat.com>
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

#include <libide-sourceview.h>

G_BEGIN_DECLS

#define CPACK_TYPE_COMPLETION_RESULTS (cpack_completion_results_get_type())

G_DECLARE_FINAL_TYPE (CpackCompletionResults, cpack_completion_results, CPACK, COMPLETION_RESULTS, GObject)

void     cpack_completion_results_refilter        (CpackCompletionResults  *self,
                                                   const gchar             *word);
void     cpack_completion_results_populate_async  (CpackCompletionResults  *self,
                                                   const gchar * const     *build_flags,
                                                   const gchar             *prefix,
                                                   GCancellable            *cancellable,
                                                   GAsyncReadyCallback      callback,
                                                   gpointer                 user_data);
gboolean cpack_completion_results_populate_finish (CpackCompletionResults  *self,
                                                   GAsyncResult            *result,
                                                   GError                 **error);

G_END_DECLS
