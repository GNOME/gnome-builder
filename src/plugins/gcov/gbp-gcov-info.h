/*
 * gbp-gcov-info.h
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

#include <gio/gio.h>

G_BEGIN_DECLS

#define GBP_TYPE_GCOV_INFO (gbp_gcov_info_get_type())

G_DECLARE_FINAL_TYPE (GbpGcovInfo, gbp_gcov_info, GBP, GCOV_INFO, GObject)

typedef struct _GbpGcovLineInfo
{
  const char *function_name;
  guint line_number;
  guint count;
  guint unexecuted_block : 1;
} GbpGcovLineInfo;

GbpGcovInfo           *gbp_gcov_info_new              (void);
void                   gbp_gcov_info_load_file_async  (GbpGcovInfo          *self,
                                                       GFile                *file,
                                                       GCancellable         *cancellable,
                                                       GAsyncReadyCallback   callback,
                                                       gpointer              user_data);
gboolean               gbp_gcov_info_load_file_finish (GbpGcovInfo          *self,
                                                       GAsyncResult         *result,
                                                       GError              **error);
const GbpGcovLineInfo *gbp_gcov_info_get_line         (GbpGcovInfo          *self,
                                                       const char           *filename,
                                                       guint                 line);
void                   gbp_gcov_info_foreach_in_range (GbpGcovInfo          *self,
                                                       const char           *filename,
                                                       guint                 begin_line,
                                                       guint                 end_line,
                                                       GFunc                 foreach_func,
                                                       gpointer              user_data);

G_END_DECLS
