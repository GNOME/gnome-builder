/* ide-gi-version-private.c
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

#define G_LOG_DOMAIN "ide-gi-version"

#include <ide.h>

#include "ide-gi-namespace.h"
#include "ide-gi-namespace-private.h"
#include "ide-gi-version.h"

#include "ide-gi-version-private.h"

/* This method is used under a lock from ide_gi_namespace_ref/unref */
void
_ide_gi_version_set_namespace_state (IdeGiVersion   *self,
                                     IdeGiNamespace *ns,
                                     gboolean        has_ref)
{
  IdeGiNamespaceId id;
  NsState *state;
  gint ref_delta;
  gint old_ns_used_count G_GNUC_UNUSED;
  gboolean old_has_ref G_GNUC_UNUSED;

  g_assert (IDE_IS_GI_VERSION (self));
  g_assert (ns != NULL);

  id = _ide_gi_namespace_get_id (ns);
  /* The ns_table is created with the version, after that we only read it */
  state = g_hash_table_lookup (self->ns_table, &id);
  g_assert (state != NULL);

  /* Protect ns_used_count and has_keep_alive_ref */
  g_mutex_lock (&self->ns_used_count_mutex);
  /* By design, has_ref is always inverted */
  g_assert (has_ref != state->has_ref);

  ref_delta = has_ref ? 1 : -1;

  old_has_ref = state->has_ref;
  state->has_ref = has_ref;

  old_ns_used_count = self->ns_used_count;
  self->ns_used_count += ref_delta;
  g_assert (self->ns_used_count >= 0);

  if (self->ns_used_count == 0)
    {
      g_assert (ref_delta == -1);

      /* If has_keep_alive_ref == TRUE, we are sure this is not the current_version */
      if (self->has_keep_alive_ref == TRUE)
        {
          self->has_keep_alive_ref = FALSE;
          g_mutex_unlock (&self->ns_used_count_mutex);

          IDE_TRACE_MSG ("version @%i namespace:%s-%s has_ref:%i->%i ns_used_count:%i",
                         self->version_count,
                         ide_gi_namespace_get_name (ns),
                         ide_gi_namespace_get_version (ns),
                         old_has_ref, has_ref,
                         old_ns_used_count + ref_delta);

          g_object_unref (self);
          return;
        }
    }

  g_mutex_unlock (&self->ns_used_count_mutex);

  IDE_TRACE_MSG ("version @%i namespace:%s-%s has_ref:%i->%i ns_used_count:%i",
                 self->version_count,
                 ide_gi_namespace_get_name (ns),
                 ide_gi_namespace_get_version (ns),
                 old_has_ref, has_ref,
                 old_ns_used_count + ref_delta);
}

IdeGiFlatRadixTree *
_ide_gi_version_get_index_dt (IdeGiVersion *self)
{
  g_assert (IDE_IS_GI_VERSION (self));

  return self->index_dt;
}

guint64 *
_ide_gi_version_get_index_namespaces (IdeGiVersion *self)
{
  g_assert (IDE_IS_GI_VERSION (self));

  return self->index_namespaces;
}

NamespaceChunk
_ide_gi_version_get_namespace_chunk_from_id (IdeGiVersion     *self,
                                             IdeGiNamespaceId  id)
{
  IdeGiNamespaceHeader *ns_header;

  g_assert (IDE_IS_GI_VERSION (self));

  ns_header = (IdeGiNamespaceHeader *)(self->index_namespaces + id.offset64b);

  return (NamespaceChunk){.ptr       = (guint8 *)ns_header,
                          .size64b   = ns_header->size64b,
                          .offset64b = id.offset64b};
}

static inline const gchar *
get_header_blob_string (IdeGiHeaderBlob *blob,
                        guint32          offset)
{
  guint8 *base;

  g_assert (blob != NULL && IS_64B_MULTIPLE (blob));

  base = (guint8 *)blob + sizeof (IdeGiHeaderBlob);

  return (const gchar *)((guint8 *)base + offset);
};

static inline IdeGiHeaderBlob *
namespacechunk_get_header_blob (NamespaceChunk *chunk)
{
  IdeGiNamespaceHeader *ns_header;

  g_assert (chunk != NULL);

NO_CAST_ALIGN_PUSH
  ns_header = (IdeGiNamespaceHeader *)chunk->ptr;
NO_CAST_ALIGN_POP

  return (IdeGiHeaderBlob *)(ns_header + 1);
}

const gchar *
_namespacechunk_get_c_includes (NamespaceChunk *chunk)
{
  IdeGiHeaderBlob *header_blob;

  g_assert (chunk != NULL);

  header_blob = namespacechunk_get_header_blob (chunk);
  return get_header_blob_string (header_blob, header_blob->c_includes);
}

const gchar *
_namespacechunk_get_includes (NamespaceChunk *chunk)
{
  IdeGiHeaderBlob *header_blob;

  g_assert (chunk != NULL);

  header_blob = namespacechunk_get_header_blob (chunk);
  return get_header_blob_string (header_blob, header_blob->includes);
}

const gchar *
_namespacechunk_get_packages (NamespaceChunk *chunk)
{
  IdeGiHeaderBlob *header_blob;

  g_assert (chunk != NULL);

  header_blob = namespacechunk_get_header_blob (chunk);
  return get_header_blob_string (header_blob, header_blob->packages);
}

const gchar *
_namespacechunk_get_shared_library (NamespaceChunk *chunk)
{
  IdeGiHeaderBlob *header_blob;

  g_assert (chunk != NULL);

  header_blob = namespacechunk_get_header_blob (chunk);
  return get_header_blob_string (header_blob, header_blob->shared_library);
}

const gchar *
_namespacechunk_get_nsversion (NamespaceChunk *chunk)
{
  IdeGiHeaderBlob *header_blob;

  g_assert (chunk != NULL);

  header_blob = namespacechunk_get_header_blob (chunk);
  return get_header_blob_string (header_blob, header_blob->nsversion);
}

const gchar *
_namespacechunk_get_namespace (NamespaceChunk *chunk)
{
  IdeGiHeaderBlob *header_blob;

  g_assert (chunk != NULL);

  header_blob = namespacechunk_get_header_blob (chunk);
  return get_header_blob_string (header_blob, header_blob->namespace);
}

const gchar *
_namespacechunk_get_c_symbol_prefixes (NamespaceChunk *chunk)
{
  IdeGiHeaderBlob *header_blob;

  g_assert (chunk != NULL);

  header_blob = namespacechunk_get_header_blob (chunk);
  return get_header_blob_string (header_blob, header_blob->c_symbol_prefixes);
}

const gchar *
_namespacechunk_get_c_identifier_prefixes (NamespaceChunk *chunk)
{
  IdeGiHeaderBlob *header_blob;

  g_assert (chunk != NULL);

  header_blob = namespacechunk_get_header_blob (chunk);
  return get_header_blob_string (header_blob, header_blob->c_identifier_prefixes);
}

IdeGiNsIndexHeader *
_ide_gi_version_get_ns_header (IdeGiVersion     *self,
                               IdeGiNamespaceId  id)
{
  g_autofree gchar *name = NULL;
  NamespaceChunk chunk;
  const gchar *ns;
  const gchar *ns_version;
  IdeGiNsIndexHeader *ns_index_header;
  NsRecord *record;

  g_assert (IDE_IS_GI_VERSION (self));

  chunk = _ide_gi_version_get_namespace_chunk_from_id (self, id);
  ns = _namespacechunk_get_namespace (&chunk);
  ns_version = _namespacechunk_get_nsversion (&chunk);
  name = g_strdup_printf ("%s-%s@%d%s",
                          ns,
                          ns_version,
                          id.file_version,
                          INDEX_NAMESPACE_EXTENSION);
  record = _ide_gi_index_get_ns_record (self->index, name);

NO_CAST_ALIGN_PUSH
  ns_index_header =  (IdeGiNsIndexHeader *)g_mapped_file_get_contents (record->mapped_file);
NO_CAST_ALIGN_POP

  return ns_index_header;
}

void
_ide_gi_version_release_ns_header (IdeGiVersion     *self,
                                   IdeGiNamespaceId  id)
{
  g_assert (IDE_IS_GI_VERSION (self));
}
