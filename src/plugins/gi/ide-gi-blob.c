/* ide-gi-blob.c
 *
 * Copyright 2018 Sebastien Lafargue <slafargue@gnome.org>
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
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "objects/ide-gi-alias.h"
#include "objects/ide-gi-array.h"
#include "objects/ide-gi-callback.h"
#include "objects/ide-gi-class.h"
#include "objects/ide-gi-constant.h"
#include "objects/ide-gi-enum.h"
#include "objects/ide-gi-field.h"
#include "objects/ide-gi-function.h"
#include "objects/ide-gi-interface.h"
#include "objects/ide-gi-parameter.h"
#include "objects/ide-gi-property.h"
#include "objects/ide-gi-record.h"
#include "objects/ide-gi-signal.h"
#include "objects/ide-gi-type.h"
#include "objects/ide-gi-union.h"
#include "objects/ide-gi-value.h"

#include "ide-gi-blob.h"

/* Sync with IdeGiBlobType and IdeGiNsTable enum in ide-gi-blob.h */
static IdeGiBlobTypeInfo IDE_GI_BLOB_TYPE_INFO [] =
{
  { "unknow",      NULL,                 NULL,                        0,                           0},                         // IDE_GI_BLOB_TYPE_UNKNOW
  { "alias",       ide_gi_alias_new,     ide_gi_alias_free,           sizeof (IdeGiAliasBlob),     IDE_GI_NS_TABLE_ALIAS},     // IDE_GI_BLOB_TYPE_ALIAS
  { "array",       ide_gi_array_new,     ide_gi_array_free,           sizeof (IdeGiArrayBlob),     IDE_GI_NS_TABLE_ARRAY},     // IDE_GI_BLOB_TYPE_ARRAY
  { "boxed",       NULL,                 NULL,                        sizeof (IdeGiRecordBlob),    IDE_GI_NS_TABLE_RECORD},    // IDE_GI_BLOB_TYPE_BOXED
  { "callback",    ide_gi_callback_new,  ide_gi_callback_free,        sizeof (IdeGiCallbackBlob),  IDE_GI_NS_TABLE_CALLBACK},  // IDE_GI_BLOB_TYPE_CALLBACK
  { "class",       ide_gi_class_new,     ide_gi_class_free,           sizeof (IdeGiObjectBlob),    IDE_GI_NS_TABLE_OBJECT},    // IDE_GI_BLOB_TYPE_CLASS
  { "constant",    ide_gi_constant_new,  ide_gi_constant_free,        sizeof (IdeGiConstantBlob),  IDE_GI_NS_TABLE_CONSTANT},  // IDE_GI_BLOB_TYPE_CONSTANT
  { "constructor", NULL,                 NULL,                        sizeof (IdeGiFunctionBlob),  IDE_GI_NS_TABLE_FUNCTION},  // IDE_GI_BLOB_TYPE_CONSTRUCTOR
  { "doc",         NULL,                 NULL,                        sizeof (IdeGiDocBlob),       IDE_GI_NS_TABLE_DOC},       // IDE_GI_BLOB_TYPE_DOC
  { "enum",        ide_gi_enum_new,      ide_gi_enum_free,            sizeof (IdeGiEnumBlob),      IDE_GI_NS_TABLE_ENUM},      // IDE_GI_BLOB_TYPE_ENUM
  { "field",       ide_gi_field_new,     ide_gi_field_free,           sizeof (IdeGiFieldBlob),     IDE_GI_NS_TABLE_FIELD},     // IDE_GI_BLOB_TYPE_FIELD
  { "function",    ide_gi_function_new,  ide_gi_function_free,        sizeof (IdeGiFunctionBlob),  IDE_GI_NS_TABLE_FUNCTION},  // IDE_GI_BLOB_TYPE_FUNCTION
  { "header",      NULL,                 NULL,                        sizeof (IdeGiHeaderBlob),    IDE_GI_NS_TABLE_UNKNOW},    // IDE_GI_BLOB_TYPE_HEADER
  { "interface",   ide_gi_interface_new, ide_gi_interface_free,       sizeof (IdeGiObjectBlob),    IDE_GI_NS_TABLE_OBJECT},    // IDE_GI_BLOB_TYPE_INTERFACE
  { "method",      NULL,                 NULL,                        sizeof (IdeGiFunctionBlob),  IDE_GI_NS_TABLE_FUNCTION},  // IDE_GI_BLOB_TYPE_METHOD
  { "parameter",   ide_gi_parameter_new, ide_gi_parameter_free,       sizeof (IdeGiParameterBlob), IDE_GI_NS_TABLE_PARAMETER}, // IDE_GI_BLOB_TYPE_PARAMETER
  { "property",    ide_gi_property_new,  ide_gi_property_free,        sizeof (IdeGiPropertyBlob),  IDE_GI_NS_TABLE_PROPERTY},  // IDE_GI_BLOB_TYPE_PROPERTY
  { "record",      ide_gi_record_new,    ide_gi_record_free,          sizeof (IdeGiRecordBlob),    IDE_GI_NS_TABLE_RECORD},    // IDE_GI_BLOB_TYPE_RECORD
  { "signal",      ide_gi_signal_new,    ide_gi_signal_free,          sizeof (IdeGiSignalBlob),    IDE_GI_NS_TABLE_SIGNAL},    // IDE_GI_BLOB_TYPE_SIGNAL
  { "type",        ide_gi_type_new,      ide_gi_type_free,            sizeof (IdeGiTypeBlob),      IDE_GI_NS_TABLE_TYPE},      // IDE_GI_BLOB_TYPE_TYPE
  { "union",       ide_gi_union_new,     ide_gi_union_free,           sizeof (IdeGiUnionBlob),     IDE_GI_NS_TABLE_UNION},     // IDE_GI_BLOB_TYPE_UNION
  { "value",       ide_gi_value_new,     ide_gi_value_free,           sizeof (IdeGiValueBlob),     IDE_GI_NS_TABLE_VALUE},     // IDE_GI_BLOB_TYPE_VALUE
  { "vfunc",       NULL,                 NULL,                        sizeof (IdeGiFunctionBlob),  IDE_GI_NS_TABLE_FUNCTION},  // IDE_GI_BLOB_TYPE_VFUNC
};

const gchar *
ide_gi_blob_get_name (IdeGiBlobType type)
{
  g_return_val_if_fail (type != IDE_GI_BLOB_TYPE_UNKNOW, NULL);

  return IDE_GI_BLOB_TYPE_INFO[type].name;
}

IdeGiObjectConstructor
ide_gi_blob_get_constructor (IdeGiBlobType type)
{
  g_return_val_if_fail (type != IDE_GI_BLOB_TYPE_UNKNOW, NULL);

  return IDE_GI_BLOB_TYPE_INFO[type].constructor;
}

IdeGiObjectDestructor
ide_gi_blob_get_destructor (IdeGiBlobType type)
{
  g_return_val_if_fail (type != IDE_GI_BLOB_TYPE_UNKNOW, NULL);

  return IDE_GI_BLOB_TYPE_INFO[type].destructor;
}

gsize
ide_gi_blob_get_size (IdeGiBlobType type)
{
  g_return_val_if_fail (type != IDE_GI_BLOB_TYPE_UNKNOW, 0);

  return IDE_GI_BLOB_TYPE_INFO[type].blob_size;
}

IdeGiNsTable
ide_gi_blob_get_ns_table (IdeGiBlobType type)
{
  g_assert (type != IDE_GI_BLOB_TYPE_UNKNOW);

  return IDE_GI_BLOB_TYPE_INFO[type].ns_table;
}
