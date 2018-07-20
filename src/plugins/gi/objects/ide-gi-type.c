/* ide-gi-type.c
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

#include "./../ide-gi-utils.h"

#include "ide-gi-type.h"

G_DEFINE_BOXED_TYPE (IdeGiType,
                     ide_gi_type,
                     ide_gi_type_ref,
                     ide_gi_type_unref)

void
ide_gi_type_dump (IdeGiType *self,
                  guint      depth)
{
  IdeGiBasicType type;

  g_return_if_fail (self != NULL);

  ide_gi_base_dump ((IdeGiBase *)self);

  if (ide_gi_type_is_basic_type (self))
    {
     type = ide_gi_type_get_basic_type (self);
     g_print ("basic type:%s\n", ide_gi_utils_type_to_string (type));
    }

  g_print ("is conntainer:%d\n", ide_gi_type_is_container (self));
  g_print ("is local:%d\n", ide_gi_type_is_local (self));
  g_print ("c_type:%s\n", ide_gi_type_get_c_type (self));

  ide_gi_utils_typeref_dump (ide_gi_type_get_typeref_0 (self), depth);
  ide_gi_utils_typeref_dump (ide_gi_type_get_typeref_1 (self), depth);
}

gboolean
ide_gi_type_is_basic_type (IdeGiType *self)
{
  g_return_val_if_fail (self != NULL, FALSE);

  return ((IdeGiTypeBlob *)self->common_blob)->is_basic_type;
}

gboolean
ide_gi_type_is_container (IdeGiType *self)
{
  g_return_val_if_fail (self != NULL, FALSE);

  return ((IdeGiTypeBlob *)self->common_blob)->is_type_container;
}

gboolean
ide_gi_type_is_local (IdeGiType *self)
{
  g_return_val_if_fail (self != NULL, FALSE);

  return ((IdeGiTypeBlob *)self->common_blob)->is_local;
}

IdeGiBasicType
ide_gi_type_get_basic_type (IdeGiType *self)
{
  g_return_val_if_fail (self != NULL, IDE_GI_BASIC_TYPE_NONE);

  return (IdeGiBasicType)(((IdeGiTypeBlob *)self->common_blob)->basic_type);
}

IdeGiTypeRef
ide_gi_type_get_typeref_0 (IdeGiType *self)
{
  g_return_val_if_fail (self != NULL, (IdeGiTypeRef){0});

  return (IdeGiTypeRef)(((IdeGiTypeBlob *)self->common_blob)->type_ref_0);
}

IdeGiTypeRef
ide_gi_type_get_typeref_1 (IdeGiType *self)
{
  g_return_val_if_fail (self != NULL, (IdeGiTypeRef){0});

  return (IdeGiTypeRef)(((IdeGiTypeBlob *)self->common_blob)->type_ref_0);
}

const gchar *
ide_gi_type_get_c_type (IdeGiType *self)
{
  guint32 offset;

  g_return_val_if_fail (self != NULL, FALSE);

  offset = ((IdeGiTypeBlob *)self->common_blob)->c_type;
  return ide_gi_namespace_get_string (self->ns, offset);
}

IdeGiBase *
ide_gi_type_new (IdeGiNamespace *ns,
                 IdeGiBlobType   type,
                 gint32          offset)
{
  IdeGiType *self;
  guint8 *table;
  gsize type_size;

  g_return_val_if_fail (ns != NULL, NULL);
  g_return_val_if_fail (offset > -1, NULL);
  g_return_val_if_fail (type != IDE_GI_BLOB_TYPE_UNKNOW, NULL);

  self = g_slice_new0 (IdeGiType);
  self->ref_count = 1;

  self->ns = ide_gi_namespace_ref (ns);
  self->type = type;
  self->offset = offset;

  table = _ide_gi_namespace_get_table_from_type (ns, type);
  type_size = _ide_gi_namespace_get_blob_size_from_type (ns, type);

NO_CAST_ALIGN_PUSH
  self->common_blob = (IdeGiCommonBlob *)(table + offset * type_size);
NO_CAST_ALIGN_POP

  return (IdeGiBase *)self;
}

void
ide_gi_type_free (IdeGiBase *base)
{
  IdeGiType *self = (IdeGiType *)base;

  g_assert (self);
  g_assert_cmpint (self->ref_count, ==, 0);

  ide_gi_namespace_unref (self->ns);
  g_slice_free (IdeGiType, self);
}

IdeGiType *
ide_gi_type_ref (IdeGiType *self)
{
  g_return_val_if_fail (self, NULL);
  g_return_val_if_fail (self->ref_count, NULL);

  g_atomic_int_inc (&self->ref_count);

  return self;
}

void
ide_gi_type_unref (IdeGiType *self)
{
  g_return_if_fail (self);
  g_return_if_fail (self->ref_count);

  if (g_atomic_int_dec_and_test (&self->ref_count))
    ide_gi_type_free ((IdeGiBase *)self);
}
