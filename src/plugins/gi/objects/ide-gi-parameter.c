/* ide-gi-parameter.c
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

#include "ide-gi-parameter.h"

G_DEFINE_BOXED_TYPE (IdeGiParameter,
                     ide_gi_parameter,
                     ide_gi_parameter_ref,
                     ide_gi_parameter_unref)

static void
dump_flags (IdeGiParameterFlags flags)
{
    g_print ("nullable:%d\n", flags & IDE_GI_PARAMETER_FLAG_NULLABLE);
    g_print ("optional:%d\n", flags & IDE_GI_PARAMETER_FLAG_OPTIONAL);
    g_print ("allow none:%d\n", flags & IDE_GI_PARAMETER_FLAG_ALLOW_NONE);
    g_print ("caller allocates:%d\n", flags & IDE_GI_PARAMETER_FLAG_CALLER_ALLOCATES);
    g_print ("skip:%d\n", flags & IDE_GI_PARAMETER_FLAG_SKIP);
    g_print ("return value:%d\n", flags & IDE_GI_PARAMETER_FLAG_RETURN_VALUE);
    g_print ("instance parameter:%d\n", flags & IDE_GI_PARAMETER_FLAG_INSTANCE_PARAMETER);
    g_print ("varargs:%d\n", flags & IDE_GI_PARAMETER_FLAG_VARARGS);
    g_print ("has closure:%d\n", flags & IDE_GI_PARAMETER_FLAG_HAS_CLOSURE);
    g_print ("has destroy:%d\n", flags & IDE_GI_PARAMETER_FLAG_HAS_DESTROY);
}

void
ide_gi_parameter_dump (IdeGiParameter *self,
                       guint           depth)
{
  IdeGiScope scope;
  IdeGiTransferOwnership transfer_ownership;
  IdeGiDirection direction;
  IdeGiParameterFlags flags;

  g_return_if_fail (self != NULL);

  scope = ide_gi_parameter_get_scope (self);
  transfer_ownership = ide_gi_parameter_get_transfer_ownership (self);
  direction = ide_gi_parameter_get_direction (self);
  flags = ide_gi_parameter_get_flags (self);

  ide_gi_base_dump ((IdeGiBase *)self);

  g_print ("scope:%s\n", ide_gi_utils_scope_to_string (scope));
  g_print ("transfer ownership:%s\n", ide_gi_utils_transfer_ownership_to_string (transfer_ownership));
  g_print ("direction:%s\n", ide_gi_utils_direction_to_string (direction));

  dump_flags (flags);

  g_print ("closure:%s\n", ide_gi_parameter_get_closure (self));
  g_print ("destroy:%s\n", ide_gi_parameter_get_destroy (self));

  ide_gi_utils_typeref_dump (ide_gi_parameter_get_typeref (self), 0);
}

IdeGiScope
ide_gi_parameter_get_scope (IdeGiParameter *self)
{
  return ((IdeGiParameterBlob *)self->common_blob)->scope;
}

IdeGiTransferOwnership
ide_gi_parameter_get_transfer_ownership (IdeGiParameter *self)
{
  return ((IdeGiParameterBlob *)self->common_blob)->transfer_ownership;
}

IdeGiDirection
ide_gi_parameter_get_direction (IdeGiParameter *self)
{
  return ((IdeGiParameterBlob *)self->common_blob)->direction;
}

IdeGiParameterFlags
ide_gi_parameter_get_flags (IdeGiParameter *self)
{
  return ((IdeGiParameterBlob *)self->common_blob)->flags;
}

const gchar *
ide_gi_parameter_get_closure (IdeGiParameter *self)
{
  guint32 offset;

  g_return_val_if_fail (self != NULL, FALSE);

  offset = ((IdeGiParameterBlob *)self->common_blob)->closure;
  return ide_gi_namespace_get_string (self->ns, offset);
}

const gchar *
ide_gi_parameter_get_destroy (IdeGiParameter *self)
{
  guint32 offset;

  g_return_val_if_fail (self != NULL, FALSE);

  offset = ((IdeGiParameterBlob *)self->common_blob)->destroy;
  return ide_gi_namespace_get_string (self->ns, offset);
}

IdeGiTypeRef
ide_gi_parameter_get_typeref (IdeGiParameter *self)
{
  g_return_val_if_fail (self != NULL, (IdeGiTypeRef){0});

  return (IdeGiTypeRef)(((IdeGiParameterBlob *)self->common_blob)->type_ref);
}

IdeGiBase *
ide_gi_parameter_new (IdeGiNamespace *ns,
                      IdeGiBlobType   type,
                      gint32          offset)
{
  IdeGiParameter *self;
  guint8 *table;
  gsize type_size;

  g_return_val_if_fail (ns != NULL, NULL);
  g_return_val_if_fail (offset > -1, NULL);
  g_return_val_if_fail (type != IDE_GI_BLOB_TYPE_UNKNOW, NULL);

  self = g_slice_new0 (IdeGiParameter);
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
ide_gi_parameter_free (IdeGiBase *base)
{
  IdeGiParameter *self = (IdeGiParameter *)base;

  g_assert (self);
  g_assert_cmpint (self->ref_count, ==, 0);

  ide_gi_namespace_unref (self->ns);
  g_slice_free (IdeGiParameter, self);
}

IdeGiParameter *
ide_gi_parameter_ref (IdeGiParameter *self)
{
  g_return_val_if_fail (self, NULL);
  g_return_val_if_fail (self->ref_count, NULL);

  g_atomic_int_inc (&self->ref_count);

  return self;
}

void
ide_gi_parameter_unref (IdeGiParameter *self)
{
  g_return_if_fail (self);
  g_return_if_fail (self->ref_count);

  if (g_atomic_int_dec_and_test (&self->ref_count))
    ide_gi_parameter_free ((IdeGiBase *)self);
}
