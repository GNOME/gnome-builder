/* ide-gi-doc.c
 *
 * Copyright © 2018 Sebastien Lafargue <slafargue@gnome.org>
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

#include "ide-gi-doc.h"

G_DEFINE_BOXED_TYPE (IdeGiDoc,
                     ide_gi_doc,
                     ide_gi_doc_ref,
                     ide_gi_doc_unref)

void
ide_gi_doc_dump (IdeGiDoc *self)
{
  g_return_if_fail (self != NULL);

  g_print ("Doc:%s\n", ide_gi_doc_get_doc (self));
  g_print ("version:%s\n", ide_gi_doc_get_version (self));
  g_print ("deprecated:%s\n", ide_gi_doc_get_deprecated_version (self));
  g_print ("stability:%s\n", ide_gi_doc_get_stability (self));
}

/* TODO: handle doc attributes */

const gchar *
ide_gi_doc_get_doc (IdeGiDoc *self)
{
  g_return_val_if_fail (self != NULL, NULL);

  return ide_gi_namespace_get_doc_string (self->ns, self->blob->doc);
}

const gchar *
ide_gi_doc_get_version (IdeGiDoc *self)
{
  g_return_val_if_fail (self != NULL, NULL);

  return ide_gi_namespace_get_doc_string (self->ns, self->blob->doc_version);
}

const gchar *
ide_gi_doc_get_deprecated_version (IdeGiDoc *self)
{
  g_return_val_if_fail (self != NULL, NULL);

  return ide_gi_namespace_get_doc_string (self->ns, self->blob->doc_deprecated);
}

const gchar *
ide_gi_doc_get_stability (IdeGiDoc *self)
{
  g_return_val_if_fail (self != NULL, NULL);

  return ide_gi_namespace_get_doc_string (self->ns, self->blob->doc_stability);
}

IdeGiDoc *
ide_gi_doc_new (IdeGiNamespace *ns,
                gint32          offset)
{
  IdeGiDoc *self;
  guint8 *table;

  g_return_val_if_fail (ns != NULL, NULL);
  g_return_val_if_fail (offset > -1, NULL);

  if (NULL == (table = _ide_gi_namespace_get_table_from_type (ns, IDE_GI_BLOB_TYPE_DOC)))
    return NULL;

  self = g_slice_new0 (IdeGiDoc);
  self->ref_count = 1;

  self->ns = ide_gi_namespace_ref (ns);
  self->offset = offset;

NO_CAST_ALIGN_PUSH
  self->blob = (IdeGiDocBlob *)(table + offset * sizeof (IdeGiDocBlob));
NO_CAST_ALIGN_POP

  g_assert (self->blob->blob_type == IDE_GI_BLOB_TYPE_DOC);

  return self;
}

static void
ide_gi_doc_free (IdeGiDoc *self)
{
  g_assert (self);
  g_assert_cmpint (self->ref_count, ==, 0);

  ide_gi_namespace_unref (self->ns);
  g_slice_free (IdeGiDoc, self);
}

IdeGiDoc *
ide_gi_doc_ref (IdeGiDoc *self)
{
  g_return_val_if_fail (self, NULL);
  g_return_val_if_fail (self->ref_count, NULL);

  g_atomic_int_inc (&self->ref_count);

  return self;
}

void
ide_gi_doc_unref (IdeGiDoc *self)
{
  g_return_if_fail (self);
  g_return_if_fail (self->ref_count);

  if (g_atomic_int_dec_and_test (&self->ref_count))
    ide_gi_doc_free (self);
}
