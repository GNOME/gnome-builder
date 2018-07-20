/* ide-gi-interface.c
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

#include <stdio.h>

#include "./../ide-gi-utils.h"
#include "./../ide-gi-version-private.h"

#include "ide-gi-class.h"

#include "ide-gi-interface.h"

G_DEFINE_BOXED_TYPE (IdeGiInterface,
                     ide_gi_interface,
                     ide_gi_interface_ref,
                     ide_gi_interface_unref)

void
ide_gi_interface_dump (IdeGiInterface *self,
                       guint           depth)
{
  guint n_callbacks;
  guint n_constants;
  guint n_fields;
  guint n_functions;
  guint n_properties;
  guint n_prerequisites;
  guint n_signals;

  g_return_if_fail (self != NULL);

  /* TODO: parent */

  ide_gi_base_dump ((IdeGiBase *)self);

  g_print ("g_type_name:%s\n", ide_gi_interface_get_g_type_name (self));
  g_print ("g_get_type:%s\n", ide_gi_interface_get_g_get_type (self));
  g_print ("c_type:%s\n", ide_gi_interface_get_c_type (self));
  g_print ("c_symbol_prefix:%s\n", ide_gi_interface_get_c_symbol_prefix (self));

  n_callbacks = ide_gi_interface_get_n_callbacks (self);
  n_constants = ide_gi_interface_get_n_constants (self);
  n_fields = ide_gi_interface_get_n_fields (self);
  n_functions = ide_gi_interface_get_n_functions (self);
  n_properties = ide_gi_interface_get_n_properties (self);
  n_prerequisites = ide_gi_interface_get_n_prerequisites (self);
  n_signals = ide_gi_interface_get_n_signals (self);

  g_print ("nb callbacks:%d\n", n_callbacks);
  g_print ("nb constants:%d\n", n_constants);
  g_print ("nb fields:%d\n", n_fields);
  g_print ("nb functions:%d\n", n_functions);
  g_print ("nb properties:%d\n", n_properties);
  g_print ("nb prerequisite:%d\n", n_prerequisites);
  g_print ("nb signals:%d\n", n_signals);

  if (depth > 0)
    {
      for (guint i = 0; i < n_callbacks; i++)
        ide_gi_callback_dump (ide_gi_interface_get_callback (self, i), depth - 1);

      for (guint i = 0; i < n_constants; i++)
        ide_gi_constant_dump (ide_gi_interface_get_constant (self, i), depth - 1);

      for (guint i = 0; i < n_fields; i++)
        ide_gi_field_dump (ide_gi_interface_get_field (self, i), depth - 1);

      for (guint i = 0; i < n_functions; i++)
        ide_gi_function_dump (ide_gi_interface_get_function (self, i), depth - 1);

      for (guint i = 0; i < n_properties; i++)
        ide_gi_property_dump (ide_gi_interface_get_property (self, i), depth - 1);

      for (guint i = 0; i < n_prerequisites; i++)
        {
          IdeGiBase *object = ide_gi_interface_get_prerequisite (self, i);
          IdeGiBlobType type = ide_gi_base_get_object_type (object);

          if (type == IDE_GI_BLOB_TYPE_CLASS)
            ide_gi_class_dump ((IdeGiClass *)object, depth -1);
          else if (type == IDE_GI_BLOB_TYPE_INTERFACE)
            ide_gi_interface_dump ((IdeGiInterface *)object, depth -1);
          else
            g_warning ("wrong type to dump from prerequisite: %s", ide_gi_utils_blob_type_to_string (type));
        }

      for (guint i = 0; i < n_signals; i++)
        ide_gi_signal_dump (ide_gi_interface_get_signal (self, i), depth - 1);
    }
}

const gchar *
ide_gi_interface_get_g_type_name (IdeGiInterface *self)
{
  guint32 offset;

  g_return_val_if_fail (self != NULL, NULL);

  offset = ((IdeGiObjectBlob *)self->common_blob)->g_type_name;
  return ide_gi_namespace_get_string (self->ns, offset);
}

const gchar *
ide_gi_interface_get_g_get_type (IdeGiInterface *self)
{
  guint32 offset;

  g_return_val_if_fail (self != NULL, NULL);

  offset = ((IdeGiObjectBlob *)self->common_blob)->g_get_type;
  return ide_gi_namespace_get_string (self->ns, offset);
}

const gchar *
ide_gi_interface_get_c_type (IdeGiInterface *self)
{
  guint32 offset;

  g_return_val_if_fail (self != NULL, NULL);

  offset = ((IdeGiObjectBlob *)self->common_blob)->c_type;
  return ide_gi_namespace_get_string (self->ns, offset);
}

const gchar *
ide_gi_interface_get_c_symbol_prefix (IdeGiInterface *self)
{
  guint32 offset;

  g_return_val_if_fail (self != NULL, NULL);

  offset = ((IdeGiObjectBlob *)self->common_blob)->c_symbol_prefix;
  return ide_gi_namespace_get_string (self->ns, offset);
}

guint16
ide_gi_interface_get_n_prerequisites (IdeGiInterface *self)
{
  g_return_val_if_fail (self != NULL, 0);

  return ((IdeGiObjectBlob *)self->common_blob)->n_interfaces;
}

guint16
ide_gi_interface_get_n_callbacks (IdeGiInterface *self)
{
  g_return_val_if_fail (self != NULL, 0);

  return ((IdeGiObjectBlob *)self->common_blob)->n_callbacks;
}

guint16
ide_gi_interface_get_n_constants (IdeGiInterface *self)
{
  g_return_val_if_fail (self != NULL, 0);

  return ((IdeGiObjectBlob *)self->common_blob)->n_constants;
}

guint16
ide_gi_interface_get_n_fields (IdeGiInterface *self)
{
  g_return_val_if_fail (self != NULL, 0);

  return ((IdeGiObjectBlob *)self->common_blob)->n_fields;
}

guint16
ide_gi_interface_get_n_functions (IdeGiInterface *self)
{
  g_return_val_if_fail (self != NULL, 0);

  return ((IdeGiObjectBlob *)self->common_blob)->n_functions;
}

guint16
ide_gi_interface_get_n_properties (IdeGiInterface *self)
{
  g_return_val_if_fail (self != NULL, 0);

  return ((IdeGiObjectBlob *)self->common_blob)->n_properties;
}

guint16
ide_gi_interface_get_n_signals (IdeGiInterface *self)
{
  g_return_val_if_fail (self != NULL, 0);

  return ((IdeGiObjectBlob *)self->common_blob)->n_signals;
}

IdeGiCallback *
ide_gi_interface_get_callback (IdeGiInterface *self,
                               guint16         nth)
{
  IdeGiCallback *callback = NULL;
  guint16 n_callbacks;

  g_return_val_if_fail (self != NULL, NULL);

  if (nth + 1 > (n_callbacks = ide_gi_interface_get_n_callbacks (self)))
    g_warning ("Callback %d is out of bounds (nb callbacks = %d)", nth, n_callbacks);
  else
    {
      guint16 offset = ((IdeGiObjectBlob *)self->common_blob)->callbacks + nth;
      callback = (IdeGiCallback *)ide_gi_callback_new (self->ns, IDE_GI_BLOB_TYPE_CALLBACK, offset);
    }

  return callback;
}

IdeGiCallback *
ide_gi_interface_lookup_callback  (IdeGiInterface *self,
                                   const gchar    *name)
{
  guint16 n_callbacks;

  g_return_val_if_fail (self != NULL, NULL);
  g_return_val_if_fail (!dzl_str_empty0 (name), NULL);

  n_callbacks = ide_gi_interface_get_n_callbacks (self);
  for (guint i = 0; i < n_callbacks; i++)
    {
      const gchar *callback_name;
      g_autoptr(IdeGiCallback) callback = ide_gi_interface_get_callback (self, i);

      if (callback == NULL)
        continue;

      callback_name = ide_gi_base_get_name ((IdeGiBase *)callback);
      if (dzl_str_equal0 (callback_name, name))
        return g_steal_pointer (&callback);
    }

  return NULL;
}

IdeGiConstant *
ide_gi_interface_get_constant (IdeGiInterface *self,
                               guint16         nth)
{
  IdeGiConstant *constant = NULL;
  guint16 n_constants;

  g_return_val_if_fail (self != NULL, NULL);

  if (nth + 1 > (n_constants = ide_gi_interface_get_n_constants (self)))
    g_warning ("Constant %d is out of bounds (nb constants = %d)", nth, n_constants);
  else
    {
      guint16 offset = ((IdeGiObjectBlob *)self->common_blob)->constants + nth;
      constant = (IdeGiConstant *)ide_gi_constant_new (self->ns, IDE_GI_BLOB_TYPE_CONSTANT, offset);
    }

  return constant;
}

IdeGiConstant *
ide_gi_interface_lookup_constant (IdeGiInterface *self,
                                  const gchar    *name)
{
  guint16 n_constants;

  g_return_val_if_fail (self != NULL, NULL);
  g_return_val_if_fail (!dzl_str_empty0 (name), NULL);

  n_constants = ide_gi_interface_get_n_constants (self);
  for (guint i = 0; i < n_constants; i++)
    {
      const gchar *constant_name;
      g_autoptr(IdeGiConstant) constant = ide_gi_interface_get_constant (self, i);

      if (constant == NULL)
        continue;

      constant_name = ide_gi_base_get_name ((IdeGiBase *)constant);
      if (dzl_str_equal0 (constant_name, name))
        return g_steal_pointer (&constant);
    }

  return NULL;
}

IdeGiField *
ide_gi_interface_get_field (IdeGiInterface *self,
                            guint16         nth)
{
  IdeGiField *field = NULL;
  guint16 n_fields;

  g_return_val_if_fail (self != NULL, NULL);

  if (nth + 1 > (n_fields = ide_gi_interface_get_n_fields (self)))
    g_warning ("Field %d is out of bounds (nb fields = %d)", nth, n_fields);
  else
    {
      guint16 offset = ((IdeGiObjectBlob *)self->common_blob)->fields + nth;
      field = (IdeGiField *)ide_gi_field_new (self->ns, IDE_GI_BLOB_TYPE_FIELD, offset);
    }

  return field;
}

IdeGiField *
ide_gi_interface_lookup_field (IdeGiInterface *self,
                               const gchar    *name)
{
  guint16 n_fields;

  g_return_val_if_fail (self != NULL, NULL);
  g_return_val_if_fail (!dzl_str_empty0 (name), NULL);

  n_fields = ide_gi_interface_get_n_fields (self);
  for (guint i = 0; i < n_fields; i++)
    {
      const gchar *field_name;
      g_autoptr(IdeGiField) field = ide_gi_interface_get_field (self, i);

      if (field == NULL)
        continue;

      field_name = ide_gi_base_get_name ((IdeGiBase *)field);
      if (dzl_str_equal0 (field_name, name))
        return g_steal_pointer (&field);
    }

  return NULL;
}

IdeGiFunction *
ide_gi_interface_get_function (IdeGiInterface *self,
                               guint16         nth)
{
  IdeGiFunction *function = NULL;
  guint16 n_functions;

  g_return_val_if_fail (self != NULL, NULL);

  if (nth + 1 > (n_functions = ide_gi_interface_get_n_functions (self)))
    g_warning ("Function %d is out of bounds (nb functions = %d)", nth, n_functions);
  else
    {
      guint16 offset = ((IdeGiObjectBlob *)self->common_blob)->functions + nth;
      function = (IdeGiFunction  *)ide_gi_function_new (self->ns, IDE_GI_BLOB_TYPE_FUNCTION, offset);
    }

  return function;
}

IdeGiFunction *
ide_gi_interface_lookup_function (IdeGiInterface *self,
                                  const gchar    *name)
{
  guint16 n_functions;

  g_return_val_if_fail (self != NULL, NULL);
  g_return_val_if_fail (!dzl_str_empty0 (name), NULL);

  n_functions = ide_gi_interface_get_n_functions (self);
  for (guint i = 0; i < n_functions; i++)
    {
      const gchar *function_name;
      g_autoptr(IdeGiFunction) function = ide_gi_interface_get_function (self, i);

      if (function == NULL)
        continue;

      function_name = ide_gi_base_get_name ((IdeGiBase *)function);
      if (dzl_str_equal0 (function_name, name))
        return g_steal_pointer (&function);
    }

  return NULL;
}

IdeGiBase *
ide_gi_interface_get_prerequisite (IdeGiInterface *self,
                                   guint16         nth)
{
  guint16 n_prerequisite;
  guint16 base_offset;
  IdeGiCrossRef *crossref;

  g_return_val_if_fail (self != NULL, NULL);

  if (nth + 1 > (n_prerequisite = ide_gi_interface_get_n_prerequisites (self)))
    {
      g_warning ("Prerequisite %d is out of bounds (nb prerequisite = %d)", nth, n_prerequisite);
      return NULL;
    }

  base_offset = ((IdeGiObjectBlob *)self->common_blob)->interfaces;
  crossref = _ide_gi_namespace_get_crossref (self->ns, base_offset + nth);
  if (!crossref->is_resolved)
    {
      if (!crossref->is_local)
        {
          /* TODO: try use ns req to resolve */
          ;
        }

      return NULL;
    }

  if (crossref->is_local)
    {
      g_assert (crossref->type == IDE_GI_BLOB_TYPE_INTERFACE ||
                crossref->type == IDE_GI_BLOB_TYPE_CLASS);

      return ide_gi_base_new (self->ns, crossref->type, crossref->offset);
    }
  else
    {
      IdeGiVersion *version;
      const gchar *qname;
      IdeGiBase *object;
      IdeGiBlobType type;

      version = ide_gi_namespace_get_repository_version (self->ns);
      qname = ide_gi_namespace_get_string (self->ns, crossref->qname);
      if (NULL == (object = ide_gi_version_lookup_root_object (version,
                                                               qname,
                                                               crossref->ns_major_version,
                                                               crossref->ns_minor_version)))
        return NULL;

      type = ide_gi_base_get_object_type (object);
      g_assert (type == IDE_GI_BLOB_TYPE_INTERFACE ||type == IDE_GI_BLOB_TYPE_CLASS);

      return object;
    }

  return NULL;
}

IdeGiBase *
ide_gi_interface_lookup_prerequisite (IdeGiInterface *self,
                                      const gchar    *name)
{
  guint16 n_prerequisites;

  g_return_val_if_fail (self != NULL, NULL);
  g_return_val_if_fail (!dzl_str_empty0 (name), NULL);

  n_prerequisites = ide_gi_interface_get_n_prerequisites (self);
  for (guint i = 0; i < n_prerequisites; i++)
    {
      const gchar *prerequisite_name;
      g_autoptr(IdeGiBase) prerequisite = ide_gi_interface_get_prerequisite (self, i);

      if (prerequisite == NULL)
        continue;

      prerequisite_name = ide_gi_base_get_name ((IdeGiBase *)prerequisite);
      if (dzl_str_equal0 (prerequisite_name, name))
        return g_steal_pointer (&prerequisite);
    }

  return NULL;
}

IdeGiProperty *
ide_gi_interface_get_property (IdeGiInterface *self,
                               guint16         nth)
{
  IdeGiProperty *property = NULL;
  guint16 n_properties;

  g_return_val_if_fail (self != NULL, NULL);

  if (nth + 1 > (n_properties = ide_gi_interface_get_n_properties (self)))
    g_warning ("Property %d is out of bounds (nb properties = %d)", nth, n_properties);
  else
    {
      guint16 offset = ((IdeGiObjectBlob *)self->common_blob)->properties + nth;
      property = (IdeGiProperty *)ide_gi_property_new (self->ns, IDE_GI_BLOB_TYPE_PROPERTY, offset);
    }

  return property;
}

IdeGiProperty *
ide_gi_interface_lookup_property (IdeGiInterface *self,
                                  const gchar    *name)
{
  guint16 n_properties;

  g_return_val_if_fail (self != NULL, NULL);
  g_return_val_if_fail (!dzl_str_empty0 (name), NULL);

  n_properties = ide_gi_interface_get_n_properties (self);
  for (guint i = 0; i < n_properties; i++)
    {
      const gchar *property_name;
      g_autoptr(IdeGiProperty) property = ide_gi_interface_get_property (self, i);

      if (property == NULL)
        continue;

      property_name = ide_gi_base_get_name ((IdeGiBase *)property);
      if (dzl_str_equal0 (property_name, name))
        return g_steal_pointer (&property);
    }

  return NULL;
}

IdeGiSignal *
ide_gi_interface_get_signal (IdeGiInterface *self,
                             guint16         nth)
{
  IdeGiSignal *signal = NULL;
  guint16 n_signals;

  g_return_val_if_fail (self != NULL, NULL);

  if (nth + 1 > (n_signals = ide_gi_interface_get_n_signals (self)))
    g_warning ("Signal %d is out of bounds (nb signals = %d)", nth, n_signals);
  else
    {
      guint16 offset = ((IdeGiObjectBlob *)self->common_blob)->signals + nth;
      signal = (IdeGiSignal *)ide_gi_signal_new (self->ns, IDE_GI_BLOB_TYPE_SIGNAL, offset);
    }

  return signal;
}

IdeGiSignal *
ide_gi_interface_lookup_signal  (IdeGiInterface *self,
                                 const gchar    *name)
{
  guint16 n_signals;

  g_return_val_if_fail (self != NULL, NULL);
  g_return_val_if_fail (!dzl_str_empty0 (name), NULL);

  n_signals = ide_gi_interface_get_n_signals (self);
  for (guint i = 0; i < n_signals; i++)
    {
      const gchar *signal_name;
      g_autoptr(IdeGiSignal) signal = ide_gi_interface_get_signal (self, i);

      if (signal == NULL)
        continue;

      signal_name = ide_gi_base_get_name ((IdeGiBase *)signal);
      if (dzl_str_equal0 (signal_name, name))
        return g_steal_pointer (&signal);
    }

  return NULL;
}

IdeGiBase *
ide_gi_interface_new (IdeGiNamespace *ns,
                      IdeGiBlobType   type,
                      gint32          offset)
{
  IdeGiInterface *self;
  guint8 *table;
  gsize type_size;

  g_return_val_if_fail (ns != NULL, NULL);
  g_return_val_if_fail (offset > -1, NULL);
  g_return_val_if_fail (type != IDE_GI_BLOB_TYPE_UNKNOW, NULL);

  self = g_slice_new0 (IdeGiInterface);
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
ide_gi_interface_free (IdeGiBase *base)
{
  IdeGiInterface *self = (IdeGiInterface *)base;

  g_assert (self);
  g_assert_cmpint (self->ref_count, ==, 0);

  ide_gi_namespace_unref (self->ns);
  g_slice_free (IdeGiInterface, self);
}

IdeGiInterface *
ide_gi_interface_ref (IdeGiInterface *self)
{
  g_return_val_if_fail (self, NULL);
  g_return_val_if_fail (self->ref_count, NULL);

  g_atomic_int_inc (&self->ref_count);

  return self;
}

void
ide_gi_interface_unref (IdeGiInterface *self)
{
  g_return_if_fail (self);
  g_return_if_fail (self->ref_count);

  if (g_atomic_int_dec_and_test (&self->ref_count))
    ide_gi_interface_free ((IdeGiBase *)self);
}
