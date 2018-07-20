/* ide-gi-parser-result.c
 *
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

#define G_LOG_DOMAIN "ide-gi-parser-result"

#include <dazzle.h>
#include <ide.h>
#include <stdio.h>
#include <string.h>

#include "ide-gi-utils.h"

#include "ide-gi-parser-result.h"

G_DEFINE_TYPE (IdeGiParserResult, ide_gi_parser_result, G_TYPE_OBJECT)

enum {
  PROP_0,
  PROP_FILE,
  N_PROPS
};

static GParamSpec *properties [N_PROPS];

static void
compute_total_size (IdeGiParserResult *self,
                    gsize             *total,
                    gsize             *tables_total)
{
  gsize tmp_total = 0;
  gsize tmp_tables_total = 0;

  for (guint i = 0; i < IDE_GI_NS_TABLE_NB_TABLES; i++)
    if (self->tables[i]->len > 0)
      tmp_tables_total += self->tables[i]->len * g_array_get_element_size (self->tables[i]);

  if (tables_total != NULL)
    *tables_total = tmp_tables_total;

  tmp_total = tmp_tables_total;
  tmp_total += self->crossrefs->len * sizeof (IdeGiCrossRef);
  tmp_total += self->strings->len;
  tmp_total += self->doc_strings->len;
  tmp_total += self->annotation_strings->len;

  if (total != NULL)
    *total = tmp_total;
}

void ide_gi_parser_result_print_stats (IdeGiParserResult *self)
{
  g_autofree gchar *path = g_file_get_path (self->file);
  gsize total;
  gsize tables_total;
  gsize size;

  compute_total_size (self, &total, &tables_total);

  g_print ("file:%s\n", path);
  g_print ("namespace:%s\n", ide_gi_parser_result_get_namespace (self));
  g_print ("total size:%ld tables size:%ld (%.2lf%%)\n",
            total,
            tables_total,
            (gdouble)tables_total / total * 100);

  for (guint i = 0; i < IDE_GI_NS_TABLE_NB_TABLES; i++)
    {
      if (self->tables[i]->len > 0)
        {
          size = self->tables[i]->len * g_array_get_element_size (self->tables[i]);

          g_print ("%-20s nb:%6d size:%6ld (%5.2lf%%)\n",
                   ide_gi_utils_ns_table_to_string (i),
                   self->tables[i]->len,
                   size,
                   (gdouble)size / total * 100);
        }
    }

  size = self->crossrefs->len * sizeof (IdeGiCrossRef);
  g_print ("crossrefs            nb:%6d size:%6ld (%5.2lf%%)\n",
           self->crossrefs->len,
           size,
           (gdouble)size/ total * 100);

  g_print ("strings              nb:%6ld size:%6d (%5.2lf%%)\n",
           self->nb_strings,
           self->strings->len,
           (gdouble)self->strings->len / total * 100);

  g_print ("doc strings          nb:%6ld size:%6d (%5.2lf%%)\n",
           self->nb_doc_strings,
           self->doc_strings->len,
           (gdouble)self->doc_strings->len / total * 100);

  g_print ("annotation strings   nb:%6ld size:%6d (%5.2lf%%)\n",
           self->nb_annotation_strings,
           self->annotation_strings->len,
           (gdouble)self->annotation_strings->len / total * 100);
}

GArray *
ide_gi_parser_result_get_table (IdeGiParserResult *self,
                                IdeGiNsTable       table)
{
  g_return_val_if_fail (IDE_IS_GI_PARSER_RESULT (self), NULL);

  return g_array_ref (self->tables [table]);
}

GByteArray *
ide_gi_parser_result_get_header_strings (IdeGiParserResult *self)
{
  g_return_val_if_fail (IDE_IS_GI_PARSER_RESULT (self), NULL);

  return g_byte_array_ref (self->header_strings);
}

guint32
ide_gi_parser_result_add_header_string (IdeGiParserResult *self,
                                        const gchar       *string)
{
  guint32 offset;

  g_return_val_if_fail (IDE_IS_GI_PARSER_RESULT (self), 0);

  if (dzl_str_empty0 (string))
    return 0;

  offset = self->header_strings->len;
  g_byte_array_append (self->header_strings, (guint8 *)string, strlen (string) + 1);

  return offset;
}

const gchar *
ide_gi_parser_result_get_header_string (IdeGiParserResult *self,
                                        guint32            offset)
{
  g_return_val_if_fail (IDE_IS_GI_PARSER_RESULT (self), NULL);

  return (const gchar *)(self->header_strings->data + offset);
};

GByteArray *
ide_gi_parser_result_get_strings (IdeGiParserResult *self)
{
  g_return_val_if_fail (IDE_IS_GI_PARSER_RESULT (self), NULL);

  return g_byte_array_ref (self->strings);
}

guint32
ide_gi_parser_result_add_string (IdeGiParserResult *self,
                                 const gchar       *string)
{
  guint32 offset;

  g_return_val_if_fail (IDE_IS_GI_PARSER_RESULT (self), 0);

  if (dzl_str_empty0 (string))
    return 0;

  offset = self->strings->len;
  g_byte_array_append (self->strings, (guint8 *)string, strlen (string) + 1);
  self->nb_strings++;

  return offset;
}

const gchar *
ide_gi_parser_result_get_string (IdeGiParserResult *self,
                                 guint32            offset)
{
  g_return_val_if_fail (IDE_IS_GI_PARSER_RESULT (self), NULL);

  return (const gchar *)(self->strings->data + offset);
};

GByteArray *
ide_gi_parser_result_get_doc_strings (IdeGiParserResult *self)
{
  g_return_val_if_fail (IDE_IS_GI_PARSER_RESULT (self), NULL);

  return g_byte_array_ref (self->doc_strings);
}

guint32
ide_gi_parser_result_add_doc_string (IdeGiParserResult *self,
                                     const gchar       *string)
{
  guint32 offset;

  g_return_val_if_fail (IDE_IS_GI_PARSER_RESULT (self), 0);

  if (dzl_str_empty0 (string))
    return 0;

  offset = self->doc_strings->len;
  g_byte_array_append (self->doc_strings, (guint8 *)string, strlen (string) + 1);
  self->nb_doc_strings++;

  return offset;
}

const gchar *
ide_gi_parser_result_get_doc_string (IdeGiParserResult *self,
                                     guint32            offset)
{
  g_return_val_if_fail (IDE_IS_GI_PARSER_RESULT (self), NULL);

  return (const gchar *)(self->doc_strings->data + offset);
};

GByteArray *
ide_gi_parser_result_get_annotation_strings (IdeGiParserResult *self)
{
  g_return_val_if_fail (IDE_IS_GI_PARSER_RESULT (self), NULL);

  return g_byte_array_ref (self->annotation_strings);
}

guint32
ide_gi_parser_result_add_annotation_string (IdeGiParserResult *self,
                                            const gchar       *string)
{
  guint32 offset;

  g_return_val_if_fail (IDE_IS_GI_PARSER_RESULT (self), 0);

  if (dzl_str_empty0 (string))
    return 0;

  offset = self->annotation_strings->len;
  g_byte_array_append (self->annotation_strings, (guint8 *)string, strlen (string) + 1);
  self->nb_annotation_strings++;

  return offset;
}

const gchar *
ide_gi_parser_result_get_annotation_string (IdeGiParserResult *self,
                                            guint32            offset)
{
  g_return_val_if_fail (IDE_IS_GI_PARSER_RESULT (self), NULL);

  return (const gchar *)(self->annotation_strings->data + offset);
};

const gchar *
ide_gi_parser_result_get_namespace (IdeGiParserResult *self)
{
  g_return_val_if_fail (IDE_IS_GI_PARSER_RESULT (self), NULL);

  if (self->header_blob.namespace > 0)
    return (const gchar *)self->header_strings->data + self->header_blob.namespace;
  else
    return NULL;
}

const gchar *
ide_gi_parser_result_get_packages (IdeGiParserResult *self)
{
  g_return_val_if_fail (IDE_IS_GI_PARSER_RESULT (self), NULL);

  return ide_gi_parser_result_get_header_string (self, self->header_blob.packages);
}

const gchar *
ide_gi_parser_result_get_includes (IdeGiParserResult *self)
{
  g_return_val_if_fail (IDE_IS_GI_PARSER_RESULT (self), NULL);

  return ide_gi_parser_result_get_header_string (self,self->header_blob.includes);
}

const gchar *
ide_gi_parser_result_get_c_includes (IdeGiParserResult *self)
{
  g_return_val_if_fail (IDE_IS_GI_PARSER_RESULT (self), NULL);

  return ide_gi_parser_result_get_header_string (self, self->header_blob.c_includes);
}

const gchar *
ide_gi_parser_result_get_shared_library (IdeGiParserResult *self)
{
  g_return_val_if_fail (IDE_IS_GI_PARSER_RESULT (self), NULL);

  return ide_gi_parser_result_get_header_string (self, self->header_blob.shared_library);
}

const gchar *
ide_gi_parser_result_get_c_symbol_prefixes (IdeGiParserResult  *self)
{
  g_return_val_if_fail (IDE_IS_GI_PARSER_RESULT (self), NULL);

  return ide_gi_parser_result_get_header_string (self, self->header_blob.c_symbol_prefixes);
}

const gchar *
ide_gi_parser_result_get_c_identifier_prefixes (IdeGiParserResult  *self)
{
  g_return_val_if_fail (IDE_IS_GI_PARSER_RESULT (self), NULL);

  return ide_gi_parser_result_get_header_string (self, self->header_blob.c_identifier_prefixes);
}

gint32
ide_gi_parser_result_add_alias (IdeGiParserResult *self,
                                IdeGiAliasBlob    *blob)
{
  GArray *table;
  gint32 offset;

  g_return_val_if_fail (IDE_IS_GI_PARSER_RESULT (self), -1);
  g_return_val_if_fail (blob != NULL, -1);

  table = self->tables [IDE_GI_NS_TABLE_ALIAS];
  offset = table->len;
  g_array_append_val (table, *blob);

  return offset;
}

IdeGiTypeRef
ide_gi_parser_result_add_array (IdeGiParserResult *self,
                                IdeGiArrayBlob    *blob)
{
  GArray *table;
  IdeGiTypeRef ref = {0};

  g_return_val_if_fail (IDE_IS_GI_PARSER_RESULT (self), ref);
  g_return_val_if_fail (blob != NULL, ref);

  table = self->tables [IDE_GI_NS_TABLE_ARRAY];
  g_array_append_val (table, *blob);

  ref.type = blob->array_type;
  ref.offset = table->len - 1;

  return ref;
}

IdeGiTypeRef
ide_gi_parser_result_add_callback (IdeGiParserResult *self,
                                   IdeGiCallbackBlob *blob)
{
  GArray *table;
  IdeGiTypeRef ref = {0};

  g_return_val_if_fail (IDE_IS_GI_PARSER_RESULT (self), ref);
  g_return_val_if_fail (blob != NULL, ref);

  table = self->tables [IDE_GI_NS_TABLE_CALLBACK];
  g_array_append_val (table, *blob);

  ref.type = IDE_GI_BASIC_TYPE_CALLBACK;
  ref.offset = table->len  - 1;

  return ref;
}

gint32
ide_gi_parser_result_add_constant (IdeGiParserResult *self,
                                   IdeGiConstantBlob *blob)
{
  GArray *table;
  gint32 offset;

  g_return_val_if_fail (IDE_IS_GI_PARSER_RESULT (self), -1);
  g_return_val_if_fail (blob != NULL, -1);

  table = self->tables [IDE_GI_NS_TABLE_CONSTANT];
  offset = table->len;
  g_array_append_val (table, *blob);

  return offset;
}

gint32
ide_gi_parser_result_add_doc (IdeGiParserResult *self,
                              IdeGiDocBlob      *blob)
{
  GArray *table;
  gint32 offset;

  g_return_val_if_fail (IDE_IS_GI_PARSER_RESULT (self), -1);
  g_return_val_if_fail (blob != NULL, -1 && blob->blob_type == IDE_GI_BLOB_TYPE_DOC);

  table = self->tables [IDE_GI_NS_TABLE_DOC];
  offset = table->len;
  g_array_append_val (table, *blob);

  return offset;
}

gint32
ide_gi_parser_result_add_enum (IdeGiParserResult *self,
                               IdeGiEnumBlob     *blob)
{
  GArray *table;
  gint32 offset;

  g_return_val_if_fail (IDE_IS_GI_PARSER_RESULT (self), -1);
  g_return_val_if_fail (blob != NULL, -1);

  table = self->tables [IDE_GI_NS_TABLE_ENUM];
  offset = table->len;
  g_array_append_val (table, *blob);

  return offset;
}

gint32
ide_gi_parser_result_add_field (IdeGiParserResult *self,
                                IdeGiFieldBlob    *blob)
{
  GArray *table;
  gint32 offset;

  g_return_val_if_fail (IDE_IS_GI_PARSER_RESULT (self), -1);
  g_return_val_if_fail (blob != NULL, -1);

  table = self->tables [IDE_GI_NS_TABLE_FIELD];
  offset = table->len;
  g_array_append_val (table, *blob);

  return offset;
}

gint32
ide_gi_parser_result_add_function (IdeGiParserResult *self,
                                   IdeGiFunctionBlob *blob)
{
  GArray *table;
  gint32 offset;

  g_return_val_if_fail (IDE_IS_GI_PARSER_RESULT (self), -1);
  g_return_val_if_fail (blob != NULL, -1);

  table = self->tables [IDE_GI_NS_TABLE_FUNCTION];
  offset = table->len;
  g_array_append_val (table, *blob);

  return offset;
}

gint32
ide_gi_parser_result_add_object (IdeGiParserResult *self,
                                 IdeGiObjectBlob   *blob)
{
  GArray *table;
  gint32 offset;

  g_return_val_if_fail (IDE_IS_GI_PARSER_RESULT (self), -1);
  g_return_val_if_fail (blob != NULL, -1);

  table = self->tables [IDE_GI_NS_TABLE_OBJECT];
  offset = table->len;
  g_array_append_val (table, *blob);

  return offset;
}

void
ide_gi_parser_result_add_object_index (IdeGiParserResult  *self,
                                       const gchar        *name,
                                       IdeGiBlobType       type,
                                       gint32              offset)
{
  RoTreePayload payload;

  g_return_if_fail (IDE_IS_GI_PARSER_RESULT (self));
  g_return_if_fail (!dzl_str_empty0 (name));

  payload.type = type;
  payload.offset = offset;

  ide_gi_radix_tree_builder_add (self->object_index, name, RO_TREE_PAYLOAD_N64_SIZE, &payload);
}

IdeGiRadixTreeBuilder *
ide_gi_parser_result_get_object_index_builder (IdeGiParserResult *self)
{
  g_return_val_if_fail (IDE_IS_GI_PARSER_RESULT (self), NULL);

  return g_object_ref (self->object_index);
}

void
ide_gi_parser_result_add_global_index (IdeGiParserResult *self,
                                       const gchar       *name,
                                       guint32            object_offset,
                                       IdeGiPrefixType    type,
                                       IdeGiBlobType      object_type,
                                       gboolean           is_buildable)
{
  IdeGiGlobalIndexEntry entry;

  g_return_if_fail (IDE_IS_GI_PARSER_RESULT (self));
  g_return_if_fail (!dzl_str_empty0 (name));

  entry.name = g_strdup (name);
  entry.object_offset = object_offset;
  entry.type = type;
  entry.object_type = object_type;
  entry.is_buildable = is_buildable;

  g_array_append_val (self->global_index, entry);
}

GArray *
ide_gi_parser_result_get_global_index (IdeGiParserResult *self)
{
  g_return_val_if_fail (IDE_IS_GI_PARSER_RESULT (self), NULL);

  return g_array_ref (self->global_index);
}

gint32
ide_gi_parser_result_add_parameter (IdeGiParserResult  *self,
                                    IdeGiParameterBlob *blob)
{
  GArray *table;
  gint32 offset;

  g_return_val_if_fail (IDE_IS_GI_PARSER_RESULT (self), -1);
  g_return_val_if_fail (blob != NULL, -1);

  table = self->tables [IDE_GI_NS_TABLE_PARAMETER];
  offset = table->len;
  g_array_append_val (table, *blob);

  return offset;
}

gint32
ide_gi_parser_result_add_property (IdeGiParserResult *self,
                                   IdeGiPropertyBlob *blob)
{
  GArray *table;
  gint32 offset;

  g_return_val_if_fail (IDE_IS_GI_PARSER_RESULT (self), -1);
  g_return_val_if_fail (blob != NULL, -1);

  table = self->tables [IDE_GI_NS_TABLE_PROPERTY];
  offset = table->len;
  g_array_append_val (table, *blob);

  return offset;
}

gint32
ide_gi_parser_result_add_record (IdeGiParserResult *self,
                                 IdeGiRecordBlob   *blob)
{
  GArray *table;
  gint32 offset;

  g_return_val_if_fail (IDE_IS_GI_PARSER_RESULT (self), -1);
  g_return_val_if_fail (blob != NULL, -1);

  table = self->tables [IDE_GI_NS_TABLE_RECORD];
  offset = table->len;
  g_array_append_val (table, *blob);

  return offset;
}

gint32
ide_gi_parser_result_add_signal (IdeGiParserResult *self,
                                 IdeGiSignalBlob   *blob)
{
  GArray *table;
  gint32 offset;

  g_return_val_if_fail (IDE_IS_GI_PARSER_RESULT (self), -1);
  g_return_val_if_fail (blob != NULL, -1);

  table = self->tables [IDE_GI_NS_TABLE_SIGNAL];
  offset = table->len;
  g_array_append_val (table, *blob);

  return offset;
}

IdeGiTypeRef
ide_gi_parser_result_add_type (IdeGiParserResult *self,
                               IdeGiTypeBlob     *blob)
{
  GArray *table;
  IdeGiTypeRef ref = {0};

  g_return_val_if_fail (IDE_IS_GI_PARSER_RESULT (self), ref);
  g_return_val_if_fail (blob != NULL, ref);

  table = self->tables [IDE_GI_NS_TABLE_TYPE];
  g_array_append_val (table, *blob);

  ref.offset = table->len - 1;
  ref.type = blob->basic_type;

  return ref;
}

gint32
ide_gi_parser_result_add_union (IdeGiParserResult *self,
                                IdeGiUnionBlob    *blob)
{
  GArray *table;
  gint32 offset;

  g_return_val_if_fail (IDE_IS_GI_PARSER_RESULT (self), -1);
  g_return_val_if_fail (blob != NULL, -1);

  table = self->tables [IDE_GI_NS_TABLE_UNION];
  offset = table->len;
  g_array_append_val (table, *blob);

  return offset;
}

gint32
ide_gi_parser_result_add_value (IdeGiParserResult *self,
                                IdeGiValueBlob    *blob)
{
  GArray *table;
  gint32 offset;

  g_return_val_if_fail (IDE_IS_GI_PARSER_RESULT (self), -1);
  g_return_val_if_fail (blob != NULL, -1);

  table = self->tables [IDE_GI_NS_TABLE_VALUE];
  offset = table->len;
  g_array_append_val (table, *blob);

  return offset;
}

gint32
ide_gi_parser_result_add_crossref (IdeGiParserResult *self,
                                   IdeGiBlobType      type,
                                   const gchar       *qname,
                                   gboolean           is_local)
{
  IdeGiCrossRef crossref;
  gint32 offset;

  g_return_val_if_fail (IDE_IS_GI_PARSER_RESULT (self), -1);
  g_return_val_if_fail (!dzl_str_empty0 (qname), -1);
  g_return_val_if_fail (strchr (qname, '.') != NULL, -1);

  offset = self->crossrefs->len;
  crossref.type = type;
  crossref.qname = ide_gi_parser_result_add_string (self, qname);
  crossref.is_local = is_local;

  g_array_append_val (self->crossrefs, crossref);

  return offset;
}

GArray *
ide_gi_parser_result_get_crossrefs (IdeGiParserResult *self)
{
  g_return_val_if_fail (IDE_IS_GI_PARSER_RESULT (self), NULL);

  return g_array_ref (self->crossrefs);
}

GFile *
ide_gi_parser_result_get_file (IdeGiParserResult *self)
{
  g_return_val_if_fail (IDE_IS_GI_PARSER_RESULT (self), NULL);

  return g_object_ref (self->file);
}

void
ide_gi_parser_result_set_parser (IdeGiParserResult *self,
                                 IdeGiParser       *parser)
{
  g_return_if_fail (IDE_IS_GI_PARSER_RESULT (self));
  g_return_if_fail (IDE_IS_GI_PARSER (parser));

  g_clear_object (&self->parser);
  self->parser = g_object_ref (parser);
}

IdeGiParser *
ide_gi_parser_result_get_parser (IdeGiParserResult *self)
{
  g_return_val_if_fail (IDE_IS_GI_PARSER_RESULT (self), NULL);

  return self->parser;
}

void
ide_gi_parser_result_set_pool (IdeGiParserResult *self,
                               IdeGiPool         *pool)
{
  g_return_if_fail (IDE_IS_GI_PARSER_RESULT (self));
  g_return_if_fail (IDE_IS_GI_POOL (pool));

  g_set_object (&self->pool, pool);
}

IdeGiPool *
ide_gi_parser_result_get_pool (IdeGiParserResult *self)
{
  g_return_val_if_fail (IDE_IS_GI_PARSER_RESULT (self), NULL);

  return self->pool;
}

const IdeGiHeaderBlob *
ide_gi_parser_result_get_header (IdeGiParserResult *self)
{
  g_return_val_if_fail (IDE_IS_GI_PARSER_RESULT (self), NULL);

  return &self->header_blob;
}

void ide_gi_parser_result_set_header (IdeGiParserResult *self,
                                      IdeGiHeaderBlob   *header)
{
  g_return_if_fail (IDE_IS_GI_PARSER_RESULT (self));
  g_return_if_fail (header != NULL);

  self->header_blob = *header;
}

static void
ide_gi_parser_result_get_property (GObject    *object,
                                   guint       prop_id,
                                   GValue     *value,
                                   GParamSpec *pspec)
{
  IdeGiParserResult *self = IDE_GI_PARSER_RESULT (object);

  switch (prop_id)
    {
    case PROP_FILE:
      g_value_set_object (value, self->file);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
ide_gi_parser_result_set_property (GObject      *object,
                                   guint         prop_id,
                                   const GValue *value,
                                   GParamSpec   *pspec)
{
  IdeGiParserResult *self = IDE_GI_PARSER_RESULT (object);

  switch (prop_id)
    {
    case PROP_FILE:
      g_set_object (&self->file, g_value_get_object (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

IdeGiParserResult *
ide_gi_parser_result_new (GFile *file)
{
  return g_object_new (IDE_TYPE_GI_PARSER_RESULT,
                       "file", file,
                       NULL);
}

static void
ide_gi_parser_result_finalize (GObject *object)
{
  IdeGiParserResult *self = (IdeGiParserResult *)object;

  g_clear_object (&self->file);
  g_clear_object (&self->parser);
  g_clear_object (&self->pool);
  g_clear_object (&self->object_index);

  dzl_clear_pointer (&self->global_index, g_array_unref);
  dzl_clear_pointer (&self->crossrefs, g_array_unref);

  for (guint i = 0; i < IDE_GI_NS_TABLE_NB_TABLES; i++)
    g_array_unref (self->tables[i]);

  dzl_clear_pointer (&self->strings, g_byte_array_unref);
  dzl_clear_pointer (&self->header_strings, g_byte_array_unref);
  dzl_clear_pointer (&self->annotation_strings, g_byte_array_unref);
  dzl_clear_pointer (&self->doc_strings, g_byte_array_unref);

  G_OBJECT_CLASS (ide_gi_parser_result_parent_class)->finalize (object);
}

static void
ide_gi_parser_result_class_init (IdeGiParserResultClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = ide_gi_parser_result_finalize;
  object_class->get_property = ide_gi_parser_result_get_property;
  object_class->set_property = ide_gi_parser_result_set_property;

  properties [PROP_FILE] =
    g_param_spec_object ("file",
                         "Parsed File",
                         "The parsed file.",
                         G_TYPE_FILE,
                         (G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, N_PROPS, properties);
}

static void
ide_gi_parser_result_init (IdeGiParserResult *self)
{
  self->header_strings = g_byte_array_new ();
  self->strings = g_byte_array_new ();
  self->doc_strings = g_byte_array_new ();
  self->annotation_strings = g_byte_array_new ();

  /* add a zero at start so offset 0 return an empty string */
  g_byte_array_append (self->header_strings, (guint8 *)"\0", 1);
  g_byte_array_append (self->strings, (guint8 *)"\0", 1);
  g_byte_array_append (self->doc_strings, (guint8 *)"\0", 1);
  g_byte_array_append (self->annotation_strings, (guint8 *)"\0", 1);

  self->tables [IDE_GI_NS_TABLE_ALIAS] = g_array_new (FALSE, TRUE, sizeof (IdeGiAliasBlob));
  self->tables [IDE_GI_NS_TABLE_ARRAY] = g_array_new (FALSE, TRUE, sizeof (IdeGiArrayBlob));
  self->tables [IDE_GI_NS_TABLE_CALLBACK] = g_array_new (FALSE, TRUE, sizeof (IdeGiCallbackBlob));
  self->tables [IDE_GI_NS_TABLE_CONSTANT] = g_array_new (FALSE, TRUE, sizeof (IdeGiConstantBlob));
  self->tables [IDE_GI_NS_TABLE_DOC] = g_array_new (FALSE, TRUE, sizeof (IdeGiDocBlob));
  self->tables [IDE_GI_NS_TABLE_ENUM] = g_array_new (FALSE, TRUE, sizeof (IdeGiEnumBlob));
  self->tables [IDE_GI_NS_TABLE_FIELD] = g_array_new (FALSE, TRUE, sizeof (IdeGiFieldBlob));
  self->tables [IDE_GI_NS_TABLE_FUNCTION] = g_array_new (FALSE, TRUE, sizeof (IdeGiFunctionBlob));
  self->tables [IDE_GI_NS_TABLE_OBJECT] = g_array_new (FALSE, TRUE, sizeof (IdeGiObjectBlob));
  self->tables [IDE_GI_NS_TABLE_PARAMETER] = g_array_new (FALSE, TRUE, sizeof (IdeGiParameterBlob));
  self->tables [IDE_GI_NS_TABLE_PROPERTY] = g_array_new (FALSE, TRUE, sizeof (IdeGiPropertyBlob));
  self->tables [IDE_GI_NS_TABLE_RECORD] = g_array_new (FALSE, TRUE, sizeof (IdeGiRecordBlob));
  self->tables [IDE_GI_NS_TABLE_SIGNAL] = g_array_new (FALSE, TRUE, sizeof (IdeGiSignalBlob));
  self->tables [IDE_GI_NS_TABLE_TYPE] = g_array_new (FALSE, TRUE, sizeof (IdeGiTypeBlob));
  self->tables [IDE_GI_NS_TABLE_UNION] = g_array_new (FALSE, TRUE, sizeof (IdeGiUnionBlob));
  self->tables [IDE_GI_NS_TABLE_VALUE] = g_array_new (FALSE, TRUE, sizeof (IdeGiValueBlob));

  self->object_index = ide_gi_radix_tree_builder_new ();
  self->global_index = g_array_new (FALSE, FALSE, sizeof (IdeGiGlobalIndexEntry));
  self->crossrefs = g_array_new (FALSE, FALSE, sizeof (IdeGiCrossRef));
}
