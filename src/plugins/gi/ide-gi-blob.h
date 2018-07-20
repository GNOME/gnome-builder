/* ide-gi-blob.h
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

#pragma once

#include <glib.h>

#include "ide-gi-types.h"

G_BEGIN_DECLS

/* Keep in sync with:
 * ide-gi-base.h BLOB_TYPE_NAME
 * ide-gi-namespace.c IDE_GI_BLOB_TYPE_INFO
 */
#pragma pack(push, 1)

typedef enum
{
  IDE_GI_BLOB_TYPE_UNKNOW      =  0,
  IDE_GI_BLOB_TYPE_ALIAS       =  1,
  IDE_GI_BLOB_TYPE_ARRAY       =  2,
  IDE_GI_BLOB_TYPE_BOXED       =  3,
  IDE_GI_BLOB_TYPE_CALLBACK    =  4,
  IDE_GI_BLOB_TYPE_CLASS       =  5,
  IDE_GI_BLOB_TYPE_CONSTANT    =  6,
  IDE_GI_BLOB_TYPE_CONSTRUCTOR =  7,
  IDE_GI_BLOB_TYPE_DOC         =  8,
  IDE_GI_BLOB_TYPE_ENUM        =  9,
  IDE_GI_BLOB_TYPE_FIELD       = 10,
  IDE_GI_BLOB_TYPE_FUNCTION    = 11,
  IDE_GI_BLOB_TYPE_HEADER      = 12,
  IDE_GI_BLOB_TYPE_INTERFACE   = 13,
  IDE_GI_BLOB_TYPE_METHOD      = 14,
  IDE_GI_BLOB_TYPE_PARAMETER   = 15,
  IDE_GI_BLOB_TYPE_PROPERTY    = 16,
  IDE_GI_BLOB_TYPE_RECORD      = 17,
  IDE_GI_BLOB_TYPE_SIGNAL      = 18,
  IDE_GI_BLOB_TYPE_TYPE        = 19,
  IDE_GI_BLOB_TYPE_UNION       = 20,
  IDE_GI_BLOB_TYPE_VALUE       = 21,
  IDE_GI_BLOB_TYPE_VFUNC       = 22,
} IdeGiBlobType;

#pragma pack(pop)

typedef IdeGiBase * (*IdeGiObjectConstructor) (IdeGiNamespace  *ns,
                                               IdeGiBlobType    type,
                                               gint32           offset);

typedef void        (*IdeGiObjectDestructor)  (IdeGiBase       *base);

typedef struct
{
  const gchar            *name;
  IdeGiObjectConstructor  constructor;
  IdeGiObjectDestructor   destructor;
  guint16                 blob_size;
  IdeGiNsTable            ns_table;
} IdeGiBlobTypeInfo;

/* Type_ref: the eight high bits are used to indicate the IdeGiType,
 * other bits are an offset in a table, per namespace, if needed
 */
typedef struct
{
  guint32 type       : 6;
  guint32 is_const   : 1;
  guint32 is_pointer : 1;
  guint32 offset     : 24;
} IdeGiTypeRef;

G_STATIC_ASSERT (sizeof (IdeGiTypeRef) == sizeof (guint32));

typedef enum
{
  IDE_GI_BASIC_TYPE_NONE          =  0,
  IDE_GI_BASIC_TYPE_GBOOLEAN      =  1,
  IDE_GI_BASIC_TYPE_GCHAR         =  2,
  IDE_GI_BASIC_TYPE_GUCHAR        =  3,
  IDE_GI_BASIC_TYPE_GSHORT        =  4,
  IDE_GI_BASIC_TYPE_GUSHORT       =  5,
  IDE_GI_BASIC_TYPE_GINT          =  6,
  IDE_GI_BASIC_TYPE_GUINT         =  7,
  IDE_GI_BASIC_TYPE_GLONG         =  8,
  IDE_GI_BASIC_TYPE_GULONG        =  9,
  IDE_GI_BASIC_TYPE_GSSIZE        = 10,
  IDE_GI_BASIC_TYPE_GSIZE         = 11,
  IDE_GI_BASIC_TYPE_GPOINTER      = 12,
  IDE_GI_BASIC_TYPE_GINTPTR       = 13,
  IDE_GI_BASIC_TYPE_GUINTPTR      = 14,
  IDE_GI_BASIC_TYPE_GINT8         = 15,
  IDE_GI_BASIC_TYPE_GUINT8        = 16,
  IDE_GI_BASIC_TYPE_GINT16        = 17,
  IDE_GI_BASIC_TYPE_GUINT16       = 18,
  IDE_GI_BASIC_TYPE_GINT32        = 19,
  IDE_GI_BASIC_TYPE_GUINT32       = 20,
  IDE_GI_BASIC_TYPE_GINT64        = 21,
  IDE_GI_BASIC_TYPE_GUINT64       = 22,
  IDE_GI_BASIC_TYPE_GFLOAT        = 23,
  IDE_GI_BASIC_TYPE_GDOUBLE       = 24,
  IDE_GI_BASIC_TYPE_GTYPE         = 25,
  IDE_GI_BASIC_TYPE_GUTF8         = 26,
  IDE_GI_BASIC_TYPE_FILENAME      = 27,
  IDE_GI_BASIC_TYPE_GUNICHAR      = 28,
  IDE_GI_BASIC_TYPE_C_ARRAY       = 29,
  IDE_GI_BASIC_TYPE_G_ARRAY       = 30,
  IDE_GI_BASIC_TYPE_G_PTR_ARRAY   = 31,
  IDE_GI_BASIC_TYPE_G_BYTES_ARRAY = 32,
  IDE_GI_BASIC_TYPE_VARARGS       = 33,
  IDE_GI_BASIC_TYPE_CALLBACK      = 34,
} IdeGiBasicType;

typedef struct _IdeGiAliasBlob     IdeGiAliasBlob;
typedef struct _IdeGiArrayBlob     IdeGiArrayBlob;
typedef struct _IdeGiCallbackBlob  IdeGiCallbackBlob;
typedef struct _IdeGiConstantBlob  IdeGiConstantBlob;
typedef struct _IdeGiDocBlob       IdeGiDocBlob;
typedef struct _IdeGiEnumBlob      IdeGiEnumBlob;
typedef struct _IdeGiFieldBlob     IdeGiFieldBlob;
typedef struct _IdeGiFunctionBlob  IdeGiFunctionBlob;
typedef struct _IdeGiObjectBlob    IdeGiObjectBlob;
typedef struct _IdeGiParameterBlob IdeGiParameterBlob;
typedef struct _IdeGiPropertyBlob  IdeGiPropertyBlob;
typedef struct _IdeGiRecordBlob    IdeGiRecordBlob;
typedef struct _IdeGiSignalBlob    IdeGiSignalBlob;
typedef struct _IdeGiTypeBlob      IdeGiTypeBlob;
typedef struct _IdeGiUnionBlob     IdeGiUnionBlob;
typedef struct _IdeGiValueBlob     IdeGiValueBlob;
typedef struct _IdeGiHeaderBlob    IdeGiHeaderBlob;

#pragma pack(push, 1)

#define IDE_GI_TYPEREF_TYPE_MASK   0xFF000000
#define IDE_GI_TYPEREF_OFFSET_MASK 0x00FFFFFF

typedef enum
{
  IDE_GI_PARAMETER_FLAG_NONE               = 0,
  IDE_GI_PARAMETER_FLAG_NULLABLE           = 1 << 0,
  IDE_GI_PARAMETER_FLAG_OPTIONAL           = 1 << 1,
  IDE_GI_PARAMETER_FLAG_ALLOW_NONE         = 1 << 2,
  IDE_GI_PARAMETER_FLAG_CALLER_ALLOCATES   = 1 << 3,
  IDE_GI_PARAMETER_FLAG_SKIP               = 1 << 4,
  IDE_GI_PARAMETER_FLAG_RETURN_VALUE       = 1 << 5,
  IDE_GI_PARAMETER_FLAG_INSTANCE_PARAMETER = 1 << 6,
  IDE_GI_PARAMETER_FLAG_VARARGS            = 1 << 7,
  IDE_GI_PARAMETER_FLAG_HAS_CLOSURE        = 1 << 8,
  IDE_GI_PARAMETER_FLAG_HAS_DESTROY        = 1 << 9,
} IdeGiParameterFlags;

/* TODO: add unknow or error type ? */
typedef enum
{
  IDE_GI_STABILITY_STABLE,
  IDE_GI_STABILITY_UNSTABLE,
  IDE_GI_STABILITY_PRIVATE,
} IdeGiStability;

typedef enum
{
  IDE_GI_SCOPE_CALL,
  IDE_GI_SCOPE_ASYNC,
  IDE_GI_SCOPE_NOTIFIED,
} IdeGiScope;

typedef enum
{
  IDE_GI_DIRECTION_IN,
  IDE_GI_DIRECTION_OUT,
  IDE_GI_DIRECTION_INOUT,
} IdeGiDirection;

typedef enum
{
  IDE_GI_TRANSFER_OWNERSHIP_NONE,
  IDE_GI_TRANSFER_OWNERSHIP_CONTAINER,
  IDE_GI_TRANSFER_OWNERSHIP_FULL,
  IDE_GI_TRANSFER_OWNERSHIP_FLOATING,
} IdeGiTransferOwnership;


typedef enum
{
  IDE_GI_SIGNAL_WHEN_NONE,
  IDE_GI_SIGNAL_WHEN_FIRST,
  IDE_GI_SIGNAL_WHEN_LAST,
  IDE_GI_SIGNAL_WHEN_CLEANUP,
} IdeGiSignalWhen;

#pragma pack(pop)

/* We don't call it a blob because it's not keeped in the final file but just allow us
 * to keep track of the <parameter> inside <parameters> elements.
 */
typedef struct
{
  guint16 n_parameters;
  guint32 first_param_offset;
} IdeGiParametersEntry;

struct _IdeGiDocBlob
{
  guint16 blob_type : 6;

  guint32 doc;
  guint32 doc_version;
  guint32 doc_deprecated;
  guint32 doc_stability;

  guint32 n_attributes;
  guint32 attributes;
};

G_STATIC_ASSERT (IS_32B_MULTIPLE (sizeof (IdeGiDocBlob)));

struct _IdeGiHeaderBlob
{
  guint16 blob_type : 6;

  guint16 repo_major_version;
  guint16 repo_minor_version;

  guint8  major_version;
  guint8  minor_version;

  guint32 packages; /* Comma separated string */
  guint32 includes; /* Comma separated string */
  guint32 c_includes; /* Comma separated string */

  guint16 n_fields;
  guint16 n_constants;

  guint16 fields;
  guint16 constants;

  guint32 namespace;
  guint32 nsversion;
  guint32 shared_library;
  guint32 c_identifier_prefixes; /* Comma separated string */
  guint32 c_symbol_prefixes; /* Comma separated string */

  guint32 doc;
};

G_STATIC_ASSERT (IS_32B_MULTIPLE (sizeof (IdeGiHeaderBlob)));

typedef struct
{
  guint32 name;
  gint32  doc;
  guint32 version;
  guint32 deprecated_version;

  guint16 blob_type      : 6;
  guint16 introspectable : 1;
  guint16 deprecated     : 1;
  guint16 stability      : 2;
} IdeGiCommonBlob;

G_STATIC_ASSERT (IS_32B_MULTIPLE (sizeof (IdeGiCommonBlob)));

struct _IdeGiTypeBlob
{
  IdeGiCommonBlob common;

  guint16         is_basic_type     : 1;
  guint16         is_type_container : 1;
  guint16         is_local          : 1;
  guint16         basic_type        : 6;

  guint32         c_type;

  /* For sub-types: currently, there's at most two sub-types max (in HashTable) */
  IdeGiTypeRef    type_ref_0; /* Can be a Type or an Array */
  IdeGiTypeRef    type_ref_1; /* Can be a Type or an Array */
};

G_STATIC_ASSERT (IS_32B_MULTIPLE (sizeof (IdeGiTypeBlob)));

struct _IdeGiAliasBlob
{
  IdeGiCommonBlob common;

  guint32         c_type;
  IdeGiTypeRef    type_ref; /* Can be a Type */
};

G_STATIC_ASSERT (IS_32B_MULTIPLE (sizeof (IdeGiAliasBlob)));

struct _IdeGiArrayBlob
{
  IdeGiCommonBlob common;

  guint16         zero_terminated : 1;
  guint16         has_size        : 1;
  guint16         has_length      : 1;
  guint16         array_type      : 6;

  guint16         size; /* fixed-size */
  guint16         length; /* pos of length param for the size return */
  IdeGiTypeRef    type_ref; /* Can be a Type or an Array */
  guint32         c_type;
};

G_STATIC_ASSERT (IS_32B_MULTIPLE (sizeof (IdeGiArrayBlob)));

struct _IdeGiCallbackBlob
{
  IdeGiCommonBlob common;

  guint16         throws : 1;

  guint16         n_parameters;
  guint32         parameters; /* First param pointer in the list , the others follow immediately */
  guint32         return_value;

  guint32         c_type;
};

G_STATIC_ASSERT (IS_32B_MULTIPLE (sizeof (IdeGiCallbackBlob)));

/* Can be a Class or an Interface depending on the blob_type */
struct _IdeGiObjectBlob
{
  IdeGiCommonBlob common;

  guint16         abstract    : 1;
  guint16         fundamental : 1;
  guint16         has_parent  : 1;

  guint16         parent; /* This is the offset of the crossref parent entry */
  guint32         g_type_name;
  guint32         g_get_type;
  guint32         g_type_struct;
  guint32         g_ref_func;
  guint32         g_unref_func;
  guint32         g_set_value_func;
  guint32         g_get_value_func;

  guint32         c_type;
  guint32         c_symbol_prefix;

  guint16         n_callbacks;
  guint16         n_constants;
  guint16         n_fields;
  guint16         n_functions; /* in function, constructor, vfunc or method sense */
  guint16         n_interfaces; // implements for a class and prerequisite for an interfaces
  guint16         n_properties;
  guint16         n_records;
  guint16         n_signals;
  guint16         n_unions;

  guint16         callbacks;
  guint16         constants;
  guint16         fields;
  guint16         functions;
  guint16         interfaces; // This is the offset of the interfaces/prerequisite crossref entries list for this object
  guint16         properties;
  guint16         records;
  guint16         signals;
  guint16         unions;
};

G_STATIC_ASSERT (IS_32B_MULTIPLE (sizeof (IdeGiObjectBlob)));

struct _IdeGiConstantBlob
{
  IdeGiCommonBlob common;

  guint32         value;
  guint32         c_type;
  guint32         c_identifier;

  IdeGiTypeRef    type_ref; /* A Type or an Array */
};

G_STATIC_ASSERT (IS_32B_MULTIPLE (sizeof (IdeGiConstantBlob)));

struct _IdeGiValueBlob
{
  IdeGiCommonBlob common;

  /* TODO: how do we get  this ? */
  guint16         unsigned_value : 1;

  guint32         c_identifier;
  guint32         glib_nick;
  gint32          value;
};

G_STATIC_ASSERT (IS_32B_MULTIPLE (sizeof (IdeGiValueBlob)));

struct _IdeGiEnumBlob
{
  IdeGiCommonBlob common;

  guint32         c_type;
  guint32         g_type_name;
  guint32         g_get_type;
  guint32         g_error_domain;

  guint16         n_values;
  guint16         n_functions;

  guint16         values;
  guint16         functions;
};

G_STATIC_ASSERT (IS_32B_MULTIPLE (sizeof (IdeGiEnumBlob)));

struct _IdeGiFieldBlob
{
  IdeGiCommonBlob common;

  guint16         readable : 1;
  guint16         writable : 1;
  guint16         private  : 1;

  guint16         bits;

  IdeGiTypeRef    type_ref; /* Can be a Callback, a Type or an Array */
};

G_STATIC_ASSERT (IS_32B_MULTIPLE (sizeof (IdeGiFieldBlob)));

struct _IdeGiFunctionBlob
{
  /* Function, Method, Constructor or Virtual function is infered by the type */
  IdeGiCommonBlob common;

  guint16         setter : 1; // ?
  guint16         getter : 1; // ?
  guint16         throws : 1;

  guint16         n_parameters;
  guint32         parameters; /* First param pointer in the list , the others follow immediately */
  guint32         return_value;

  guint32         c_identifier;
  guint32         shadowed_by;
  guint32         shadows;
  guint32         moved_to;
  guint32         invoker;
};

G_STATIC_ASSERT (IS_32B_MULTIPLE (sizeof (IdeGiFunctionBlob)));

struct _IdeGiParameterBlob
{
  IdeGiCommonBlob     common;

  guint16             scope              : 2;
  guint16             direction          : 2;
  guint16             transfer_ownership : 2;

  IdeGiParameterFlags flags;

  guint32             closure;
  guint32             destroy;

  IdeGiTypeRef        type_ref; /* Can be a Callback, a Type or an Array */
};

G_STATIC_ASSERT (IS_32B_MULTIPLE (sizeof (IdeGiParameterBlob)));

struct _IdeGiPropertyBlob
{
  IdeGiCommonBlob common;

  guint16         readable           : 1;
  guint16         writable           : 1;
  guint16         construct          : 1;
  guint16         construct_only     : 1;
  guint16         transfer_ownership : 2;

  IdeGiTypeRef    type_ref; /* A Type or an Array */
};

G_STATIC_ASSERT (IS_32B_MULTIPLE (sizeof (IdeGiPropertyBlob)));

struct _IdeGiRecordBlob
{
  IdeGiCommonBlob common;

  guint16         disguised : 1;
  guint16         foreign   : 1;

  guint32         g_type_name;
  guint32         g_get_type;
  guint32         g_is_gtype_struct_for;
  guint32         c_type;
  guint32         c_symbol_prefix;

  guint16         n_callbacks;
  guint16         n_fields;
  guint16         n_functions; /* in function, constructor, vfunc or method sense */
  guint16         n_properties;
  guint16         n_unions;

  guint16         callbacks;
  guint16         fields;
  guint16         functions;
  guint16         properties;
  guint16         unions;
};

G_STATIC_ASSERT (IS_32B_MULTIPLE (sizeof (IdeGiRecordBlob)));

struct _IdeGiSignalBlob
{
  IdeGiCommonBlob common;

  guint16         run_when          : 2;
  guint16         no_recurse        : 1;
  guint16         detailed          : 1;
  guint16         action            : 1;
  guint16         no_hooks          : 1;
  guint16         has_class_closure : 1; /* how to get that ? */
  guint16         true_stops_emit   : 1; /* how to get that ? */

  guint16         function; // get matching vfunc
  guint32         return_value;
  guint16         n_parameters;
  guint32         parameters;
};

G_STATIC_ASSERT (IS_32B_MULTIPLE (sizeof (IdeGiSignalBlob)));

struct _IdeGiUnionBlob
{
  IdeGiCommonBlob common;

  guint16         n_fields;
  guint16         n_functions;
  guint16         n_records;

  guint16         fields;
  guint16         functions;
  guint16         records;

  guint32         g_type_name;
  guint32         g_get_type;
  guint32         c_type;
  guint32         c_symbol_prefix;
};

G_STATIC_ASSERT (IS_32B_MULTIPLE (sizeof (IdeGiUnionBlob)));

const gchar              *ide_gi_blob_get_name            (IdeGiBlobType type);
IdeGiObjectConstructor    ide_gi_blob_get_constructor     (IdeGiBlobType type);
IdeGiObjectDestructor     ide_gi_blob_get_destructor      (IdeGiBlobType type);
gsize                     ide_gi_blob_get_size            (IdeGiBlobType type);
IdeGiNsTable              ide_gi_blob_get_ns_table        (IdeGiBlobType type);

G_END_DECLS
