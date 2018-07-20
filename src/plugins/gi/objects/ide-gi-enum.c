/* ide-gi-enum.c
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

#include "ide-gi-enum.h"

G_DEFINE_BOXED_TYPE (IdeGiEnum,
                     ide_gi_enum,
                     ide_gi_enum_ref,
                     ide_gi_enum_unref)

void
ide_gi_enum_dump (IdeGiEnum *self,
                  guint      depth)
{
  guint n_functions;
  guint n_values;

  g_return_if_fail (self != NULL);

  ide_gi_base_dump ((IdeGiBase *)self);

  n_functions = ide_gi_enum_get_n_functions (self);
  n_values = ide_gi_enum_get_n_values (self);

  g_print ("c_type:%s\n", ide_gi_enum_get_c_type (self));
  g_print ("g_type_name:%s\n", ide_gi_enum_get_g_type_name (self));
  g_print ("g_get_type:%s\n", ide_gi_enum_get_g_get_type (self));
  g_print ("g_error_domain:%s\n", ide_gi_enum_get_g_error_domain (self));

  g_print ("nb functions:%d\n", n_functions);
  g_print ("nb values:%d\n", n_values);

  if (depth > 0)
    {
      for (guint i = 0; i < n_functions; i++)
        ide_gi_function_dump (ide_gi_enum_get_function (self, i), depth - 1);

      for (guint i = 0; i < n_values; i++)
        ide_gi_value_dump (ide_gi_enum_get_value (self, i), depth - 1);
    }
}

const gchar *
ide_gi_enum_get_c_type (IdeGiEnum *self)
{
  guint32 offset;

  g_return_val_if_fail (self != NULL, FALSE);

  offset = ((IdeGiEnumBlob *)self->common_blob)->c_type;
  return ide_gi_namespace_get_string (self->ns, offset);
}

const gchar *
ide_gi_enum_get_g_type_name (IdeGiEnum *self)
{
  guint32 offset;

  g_return_val_if_fail (self != NULL, FALSE);

  offset = ((IdeGiEnumBlob *)self->common_blob)->g_type_name;
  return ide_gi_namespace_get_string (self->ns, offset);
}

const gchar *
ide_gi_enum_get_g_get_type (IdeGiEnum *self)
{
  guint32 offset;

  g_return_val_if_fail (self != NULL, FALSE);

  offset = ((IdeGiEnumBlob *)self->common_blob)->g_get_type;
  return ide_gi_namespace_get_string (self->ns, offset);
}

const gchar *
ide_gi_enum_get_g_error_domain (IdeGiEnum *self)
{
  guint32 offset;

  g_return_val_if_fail (self != NULL, FALSE);

  offset = ((IdeGiEnumBlob *)self->common_blob)->g_error_domain;
  return ide_gi_namespace_get_string (self->ns, offset);
}

guint16
ide_gi_enum_get_n_functions (IdeGiEnum *self)
{
  g_return_val_if_fail (self != NULL, 0);

  return ((IdeGiEnumBlob *)self->common_blob)->n_functions;
}

guint16
ide_gi_enum_get_n_values (IdeGiEnum *self)
{
  g_return_val_if_fail (self != NULL, 0);

  return ((IdeGiEnumBlob *)self->common_blob)->n_values;
}

IdeGiFunction *
ide_gi_enum_get_function (IdeGiEnum *self,
                          guint16    nth)
{
  IdeGiFunction *function = NULL;
  guint16 n_functions;

  g_return_val_if_fail (self != NULL, NULL);

  if (nth + 1 > (n_functions = ide_gi_enum_get_n_functions (self)))
    g_warning ("Function %d is out of bounds (nb functions = %d)", nth, n_functions);
  else
    {
      guint16 offset = ((IdeGiEnumBlob *)self->common_blob)->functions + nth;
      function = (IdeGiFunction *)ide_gi_function_new (self->ns, IDE_GI_BLOB_TYPE_FUNCTION, offset);
    }

  return function;
}

IdeGiFunction *
ide_gi_enum_lookup_function (IdeGiEnum   *self,
                             const gchar *name)
{
  guint16 n_functions;

  g_return_val_if_fail (self != NULL, NULL);
  g_return_val_if_fail (!dzl_str_empty0 (name), NULL);

  n_functions = ide_gi_enum_get_n_functions (self);
  for (guint i = 0; i < n_functions; i++)
    {
      const gchar *function_name;
      g_autoptr(IdeGiFunction) function = ide_gi_enum_get_function (self, i);

      if (function == NULL)
        continue;

      function_name = ide_gi_base_get_name ((IdeGiBase *)function);
      if (dzl_str_equal0 (function_name, name))
        return g_steal_pointer (&function);
    }

  return NULL;
}

IdeGiValue *
ide_gi_enum_get_value (IdeGiEnum *self,
                       guint16    nth)
{
  IdeGiValue *value = NULL;
  guint16 n_values;

  g_return_val_if_fail (self != NULL, NULL);

  if (nth + 1 > (n_values = ide_gi_enum_get_n_values (self)))
    g_warning ("value %d is out of bounds (nb values = %d)", nth, n_values);
  else
    {
      guint16 offset = ((IdeGiEnumBlob *)self->common_blob)->values + nth;
      value = (IdeGiValue *)ide_gi_value_new (self->ns, IDE_GI_BLOB_TYPE_VALUE, offset);
    }

  return value;
}

IdeGiValue *
ide_gi_enum_lookup_value (IdeGiEnum   *self,
                          const gchar *name)
{
  guint16 n_values;

  g_return_val_if_fail (self != NULL, NULL);
  g_return_val_if_fail (!dzl_str_empty0 (name), NULL);

  n_values = ide_gi_enum_get_n_values (self);
  for (guint i = 0; i < n_values; i++)
    {
      const gchar *value_name;
      g_autoptr(IdeGiValue) value = ide_gi_enum_get_value (self, i);

      if (value == NULL)
        continue;

      value_name = ide_gi_base_get_name ((IdeGiBase *)value);
      if (dzl_str_equal0 (value_name, name))
        return g_steal_pointer (&value);
    }

  return NULL;
}

IdeGiBase *
ide_gi_enum_new (IdeGiNamespace *ns,
                 IdeGiBlobType   type,
                 gint32          offset)
{
  IdeGiEnum *self;
  guint8 *table;
  gsize type_size;

  g_return_val_if_fail (ns != NULL, NULL);
  g_return_val_if_fail (offset > -1, NULL);
  g_return_val_if_fail (type != IDE_GI_BLOB_TYPE_UNKNOW, NULL);

  self = g_slice_new0 (IdeGiEnum);
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
ide_gi_enum_free (IdeGiBase *base)
{
  IdeGiEnum *self = (IdeGiEnum *)base;

  g_assert (self);
  g_assert_cmpint (self->ref_count, ==, 0);

  ide_gi_namespace_unref (self->ns);
  g_slice_free (IdeGiEnum, self);
}

IdeGiEnum *
ide_gi_enum_ref (IdeGiEnum *self)
{
  g_return_val_if_fail (self, NULL);
  g_return_val_if_fail (self->ref_count, NULL);

  g_atomic_int_inc (&self->ref_count);

  return self;
}

void
ide_gi_enum_unref (IdeGiEnum *self)
{
  g_return_if_fail (self);
  g_return_if_fail (self->ref_count);

  if (g_atomic_int_dec_and_test (&self->ref_count))
    ide_gi_enum_free ((IdeGiBase *)self);
}
