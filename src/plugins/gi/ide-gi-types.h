/* ide-gi-types.h
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

#pragma once

#include <glib.h>
#include <glib-object.h>
#include <ide.h>

#include "ide-gi-macros.h"

G_BEGIN_DECLS

typedef union _IdeGiNamespaceId IdeGiNamespaceId;

#define IDE_TYPE_GI_REQUIRE (ide_gi_require_get_type())
typedef struct _IdeGiRequire IdeGiRequire;

#define IDE_TYPE_GI_NAMESPACE (ide_gi_namespace_get_type())
typedef struct _IdeGiNamespace IdeGiNamespace;

#define IDE_TYPE_GI_BASE (ide_gi_base_get_type())
typedef struct _IdeGiBase IdeGiBase;

#define IDE_TYPE_GI_ALIAS (ide_gi_alias_get_type())
typedef struct _IdeGiAlias IdeGiAlias;

#define IDE_TYPE_GI_ARRAY (ide_gi_array_get_type())
typedef struct _IdeGiArray IdeGiArray;

#define IDE_TYPE_GI_CALLBACK (ide_gi_callback_get_type())
typedef struct _IdeGiCallback IdeGiCallback;

#define IDE_TYPE_GI_CLASS (ide_gi_class_get_type())
typedef struct _IdeGiClass IdeGiClass;

#define IDE_TYPE_GI_CONSTANT (ide_gi_constant_get_type())
typedef struct _IdeGiConstant IdeGiConstant;

#define IDE_TYPE_GI_DOC (ide_gi_doc_get_type())
typedef struct _IdeGiDoc IdeGiDoc;

#define IDE_TYPE_GI_ENUM (ide_gi_enum_get_type())
typedef struct _IdeGiEnum IdeGiEnum;

#define IDE_TYPE_GI_FIELD (ide_gi_field_get_type())
typedef struct _IdeGiField IdeGiField;

#define IDE_TYPE_GI_FUNCTION (ide_gi_function_get_type())
typedef struct _IdeGiFunction IdeGiFunction;

#define IDE_TYPE_GI_INTERFACE (ide_gi_interface_get_type())
typedef struct _IdeGiInterface IdeGiInterface;

#define IDE_TYPE_GI_PARAMETER (ide_gi_parameter_get_type())
typedef struct _IdeGiParameter IdeGiParameter;

#define IDE_TYPE_GI_PROPERTY (ide_gi_property_get_type())
typedef struct _IdeGiProperty IdeGiProperty;

#define IDE_TYPE_GI_RECORD (ide_gi_record_get_type())
typedef struct _IdeGiRecord IdeGiRecord;

#define IDE_TYPE_GI_SIGNAL (ide_gi_signal_get_type())
typedef struct _IdeGiSignal IdeGiSignal;

#define IDE_TYPE_GI_TYPE (ide_gi_type_get_type())
typedef struct _IdeGiType IdeGiType;

#define IDE_TYPE_GI_UNION (ide_gi_union_get_type())
typedef struct _IdeGiUnion IdeGiUnion;

#define IDE_TYPE_GI_VALUE (ide_gi_value_get_type())
typedef struct _IdeGiValue IdeGiValue;

#define IDE_TYPE_GI_REPOSITORY (ide_gi_repository_get_type())
G_DECLARE_FINAL_TYPE (IdeGiRepository, ide_gi_repository, IDE, GI_REPOSITORY, IdeObject)

#define IDE_TYPE_GI_INDEX (ide_gi_index_get_type())
G_DECLARE_FINAL_TYPE (IdeGiIndex, ide_gi_index, IDE, GI_INDEX, IdeObject)

#define IDE_TYPE_GI_VERSION (ide_gi_version_get_type())
G_DECLARE_FINAL_TYPE (IdeGiVersion, ide_gi_version, IDE, GI_VERSION, GObject)

#define IDE_TYPE_GI_POOL (ide_gi_pool_get_type())
G_DECLARE_FINAL_TYPE (IdeGiPool, ide_gi_pool, IDE, GI_POOL, GObject)

#define IDE_TYPE_GI_PARSER (ide_gi_parser_get_type())
G_DECLARE_FINAL_TYPE (IdeGiParser, ide_gi_parser, IDE, GI_PARSER, GObject)

#define IDE_TYPE_GI_PARSER_RESULT (ide_gi_parser_result_get_type())
G_DECLARE_FINAL_TYPE (IdeGiParserResult, ide_gi_parser_result, IDE, GI_PARSER_RESULT, GObject)

#define IDE_TYPE_GI_PARSER_OBJECT (ide_gi_parser_object_get_type ())
G_DECLARE_DERIVABLE_TYPE (IdeGiParserObject, ide_gi_parser_object, IDE, GI_PARSER_OBJECT, GObject)

typedef enum {
  IDE_GI_PREFIX_TYPE_NAMESPACE   = 1 << 0,
  IDE_GI_PREFIX_TYPE_SYMBOL      = 1 << 1,
  IDE_GI_PREFIX_TYPE_IDENTIFIER  = 1 << 2,
  IDE_GI_PREFIX_TYPE_GTYPE       = 1 << 3,
  IDE_GI_PREFIX_TYPE_PACKAGE     = 1 << 4,
} IdeGiPrefixType;

/* This is used to keep track of our namespaces structs without creating a namespace object */
typedef struct
{
  guint8  *ptr;
  guint32  size64b;
  guint32  offset64b;
} NamespaceChunk;

struct _IdeGiParserObjectClass
{
  GObjectClass parent_class;

  gpointer (*finish) (IdeGiParserObject    *self);
  void     (*index)  (IdeGiParserObject    *self,
                      IdeGiParserResult    *result,
                      gpointer              user_data);

  gboolean (*parse)  (IdeGiParserObject    *self,
                      GMarkupParseContext  *context,
                      IdeGiParserResult    *result,
                      const gchar          *element_name,
                      const gchar         **attribute_names,
                      const gchar         **attribute_values,
                      GError              **error);

  void     (*reset)  (IdeGiParserObject    *self);
};

typedef enum
{
  IDE_GI_NS_TABLE_ALIAS,
  IDE_GI_NS_TABLE_ARRAY,
  IDE_GI_NS_TABLE_CALLBACK,
  IDE_GI_NS_TABLE_CONSTANT,
  IDE_GI_NS_TABLE_DOC,
  IDE_GI_NS_TABLE_ENUM,
  IDE_GI_NS_TABLE_FIELD,
  IDE_GI_NS_TABLE_FUNCTION,
  IDE_GI_NS_TABLE_OBJECT,
  IDE_GI_NS_TABLE_PARAMETER,
  IDE_GI_NS_TABLE_PROPERTY,
  IDE_GI_NS_TABLE_RECORD,
  IDE_GI_NS_TABLE_SIGNAL,
  IDE_GI_NS_TABLE_TYPE,
  IDE_GI_NS_TABLE_UNION,
  IDE_GI_NS_TABLE_VALUE,

  IDE_GI_NS_TABLE_NB_TABLES,
  IDE_GI_NS_TABLE_UNKNOW,
} IdeGiNsTable;

G_END_DECLS
