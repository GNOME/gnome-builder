/* ide-gi-property.c
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

#include "ide-gi-property.h"

G_DEFINE_BOXED_TYPE (IdeGiProperty,
                     ide_gi_property,
                     ide_gi_property_ref,
                     ide_gi_property_unref)

void
ide_gi_property_dump (IdeGiProperty *self,
                      guint          depth)
{
  g_return_if_fail (self != NULL);

  ide_gi_base_dump ((IdeGiBase *)self);

  g_print ("readable:%d\n", ide_gi_property_is_readable (self));
  g_print ("writable:%d\n", ide_gi_property_is_writable (self));
  g_print ("construct:%d\n", ide_gi_property_is_construct (self));
  g_print ("construct_only:%d\n", ide_gi_property_is_construct_only (self));
  g_print ("transfer ownership:%s\n",
           ide_gi_utils_transfer_ownership_to_string (ide_gi_property_get_transfer_ownership (self)));

  ide_gi_utils_typeref_dump (ide_gi_property_get_typeref (self), 0);
}

gboolean
ide_gi_property_is_readable (IdeGiProperty *self)
{
  g_return_val_if_fail (self != NULL, FALSE);

  return ((IdeGiPropertyBlob *)self->common_blob)->readable;
}

gboolean
ide_gi_property_is_writable (IdeGiProperty *self)
{
  g_return_val_if_fail (self != NULL, FALSE);

  return ((IdeGiPropertyBlob *)self->common_blob)->writable;
}

gboolean
ide_gi_property_is_construct (IdeGiProperty *self)
{
  g_return_val_if_fail (self != NULL, FALSE);

  return ((IdeGiPropertyBlob *)self->common_blob)->construct;
}

gboolean
ide_gi_property_is_construct_only (IdeGiProperty *self)
{
  g_return_val_if_fail (self != NULL, FALSE);

  return ((IdeGiPropertyBlob *)self->common_blob)->construct_only;
}

IdeGiTransferOwnership
ide_gi_property_get_transfer_ownership (IdeGiProperty *self)
{
  g_return_val_if_fail (self != NULL, FALSE);

  return ((IdeGiPropertyBlob *)self->common_blob)->transfer_ownership;
}

IdeGiTypeRef
ide_gi_property_get_typeref (IdeGiProperty *self)
{
  g_return_val_if_fail (self != NULL, (IdeGiTypeRef){0});

  return (IdeGiTypeRef)(((IdeGiPropertyBlob *)self->common_blob)->type_ref);
}

IdeGiBase *
ide_gi_property_new (IdeGiNamespace *ns,
                     IdeGiBlobType   type,
                     gint32          offset)
{
  IdeGiProperty *self;
  guint8 *table;
  gsize type_size;

  g_return_val_if_fail (ns != NULL, NULL);
  g_return_val_if_fail (offset > -1, NULL);
  g_return_val_if_fail (type != IDE_GI_BLOB_TYPE_UNKNOW, NULL);

  self = g_slice_new0 (IdeGiProperty);
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
ide_gi_property_free (IdeGiBase *base)
{
  IdeGiProperty *self = (IdeGiProperty *)base;

  g_assert (self);
  g_assert_cmpint (self->ref_count, ==, 0);

  ide_gi_namespace_unref (self->ns);
  g_slice_free (IdeGiProperty, self);
}

IdeGiProperty *
ide_gi_property_ref (IdeGiProperty *self)
{
  g_return_val_if_fail (self, NULL);
  g_return_val_if_fail (self->ref_count, NULL);

  g_atomic_int_inc (&self->ref_count);

  return self;
}

void
ide_gi_property_unref (IdeGiProperty *self)
{
  g_return_if_fail (self);
  g_return_if_fail (self->ref_count);

  if (g_atomic_int_dec_and_test (&self->ref_count))
    ide_gi_property_free ((IdeGiBase *)self);
}
