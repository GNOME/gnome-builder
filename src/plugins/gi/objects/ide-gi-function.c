/* ide-gi-function.c
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

#include "ide-gi-function.h"

G_DEFINE_BOXED_TYPE (IdeGiFunction,
                     ide_gi_function,
                     ide_gi_function_ref,
                     ide_gi_function_unref)

void
ide_gi_function_dump (IdeGiFunction *self,
                      guint          depth)
{
  guint n_parameters;

  g_return_if_fail (self != NULL);

  ide_gi_base_dump ((IdeGiBase *)self);

  g_print ("setter:%d\n", ide_gi_function_is_setter (self));
  g_print ("getter:%d\n", ide_gi_function_is_getter (self));
  g_print ("throws:%d\n", ide_gi_function_is_throws (self));

  g_print ("c_identifier:%s\n", ide_gi_function_get_c_identifier (self));
  g_print ("shadowed_by:%s\n", ide_gi_function_get_shadowed_by (self));
  g_print ("shadows:%s\n", ide_gi_function_get_shadows (self));
  g_print ("moved_to:%s\n", ide_gi_function_get_moved_to (self));
  g_print ("invoker:%s\n", ide_gi_function_get_invoker (self));

  n_parameters = ide_gi_function_get_n_parameters (self);
  g_print ("n parameters:%d\n", n_parameters);

  if (depth > 0)
    {
      for (guint i = 0; i < n_parameters; i++)
        ide_gi_parameter_dump (ide_gi_function_get_parameter (self, i), depth - 1);
    }
}

gboolean
ide_gi_function_is_setter (IdeGiFunction *self)
{
  g_return_val_if_fail (self != NULL, FALSE);

  return ((IdeGiFunctionBlob *)self->common_blob)->setter;
}

gboolean
ide_gi_function_is_getter (IdeGiFunction *self)
{
  g_return_val_if_fail (self != NULL, FALSE);

  return ((IdeGiFunctionBlob *)self->common_blob)->getter;
}

gboolean
ide_gi_function_is_throws (IdeGiFunction *self)
{
  g_return_val_if_fail (self != NULL, FALSE);

  return ((IdeGiFunctionBlob *)self->common_blob)->throws;
}

const gchar *
ide_gi_function_get_c_identifier (IdeGiFunction *self)
{
  guint32 offset;

  g_return_val_if_fail (self != NULL, NULL);

  offset = ((IdeGiFunctionBlob *)self->common_blob)->c_identifier;
  return ide_gi_namespace_get_string (self->ns, offset);
}

const gchar *
ide_gi_function_get_shadowed_by (IdeGiFunction *self)
{
  guint32 offset;

  g_return_val_if_fail (self != NULL, NULL);

  offset = ((IdeGiFunctionBlob *)self->common_blob)->shadowed_by;
  return ide_gi_namespace_get_string (self->ns, offset);
}

const gchar *
ide_gi_function_get_shadows (IdeGiFunction *self)
{
  guint32 offset;

  g_return_val_if_fail (self != NULL, NULL);

  offset = ((IdeGiFunctionBlob *)self->common_blob)->shadows;
  return ide_gi_namespace_get_string (self->ns, offset);
}

const gchar *
ide_gi_function_get_moved_to (IdeGiFunction *self)
{
  guint32 offset;

  g_return_val_if_fail (self != NULL, NULL);

  offset = ((IdeGiFunctionBlob *)self->common_blob)->moved_to;
  return ide_gi_namespace_get_string (self->ns, offset);
}

const gchar *
ide_gi_function_get_invoker (IdeGiFunction *self)
{
  guint32 offset;

  g_return_val_if_fail (self != NULL, NULL);

  offset = ((IdeGiFunctionBlob *)self->common_blob)->invoker;
  return ide_gi_namespace_get_string (self->ns, offset);
}

guint16
ide_gi_function_get_n_parameters (IdeGiFunction *self)
{
  g_return_val_if_fail (self != NULL, 0);

  return ((IdeGiFunctionBlob *)self->common_blob)->n_parameters;
}

IdeGiParameter *
ide_gi_function_get_parameter (IdeGiFunction *self,
                               guint16        nth)
{
  IdeGiParameter *parameter = NULL;
  guint16 n_parameters;

  g_return_val_if_fail (self != NULL, NULL);

  if (nth + 1 > (n_parameters = ide_gi_function_get_n_parameters (self)))
    g_warning ("Parameter %d is out of bounds (nb parameters = %d)", nth, n_parameters);
  else
    {
      guint16 offset = ((IdeGiFunctionBlob *)self->common_blob)->parameters + nth;
      parameter = (IdeGiParameter *)ide_gi_parameter_new (self->ns, IDE_GI_BLOB_TYPE_PARAMETER, offset);
    }

  return parameter;
}

IdeGiParameter *
ide_gi_function_get_return_value (IdeGiFunction *self)
{
  guint16 offset;

  g_return_val_if_fail (self != NULL, NULL);

  offset = ((IdeGiFunctionBlob *)self->common_blob)->return_value;
  return (IdeGiParameter *)ide_gi_parameter_new (self->ns, IDE_GI_BLOB_TYPE_PARAMETER, offset);
}

IdeGiParameter *
ide_gi_function_lookup_parameter (IdeGiFunction *self,
                                  const gchar   *name)
{
  guint16 n_parameters;

  g_return_val_if_fail (self != NULL, NULL);
  g_return_val_if_fail (!dzl_str_empty0 (name), NULL);

  n_parameters = ide_gi_function_get_n_parameters (self);
  for (guint i = 0; i < n_parameters; i++)
    {
      const gchar *parameter_name;
      g_autoptr(IdeGiParameter) parameter = ide_gi_function_get_parameter (self, i);

      if (parameter == NULL)
        continue;

      parameter_name = ide_gi_base_get_name ((IdeGiBase *)parameter);
      if (dzl_str_equal0 (parameter_name, name))
        return g_steal_pointer (&parameter);
    }

  return NULL;
}

IdeGiBase *
ide_gi_function_new (IdeGiNamespace *ns,
                     IdeGiBlobType   type,
                     gint32          offset)
{
  IdeGiFunction *self;
  guint8 *table;
  gsize type_size;

  g_return_val_if_fail (ns != NULL, NULL);
  g_return_val_if_fail (offset > -1, NULL);
  g_return_val_if_fail (type != IDE_GI_BLOB_TYPE_UNKNOW, NULL);

  self = g_slice_new0 (IdeGiFunction);
  self->ref_count = 1;

  self->ns = ide_gi_namespace_ref (ns);
  /* This is the generic type IDE_GI_BLOB_TYPE_FUNCTION,
   * the real type is in the blob (constructor, function, method or virtual method */
  self->offset = offset;

  table = _ide_gi_namespace_get_table_from_type (ns, type);
  type_size = _ide_gi_namespace_get_blob_size_from_type (ns, type);

NO_CAST_ALIGN_PUSH
  self->common_blob = (IdeGiCommonBlob *)(table + offset * type_size);
NO_CAST_ALIGN_POP

  /* Now we can set the real type (the connsequence is that the corresponding page is mapped in memory */
  self->type = self->common_blob->blob_type;
  return (IdeGiBase *)self;
}

void
ide_gi_function_free (IdeGiBase *base)
{
  IdeGiFunction *self = (IdeGiFunction *)base;

  g_assert (self);
  g_assert_cmpint (self->ref_count, ==, 0);

  ide_gi_namespace_unref (self->ns);
  g_slice_free (IdeGiFunction, self);
}

IdeGiFunction *
ide_gi_function_ref (IdeGiFunction *self)
{
  g_return_val_if_fail (self, NULL);
  g_return_val_if_fail (self->ref_count, NULL);

  g_atomic_int_inc (&self->ref_count);

  return self;
}

void
ide_gi_function_unref (IdeGiFunction *self)
{
  g_return_if_fail (self);
  g_return_if_fail (self->ref_count);

  if (g_atomic_int_dec_and_test (&self->ref_count))
    ide_gi_function_free ((IdeGiBase *)self);
}
