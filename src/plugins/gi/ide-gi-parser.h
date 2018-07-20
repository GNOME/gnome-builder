/* ide-gi-parser.h
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

#include <gio/gio.h>

#include "ide-gi-types.h"

G_BEGIN_DECLS

#pragma pack(push, 1)

/* Keep in sync with ?.h element_names */
typedef enum
{
  IDE_GI_ELEMENT_TYPE_UNKNOW             = (guint64)0,

  IDE_GI_ELEMENT_TYPE_ALIAS              = (guint64)1 <<  0,
  IDE_GI_ELEMENT_TYPE_ANNOTATION         = (guint64)1 <<  1,
  IDE_GI_ELEMENT_TYPE_ARRAY              = (guint64)1 <<  2,
  IDE_GI_ELEMENT_TYPE_ATTRIBUTES         = (guint64)1 <<  3,
  IDE_GI_ELEMENT_TYPE_BITFIELD           = (guint64)1 <<  4,
  IDE_GI_ELEMENT_TYPE_CALLBACK           = (guint64)1 <<  5,
  IDE_GI_ELEMENT_TYPE_C_INCLUDE          = (guint64)1 <<  6,
  IDE_GI_ELEMENT_TYPE_CLASS              = (guint64)1 <<  7,
  IDE_GI_ELEMENT_TYPE_CONSTANT           = (guint64)1 <<  8,
  IDE_GI_ELEMENT_TYPE_CONSTRUCTOR        = (guint64)1 <<  9,
  IDE_GI_ELEMENT_TYPE_DOC                = (guint64)1 << 10,
  IDE_GI_ELEMENT_TYPE_DOC_DEPRECATED     = (guint64)1 << 11,
  IDE_GI_ELEMENT_TYPE_DOC_STABILITY      = (guint64)1 << 12,
  IDE_GI_ELEMENT_TYPE_DOC_VERSION        = (guint64)1 << 13,
  IDE_GI_ELEMENT_TYPE_ENUMERATION        = (guint64)1 << 14,
  IDE_GI_ELEMENT_TYPE_FIELD              = (guint64)1 << 15,
  IDE_GI_ELEMENT_TYPE_FUNCTION           = (guint64)1 << 16,
  IDE_GI_ELEMENT_TYPE_GLIB_BOXED         = (guint64)1 << 17,
  IDE_GI_ELEMENT_TYPE_GLIB_SIGNAL        = (guint64)1 << 18,
  IDE_GI_ELEMENT_TYPE_IMPLEMENTS         = (guint64)1 << 19,
  IDE_GI_ELEMENT_TYPE_INCLUDE            = (guint64)1 << 20,
  IDE_GI_ELEMENT_TYPE_INSTANCE_PARAMETER = (guint64)1 << 21,
  IDE_GI_ELEMENT_TYPE_INTERFACE          = (guint64)1 << 22,
  IDE_GI_ELEMENT_TYPE_MEMBER             = (guint64)1 << 23,
  IDE_GI_ELEMENT_TYPE_METHOD             = (guint64)1 << 24,
  IDE_GI_ELEMENT_TYPE_NAMESPACE          = (guint64)1 << 25,
  IDE_GI_ELEMENT_TYPE_PACKAGE            = (guint64)1 << 26,
  IDE_GI_ELEMENT_TYPE_PARAMETER          = (guint64)1 << 27,
  IDE_GI_ELEMENT_TYPE_PARAMETERS         = (guint64)1 << 28,
  IDE_GI_ELEMENT_TYPE_PREREQUISITE       = (guint64)1 << 29,
  IDE_GI_ELEMENT_TYPE_PROPERTY           = (guint64)1 << 30,
  IDE_GI_ELEMENT_TYPE_RECORD             = (guint64)1 << 31,
  IDE_GI_ELEMENT_TYPE_REPOSITORY         = (guint64)1 << 32,
  IDE_GI_ELEMENT_TYPE_RETURN_VALUE       = (guint64)1 << 33,
  IDE_GI_ELEMENT_TYPE_TYPE               = (guint64)1 << 34,
  IDE_GI_ELEMENT_TYPE_UNION              = (guint64)1 << 35,
  IDE_GI_ELEMENT_TYPE_VARARGS            = (guint64)1 << 36,
  IDE_GI_ELEMENT_TYPE_VIRTUAL_METHOD     = (guint64)1 << 37,

  IDE_GI_ELEMENT_TYPE_LAST               = (guint64)1 << 38
} IdeGiElementType;

#define _DOC_MASK  (IDE_GI_ELEMENT_TYPE_DOC |            \
                    IDE_GI_ELEMENT_TYPE_DOC_DEPRECATED | \
                    IDE_GI_ELEMENT_TYPE_DOC_STABILITY |  \
                    IDE_GI_ELEMENT_TYPE_DOC_VERSION |    \
                    IDE_GI_ELEMENT_TYPE_ANNOTATION)

#define _ALIAS_MASK (_DOC_MASK | \
                     IDE_GI_ELEMENT_TYPE_TYPE)

#define _ARRAY_MASK (_DOC_MASK |                \
                     IDE_GI_ELEMENT_TYPE_TYPE | \
                     IDE_GI_ELEMENT_TYPE_ARRAY)

#define _CALLBACK_MASK (_DOC_MASK |                      \
                        IDE_GI_ELEMENT_TYPE_PARAMETERS | \
                        IDE_GI_ELEMENT_TYPE_RETURN_VALUE)

#define _CLASS_MASK (_DOC_MASK |                          \
                     IDE_GI_ELEMENT_TYPE_CALLBACK |       \
                     IDE_GI_ELEMENT_TYPE_CONSTANT |       \
                     IDE_GI_ELEMENT_TYPE_CONSTRUCTOR |    \
                     IDE_GI_ELEMENT_TYPE_FIELD |          \
                     IDE_GI_ELEMENT_TYPE_FUNCTION |       \
                     IDE_GI_ELEMENT_TYPE_GLIB_SIGNAL |    \
                     IDE_GI_ELEMENT_TYPE_METHOD |         \
                     IDE_GI_ELEMENT_TYPE_PROPERTY |       \
                     IDE_GI_ELEMENT_TYPE_RECORD |         \
                     IDE_GI_ELEMENT_TYPE_UNION |          \
                     IDE_GI_ELEMENT_TYPE_VIRTUAL_METHOD)

#define _CONSTANT_MASK (_DOC_MASK |                 \
                        IDE_GI_ELEMENT_TYPE_ARRAY | \
                        IDE_GI_ELEMENT_TYPE_TYPE)

#define _ENUMERATION_MASK (_DOC_MASK |                    \
                           IDE_GI_ELEMENT_TYPE_FUNCTION | \
                           IDE_GI_ELEMENT_TYPE_MEMBER)

#define _FIELD_MASK (_DOC_MASK |                    \
                     IDE_GI_ELEMENT_TYPE_ARRAY |    \
                     IDE_GI_ELEMENT_TYPE_CALLBACK | \
                     IDE_GI_ELEMENT_TYPE_TYPE)

#define _FUNCTION_MASK (_DOC_MASK |                      \
                        IDE_GI_ELEMENT_TYPE_PARAMETERS | \
                        IDE_GI_ELEMENT_TYPE_RETURN_VALUE)

#define _NAMESPACE_MASK (_DOC_MASK |                       \
                         IDE_GI_ELEMENT_TYPE_ALIAS |       \
                         IDE_GI_ELEMENT_TYPE_BITFIELD |    \
                         IDE_GI_ELEMENT_TYPE_CALLBACK |    \
                         IDE_GI_ELEMENT_TYPE_CLASS |       \
                         IDE_GI_ELEMENT_TYPE_CONSTANT |    \
                         IDE_GI_ELEMENT_TYPE_ENUMERATION | \
                         IDE_GI_ELEMENT_TYPE_FUNCTION |    \
                         IDE_GI_ELEMENT_TYPE_GLIB_BOXED |  \
                         IDE_GI_ELEMENT_TYPE_INTERFACE |   \
                         IDE_GI_ELEMENT_TYPE_RECORD |      \
                         IDE_GI_ELEMENT_TYPE_UNION)

#define _INTERFACE_MASK (_DOC_MASK |                        \
                         IDE_GI_ELEMENT_TYPE_CALLBACK |     \
                         IDE_GI_ELEMENT_TYPE_CONSTANT |     \
                         IDE_GI_ELEMENT_TYPE_CONSTRUCTOR |  \
                         IDE_GI_ELEMENT_TYPE_FIELD |        \
                         IDE_GI_ELEMENT_TYPE_FUNCTION |     \
                         IDE_GI_ELEMENT_TYPE_GLIB_SIGNAL |  \
                         IDE_GI_ELEMENT_TYPE_METHOD |       \
                         IDE_GI_ELEMENT_TYPE_PROPERTY |     \
                         IDE_GI_ELEMENT_TYPE_VIRTUAL_METHOD)

#define _MEMBER_MASK (_DOC_MASK)

#define _PARAMETERS_MASK (IDE_GI_ELEMENT_TYPE_INSTANCE_PARAMETER | \
                          IDE_GI_ELEMENT_TYPE_PARAMETER)

#define _PARAMETER_MASK (_DOC_MASK |                 \
                         IDE_GI_ELEMENT_TYPE_ARRAY | \
                         IDE_GI_ELEMENT_TYPE_TYPE)

#define _PROPERTY_MASK (_DOC_MASK |                 \
                        IDE_GI_ELEMENT_TYPE_ARRAY | \
                        IDE_GI_ELEMENT_TYPE_TYPE)

#define _RECORD_MASK (_DOC_MASK |                       \
                      IDE_GI_ELEMENT_TYPE_CALLBACK |    \
                      IDE_GI_ELEMENT_TYPE_CONSTRUCTOR | \
                      IDE_GI_ELEMENT_TYPE_FIELD |       \
                      IDE_GI_ELEMENT_TYPE_FUNCTION |    \
                      IDE_GI_ELEMENT_TYPE_METHOD |      \
                      IDE_GI_ELEMENT_TYPE_PROPERTY |    \
                      IDE_GI_ELEMENT_TYPE_UNION |       \
                      IDE_GI_ELEMENT_TYPE_VIRTUAL_METHOD)

#define _GLIB_SIGNAL_MASK (_DOC_MASK |                      \
                           IDE_GI_ELEMENT_TYPE_PARAMETERS | \
                           IDE_GI_ELEMENT_TYPE_RETURN_VALUE)

#define _TYPE_MASK (_DOC_MASK |                 \
                    IDE_GI_ELEMENT_TYPE_ARRAY | \
                    IDE_GI_ELEMENT_TYPE_TYPE)

#define _UNION_MASK (_DOC_MASK |                       \
                     IDE_GI_ELEMENT_TYPE_CONSTRUCTOR | \
                     IDE_GI_ELEMENT_TYPE_FIELD |       \
                     IDE_GI_ELEMENT_TYPE_FUNCTION |    \
                     IDE_GI_ELEMENT_TYPE_METHOD |      \
                     IDE_GI_ELEMENT_TYPE_RECORD)

/* Mask for the elements handle by the parsers:
 * (element_type & mask) == TRUE if the parser handle the element
 */
typedef enum
{
  IDE_GI_PARSER_ELEMENT_MASK_ALIAS       = _ALIAS_MASK,
  IDE_GI_PARSER_ELEMENT_MASK_ARRAY       = _ARRAY_MASK,
  IDE_GI_PARSER_ELEMENT_MASK_CALLBACK    = _CALLBACK_MASK,
  IDE_GI_PARSER_ELEMENT_MASK_CLASS       = _CLASS_MASK,
  IDE_GI_PARSER_ELEMENT_MASK_CONSTANT    = _CONSTANT_MASK,
  IDE_GI_PARSER_ELEMENT_MASK_DOC         = _DOC_MASK,
  IDE_GI_PARSER_ELEMENT_MASK_ENUMERATION = _ENUMERATION_MASK,
  IDE_GI_PARSER_ELEMENT_MASK_FIELD       = _FIELD_MASK,
  IDE_GI_PARSER_ELEMENT_MASK_FUNCTION    = _FUNCTION_MASK,
  IDE_GI_PARSER_ELEMENT_MASK_NAMESPACE   = _NAMESPACE_MASK,
  IDE_GI_PARSER_ELEMENT_MASK_INTERFACE   = _INTERFACE_MASK,
  IDE_GI_PARSER_ELEMENT_MASK_MEMBER      = _MEMBER_MASK,
  IDE_GI_PARSER_ELEMENT_MASK_PARAMETERS  = _PARAMETERS_MASK,
  IDE_GI_PARSER_ELEMENT_MASK_PARAMETER   = _PARAMETER_MASK,
  IDE_GI_PARSER_ELEMENT_MASK_PROPERTY    = _PROPERTY_MASK,
  IDE_GI_PARSER_ELEMENT_MASK_RECORD      = _RECORD_MASK,
  IDE_GI_PARSER_ELEMENT_MASK_GLIB_SIGNAL = _GLIB_SIGNAL_MASK,
  IDE_GI_PARSER_ELEMENT_MASK_TYPE        = _TYPE_MASK,
  IDE_GI_PARSER_ELEMENT_MASK_UNION       = _UNION_MASK
} IdeGiParserElementMask;

#pragma pack(pop)

IdeGiParser            *ide_gi_parser_new                        (void);

IdeGiElementType        ide_gi_parser_get_element_type           (const gchar       *element_name);
const gchar            *ide_gi_parser_get_element_type_string    (IdeGiElementType   type);
IdeGiPool              *ide_gi_parser_get_pool                   (IdeGiParser       *self);
gboolean                ide_gi_parser_global_init                (void);
gboolean                ide_gi_parser_global_cleanup             (void);
IdeGiParserResult      *ide_gi_parser_parse_file                 (IdeGiParser       *self,
                                                                  GFile             *file,
                                                                  GCancellable      *cancellable,
                                                                  GError           **error);
void                    ide_gi_parser_set_pool                   (IdeGiParser       *self,
                                                                  IdeGiPool         *pool);


G_END_DECLS
