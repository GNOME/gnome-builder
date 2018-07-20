/* ide-gi-record.c
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

#include "ide-gi-record.h"

G_DEFINE_BOXED_TYPE (IdeGiRecord,
                     ide_gi_record,
                     ide_gi_record_ref,
                     ide_gi_record_unref)

void
ide_gi_record_dump (IdeGiRecord *self,
                    guint        depth)
{
  guint n_callbacks;
  guint n_fields;
  guint n_functions;
  guint n_properties;
  guint n_unions;

  g_return_if_fail (self != NULL);

  ide_gi_base_dump ((IdeGiBase *)self);

  g_print ("disguised:%d\n", ide_gi_record_is_disguised (self));
  g_print ("foreign:%d\n", ide_gi_record_is_foreign (self));

  g_print ("g_type_name:%s\n", ide_gi_record_get_g_type_name (self));
  g_print ("g_get_type:%s\n", ide_gi_record_get_g_get_type (self));
  g_print ("g_is_gtype_struct_for:%s\n", ide_gi_record_get_g_is_gtype_struct_for (self));
  g_print ("c_type:%s\n", ide_gi_record_get_c_type (self));
  g_print ("c_symbol_prefix:%s\n", ide_gi_record_get_c_symbol_prefix (self));

  n_callbacks = ide_gi_record_get_n_callbacks (self);
  n_fields = ide_gi_record_get_n_fields (self);
  n_functions = ide_gi_record_get_n_functions (self);
  n_properties = ide_gi_record_get_n_properties (self);
  n_unions = ide_gi_record_get_n_unions (self);

  g_print ("nb callbacks:%d\n", n_callbacks);
  g_print ("nb fields:%d\n", n_fields);
  g_print ("nb functions:%d\n", n_functions);
  g_print ("nb properties:%d\n", n_properties);
  g_print ("nb unions:%d\n", n_unions);

  if (depth > 0)
    {
      for (guint i = 0; i < n_callbacks; i++)
        ide_gi_callback_dump (ide_gi_record_get_callback (self, i), depth - 1);

      for (guint i = 0; i < n_fields; i++)
        ide_gi_field_dump (ide_gi_record_get_field (self, i), depth - 1);

      for (guint i = 0; i < n_functions; i++)
        ide_gi_function_dump (ide_gi_record_get_function (self, i), depth - 1);

      for (guint i = 0; i < n_properties; i++)
        ide_gi_property_dump (ide_gi_record_get_property (self, i), depth - 1);

      for (guint i = 0; i < n_unions; i++)
        ide_gi_union_dump (ide_gi_record_get_union (self, i), depth - 1);
    }
}

gboolean
ide_gi_record_is_disguised (IdeGiRecord *self)
{
  g_return_val_if_fail (self != NULL, FALSE);

  return ((IdeGiRecordBlob *)self->common_blob)->disguised;
}

gboolean
ide_gi_record_is_foreign (IdeGiRecord *self)
{
  g_return_val_if_fail (self != NULL, FALSE);

  return ((IdeGiRecordBlob *)self->common_blob)->foreign;
}

const gchar *
ide_gi_record_get_g_type_name (IdeGiRecord *self)
{
  guint32 offset;

  g_return_val_if_fail (self != NULL, FALSE);

  offset = ((IdeGiRecordBlob *)self->common_blob)->g_type_name;
  return ide_gi_namespace_get_string (self->ns, offset);
}

const gchar *
ide_gi_record_get_g_get_type (IdeGiRecord *self)
{
  guint32 offset;

  g_return_val_if_fail (self != NULL, FALSE);

  offset = ((IdeGiRecordBlob *)self->common_blob)->g_get_type;
  return ide_gi_namespace_get_string (self->ns, offset);
}

const gchar *
ide_gi_record_get_g_is_gtype_struct_for (IdeGiRecord *self)
{
  guint32 offset;

  g_return_val_if_fail (self != NULL, FALSE);

  offset = ((IdeGiRecordBlob *)self->common_blob)->g_is_gtype_struct_for;
  return ide_gi_namespace_get_string (self->ns, offset);
}

const gchar *
ide_gi_record_get_c_type (IdeGiRecord *self)
{
  guint32 offset;

  g_return_val_if_fail (self != NULL, FALSE);

  offset = ((IdeGiRecordBlob *)self->common_blob)->c_type;
  return ide_gi_namespace_get_string (self->ns, offset);
}

const gchar *
ide_gi_record_get_c_symbol_prefix (IdeGiRecord *self)
{
  guint32 offset;

  g_return_val_if_fail (self != NULL, FALSE);

  offset = ((IdeGiRecordBlob *)self->common_blob)->c_symbol_prefix;
  return ide_gi_namespace_get_string (self->ns, offset);
}

guint16
ide_gi_record_get_n_callbacks (IdeGiRecord *self)
{
  g_return_val_if_fail (self != NULL, 0);

  return ((IdeGiRecordBlob *)self->common_blob)->n_callbacks;
}

guint16
ide_gi_record_get_n_fields (IdeGiRecord *self)
{
  g_return_val_if_fail (self != NULL, 0);

  return ((IdeGiRecordBlob *)self->common_blob)->n_fields;
}

guint16
ide_gi_record_get_n_functions (IdeGiRecord *self)
{
  g_return_val_if_fail (self != NULL, 0);

  return ((IdeGiRecordBlob *)self->common_blob)->n_functions;
}

guint16
ide_gi_record_get_n_properties (IdeGiRecord *self)
{
  g_return_val_if_fail (self != NULL, 0);

  return ((IdeGiRecordBlob *)self->common_blob)->n_properties;
}

guint16
ide_gi_record_get_n_unions (IdeGiRecord *self)
{
  g_return_val_if_fail (self != NULL, 0);

  return ((IdeGiRecordBlob *)self->common_blob)->n_unions;
}

IdeGiCallback *
ide_gi_record_get_callback (IdeGiRecord *self,
                            guint16      nth)
{
  IdeGiCallback *callback = NULL;
  guint16 n_callbacks;

  g_return_val_if_fail (self != NULL, NULL);

  if (nth + 1 > (n_callbacks = ide_gi_record_get_n_callbacks (self)))
    g_warning ("Callback %d is out of bounds (nb callbacks = %d)", nth, n_callbacks);
  else
    {
      guint16 offset = ((IdeGiRecordBlob *)self->common_blob)->callbacks + nth;
      callback = (IdeGiCallback *)ide_gi_callback_new (self->ns, IDE_GI_BLOB_TYPE_CALLBACK, offset);
    }

  return callback;
}

IdeGiCallback *
ide_gi_record_lookup_callback (IdeGiRecord *self,
                               const gchar *name)
{
  guint16 n_callbacks;

  g_return_val_if_fail (self != NULL, NULL);
  g_return_val_if_fail (!dzl_str_empty0 (name), NULL);

  n_callbacks = ide_gi_record_get_n_callbacks (self);
  for (guint i = 0; i < n_callbacks; i++)
    {
      const gchar *callback_name;
      g_autoptr(IdeGiCallback) callback = ide_gi_record_get_callback (self, i);

      if (callback == NULL)
        continue;

      callback_name = ide_gi_base_get_name ((IdeGiBase *)callback);
      if (dzl_str_equal0 (callback_name, name))
        return g_steal_pointer (&callback);
    }

  return NULL;
}

IdeGiField *
ide_gi_record_get_field (IdeGiRecord *self,
                         guint16      nth)
{
  IdeGiField *field = NULL;
  guint16 n_fields;

  g_return_val_if_fail (self != NULL, NULL);

  if (nth + 1 > (n_fields = ide_gi_record_get_n_fields (self)))
    g_warning ("Field %d is out of bounds (nb fields = %d)", nth, n_fields);
  else
    {
      guint16 offset = ((IdeGiRecordBlob *)self->common_blob)->fields + nth;
      field = (IdeGiField *)ide_gi_field_new (self->ns, IDE_GI_BLOB_TYPE_FIELD, offset);
    }

  return field;
}

IdeGiField *
ide_gi_record_lookup_field (IdeGiRecord *self,
                            const gchar *name)
{
  guint16 n_fields;

  g_return_val_if_fail (self != NULL, NULL);
  g_return_val_if_fail (!dzl_str_empty0 (name), NULL);

  n_fields = ide_gi_record_get_n_fields (self);
  for (guint i = 0; i < n_fields; i++)
    {
      const gchar *field_name;
      g_autoptr(IdeGiField) field = ide_gi_record_get_field (self, i);

      if (field == NULL)
        continue;

      field_name = ide_gi_base_get_name ((IdeGiBase *)field);
      if (dzl_str_equal0 (field_name, name))
        return g_steal_pointer (&field);
    }

  return NULL;
}

IdeGiFunction *
ide_gi_record_get_function (IdeGiRecord *self,
                            guint16      nth)
{
  IdeGiFunction *function = NULL;
  guint16 n_functions;

  g_return_val_if_fail (self != NULL, NULL);

  if (nth + 1 > (n_functions = ide_gi_record_get_n_functions (self)))
    g_warning ("Function %d is out of bounds (nb functions = %d)", nth, n_functions);
  else
    {
      guint16 offset = ((IdeGiRecordBlob *)self->common_blob)->functions + nth;
      function = (IdeGiFunction *)ide_gi_function_new (self->ns, IDE_GI_BLOB_TYPE_FUNCTION, offset);
    }

  return function;
}

IdeGiFunction *
ide_gi_record_lookup_function (IdeGiRecord *self,
                               const gchar *name)
{
  guint16 n_functions;

  g_return_val_if_fail (self != NULL, NULL);
  g_return_val_if_fail (!dzl_str_empty0 (name), NULL);

  n_functions = ide_gi_record_get_n_functions (self);
  for (guint i = 0; i < n_functions; i++)
    {
      const gchar *function_name;
      g_autoptr(IdeGiFunction) function = ide_gi_record_get_function (self, i);

      if (function == NULL)
        continue;

      function_name = ide_gi_base_get_name ((IdeGiBase *)function);
      if (dzl_str_equal0 (function_name, name))
        return g_steal_pointer (&function);
    }

  return NULL;
}

IdeGiProperty *
ide_gi_record_get_property (IdeGiRecord *self,
                            guint16      nth)
{
  IdeGiProperty *property = NULL;
  guint16 n_properties;

  g_return_val_if_fail (self != NULL, NULL);

  if (nth + 1 > (n_properties = ide_gi_record_get_n_properties (self)))
    g_warning ("Property %d is out of bounds (nb properties = %d)", nth, n_properties);
  else
    {
      guint16 offset = ((IdeGiRecordBlob *)self->common_blob)->properties + nth;
      property = (IdeGiProperty *)ide_gi_property_new (self->ns, IDE_GI_BLOB_TYPE_PROPERTY, offset);
    }

  return property;
}

IdeGiProperty *
ide_gi_record_lookup_property (IdeGiRecord *self,
                               const gchar *name)
{
  guint16 n_properties;

  g_return_val_if_fail (self != NULL, NULL);
  g_return_val_if_fail (!dzl_str_empty0 (name), NULL);

  n_properties = ide_gi_record_get_n_properties (self);
  for (guint i = 0; i < n_properties; i++)
    {
      const gchar *property_name;
      g_autoptr(IdeGiProperty) property = ide_gi_record_get_property (self, i);

      if (property == NULL)
        continue;

      property_name = ide_gi_base_get_name ((IdeGiBase *)property);
      if (dzl_str_equal0 (property_name, name))
        return g_steal_pointer (&property);
    }

  return NULL;
}

IdeGiUnion *
ide_gi_record_get_union (IdeGiRecord *self,
                         guint16      nth)
{
  IdeGiUnion *_union = NULL;
  guint16 n_unions;

  g_return_val_if_fail (self != NULL, NULL);

  if (nth + 1 > (n_unions = ide_gi_record_get_n_unions (self)))
    g_warning ("Union %d is out of bounds (nb unions = %d)", nth, n_unions);
  else
    {
      guint16 offset = ((IdeGiRecordBlob *)self->common_blob)->unions + nth;
      _union = (IdeGiUnion *)ide_gi_union_new (self->ns, IDE_GI_BLOB_TYPE_UNION, offset);
    }

  return _union;
}

IdeGiUnion *
ide_gi_record_lookup_union (IdeGiRecord *self,
                            const gchar *name)
{
  guint16 n_unions;

  g_return_val_if_fail (self != NULL, NULL);
  g_return_val_if_fail (!dzl_str_empty0 (name), NULL);

  n_unions = ide_gi_record_get_n_unions (self);
  for (guint i = 0; i < n_unions; i++)
    {
      const gchar *union_name;
      g_autoptr(IdeGiUnion) _union = ide_gi_record_get_union (self, i);

      if (_union == NULL)
        continue;

      union_name = ide_gi_base_get_name ((IdeGiBase *)_union);
      if (dzl_str_equal0 (union_name, name))
        return g_steal_pointer (&_union);
    }

  return NULL;
}

IdeGiBase *
ide_gi_record_new (IdeGiNamespace *ns,
                   IdeGiBlobType   type,
                   gint32          offset)
{
  IdeGiRecord *self;
  guint8 *table;
  gsize type_size;

  g_return_val_if_fail (ns != NULL, NULL);
  g_return_val_if_fail (offset > -1, NULL);
  g_return_val_if_fail (type != IDE_GI_BLOB_TYPE_UNKNOW, NULL);

  self = g_slice_new0 (IdeGiRecord);
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
ide_gi_record_free (IdeGiBase *base)
{
  IdeGiRecord *self = (IdeGiRecord *)base;

  g_assert (self);
  g_assert_cmpint (self->ref_count, ==, 0);

  ide_gi_namespace_unref (self->ns);
  g_slice_free (IdeGiRecord, self);
}

IdeGiRecord *
ide_gi_record_ref (IdeGiRecord *self)
{
  g_return_val_if_fail (self, NULL);
  g_return_val_if_fail (self->ref_count, NULL);

  g_atomic_int_inc (&self->ref_count);

  return self;
}

void
ide_gi_record_unref (IdeGiRecord *self)
{
  g_return_if_fail (self);
  g_return_if_fail (self->ref_count);

  if (g_atomic_int_dec_and_test (&self->ref_count))
    ide_gi_record_free ((IdeGiBase *)self);
}
