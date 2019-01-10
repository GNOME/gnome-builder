/* gbp-grep-model.h
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

#include <libide-core.h>

G_BEGIN_DECLS

#define GBP_TYPE_GREP_MODEL (gbp_grep_model_get_type())

typedef struct
{
  gint match_begin;
  gint match_end;
  gint match_begin_bytes;
  gint match_end_bytes;
} GbpGrepModelMatch;

typedef struct
{
  const gchar *start_of_line;
  const gchar *start_of_message;
  gchar       *path;
  GArray      *matches;
  guint        line;
} GbpGrepModelLine;

G_DECLARE_FINAL_TYPE (GbpGrepModel, gbp_grep_model, GBP, GREP_MODEL, IdeObject)

GbpGrepModel *gbp_grep_model_new                    (IdeContext              *context);
GFile        *gbp_grep_model_get_directory          (GbpGrepModel            *self);
void          gbp_grep_model_set_directory          (GbpGrepModel            *self,
                                                     GFile                   *directory);
gboolean      gbp_grep_model_get_use_regex          (GbpGrepModel            *self);
void          gbp_grep_model_set_use_regex          (GbpGrepModel            *self,
                                                     gboolean                 use_regex);
gboolean      gbp_grep_model_get_recursive          (GbpGrepModel            *self);
void          gbp_grep_model_set_recursive          (GbpGrepModel            *self,
                                                     gboolean                 recursive);
gboolean      gbp_grep_model_get_case_sensitive     (GbpGrepModel            *self);
void          gbp_grep_model_set_case_sensitive     (GbpGrepModel            *self,
                                                     gboolean                 case_sensitive);
gboolean      gbp_grep_model_get_at_word_boundaries (GbpGrepModel            *self);
void          gbp_grep_model_set_at_word_boundaries (GbpGrepModel            *self,
                                                     gboolean                 at_word_boundaries);
const gchar  *gbp_grep_model_get_query              (GbpGrepModel            *self);
void          gbp_grep_model_set_query              (GbpGrepModel            *self,
                                                     const gchar             *query);
GPtrArray    *gbp_grep_model_create_edits           (GbpGrepModel            *self);
void          gbp_grep_model_select_all             (GbpGrepModel            *self);
void          gbp_grep_model_select_none            (GbpGrepModel            *self);
void          gbp_grep_model_toggle_mode            (GbpGrepModel            *self);
void          gbp_grep_model_toggle_row             (GbpGrepModel            *self,
                                                     GtkTreeIter             *iter);
void          gbp_grep_model_get_line               (GbpGrepModel            *self,
                                                     GtkTreeIter             *iter,
                                                     const GbpGrepModelLine **line);
GFile        *gbp_grep_model_get_file               (GbpGrepModel            *self,
                                                     const gchar             *path);
void          gbp_grep_model_scan_async             (GbpGrepModel            *self,
                                                     GCancellable            *cancellable,
                                                     GAsyncReadyCallback      callback,
                                                     gpointer                 user_data);
gboolean      gbp_grep_model_scan_finish            (GbpGrepModel            *self,
                                                     GAsyncResult            *result,
                                                     GError                 **error);

G_END_DECLS
