/* ide-gi-union.c
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

#include "ide-gi-union.h"

G_DEFINE_BOXED_TYPE (IdeGiUnion,
                     ide_gi_union,
                     ide_gi_union_ref,
                     ide_gi_union_unref)

void
ide_gi_union_dump (IdeGiUnion *self,
                   guint       depth)
{
  guint n_fields;
  guint n_functions;
  guint n_records;

  g_return_if_fail (self != NULL);

  ide_gi_base_dump ((IdeGiBase *)self);

  g_print ("g_type_name:%s\n", ide_gi_union_get_g_type_name (self));
  g_print ("g_get_type:%s\n", ide_gi_union_get_g_get_type (self));
  g_print ("c_type:%s\n", ide_gi_union_get_c_type (self));
  g_print ("c_symbol_prefix:%s\n", ide_gi_union_get_c_symbol_prefix (self));

  n_fields = ide_gi_union_get_n_fields (self);
  n_functions = ide_gi_union_get_n_functions (self);
  n_records = ide_gi_union_get_n_records (self);

  g_print ("nb fields:%d\n", n_fields);
  g_print ("nb functions:%d\n", n_functions);
  g_print ("nb records:%d\n", n_records);

  if (depth > 0)
    {
      for (guint i = 0; i < n_fields; i++)
        ide_gi_field_dump (ide_gi_union_get_field (self, i), depth - 1);

      for (guint i = 0; i < n_functions; i++)
        ide_gi_function_dump (ide_gi_union_get_function (self, i), depth - 1);

      for (guint i = 0; i < n_fields; i++)
        ide_gi_record_dump (ide_gi_union_get_record (self, i), depth - 1);
    }
}

const gchar *
ide_gi_union_get_g_type_name (IdeGiUnion *self)
{
  guint32 offset;

  g_return_val_if_fail (self != NULL, FALSE);

  offset = ((IdeGiUnionBlob *)self->common_blob)->g_type_name;
  return ide_gi_namespace_get_string (self->ns, offset);
}

const gchar *
ide_gi_union_get_g_get_type (IdeGiUnion *self)
{
  guint32 offset;

  g_return_val_if_fail (self != NULL, FALSE);

  offset = ((IdeGiUnionBlob *)self->common_blob)->g_get_type;
  return ide_gi_namespace_get_string (self->ns, offset);
}

const gchar *
ide_gi_union_get_c_type (IdeGiUnion *self)
{
  guint32 offset;

  g_return_val_if_fail (self != NULL, FALSE);

  offset = ((IdeGiUnionBlob *)self->common_blob)->c_type;
  return ide_gi_namespace_get_string (self->ns, offset);
}

const gchar *
ide_gi_union_get_c_symbol_prefix (IdeGiUnion *self)
{
  guint32 offset;

  g_return_val_if_fail (self != NULL, FALSE);

  offset = ((IdeGiUnionBlob *)self->common_blob)->c_symbol_prefix;
  return ide_gi_namespace_get_string (self->ns, offset);
}

guint16
ide_gi_union_get_n_fields (IdeGiUnion *self)
{
  g_return_val_if_fail (self != NULL, 0);

  return ((IdeGiUnionBlob *)self->common_blob)->n_fields;
}

guint16
ide_gi_union_get_n_functions (IdeGiUnion *self)
{
  g_return_val_if_fail (self != NULL, 0);

  return ((IdeGiUnionBlob *)self->common_blob)->n_functions;
}

guint16
ide_gi_union_get_n_records (IdeGiUnion *self)
{
  g_return_val_if_fail (self != NULL, 0);

  return ((IdeGiUnionBlob *)self->common_blob)->n_records;
}

IdeGiField *
ide_gi_union_get_field (IdeGiUnion *self,
                        guint16     nth)
{
  IdeGiField *field = NULL;
  guint16 n_fields;

  g_return_val_if_fail (self != NULL, NULL);

  if (nth + 1 > (n_fields = ide_gi_union_get_n_fields (self)))
    g_warning ("Field %d is out of bounds (nb fields = %d)", nth, n_fields);
  else
    {
      guint16 offset = ((IdeGiUnionBlob *)self->common_blob)->fields + nth;
      field = (IdeGiField *)ide_gi_field_new (self->ns, IDE_GI_BLOB_TYPE_FIELD, offset);
    }

  return field;
}

IdeGiField *
ide_gi_union_lookup_field (IdeGiUnion  *self,
                           const gchar *name)
{
  guint16 n_fields;

  g_return_val_if_fail (self != NULL, NULL);
  g_return_val_if_fail (!dzl_str_empty0 (name), NULL);

  n_fields = ide_gi_union_get_n_fields (self);
  for (guint i = 0; i < n_fields; i++)
    {
      const gchar *field_name;
      g_autoptr(IdeGiField) field = ide_gi_union_get_field (self, i);

      if (field == NULL)
        continue;

      field_name = ide_gi_base_get_name ((IdeGiBase *)field);
      if (dzl_str_equal0 (field_name, name))
        return g_steal_pointer (&field);
    }

  return NULL;
}

IdeGiFunction *
ide_gi_union_get_function (IdeGiUnion *self,
                           guint16     nth)
{
  IdeGiFunction *function = NULL;
  guint16 n_functions;

  g_return_val_if_fail (self != NULL, NULL);

  if (nth + 1 > (n_functions = ide_gi_union_get_n_functions (self)))
    g_warning ("Function %d is out of bounds (nb functions = %d)", nth, n_functions);
  else
    {
      guint16 offset = ((IdeGiUnionBlob *)self->common_blob)->functions + nth;
      function = (IdeGiFunction *)ide_gi_function_new (self->ns, IDE_GI_BLOB_TYPE_FUNCTION, offset);
    }

  return function;
}

IdeGiFunction *
ide_gi_union_lookup_function (IdeGiUnion  *self,
                              const gchar *name)
{
  guint16 n_functions;

  g_return_val_if_fail (self != NULL, NULL);
  g_return_val_if_fail (!dzl_str_empty0 (name), NULL);

  n_functions = ide_gi_union_get_n_functions (self);
  for (guint i = 0; i < n_functions; i++)
    {
      const gchar *function_name;
      g_autoptr(IdeGiFunction) function = ide_gi_union_get_function (self, i);

      if (function == NULL)
        continue;

      function_name = ide_gi_base_get_name ((IdeGiBase *)function);
      if (dzl_str_equal0 (function_name, name))
        return g_steal_pointer (&function);
    }

  return NULL;
}

IdeGiRecord *
ide_gi_union_get_record (IdeGiUnion *self,
                         guint16     nth)
{
  IdeGiRecord *record = NULL;
  guint16 n_records;

  g_return_val_if_fail (self != NULL, NULL);

  if (nth + 1 > (n_records = ide_gi_union_get_n_records (self)))
    g_warning ("Record %d is out of bounds (nb records = %d)", nth, n_records);
  else
    {
      guint16 offset = ((IdeGiUnionBlob *)self->common_blob)->records + nth;
      record = (IdeGiRecord *)ide_gi_record_new (self->ns, IDE_GI_BLOB_TYPE_RECORD, offset);
    }

  return record;
}

IdeGiRecord *
ide_gi_union_lookup_record (IdeGiUnion  *self,
                            const gchar *name)
{
  guint16 n_records;

  g_return_val_if_fail (self != NULL, NULL);
  g_return_val_if_fail (!dzl_str_empty0 (name), NULL);

  n_records = ide_gi_union_get_n_records (self);
  for (guint i = 0; i < n_records; i++)
    {
      const gchar *record_name;
      g_autoptr(IdeGiRecord) record = ide_gi_union_get_record (self, i);

      if (record == NULL)
        continue;

      record_name = ide_gi_base_get_name ((IdeGiBase *)record);
      if (dzl_str_equal0 (record_name, name))
        return g_steal_pointer (&record);
    }

  return NULL;
}

IdeGiBase *
ide_gi_union_new (IdeGiNamespace *ns,
                  IdeGiBlobType   type,
                  gint32          offset)
{
  IdeGiUnion *self;
  guint8 *table;
  gsize type_size;

  g_return_val_if_fail (ns != NULL, NULL);
  g_return_val_if_fail (offset > -1, NULL);
  g_return_val_if_fail (type != IDE_GI_BLOB_TYPE_UNKNOW, NULL);

  self = g_slice_new0 (IdeGiUnion);
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
ide_gi_union_free (IdeGiBase *base)
{
  IdeGiUnion *self = (IdeGiUnion *)base;

  g_assert (self);
  g_assert_cmpint (self->ref_count, ==, 0);

  ide_gi_namespace_unref (self->ns);
  g_slice_free (IdeGiUnion, self);
}

IdeGiUnion *
ide_gi_union_ref (IdeGiUnion *self)
{
  g_return_val_if_fail (self, NULL);
  g_return_val_if_fail (self->ref_count, NULL);

  g_atomic_int_inc (&self->ref_count);

  return self;
}

void
ide_gi_union_unref (IdeGiUnion *self)
{
  g_return_if_fail (self);
  g_return_if_fail (self->ref_count);

  if (g_atomic_int_dec_and_test (&self->ref_count))
    ide_gi_union_free ((IdeGiBase *)self);
}
