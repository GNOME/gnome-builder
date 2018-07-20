/* ide-gi-version-private.h
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

#include <glib.h>

#include "ide-gi-macros.h"
#include "ide-gi-types.h"
#include "ide-gi-blob.h"

#include "ide-gi-index.h"
#include "ide-gi-index-private.h"
#include "ide-gi-require.h"
#include "ide-gi-version.h"

#include "radix-tree/ide-gi-flat-radix-tree.h"

G_BEGIN_DECLS

struct _IdeGiVersion
{
  GObject              parent_instance;

  IdeGiIndex          *index;
  GFile               *cache_dir;
  GMappedFile         *index_map;
  IdeGiFlatRadixTree  *index_dt;
  GHashTable          *ns_table;
  IdeGiRequire        *req_highest_versions;
  GMutex               ns_used_count_mutex;

  /* Those are pointers into the mapped index.tree file (index_map) */
  IndexHeader         *index_header;
  guint64             *index_namespaces;
  gchar               *index_name;
  gchar               *file_suffix;
  guint16              version_count;

  gint                 ns_used_count;
  guint                is_removing        : 1;
  guint                has_keep_alive_ref : 1;
};

 /* Fields suffixed 64b represent 64bits quantity */

typedef struct
{
  guint8             is_buildable : 1;
  guint8             is_new       : 1;
  guint8             has_ro_tree  : 1;

  IdeGiPrefixType    type;
  IdeGiBlobType      object_type;

  guint32            object_offset;
  guint32            namespace_size64b;
  IdeGiNamespaceId   id;
  guint64            mtime;
} DtPayload;

typedef struct
{
  GOnce           once;
  gboolean        has_ref;
} NsState;

G_STATIC_ASSERT (IS_64B_MULTIPLE (sizeof (DtPayload)));

#define DT_PAYLOAD_N64_SIZE (sizeof (DtPayload) >> 3)

const gchar              *_namespacechunk_get_c_identifier_prefixes    (NamespaceChunk    *chunk);
const gchar              *_namespacechunk_get_c_includes               (NamespaceChunk    *chunk);
const gchar              *_namespacechunk_get_c_symbol_prefixes        (NamespaceChunk    *chunk);
const gchar              *_namespacechunk_get_includes                 (NamespaceChunk    *chunk);
const gchar              *_namespacechunk_get_namespace                (NamespaceChunk    *chunk);
const gchar              *_namespacechunk_get_nsversion                (NamespaceChunk    *chunk);
const gchar              *_namespacechunk_get_packages                 (NamespaceChunk    *chunk);
const gchar              *_namespacechunk_get_shared_library           (NamespaceChunk    *chunk);

NamespaceChunk            _ide_gi_version_get_namespace_chunk_from_id  (IdeGiVersion      *self,
                                                                        IdeGiNamespaceId   id);
IdeGiIndex               *_ide_gi_version_get_index                    (IdeGiVersion      *self);
IdeGiFlatRadixTree       *_ide_gi_version_get_index_dt                 (IdeGiVersion      *self);
guint64                  *_ide_gi_version_get_index_namespaces         (IdeGiVersion      *self);

IdeGiNsIndexHeader       *_ide_gi_version_get_ns_header                (IdeGiVersion      *self,
                                                                        IdeGiNamespaceId   id);
void                      _ide_gi_version_release_ns_header            (IdeGiVersion      *self,
                                                                        IdeGiNamespaceId   id);
void                      _ide_gi_version_set_namespace_state          (IdeGiVersion      *self,
                                                                        IdeGiNamespace    *ns,
                                                                        gboolean           has_ref);

G_END_DECLS
