/* gbp-file-search-index.h
 *
 * Copyright 2015-2019 Christian Hergert <christian@hergert.me>
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

#include <libide-search.h>

G_BEGIN_DECLS

#define GBP_TYPE_FILE_SEARCH_INDEX (gbp_file_search_index_get_type())

G_DECLARE_FINAL_TYPE (GbpFileSearchIndex, gbp_file_search_index, GBP, FILE_SEARCH_INDEX, IdeObject)

GPtrArray *gbp_file_search_index_populate     (GbpFileSearchIndex    *self,
                                              const gchar          *query,
                                              gsize                 max_results);
void       gbp_file_search_index_build_async  (GbpFileSearchIndex    *self,
                                              GCancellable         *cancellable,
                                              GAsyncReadyCallback   callback,
                                              gpointer              user_data);
gboolean   gbp_file_search_index_build_finish (GbpFileSearchIndex    *self,
                                              GAsyncResult         *result,
                                              GError              **error);
gboolean   gbp_file_search_index_contains     (GbpFileSearchIndex    *self,
                                              const gchar          *relative_path);
void       gbp_file_search_index_insert       (GbpFileSearchIndex    *self,
                                              const gchar          *relative_path);
void       gbp_file_search_index_remove       (GbpFileSearchIndex    *self,
                                              const gchar          *relative_path);

G_END_DECLS
