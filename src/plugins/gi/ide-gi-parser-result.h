/* ide-gi-parser-result.h
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
#include <glib-object.h>

#include "ide-gi.h"
#include "ide-gi-types.h"
#include "ide-gi-blob.h"

#include "ide-gi-namespace.h"
#include "ide-gi-parser-object.h"
#include "ide-gi-pool.h"

#include "radix-tree/ide-gi-radix-tree-builder.h"

G_BEGIN_DECLS

struct _IdeGiParserResult
{
  GObject                parent_instance;

  IdeGiParser           *parser;
  IdeGiPool             *pool;
  GFile                 *file;

  IdeGiHeaderBlob        header_blob;

  GByteArray            *header_strings;
  GByteArray            *strings;
  GByteArray            *doc_strings;
  GByteArray            *annotation_strings;

  gsize                  nb_strings;
  gsize                  nb_doc_strings;
  gsize                  nb_annotation_strings;

  GArray                *tables[IDE_GI_NS_TABLE_NB_TABLES];

  IdeGiRadixTreeBuilder *object_index;
  GArray                *global_index;
  GArray                *crossrefs;
};

IdeGiParserResult     *ide_gi_parser_result_new                           (GFile                  *file);

guint32                ide_gi_parser_result_add_annotation_string         (IdeGiParserResult      *self,
                                                                           const gchar            *string);
guint32                ide_gi_parser_result_add_header_string             (IdeGiParserResult      *self,
                                                                           const gchar            *string);
guint32                ide_gi_parser_result_add_string                    (IdeGiParserResult      *self,
                                                                           const gchar            *string);
guint32                ide_gi_parser_result_add_doc_string                (IdeGiParserResult      *self,
                                                                           const gchar            *string);
gint32                 ide_gi_parser_result_add_alias                     (IdeGiParserResult      *self,
                                                                           IdeGiAliasBlob         *blob);
IdeGiTypeRef           ide_gi_parser_result_add_array                     (IdeGiParserResult      *self,
                                                                           IdeGiArrayBlob         *blob);
IdeGiTypeRef           ide_gi_parser_result_add_callback                  (IdeGiParserResult      *self,
                                                                           IdeGiCallbackBlob      *blob);
gint32                 ide_gi_parser_result_add_constant                  (IdeGiParserResult      *self,
                                                                           IdeGiConstantBlob      *blob);
gint32                 ide_gi_parser_result_add_crossref                  (IdeGiParserResult      *self,
                                                                           IdeGiBlobType           type,
                                                                           const gchar            *name,
                                                                           gboolean                is_local);
gint32                 ide_gi_parser_result_add_doc                       (IdeGiParserResult      *self,
                                                                           IdeGiDocBlob           *blob);
gint32                 ide_gi_parser_result_add_enum                      (IdeGiParserResult      *self,
                                                                           IdeGiEnumBlob          *blob);
gint32                 ide_gi_parser_result_add_field                     (IdeGiParserResult      *self,
                                                                           IdeGiFieldBlob         *blob);
gint32                 ide_gi_parser_result_add_function                  (IdeGiParserResult      *self,
                                                                           IdeGiFunctionBlob      *blob);
void                   ide_gi_parser_result_add_global_index              (IdeGiParserResult      *self,
                                                                           const gchar            *name,
                                                                           guint32                 object_offset,
                                                                           IdeGiPrefixType         type,
                                                                           IdeGiBlobType           object_type,
                                                                           gboolean                is_buildable);
gint32                 ide_gi_parser_result_add_object                    (IdeGiParserResult      *self,
                                                                           IdeGiObjectBlob        *blob);
void                   ide_gi_parser_result_add_object_index              (IdeGiParserResult      *self,
                                                                           const gchar            *name,
                                                                           IdeGiBlobType           type,
                                                                           gint32                  offset);
gint32                 ide_gi_parser_result_add_parameter                 (IdeGiParserResult      *self,
                                                                           IdeGiParameterBlob     *blob);
gint32                 ide_gi_parser_result_add_property                  (IdeGiParserResult      *self,
                                                                           IdeGiPropertyBlob      *blob);
gint32                 ide_gi_parser_result_add_record                     (IdeGiParserResult      *self,
                                                                           IdeGiRecordBlob        *blob);
gint32                 ide_gi_parser_result_add_signal                    (IdeGiParserResult      *self,
                                                                           IdeGiSignalBlob        *blob);
IdeGiTypeRef           ide_gi_parser_result_add_type                      (IdeGiParserResult      *self,
                                                                           IdeGiTypeBlob          *blob);
gint32                 ide_gi_parser_result_add_union                     (IdeGiParserResult      *self,
                                                                           IdeGiUnionBlob         *blob);
gint32                 ide_gi_parser_result_add_value                     (IdeGiParserResult      *self,
                                                                           IdeGiValueBlob         *blob);

const gchar           *ide_gi_parser_result_get_annotation_string         (IdeGiParserResult      *self,
                                                                           guint32                 offset);
GByteArray            *ide_gi_parser_result_get_annotation_strings        (IdeGiParserResult      *self);
const gchar           *ide_gi_parser_result_get_doc_string                (IdeGiParserResult      *self,
                                                                           guint32                 offset);
GByteArray            *ide_gi_parser_result_get_doc_strings               (IdeGiParserResult      *self);
const gchar           *ide_gi_parser_result_get_header_string             (IdeGiParserResult      *self,
                                                                           guint32                 offset);
GByteArray            *ide_gi_parser_result_get_header_strings            (IdeGiParserResult      *self);
const gchar           *ide_gi_parser_result_get_string                    (IdeGiParserResult      *self,
                                                                           guint32                 offset);
GByteArray            *ide_gi_parser_result_get_strings                   (IdeGiParserResult      *self);
const gchar           *ide_gi_parser_result_get_c_includes                (IdeGiParserResult      *self);
const gchar           *ide_gi_parser_result_get_c_identifier_prefixes     (IdeGiParserResult      *self);
const gchar           *ide_gi_parser_result_get_c_symbol_prefixes         (IdeGiParserResult      *self);
const gchar           *ide_gi_parser_result_get_includes                  (IdeGiParserResult      *self);
const gchar           *ide_gi_parser_result_get_namespace                 (IdeGiParserResult      *self);
const gchar           *ide_gi_parser_result_get_packages                  (IdeGiParserResult      *self);
const gchar           *ide_gi_parser_result_get_shared_library            (IdeGiParserResult      *self);

GFile                 *ide_gi_parser_result_get_file                      (IdeGiParserResult      *self);
const IdeGiHeaderBlob *ide_gi_parser_result_get_header                    (IdeGiParserResult      *self);
void                   ide_gi_parser_result_set_header                    (IdeGiParserResult      *self,
                                                                           IdeGiHeaderBlob        *header);
GArray                *ide_gi_parser_result_get_crossrefs                 (IdeGiParserResult      *self);
GArray                *ide_gi_parser_result_get_global_index              (IdeGiParserResult      *self);
IdeGiRadixTreeBuilder *ide_gi_parser_result_get_object_index_builder      (IdeGiParserResult      *self);
IdeGiParser           *ide_gi_parser_result_get_parser                    (IdeGiParserResult      *self);
void                   ide_gi_parser_result_set_parser                    (IdeGiParserResult      *self,
                                                                           IdeGiParser            *parser);
void                   ide_gi_parser_result_set_pool                      (IdeGiParserResult      *self,
                                                                           IdeGiPool              *pool);
IdeGiPool             *ide_gi_parser_result_get_pool                      (IdeGiParserResult      *self);
GArray                *ide_gi_parser_result_get_table                     (IdeGiParserResult      *self,
                                                                           IdeGiNsTable            table);
void                   ide_gi_parser_result_print_stats                   (IdeGiParserResult      *self);

G_END_DECLS
