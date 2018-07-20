/* ide-gi-utils.h
 *
 * Copyright (C) 2017 Sebastien Lafargue <slafargue@gnome.org>
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
 */

#pragma once

#include <glib.h>
#include <gio/gio.h>
#include <ide.h>

#include "ide-gi.h"
#include "ide-gi-types.h"

#include "ide-gi-blob.h"
#include "ide-gi-namespace.h"

G_BEGIN_DECLS

#if defined(_MSC_VER)
# define IDE_GI_ALIGNED_BEGIN(_N) __declspec(align(_N))
# define IDE_GI_ALIGNED_END(_N)
#else
# define IDE_GI_ALIGNED_BEGIN(_N)
# define IDE_GI_ALIGNED_END(_N) __attribute__((aligned(_N)))
#endif

extern const gchar * IDE_GI_SIGNAL_WHEN_NAMES [4];
extern const gchar * IDE_GI_TRANSFER_OWNERSHIP_NAMES [4];
extern const gchar * IDE_GI_DIRECTION_NAMES [3];
extern const gchar * IDE_GI_SCOPE_NAMES [3];
extern const gchar * IDE_GI_STABILITY_NAMES [3];

GPtrArray     *ide_gi_utils_get_files_from_directories                        (GPtrArray               *directories,
                                                                               const gchar             *suffix,
                                                                               gboolean                 recursif);
void           ide_gi_utils_get_files_from_directories_async                  (GPtrArray               *directories,
                                                                               const gchar             *suffix,
                                                                               gboolean                 recursif,
                                                                               GCancellable            *cancellable,
                                                                               GAsyncReadyCallback      callback,
                                                                               gpointer                 user_data);
GPtrArray     *ide_gi_utils_get_files_from_directories_finish                 (GAsyncResult            *result,
                                                                               GError                 **error);
GPtrArray     *ide_gi_utils_get_files_from_directory                          (GFile                   *directory,
                                                                               const gchar             *suffix,
                                                                               gboolean                 recursif);
void           ide_gi_utils_get_files_from_directory_async                    (GFile                   *directory,
                                                                               const gchar             *suffix,
                                                                               gboolean                 recursif,
                                                                               GCancellable            *cancellable,
                                                                               GAsyncReadyCallback      callback,
                                                                               gpointer                 user_data);
GPtrArray     *ide_gi_utils_get_files_from_directory_finish                   (GAsyncResult            *result,
                                                                               GError                 **error);
gboolean       ide_gi_utils_files_list_dedup                                  (GPtrArray               *files_list);
GPtrArray     *ide_gi_utils_files_list_difference                             (GPtrArray               *a,
                                                                               GPtrArray               *b);
gboolean       ide_gi_utils_get_gir_components                                (GFile                   *file,
                                                                               gchar                  **name,
                                                                               gchar                  **version);
gboolean       ide_gi_utils_parse_version                                     (const gchar             *version,
                                                                               guint16                 *major,
                                                                               guint16                 *minor,
                                                                               guint16                 *micro);
void           ide_gi_utils_remove_basenames_async                            (GFile                   *base_dir,
                                                                               GPtrArray               *basenames,
                                                                               GCancellable            *cancellable,
                                                                               GAsyncReadyCallback      callback,
                                                                               gpointer                 user_data);
gboolean       ide_gi_utils_remove_basenames_finish                           (GAsyncResult            *result,
                                                                               GError                 **error);
void           ide_gi_utils_remove_files_list                                 (GPtrArray               *files_list);

const gchar   *ide_gi_utils_blob_type_to_string                               (IdeGiBlobType            type);
const gchar   *ide_gi_utils_direction_to_string                               (IdeGiDirection           direction);
const gchar   *ide_gi_utils_prefix_type_to_string                             (IdeGiPrefixType          type);
const gchar   *ide_gi_utils_scope_to_string                                   (IdeGiScope               scope);
const gchar   *ide_gi_utils_signal_when_to_string                             (IdeGiSignalWhen          signal_when);
const gchar   *ide_gi_utils_stability_to_string                               (IdeGiStability           stability);
const gchar   *ide_gi_utils_transfer_ownership_to_string                      (IdeGiTransferOwnership   transfer_ownership);

const gchar   *ide_gi_utils_ns_table_to_string                                (IdeGiNsTable             table);
const gchar   *ide_gi_utils_type_to_string                                    (IdeGiBasicType           type);

void           ide_gi_utils_typeref_dump                                      (IdeGiTypeRef             typeref,
                                                                               guint                    depth);

G_END_DECLS
