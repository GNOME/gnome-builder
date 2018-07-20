/* ide-gi-constant.c
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

#include "ide-gi-constant.h"

G_DEFINE_BOXED_TYPE (IdeGiConstant,
                     ide_gi_constant,
                     ide_gi_constant_ref,
                     ide_gi_constant_unref)

void
ide_gi_constant_dump (IdeGiConstant *self,
                      guint          depth)
{
  g_return_if_fail (self != NULL);

  ide_gi_base_dump ((IdeGiBase *)self);

  g_print ("value:%s\n", ide_gi_constant_get_value (self));
  g_print ("c_type:%s\n", ide_gi_constant_get_c_type (self));
  g_print ("c_identifier:%s\n", ide_gi_constant_get_c_identifier (self));

  ide_gi_utils_typeref_dump (ide_gi_constant_get_typeref (self), 0);
}

const gchar *
ide_gi_constant_get_value (IdeGiConstant *self)
{
  guint32 offset;

  g_return_val_if_fail (self != NULL, FALSE);

  offset = ((IdeGiConstantBlob *)self->common_blob)->value;
  return ide_gi_namespace_get_string (self->ns, offset);
}

const gchar *
ide_gi_constant_get_c_type (IdeGiConstant *self)
{
  guint32 offset;

  g_return_val_if_fail (self != NULL, FALSE);

  offset = ((IdeGiConstantBlob *)self->common_blob)->c_type;
  return ide_gi_namespace_get_string (self->ns, offset);
}

const gchar *
ide_gi_constant_get_c_identifier (IdeGiConstant *self)
{
  guint32 offset;

  g_return_val_if_fail (self != NULL, FALSE);

  offset = ((IdeGiConstantBlob *)self->common_blob)->c_identifier;
  return ide_gi_namespace_get_string (self->ns, offset);
}

IdeGiTypeRef
ide_gi_constant_get_typeref (IdeGiConstant *self)
{
  g_return_val_if_fail (self != NULL, (IdeGiTypeRef){0});

  return (IdeGiTypeRef)(((IdeGiConstantBlob *)self->common_blob)->type_ref);
}

IdeGiBase *
ide_gi_constant_new (IdeGiNamespace *ns,
                     IdeGiBlobType   type,
                     gint32          offset)
{
  IdeGiConstant *self;
  guint8 *table;
  gsize type_size;

  g_return_val_if_fail (ns != NULL, NULL);
  g_return_val_if_fail (offset > -1, NULL);
  g_return_val_if_fail (type != IDE_GI_BLOB_TYPE_UNKNOW, NULL);

  self = g_slice_new0 (IdeGiConstant);
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
ide_gi_constant_free (IdeGiBase *base)
{
  IdeGiConstant *self = (IdeGiConstant *)base;

  g_assert (self);
  g_assert_cmpint (self->ref_count, ==, 0);

  ide_gi_namespace_unref (self->ns);
  g_slice_free (IdeGiConstant, self);
}

IdeGiConstant *
ide_gi_constant_ref (IdeGiConstant *self)
{
  g_return_val_if_fail (self, NULL);
  g_return_val_if_fail (self->ref_count, NULL);

  g_atomic_int_inc (&self->ref_count);

  return self;
}

void
ide_gi_constant_unref (IdeGiConstant *self)
{
  g_return_if_fail (self);
  g_return_if_fail (self->ref_count);

  if (g_atomic_int_dec_and_test (&self->ref_count))
    ide_gi_constant_free ((IdeGiBase *)self);
}
