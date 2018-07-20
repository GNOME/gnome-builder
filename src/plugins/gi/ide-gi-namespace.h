/* ide-gi-namespace.h
 *
 * Copyright © 2017 Sebastien Lafargue <slafargue@gnome.org>
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
#include <glib-object.h>

#include "ide-gi.h"
#include "ide-gi-macros.h"
#include "ide-gi-types.h"
#include "ide-gi-blob.h"

#include "ide-gi-index.h"
#include "ide-gi-require.h"
#include "ide-gi-version.h"

G_BEGIN_DECLS

#define NS_INDEX_HEADER_MAGIC 0x584449534E494749 /* IGINSIDX (Ide GI namespace index) on little-endian arch */

union _IdeGiNamespaceId
{
  struct
  {
    /* Some .gir have a namespace with no minor version, we need to record this information */
    guint16 no_minor_version : 1;
    guint16 file_version     : 15;
    guint8  major_version;
    guint8  minor_version;
    guint32 offset64b;
  };

  guint64 value;
};

G_STATIC_ASSERT (sizeof (IdeGiNamespaceId) == sizeof (guint64));

typedef struct
{
  guint64 type : 6;
  guint64 offset : 32;
} RoTreePayload;

G_STATIC_ASSERT (IS_64B_MULTIPLE (sizeof (RoTreePayload)));

#define RO_TREE_PAYLOAD_N64_SIZE (sizeof (RoTreePayload) >> 3)

/* sizes and offsets represent 64bits quantity.
 * offsets are relative to the start of the NamespaceHeader.
 */
typedef struct {
  guint32 size64b;
  guint32 pad;
  guint32 ro_tree_offset64b;
  guint32 ro_tree_size64b;
} IdeGiNamespaceHeader;

/* NamespaceHeader need to be a multiple of 64 bits */
G_STATIC_ASSERT (IS_64B_MULTIPLE (sizeof (IdeGiNamespaceHeader)));

typedef struct
{
  guint64 magic;
  gint32  elements_tables [IDE_GI_NS_TABLE_NB_TABLES];
  guint32 strings;
  guint32 doc_strings;
  guint32 annotation_strings;
  gint32  crossrefs;
} IdeGiNsIndexHeader;

G_STATIC_ASSERT (IS_32B_MULTIPLE (sizeof (IdeGiNsIndexHeader)));

struct _IdeGiNamespace
{
  IdeGiVersion         *version;
  IdeGiRequire         *req;
  IdeGiNamespaceHeader *head_header;
  IdeGiNsIndexHeader   *tail_header;
  IdeGiNamespaceId      id;
  NamespaceChunk        chunk;

  gchar                *c_includes;
  gchar                *includes;
  gchar                *packages;
  gchar                *shared_library;
  gchar                *ns;
  gchar                *ns_version;
  gchar                *c_identifiers_prefixes;
  gchar                *c_symbol_prefixes;

  guint8                major_version;
  guint8                minor_version;

  volatile gint         ns_count;
  GMutex                mutex;

  guint                 is_valid : 1;
};

void                ide_gi_namespace_dump                         (IdeGiNamespace    *self);
const gchar        *ide_gi_namespace_get_string                   (IdeGiNamespace    *self,
                                                                   guint32            offset);
const gchar        *ide_gi_namespace_get_doc_string               (IdeGiNamespace    *self,
                                                                   guint32            offset);
const gchar        *ide_gi_namespace_get_annnotation_string       (IdeGiNamespace    *self,
                                                                   guint32            offset);
IdeGiVersion       *ide_gi_namespace_get_repository_version       (IdeGiNamespace    *self);
IdeGiBase          *ide_gi_namespace_get_object                   (IdeGiNamespace    *self,
                                                                   IdeGiBlobType      type,
                                                                   guint16            offset);
gboolean            ide_gi_namespace_is_valid                     (IdeGiNamespace    *self);
IdeGiNamespace     *ide_gi_namespace_ref                          (IdeGiNamespace    *self);
void                ide_gi_namespace_unref                        (IdeGiNamespace    *self);

const gchar        *ide_gi_namespace_get_c_includes               (IdeGiNamespace    *self);
const gchar        *ide_gi_namespace_get_includes                 (IdeGiNamespace    *self);
const gchar        *ide_gi_namespace_get_packages                 (IdeGiNamespace    *self);
const gchar        *ide_gi_namespace_get_shared_library           (IdeGiNamespace    *self);
const gchar        *ide_gi_namespace_get_name                     (IdeGiNamespace    *self);
const gchar        *ide_gi_namespace_get_version                  (IdeGiNamespace    *self);
const gchar        *ide_gi_namespace_get_c_identifiers_prefixes   (IdeGiNamespace    *self);
const gchar        *ide_gi_namespace_get_c_symbol_prefixes        (IdeGiNamespace    *self);

guint8              ide_gi_namespace_get_major_version            (IdeGiNamespace    *self);
guint8              ide_gi_namespace_get_minor_version            (IdeGiNamespace    *self);
IdeGiRequire       *ide_gi_namespace_get_require                  (IdeGiNamespace    *self);

G_DEFINE_AUTOPTR_CLEANUP_FUNC (IdeGiNamespace, ide_gi_namespace_unref)

G_END_DECLS
