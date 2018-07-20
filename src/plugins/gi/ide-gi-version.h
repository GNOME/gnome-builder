/* ide-gi-version.h
 *
 * Copyright 2018 Sebastien Lafargue <slafargue@gnome.org>
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

#include "ide-gi.h"
#include "ide-gi-macros.h"

#include "ide-gi-complete.h"
#include "ide-gi-require.h"

#include "radix-tree/ide-gi-flat-radix-tree.h"

G_BEGIN_DECLS

/* Fields suffixed 64b represent 64bits quantity.
 * Offsets are relative to the start of the IndexHeader.
 */

typedef struct
{
  guint16 abi_version;
  guint16 n_basic_types;
  guint32 id_offset64b;
  guint32 dt_offset64b;
  guint32 dt_size64b;
  guint32 namespaces_offset64b;
  guint32 namespaces_size64b;
  guint32 basic_types_offset64b;
  guint32 strings_offset64b;
  guint32 strings_size;
  guint32 res;
} IndexHeader;

G_STATIC_ASSERT (IS_64B_MULTIPLE (sizeof (IndexHeader)));

typedef enum
{
  IDE_GI_VERSION_ERROR_UNKNOWN         = 0,
  IDE_GI_VERSION_ERROR_WRONG_ABI       = 1,
  IDE_GI_VERSION_ERROR_INDEX_NOT_FOUND = 2,
} IdeGiVersionError;

#define IDE_GI_VERSION_ERROR (ide_gi_version_error_quark())

GQuark                    ide_gi_version_error_quark                 (void) G_GNUC_CONST;

GArray                   *ide_gi_version_complete_gtype              (IdeGiVersion            *self,
                                                                      IdeGiRequire            *req,
                                                                      IdeGiCompleteRootFlags   flags,
                                                                      gboolean                 case_sensitive,
                                                                      const gchar             *word);
GArray                   *ide_gi_version_complete_root_objects       (IdeGiVersion            *self,
                                                                      IdeGiRequire            *req,
                                                                      IdeGiNamespace          *ns,
                                                                      IdeGiCompleteRootFlags   flags,
                                                                      gboolean                 case_sensitive,
                                                                      const gchar             *word);
GArray                   *ide_gi_version_complete_prefix             (IdeGiVersion            *self,
                                                                      IdeGiRequire            *req,
                                                                      IdeGiPrefixType          flags,
                                                                      gboolean                 get_prefix,
                                                                      gboolean                 case_sensitive,
                                                                      const gchar             *word);
guint                     ide_gi_version_get_count                   (IdeGiVersion            *self);
GPtrArray                *ide_gi_version_get_namespaces_basenames    (IdeGiVersion            *self);
gchar                    *ide_gi_version_get_versionned_filename     (IdeGiVersion            *self,
                                                                      const gchar             *name);
gchar                    *ide_gi_version_get_versionned_index_name   (IdeGiVersion            *self);
IdeGiBase                *ide_gi_version_lookup_gtype                (IdeGiVersion            *self,
                                                                      IdeGiRequire            *req,
                                                                      const gchar             *name);
IdeGiBase                *ide_gi_version_lookup_gtype_in_ns          (IdeGiVersion            *self,
                                                                      IdeGiNamespace          *ns,
                                                                      const gchar             *name);
IdeGiNamespace           *ide_gi_version_lookup_namespace            (IdeGiVersion            *self,
                                                                      const gchar             *name,
                                                                      guint16                  ns_major_version,
                                                                      guint16                  ns_minor_version);
GPtrArray                *ide_gi_version_lookup_namespaces           (IdeGiVersion            *self,
                                                                      const gchar             *name,
                                                                      IdeGiRequire            *req);
IdeGiBase                *ide_gi_version_lookup_root_object          (IdeGiVersion            *self,
                                                                      const gchar             *qname,
                                                                      guint16                  ns_major_version,
                                                                      guint16                  ns_minor_version);
IdeGiRequire             *ide_gi_version_get_highest_versions        (IdeGiVersion            *self);
IdeGiVersion             *ide_gi_version_new                         (IdeGiIndex              *index,
                                                                      GFile                   *cache_dir,
                                                                      guint                    count,
                                                                      GCancellable            *cancellable,
                                                                      GError                 **error);
void                      ide_gi_version_new_async                   (IdeGiIndex              *index,
                                                                      GFile                   *cache_dir,
                                                                      guint                    count,
                                                                      GCancellable            *cancellable,
                                                                      GAsyncReadyCallback      callback,
                                                                      gpointer                 user_data);
IdeGiVersion             *ide_gi_version_new_finish                  (GAsyncInitable          *initable,
                                                                      GAsyncResult            *result,
                                                                      GError                 **error);

G_END_DECLS
