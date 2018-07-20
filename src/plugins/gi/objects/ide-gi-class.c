/* ide-gi-class.c
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

#include "./../ide-gi-version-private.h"

#include "ide-gi-class.h"

G_DEFINE_BOXED_TYPE (IdeGiClass,
                     ide_gi_class,
                     ide_gi_class_ref,
                     ide_gi_class_unref)

void
ide_gi_class_dump (IdeGiClass *self,
                   guint       depth)
{
  guint n_callbacks;
  guint n_constants;
  guint n_fields;
  guint n_functions;
  guint n_interfaces;
  guint n_properties;
  guint n_records;
  guint n_signals;
  guint n_unions;

  g_return_if_fail (self != NULL);

  ide_gi_base_dump ((IdeGiBase *)self);

  g_print ("is abstract:%d\n", ide_gi_class_is_abstract (self));
  g_print ("is fundamental:%d\n", ide_gi_class_is_fundamental (self));

  g_print ("g_type_name:%s\n", ide_gi_class_get_g_type_name (self));
  g_print ("g_get_type:%s\n", ide_gi_class_get_g_get_type (self));
  g_print ("g_type_struct:%s\n", ide_gi_class_get_g_type_struct (self));
  g_print ("g_ref_func:%s\n", ide_gi_class_get_g_ref_func (self));
  g_print ("g_unref_func:%s\n", ide_gi_class_get_g_unref_func (self));
  g_print ("g_set_value_func:%s\n", ide_gi_class_get_g_set_value_func (self));
  g_print ("g_get_value_func:%s\n", ide_gi_class_get_g_get_value_func (self));
  g_print ("c_type:%s\n", ide_gi_class_get_c_type (self));
  g_print ("c_symbol_prefix:%s\n", ide_gi_class_get_c_symbol_prefix (self));

  n_callbacks = ide_gi_class_get_n_callbacks (self);
  n_constants = ide_gi_class_get_n_constants (self);
  n_fields = ide_gi_class_get_n_fields (self);
  n_functions = ide_gi_class_get_n_functions (self);
  n_interfaces = ide_gi_class_get_n_interfaces (self);
  n_properties = ide_gi_class_get_n_properties (self);
  n_records = ide_gi_class_get_n_records (self);
  n_signals = ide_gi_class_get_n_signals (self);
  n_unions = ide_gi_class_get_n_unions (self);

  g_print ("nb callbacks:%d\n", n_callbacks);
  g_print ("nb constants:%d\n", n_constants);
  g_print ("nb fields:%d\n", n_fields);
  g_print ("nb functions:%d\n", n_functions);
  g_print ("nb interfaces:%d\n", n_interfaces);
  g_print ("nb properties:%d\n", n_properties);
  g_print ("nb records:%d\n", n_records);
  g_print ("nb signals:%d\n", n_signals);
  g_print ("nb unions:%d\n", n_unions);

  if (depth > 0)
    {
      for (guint i = 0; i < n_callbacks; i++)
        ide_gi_callback_dump (ide_gi_class_get_callback (self, i), depth - 1);

      for (guint i = 0; i < n_constants; i++)
        ide_gi_constant_dump (ide_gi_class_get_constant (self, i), depth - 1);

      for (guint i = 0; i < n_fields; i++)
        ide_gi_field_dump (ide_gi_class_get_field (self, i), depth - 1);

      for (guint i = 0; i < n_functions; i++)
        ide_gi_function_dump (ide_gi_class_get_function (self, i), depth - 1);

      for (guint i = 0; i < n_interfaces; i++)
        ide_gi_interface_dump (ide_gi_class_get_interface (self, i), depth - 1);

      for (guint i = 0; i < n_properties; i++)
        ide_gi_property_dump (ide_gi_class_get_property (self, i), depth - 1);

      for (guint i = 0; i < n_records; i++)
        ide_gi_record_dump (ide_gi_class_get_record (self, i), depth - 1);

      for (guint i = 0; i < n_signals; i++)
        ide_gi_signal_dump (ide_gi_class_get_signal (self, i), depth - 1);

      for (guint i = 0; i < n_unions; i++)
        ide_gi_union_dump (ide_gi_class_get_union (self, i), depth - 1);
    }
}

gboolean
ide_gi_class_is_abstract (IdeGiClass *self)
{
  g_return_val_if_fail (self != NULL, FALSE);

  return ((IdeGiObjectBlob *)self->common_blob)->abstract;
}

gboolean
ide_gi_class_is_fundamental (IdeGiClass *self)
{
  g_return_val_if_fail (self != NULL, FALSE);

  return ((IdeGiObjectBlob *)self->common_blob)->fundamental;
}

const gchar *
ide_gi_class_get_g_type_name (IdeGiClass *self)
{
  guint32 offset;

  g_return_val_if_fail (self != NULL, NULL);

  offset = ((IdeGiObjectBlob *)self->common_blob)->g_type_name;
  return ide_gi_namespace_get_string (self->ns, offset);
}

const gchar *
ide_gi_class_get_g_get_type (IdeGiClass *self)
{
  guint32 offset;

  g_return_val_if_fail (self != NULL, NULL);

  offset = ((IdeGiObjectBlob *)self->common_blob)->g_get_type;
  return ide_gi_namespace_get_string (self->ns, offset);
}

const gchar *
ide_gi_class_get_g_type_struct (IdeGiClass *self)
{
  guint32 offset;

  g_return_val_if_fail (self != NULL, NULL);

  offset = ((IdeGiObjectBlob *)self->common_blob)->g_type_struct;
  return ide_gi_namespace_get_string (self->ns, offset);
}

const gchar *
ide_gi_class_get_g_ref_func (IdeGiClass *self)
{
  guint32 offset;

  g_return_val_if_fail (self != NULL, NULL);

  offset = ((IdeGiObjectBlob *)self->common_blob)->g_ref_func;
  return ide_gi_namespace_get_string (self->ns, offset);
}

const gchar *
ide_gi_class_get_g_unref_func (IdeGiClass *self)
{
  guint32 offset;

  g_return_val_if_fail (self != NULL, NULL);

  offset = ((IdeGiObjectBlob *)self->common_blob)->g_unref_func;
  return ide_gi_namespace_get_string (self->ns, offset);
}

const gchar *
ide_gi_class_get_g_set_value_func (IdeGiClass *self)
{
  guint32 offset;

  g_return_val_if_fail (self != NULL, NULL);

  offset = ((IdeGiObjectBlob *)self->common_blob)->g_set_value_func;
  return ide_gi_namespace_get_string (self->ns, offset);
}

const gchar *
ide_gi_class_get_g_get_value_func (IdeGiClass *self)
{
  guint32 offset;

  g_return_val_if_fail (self != NULL, NULL);

  offset = ((IdeGiObjectBlob *)self->common_blob)->g_get_value_func;
  return ide_gi_namespace_get_string (self->ns, offset);
}

const gchar *
ide_gi_class_get_c_type (IdeGiClass *self)
{
  guint32 offset;

  g_return_val_if_fail (self != NULL, NULL);

  offset = ((IdeGiObjectBlob *)self->common_blob)->c_type;
  return ide_gi_namespace_get_string (self->ns, offset);
}

const gchar *
ide_gi_class_get_c_symbol_prefix (IdeGiClass *self)
{
  guint32 offset;

  g_return_val_if_fail (self != NULL, NULL);

  offset = ((IdeGiObjectBlob *)self->common_blob)->c_symbol_prefix;
  return ide_gi_namespace_get_string (self->ns, offset);
}

guint16
ide_gi_class_get_n_interfaces (IdeGiClass *self)
{
  g_return_val_if_fail (self != NULL, 0);

  return ((IdeGiObjectBlob *)self->common_blob)->n_interfaces;
}

guint16
ide_gi_class_get_n_fields (IdeGiClass *self)
{
  g_return_val_if_fail (self != NULL, 0);

  return ((IdeGiObjectBlob *)self->common_blob)->n_fields;
}

guint16
ide_gi_class_get_n_properties (IdeGiClass *self)
{
  g_return_val_if_fail (self != NULL, 0);

  return ((IdeGiObjectBlob *)self->common_blob)->n_properties;
}

guint16
ide_gi_class_get_n_functions (IdeGiClass *self)
{
  g_return_val_if_fail (self != NULL, 0);

  return ((IdeGiObjectBlob *)self->common_blob)->n_functions;
}

guint16
ide_gi_class_get_n_signals (IdeGiClass *self)
{
  g_return_val_if_fail (self != NULL, 0);

  return ((IdeGiObjectBlob *)self->common_blob)->n_signals;
}

guint16
ide_gi_class_get_n_constants (IdeGiClass *self)
{
  g_return_val_if_fail (self != NULL, 0);

  return ((IdeGiObjectBlob *)self->common_blob)->n_constants;
}

guint16
ide_gi_class_get_n_unions (IdeGiClass *self)
{
  g_return_val_if_fail (self != NULL, 0);

  return ((IdeGiObjectBlob *)self->common_blob)->n_unions;
}

guint16
ide_gi_class_get_n_records (IdeGiClass *self)
{
  g_return_val_if_fail (self != NULL, 0);

  return ((IdeGiObjectBlob *)self->common_blob)->n_records;
}

guint16
ide_gi_class_get_n_callbacks (IdeGiClass *self)
{
  g_return_val_if_fail (self != NULL, 0);

  return ((IdeGiObjectBlob *)self->common_blob)->n_callbacks;
}

static IdeGiBase *
resolve_crossref (IdeGiClass    *self,
                  IdeGiCrossRef *crossref,
                  IdeGiBlobType  type)
{
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
      g_assert (crossref->type == type);
      return ide_gi_base_new (self->ns, type, crossref->offset);
    }
  else
    {
      IdeGiVersion *version;
      const gchar *qname;
      IdeGiBase *object;

      version = ide_gi_namespace_get_repository_version (self->ns);
      qname = ide_gi_namespace_get_string (self->ns, crossref->qname);
      if (NULL == (object = ide_gi_version_lookup_root_object (version,
                                                               qname,
                                                               crossref->ns_major_version,
                                                               crossref->ns_minor_version)))
        return NULL;

      g_assert (ide_gi_base_get_object_type (object) == type);
      return object;
    }
}

IdeGiInterface *
ide_gi_class_get_interface (IdeGiClass *self,
                            guint16     nth)
{
  guint16 n_interfaces;
  guint16 base_offset;
  IdeGiCrossRef *crossref;

  g_return_val_if_fail (self != NULL, NULL);

  if (nth + 1 > (n_interfaces = ide_gi_class_get_n_interfaces (self)))
    {
      g_warning ("Interface %d is out of bounds (nb interfaces = %d)", nth, n_interfaces);
      return NULL;
    }

  base_offset = ((IdeGiObjectBlob *)self->common_blob)->interfaces;
  crossref = _ide_gi_namespace_get_crossref (self->ns, base_offset + nth);

  return (IdeGiInterface *)resolve_crossref (self, crossref, IDE_GI_BLOB_TYPE_INTERFACE);
}

IdeGiInterface *
ide_gi_class_lookup_interface (IdeGiClass  *self,
                               const gchar *name)
{
  guint16 n_interfaces;

  g_return_val_if_fail (self != NULL, NULL);
  g_return_val_if_fail (!dzl_str_empty0 (name), NULL);

  n_interfaces = ide_gi_class_get_n_interfaces (self);
  for (guint i = 0; i < n_interfaces; i++)
    {
      const gchar *iface_name;
      g_autoptr(IdeGiInterface) iface = ide_gi_class_get_interface (self, i);

      if (iface == NULL)
        continue;

      iface_name = ide_gi_base_get_name ((IdeGiBase *)iface);
      if (dzl_str_equal0 (iface_name, name))
        return g_steal_pointer (&iface);
    }

  return NULL;
}

static IdeGiCrossRef *
get_parent_crossref (IdeGiClass *self)
{
  guint16 offset;

  if (!((IdeGiObjectBlob *)self->common_blob)->has_parent)
    return NULL;

  offset = ((IdeGiObjectBlob *)self->common_blob)->parent;
  return _ide_gi_namespace_get_crossref (self->ns, offset);
}

IdeGiClass *
ide_gi_class_get_parent (IdeGiClass *self)
{
  IdeGiCrossRef *crossref;

  g_return_val_if_fail (self != NULL, NULL);

  if ((crossref = get_parent_crossref (self)))
    return (IdeGiClass *)resolve_crossref (self, crossref, IDE_GI_BLOB_TYPE_CLASS);

  return NULL;
}

gboolean
ide_gi_class_has_parent (IdeGiClass *self)
{
  g_return_val_if_fail (self != NULL, FALSE);

  return ((IdeGiObjectBlob *)self->common_blob)->has_parent;
}

const gchar *
ide_gi_class_get_parent_qname (IdeGiClass *self)
{
  IdeGiCrossRef *crossref;

  g_return_val_if_fail (self != NULL, NULL);

  if (NULL == (crossref = get_parent_crossref (self)))
    return "";

  return ide_gi_namespace_get_string (self->ns, crossref->qname);
}

IdeGiField *
ide_gi_class_get_field (IdeGiClass *self,
                        guint16     nth)
{
  IdeGiField *field = NULL;
  guint16 n_fields;

  g_return_val_if_fail (self != NULL, NULL);

  if (nth + 1 > (n_fields = ide_gi_class_get_n_fields (self)))
    g_warning ("Field %d is out of bounds (nb fields = %d)", nth, n_fields);
  else
    {
      guint16 offset = ((IdeGiObjectBlob *)self->common_blob)->fields + nth;
      field = (IdeGiField *)ide_gi_field_new (self->ns, IDE_GI_BLOB_TYPE_FIELD, offset);
    }

  return field;
}

IdeGiField *
ide_gi_class_lookup_field (IdeGiClass  *self,
                           const gchar *name)
{
  guint16 n_fields;

  g_return_val_if_fail (self != NULL, NULL);
  g_return_val_if_fail (!dzl_str_empty0 (name), NULL);

  n_fields = ide_gi_class_get_n_fields (self);
  for (guint i = 0; i < n_fields; i++)
    {
      const gchar *field_name;
      g_autoptr(IdeGiField) field = ide_gi_class_get_field (self, i);

      if (field == NULL)
        continue;

      field_name = ide_gi_base_get_name ((IdeGiBase *)field);
      if (dzl_str_equal0 (field_name, name))
        return g_steal_pointer (&field);
    }

  return NULL;
}

IdeGiProperty *
ide_gi_class_get_property (IdeGiClass *self,
                           guint16     nth)
{
  IdeGiProperty *property = NULL;
  guint16 n_properties;

  g_return_val_if_fail (self != NULL, NULL);

  if (nth + 1 > (n_properties = ide_gi_class_get_n_properties (self)))
    g_warning ("Property %d is out of bounds (nb properties = %d)", nth, n_properties);
  else
    {
      guint16 offset = ((IdeGiObjectBlob *)self->common_blob)->properties + nth;
      property = (IdeGiProperty *)ide_gi_property_new (self->ns, IDE_GI_BLOB_TYPE_PROPERTY, offset);
    }

  return property;
}

IdeGiProperty *
ide_gi_class_lookup_property (IdeGiClass  *self,
                              const gchar *name)
{
  guint16 n_properties;

  g_return_val_if_fail (self != NULL, NULL);
  g_return_val_if_fail (!dzl_str_empty0 (name), NULL);

  n_properties = ide_gi_class_get_n_properties (self);
  for (guint i = 0; i < n_properties; i++)
    {
      const gchar *property_name;
      g_autoptr(IdeGiProperty) property = ide_gi_class_get_property (self, i);

      if (property == NULL)
        continue;

      property_name = ide_gi_base_get_name ((IdeGiBase *)property);
      if (dzl_str_equal0 (property_name, name))
        return g_steal_pointer (&property);
    }

  return NULL;
}

IdeGiFunction *
ide_gi_class_get_function (IdeGiClass *self,
                           guint16     nth)
{
  IdeGiFunction *function = NULL;
  guint16 n_functions;

  g_return_val_if_fail (self != NULL, NULL);

  if (nth + 1 > (n_functions = ide_gi_class_get_n_functions (self)))
    g_warning ("Function %d is out of bounds (nb functions = %d)", nth, n_functions);
  else
    {
      guint16 offset = ((IdeGiObjectBlob *)self->common_blob)->functions + nth;
      function = (IdeGiFunction *)ide_gi_function_new (self->ns, IDE_GI_BLOB_TYPE_FUNCTION, offset);
    }

  return function;
}

IdeGiFunction *
ide_gi_class_lookup_function (IdeGiClass  *self,
                              const gchar *name)
{
  guint16 n_functions;

  g_return_val_if_fail (self != NULL, NULL);
  g_return_val_if_fail (!dzl_str_empty0 (name), NULL);

  n_functions = ide_gi_class_get_n_functions (self);
  for (guint i = 0; i < n_functions; i++)
    {
      const gchar *function_name;
      g_autoptr(IdeGiFunction) function = ide_gi_class_get_function (self, i);

      if (function == NULL)
        continue;

      function_name = ide_gi_base_get_name ((IdeGiBase *)function);
      if (dzl_str_equal0 (function_name, name))
        return g_steal_pointer (&function);
    }

  return NULL;
}

IdeGiSignal *
ide_gi_class_get_signal (IdeGiClass *self,
                         guint16     nth)
{
  IdeGiSignal *signal = NULL;
  guint16 n_signals;

  g_return_val_if_fail (self != NULL, NULL);

  if (nth + 1 > (n_signals = ide_gi_class_get_n_signals (self)))
    g_warning ("Signal %d is out of bounds (nb signals = %d)", nth, n_signals);
  else
    {
      guint16 offset = ((IdeGiObjectBlob *)self->common_blob)->signals + nth;
      signal = (IdeGiSignal *)ide_gi_signal_new (self->ns, IDE_GI_BLOB_TYPE_SIGNAL, offset);
    }

  return signal;
}

IdeGiSignal *
ide_gi_class_lookup_signal (IdeGiClass  *self,
                            const gchar *name)
{
  guint16 n_signals;

  g_return_val_if_fail (self != NULL, NULL);
  g_return_val_if_fail (!dzl_str_empty0 (name), NULL);

  n_signals = ide_gi_class_get_n_signals (self);
  for (guint i = 0; i < n_signals; i++)
    {
      const gchar *signal_name;
      g_autoptr(IdeGiSignal) signal = ide_gi_class_get_signal (self, i);

      if (signal == NULL)
        continue;

      signal_name = ide_gi_base_get_name ((IdeGiBase *)signal);
      if (dzl_str_equal0 (signal_name, name))
        return g_steal_pointer (&signal);
    }

  return NULL;
}

IdeGiConstant *
ide_gi_class_get_constant (IdeGiClass *self,
                           guint16     nth)
{
  IdeGiConstant *constant = NULL;
  guint16 n_constants;

  g_return_val_if_fail (self != NULL, NULL);

  if (nth + 1 > (n_constants = ide_gi_class_get_n_constants (self)))
    g_warning ("Constant %d is out of bounds (nb constants = %d)", nth, n_constants);
  else
    {
      guint16 offset = ((IdeGiObjectBlob *)self->common_blob)->constants + nth;
      constant = (IdeGiConstant *)ide_gi_constant_new (self->ns, IDE_GI_BLOB_TYPE_CONSTANT, offset);
    }

  return constant;
}

IdeGiConstant *
ide_gi_class_lookup_constant (IdeGiClass  *self,
                              const gchar *name)
{
  guint16 n_constants;

  g_return_val_if_fail (self != NULL, NULL);
  g_return_val_if_fail (!dzl_str_empty0 (name), NULL);

  n_constants = ide_gi_class_get_n_constants (self);
  for (guint i = 0; i < n_constants; i++)
    {
      const gchar *constant_name;
      g_autoptr(IdeGiConstant) constant = ide_gi_class_get_constant (self, i);

      if (constant == NULL)
        continue;

      constant_name = ide_gi_base_get_name ((IdeGiBase *)constant);
      if (dzl_str_equal0 (constant_name, name))
        return g_steal_pointer (&constant);
    }

  return NULL;
}

IdeGiUnion *
ide_gi_class_get_union (IdeGiClass *self,
                        guint16     nth)
{
  IdeGiUnion *_union = NULL;
  guint16 n_unions;

  g_return_val_if_fail (self != NULL, NULL);

  if (nth + 1 > (n_unions = ide_gi_class_get_n_unions (self)))
    g_warning ("Union %d is out of bounds (nb unions = %d)", nth, n_unions);
  else
    {
      guint16 offset = ((IdeGiObjectBlob *)self->common_blob)->unions + nth;
      _union = (IdeGiUnion *)ide_gi_union_new (self->ns, IDE_GI_BLOB_TYPE_UNION, offset);
    }

  return _union;
}

IdeGiUnion *
ide_gi_class_lookup_union (IdeGiClass  *self,
                           const gchar *name)
{
  guint16 n_unions;

  g_return_val_if_fail (self != NULL, NULL);
  g_return_val_if_fail (!dzl_str_empty0 (name), NULL);

  n_unions = ide_gi_class_get_n_unions (self);
  for (guint i = 0; i < n_unions; i++)
    {
      const gchar *union_name;
      g_autoptr(IdeGiUnion) _union = ide_gi_class_get_union (self, i);

      if (_union == NULL)
        continue;

      union_name = ide_gi_base_get_name ((IdeGiBase *)_union);
      if (dzl_str_equal0 (union_name, name))
        return g_steal_pointer (&_union);
    }

  return NULL;
}

IdeGiRecord *
ide_gi_class_get_record (IdeGiClass *self,
                         guint16     nth)
{
  IdeGiRecord *record = NULL;
  guint16 n_records;

  g_return_val_if_fail (self != NULL, NULL);

  if (nth + 1 > (n_records = ide_gi_class_get_n_records (self)))
    g_warning ("Record %d is out of bounds (nb records = %d)", nth, n_records);
  else
    {
      guint16 offset = ((IdeGiObjectBlob *)self->common_blob)->records + nth;
      record = (IdeGiRecord *)ide_gi_record_new (self->ns, IDE_GI_BLOB_TYPE_RECORD, offset);
    }

  return record;
}

IdeGiRecord *
ide_gi_class_lookup_record (IdeGiClass  *self,
                            const gchar *name)
{
  guint16 n_records;

  g_return_val_if_fail (self != NULL, NULL);
  g_return_val_if_fail (!dzl_str_empty0 (name), NULL);

  n_records = ide_gi_class_get_n_records (self);
  for (guint i = 0; i < n_records; i++)
    {
      const gchar *record_name;
      g_autoptr(IdeGiRecord) record = ide_gi_class_get_record (self, i);

      if (record == NULL)
        continue;

      record_name = ide_gi_base_get_name ((IdeGiBase *)record);
      if (dzl_str_equal0 (record_name, name))
        return g_steal_pointer (&record);
    }

  return NULL;
}

IdeGiCallback *
ide_gi_class_get_callback (IdeGiClass *self,
                           guint16     nth)
{
  IdeGiCallback *callback = NULL;
  guint16 n_callbacks;

  g_return_val_if_fail (self != NULL, NULL);

  if (nth + 1 > (n_callbacks = ide_gi_class_get_n_callbacks (self)))
    g_warning ("Callback %d is out of bounds (nb callbacks = %d)", nth, n_callbacks);
  else
    {
      guint16 offset = ((IdeGiObjectBlob *)self->common_blob)->callbacks + nth;
      callback = (IdeGiCallback *)ide_gi_callback_new (self->ns, IDE_GI_BLOB_TYPE_CALLBACK, offset);
    }

  return callback;
}

IdeGiCallback *
ide_gi_class_lookup_callback (IdeGiClass  *self,
                              const gchar *name)
{
  guint16 n_callbacks;

  g_return_val_if_fail (self != NULL, NULL);
  g_return_val_if_fail (!dzl_str_empty0 (name), NULL);

  n_callbacks = ide_gi_class_get_n_callbacks (self);
  for (guint i = 0; i < n_callbacks; i++)
    {
      const gchar *callback_name;
      g_autoptr(IdeGiCallback) callback = ide_gi_class_get_callback (self, i);

      if (callback == NULL)
        continue;

      callback_name = ide_gi_base_get_name ((IdeGiBase *)callback);
      if (dzl_str_equal0 (callback_name, name))
        return g_steal_pointer (&callback);
    }

  return NULL;
}

IdeGiBase *
ide_gi_class_new (IdeGiNamespace *ns,
                  IdeGiBlobType   type,
                  gint32          offset)
{
  IdeGiClass *self;
  guint8 *table;
  gsize type_size;

  g_return_val_if_fail (ns != NULL, NULL);
  g_return_val_if_fail (offset > -1, NULL);
  g_return_val_if_fail (type != IDE_GI_BLOB_TYPE_UNKNOW, NULL);

  self = g_slice_new0 (IdeGiClass);
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
ide_gi_class_free (IdeGiBase *base)
{
  IdeGiClass *self = (IdeGiClass *)base;

  g_assert (self);
  g_assert_cmpint (self->ref_count, ==, 0);

  ide_gi_namespace_unref (self->ns);
  g_slice_free (IdeGiClass, self);
}

IdeGiClass *
ide_gi_class_ref (IdeGiClass *self)
{
  g_return_val_if_fail (self, NULL);
  g_return_val_if_fail (self->ref_count, NULL);

  g_atomic_int_inc (&self->ref_count);

  return self;
}

void
ide_gi_class_unref (IdeGiClass *self)
{
  g_return_if_fail (self);
  g_return_if_fail (self->ref_count);

  if (g_atomic_int_dec_and_test (&self->ref_count))
    ide_gi_class_free ((IdeGiBase *)self);
}
