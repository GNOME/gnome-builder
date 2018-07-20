/* ide-gi-namespace.c
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

#define G_LOG_DOMAIN "ide-gi-namespace"

#include <gio/gio.h>
#include <dazzle.h>
#include <ide.h>
#include <stdio.h>

#include "ide-gi-repository.h"
#include "ide-gi-utils.h"
#include "ide-gi-version-private.h"

#include "ide-gi-namespace.h"
#include "ide-gi-namespace-private.h"

G_DEFINE_BOXED_TYPE (IdeGiNamespace, ide_gi_namespace, ide_gi_namespace_ref, ide_gi_namespace_unref)

IdeGiNamespaceId
_ide_gi_namespace_get_id (IdeGiNamespace *self)
{
  g_return_val_if_fail (self != NULL, (IdeGiNamespaceId){{0}});

  return self->id;
}

gsize
_ide_gi_namespace_get_blob_size_from_type (IdeGiNamespace *self,
                                           IdeGiBlobType   type)
{
  g_return_val_if_fail (self != NULL, 0);

  return ide_gi_blob_get_size (type);
}

guint8 *
_ide_gi_namespace_get_table_from_type (IdeGiNamespace *self,
                                       IdeGiBlobType   type)
{
  IdeGiNsTable table;
  gint32 table_offset;
  guint8 *base;

  g_return_val_if_fail (self != NULL, 0);

  table = ide_gi_blob_get_ns_table (type);
  g_assert (table != IDE_GI_NS_TABLE_UNKNOW);

  table_offset = self->tail_header->elements_tables [table];
  if (table_offset == -1)
    return NULL;

  base = (guint8 *)self->tail_header + sizeof (IdeGiNsIndexHeader) + table_offset;
  g_assert (IS_32B_MULTIPLE(base));

  return base;
}

static gchar *
split_include (const gchar *include,
               guint16     *major_version,
               guint16     *minor_version)
{
  const gchar *ptr;

  g_assert (!dzl_str_empty0 (include) && *include != ':');
  g_assert (major_version != NULL);
  g_assert (minor_version != NULL);

  if ((ptr = strchr (include, ':')) &&
      ide_gi_utils_parse_version (ptr + 1, major_version, minor_version, NULL))
    {
      return g_strndup (include, ptr - include);
    }

  return NULL;
}

static IdeGiRequire *
create_namespace_require (IdeGiNamespace *self)
{
  g_auto(GStrv) parts = NULL;
  IdeGiRequire *req;

  req = ide_gi_require_new ();
  parts = g_strsplit (self->includes, ",", -1);
  for (guint i = 0; parts[i] != NULL; i++)
    {
      g_autofree gchar *ns = NULL;
      guint16 major_version;
      guint16 minor_version;

      if (*parts[i] == '\0' ||
          NULL == (ns = split_include (parts[i], &major_version, &minor_version)))
        continue;

      ide_gi_require_add (req,
                          ns,
                          (IdeGiRequireBound){IDE_GI_REQUIRE_COMP_EQUAL, major_version, minor_version});
    }

  return req;
}

/**
 * ide_gi_namespace_get_require:
 *
 * @self: #IdeGiNamespace.
 *
 * Return a #IdeGiRequire based on the namespace includes.
 *
 * Returns: (transfer full): a #IdeGiRequire.
 */
IdeGiRequire *
ide_gi_namespace_get_require (IdeGiNamespace *self)
{
  g_return_val_if_fail (self != NULL, NULL);

  if (self->req == NULL)
    self->req = create_namespace_require (self);

  return ide_gi_require_copy (self->req);
}

IdeGiBase *
ide_gi_namespace_get_object (IdeGiNamespace *self,
                             IdeGiBlobType   type,
                             guint16         offset)
{
  g_return_val_if_fail (self != NULL, NULL);

  return ide_gi_base_new (self, type, offset);
}

IdeGiCrossRef *
_ide_gi_namespace_get_crossref (IdeGiNamespace *self,
                                guint32         offset)
{
  guint8 *base;

  g_return_val_if_fail (self != NULL, NULL);

  base = (guint8 *)self->tail_header + sizeof (IdeGiNsIndexHeader) + self->tail_header->crossrefs;

NO_CAST_ALIGN_PUSH
  return (IdeGiCrossRef *)base + offset;
NO_CAST_ALIGN_POP
}

const gchar *
ide_gi_namespace_get_string (IdeGiNamespace *self,
                             guint32         offset)
{
  const gchar *base;

  g_return_val_if_fail (self != NULL, NULL);

  base = (gchar *)self->tail_header + sizeof (IdeGiNsIndexHeader) + self->tail_header->strings;

  return (const gchar *)(base + offset);
};

const gchar *
ide_gi_namespace_get_doc_string (IdeGiNamespace *self,
                                 guint32         offset)
{
  const gchar *base;

  g_return_val_if_fail (self != NULL, NULL);

  base = (gchar *)self->tail_header + sizeof (IdeGiNsIndexHeader) + self->tail_header->doc_strings;

  return (const gchar *)(base + offset);
};

const gchar *
ide_gi_namespace_get_annnotation_string (IdeGiNamespace *self,
                                         guint32         offset)
{
  const gchar *base;

  g_return_val_if_fail (self != NULL, NULL);

  base = (gchar *)self->tail_header + sizeof (IdeGiNsIndexHeader) + self->tail_header->annotation_strings;

  return (const gchar *)(base + offset);
};

static inline const gchar *
get_header_blob_string (IdeGiHeaderBlob *blob,
                        guint32          offset)
{
  return (const gchar *)((guint8 *)blob + sizeof (IdeGiHeaderBlob) + offset);
};

void
ide_gi_namespace_dump (IdeGiNamespace *self)
{
  g_return_if_fail (self != NULL);

  g_print ("ns:'%s' version:'%s'(%d,%d)\n"
           "symbols:'%s'\n"
           "identifiers:'%s'\n"
           "includes:'%s'\n"
           "c_includes:'%s'\n"
           "packages:'%s'\n"
           "shared library:%s\n\n",
           self->ns,
           self->ns_version, self->major_version, self->minor_version,
           self->c_symbol_prefixes,
           self->c_identifiers_prefixes,
           self->c_includes,
           self->includes,
           self->packages,
           self->shared_library);
}

/* TODO: still needed ? */
gboolean
ide_gi_namespace_is_valid (IdeGiNamespace *self)
{
  g_return_val_if_fail (self != NULL, FALSE);

  return self->is_valid;
}

const gchar *
ide_gi_namespace_get_c_includes (IdeGiNamespace *self)
{
  g_return_val_if_fail (self != NULL, NULL);

  return self->c_includes;
}

const gchar *
ide_gi_namespace_get_includes (IdeGiNamespace *self)
{
  g_return_val_if_fail (self != NULL, NULL);

  return self->includes;
}

const gchar *
ide_gi_namespace_get_packages (IdeGiNamespace *self)
{
  g_return_val_if_fail (self != NULL, NULL);

  return self->packages;
}

const gchar *
ide_gi_namespace_get_shared_library (IdeGiNamespace *self)
{
  g_return_val_if_fail (self != NULL, NULL);

  return self->shared_library;
}

const gchar *
ide_gi_namespace_get_name (IdeGiNamespace *self)
{
  g_return_val_if_fail (self != NULL, NULL);

  return self->ns;
}

const gchar *
ide_gi_namespace_get_version (IdeGiNamespace *self)
{
  g_return_val_if_fail (self != NULL, NULL);

  return self->ns_version;
}

const gchar *
ide_gi_namespace_get_c_identifiers_prefixes (IdeGiNamespace *self)
{
  g_return_val_if_fail (self != NULL, NULL);

  return self->c_identifiers_prefixes;
}

const gchar *
ide_gi_namespace_get_c_symbol_prefixes (IdeGiNamespace *self)
{
  g_return_val_if_fail (self != NULL, NULL);

  return self->c_symbol_prefixes;
}

guint8
ide_gi_namespace_get_major_version (IdeGiNamespace *self)
{
  g_return_val_if_fail (self != NULL, 0);

  return self->major_version;
}

guint8
ide_gi_namespace_get_minor_version (IdeGiNamespace *self)
{
  g_return_val_if_fail (self != NULL, 0);

  return self->minor_version;
}

IdeGiVersion *
ide_gi_namespace_get_repository_version (IdeGiNamespace *self)
{
  g_return_val_if_fail (self != NULL, NULL);

  return self->version;
}

IdeGiNamespace *
_ide_gi_namespace_new (IdeGiVersion     *version,
                       IdeGiNamespaceId  id)
{
  IdeGiNamespace *self;
  guint64 *index_namespaces;
  guint32 offset;

  self = g_slice_new0 (IdeGiNamespace);
  self->version = version;

  offset = id.offset64b;
  index_namespaces = _ide_gi_version_get_index_namespaces (version);

  self->id = id;
  self->head_header = (IdeGiNamespaceHeader *)(index_namespaces + offset);
  self->chunk = _ide_gi_version_get_namespace_chunk_from_id (version, id);

  self->c_includes = g_strdup (_namespacechunk_get_c_includes (&self->chunk));
  self->includes = g_strdup (_namespacechunk_get_includes (&self->chunk));
  self->packages = g_strdup (_namespacechunk_get_packages (&self->chunk));
  self->shared_library = g_strdup (_namespacechunk_get_shared_library (&self->chunk));
  self->ns = g_strdup (_namespacechunk_get_namespace (&self->chunk));
  self->ns_version = g_strdup (_namespacechunk_get_nsversion (&self->chunk));
  self->c_identifiers_prefixes = g_strdup (_namespacechunk_get_c_identifier_prefixes (&self->chunk));
  self->c_symbol_prefixes = g_strdup (_namespacechunk_get_c_symbol_prefixes (&self->chunk));

  self->major_version = id.major_version;
  self->minor_version = id.minor_version;
  self->is_valid = TRUE;
  self->tail_header = _ide_gi_version_get_ns_header (version, id);

  if (self->tail_header->magic != NS_INDEX_HEADER_MAGIC)
    {
      g_warning ("wrong magic number, "INDEX_NAMESPACE_EXTENSION" file probably wrong");
    }

  g_mutex_init (&self->mutex);
  return self;
}

void
_ide_gi_namespace_free (IdeGiNamespace *self)
{
  g_assert (self);
  g_assert_cmpint (self->ns_count, ==, 0);

  g_free (self->c_includes);
  g_free (self->includes);
  g_free (self->packages);
  g_free (self->shared_library);
  g_free (self->ns);
  g_free (self->c_identifiers_prefixes);
  g_free (self->c_symbol_prefixes);

  dzl_clear_pointer (&self->req, ide_gi_require_unref);
  g_mutex_clear (&self->mutex);

  g_slice_free (IdeGiNamespace, self);
}

/* new/free are used from ide-gi internals to manage its lifetime and
 * ref/unref are used mostly from outside code, to count the number of users.
 */
IdeGiNamespace *
ide_gi_namespace_ref (IdeGiNamespace *self)
{
  g_return_val_if_fail (self, NULL);

  g_mutex_lock (&self->mutex);

  if (++self->ns_count == 1)
    _ide_gi_version_set_namespace_state (self->version, self, TRUE);

  g_mutex_unlock (&self->mutex);

  return self;
}

void
ide_gi_namespace_unref (IdeGiNamespace *self)
{
  g_return_if_fail (self);
  g_return_if_fail (self->ns_count > 0);

  g_mutex_lock (&self->mutex);

  if (--self->ns_count == 0)
    _ide_gi_version_set_namespace_state (self->version, self, FALSE);

  g_mutex_unlock (&self->mutex);
}
