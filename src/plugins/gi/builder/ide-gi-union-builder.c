/* ide-gi-union-builder.c
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

#include <ide.h>
#include <string.h>

#include "ide-gi-union-builder.h"

#include "ide-gi-doc-builder.h"
#include "ide-gi-field-builder.h"
#include "ide-gi-record-builder.h"
#include "ide-gi-function-builder.h"

struct _IdeGiUnionBuilder
{
  IdeGiParserObject  parent_instance;

  IdeGiDocBlob       doc_blob;
  IdeGiUnionBlob     blob;

  GArray            *fields;
  GArray            *functions;
  GArray            *records;

  guint              has_doc_blob : 1;
};

G_DEFINE_TYPE (IdeGiUnionBuilder, ide_gi_union_builder, IDE_TYPE_GI_PARSER_OBJECT)

static void
ide_gi_union_builder_index (IdeGiParserObject *parse_object,
                            IdeGiParserResult *result,
                            gpointer           user_data)
{
  IdeGiUnionBuilder *self = (IdeGiUnionBuilder *)parse_object;
  gint32 offset = GPOINTER_TO_INT (user_data);
  const gchar *name;

  g_return_if_fail (IDE_IS_GI_PARSER_OBJECT (self));
  g_return_if_fail (IDE_IS_GI_PARSER_RESULT (result));

  name = ide_gi_parser_result_get_string (result, self->blob.common.name);
  ide_gi_parser_result_add_object_index (result, name, IDE_GI_BLOB_TYPE_UNION, offset);

  name = ide_gi_parser_result_get_string (result, self->blob.g_type_name);
  if (!dzl_str_empty0 (name))
    ide_gi_parser_result_add_global_index (result,
                                           name,
                                           offset,
                                           IDE_GI_PREFIX_TYPE_GTYPE,
                                           IDE_GI_BLOB_TYPE_UNION,
                                           FALSE);
}

static void
ide_gi_union_builder_start_element (GMarkupParseContext  *context,
                                    const gchar          *element_name,
                                    const gchar         **attribute_names,
                                    const gchar         **attribute_values,
                                    gpointer              user_data,
                                    GError              **error)
{
  IdeGiUnionBuilder *self = (IdeGiUnionBuilder *)user_data;
  IdeGiParserResult *result;
  IdeGiPool *pool;
  IdeGiParserObject *child;
  IdeGiElementType element_type;

  g_assert (IDE_IS_GI_UNION_BUILDER (self));
  g_assert (context != NULL);
  g_assert (element_name != NULL);
  g_assert (attribute_names != NULL);
  g_assert (attribute_values != NULL);

  result = ide_gi_parser_object_get_result (IDE_GI_PARSER_OBJECT (self));
  pool = ide_gi_parser_result_get_pool (result);
  element_type = ide_gi_parser_get_element_type (element_name);

  if (element_type & IDE_GI_PARSER_ELEMENT_MASK_UNION)
    {
      child = ide_gi_pool_get_object (pool, element_type);

      if (!ide_gi_parser_object_parse (IDE_GI_PARSER_OBJECT (child),
                                       context,
                                       result,
                                       element_name,
                                       attribute_names,
                                       attribute_values,
                                       error))
        return;
    }
  else
    {
      g_autoptr(GFile) file = ide_gi_parser_result_get_file (result);

      ide_gi_pool_set_unhandled_element (pool, element_name);
      child = ide_gi_pool_get_current_parser_object (pool);
      ide_gi_helper_parsing_error (child, context, file);

      return;
    }
}

static void
ide_gi_union_builder_end_element (GMarkupParseContext  *context,
                                  const gchar          *element_name,
                                  gpointer              user_data,
                                  GError              **error)
{
  IdeGiUnionBuilder *self = (IdeGiUnionBuilder *)user_data;
  IdeGiParserResult *result;
  IdeGiPool *pool;
  IdeGiParserObject *child;
  IdeGiElementType element_type;

  g_assert (IDE_IS_GI_UNION_BUILDER (self));
  g_assert (context != NULL);
  g_assert (element_name != NULL);

  result = ide_gi_parser_object_get_result (IDE_GI_PARSER_OBJECT (self));
  pool = ide_gi_parser_result_get_pool (result);
  child = ide_gi_pool_get_current_parser_object (pool);
  element_type = ide_gi_parser_get_element_type (element_name);

  if (element_type & IDE_GI_PARSER_ELEMENT_MASK_UNION)
    {
      if (element_type & IDE_GI_PARSER_ELEMENT_MASK_DOC)
        {
          g_autofree gchar *str = ide_gi_parser_object_finish (child);
          ide_gi_helper_update_doc_blob (result, &self->doc_blob, element_type, str);
          self->has_doc_blob = TRUE;
        }
      else if (element_type == IDE_GI_ELEMENT_TYPE_CONSTRUCTOR ||
               element_type == IDE_GI_ELEMENT_TYPE_FUNCTION ||
               element_type == IDE_GI_ELEMENT_TYPE_METHOD)
        {
          IdeGiFunctionBlob *blob = ide_gi_parser_object_finish (child);

          if (self->functions == NULL)
            self->functions = g_array_new (FALSE, FALSE, sizeof (IdeGiFunctionBlob));

          g_array_append_val (self->functions, *blob);
        }
      else if (element_type == IDE_GI_ELEMENT_TYPE_BITFIELD)
        {
          IdeGiFieldBlob *blob = ide_gi_parser_object_finish (child);

          if (self->fields == NULL)
            self->fields = g_array_new (FALSE, FALSE, sizeof (IdeGiFieldBlob));

          g_array_append_val (self->fields, *blob);
        }
      else // IDE_GI_ELEMENT_TYPE_RECORD
        {
          IdeGiRecordBlob *blob = ide_gi_parser_object_finish (child);

          if (self->records == NULL)
            self->records = g_array_new (FALSE, FALSE, sizeof (IdeGiRecordBlob));

          g_array_append_val (self->records, *blob);
        }

      ide_gi_pool_release_object (pool);
      g_markup_parse_context_pop (context);
    }
  else if (!dzl_str_equal0 (element_name, ide_gi_pool_get_unhandled_element (pool)))
    {
      g_autoptr(GFile) file = ide_gi_parser_result_get_file (result);

      ide_gi_helper_parsing_error (child, context, file);
    }
}

static const GMarkupParser markup_parser = {
  ide_gi_union_builder_start_element,
  ide_gi_union_builder_end_element,
  NULL,
  NULL,
  NULL,
};

static gboolean
ide_gi_union_builder_parse (IdeGiParserObject    *parser_object,
                            GMarkupParseContext  *context,
                            IdeGiParserResult    *result,
                            const gchar          *element_name,
                            const gchar         **attribute_names,
                            const gchar         **attribute_values,
                            GError              **error)
{
  IdeGiUnionBuilder *self = IDE_GI_UNION_BUILDER (parser_object);
  gboolean introspectable;
  gboolean deprecated;
  IdeGiStability stability;

  g_assert (IDE_IS_GI_UNION_BUILDER (self));
  g_assert (IDE_IS_GI_PARSER_RESULT (result));
  g_assert (g_str_equal (element_name, "union"));

  if (!ide_gi_helper_markup_collect_attributes (result,
                                                context,
                                                element_name, attribute_names, attribute_values, error,
                                                IDE_GI_MARKUP_COLLECT_BOOLEAN | IDE_GI_MARKUP_COLLECT_OPTIONAL, "0", "introspectable", &introspectable,
                                                IDE_GI_MARKUP_COLLECT_BOOLEAN | IDE_GI_MARKUP_COLLECT_OPTIONAL, "0", "deprecated", &deprecated,
                                                IDE_GI_MARKUP_COLLECT_STABILITY | IDE_GI_MARKUP_COLLECT_OPTIONAL, "Stable", "stability", &stability,
                                                IDE_GI_MARKUP_COLLECT_OFFSET32_STRING | IDE_GI_MARKUP_COLLECT_OPTIONAL, "", "deprecated-version", &self->blob.common.deprecated_version,
                                                IDE_GI_MARKUP_COLLECT_OFFSET32_STRING | IDE_GI_MARKUP_COLLECT_OPTIONAL, "", "version", &self->blob.common.version,
                                                IDE_GI_MARKUP_COLLECT_OFFSET32_STRING | IDE_GI_MARKUP_COLLECT_OPTIONAL, "", "name", &self->blob.common.name,
                                                IDE_GI_MARKUP_COLLECT_OFFSET32_STRING | IDE_GI_MARKUP_COLLECT_OPTIONAL, "", "c:type", &self->blob.c_type,
                                                IDE_GI_MARKUP_COLLECT_OFFSET32_STRING | IDE_GI_MARKUP_COLLECT_OPTIONAL, "", "c:symbol-prefix", &self->blob.c_symbol_prefix,
                                                IDE_GI_MARKUP_COLLECT_OFFSET32_STRING | IDE_GI_MARKUP_COLLECT_OPTIONAL, "", "glib:get-type", &self->blob.g_get_type,
                                                IDE_GI_MARKUP_COLLECT_OFFSET32_STRING | IDE_GI_MARKUP_COLLECT_OPTIONAL, "", "glib:type-name", &self->blob.g_type_name,
                                                IDE_GI_MARKUP_COLLECT_INVALID, NULL, NULL, NULL, NULL))
    return FALSE;

  self->blob.common.blob_type = IDE_GI_BLOB_TYPE_UNION;

  self->blob.common.introspectable = introspectable;
  self->blob.common.deprecated = deprecated;
  self->blob.common.stability = stability;

  ide_gi_parser_object_set_result (parser_object, result);
  g_markup_parse_context_push (context, &markup_parser, self);

  return TRUE;
}

static gpointer
ide_gi_union_builder_finish (IdeGiParserObject *parser_object)
{
  IdeGiUnionBuilder *self = IDE_GI_UNION_BUILDER (parser_object);
  IdeGiParserResult *result = ide_gi_parser_object_get_result (IDE_GI_PARSER_OBJECT (self));

  if (self->has_doc_blob)
    {
      self->doc_blob.blob_type = IDE_GI_BLOB_TYPE_DOC;
      self->blob.common.doc = ide_gi_parser_result_add_doc (result, &self->doc_blob);
    }
  else
    self->blob.common.doc = -1;

  if (self->fields != NULL)
    {
      self->blob.fields = ide_gi_parser_result_add_field (result, &g_array_index (self->fields, IdeGiFieldBlob, 0));
      if (self->fields->len > 1)
        for (guint i = 1; i < self->fields->len; i++)
          ide_gi_parser_result_add_field (result, &g_array_index (self->fields, IdeGiFieldBlob, i));

      self->blob.n_fields = self->fields->len;
      dzl_clear_pointer (&self->fields, g_array_unref);
    }

  if (self->functions != NULL)
    {
      self->blob.functions = ide_gi_parser_result_add_function (result, &g_array_index (self->functions, IdeGiFunctionBlob, 0));
      if (self->functions->len > 1)
        for (guint i = 1; i < self->functions->len; i++)
          ide_gi_parser_result_add_function (result, &g_array_index (self->functions, IdeGiFunctionBlob, i));

      self->blob.n_functions = self->functions->len;
      dzl_clear_pointer (&self->functions, g_array_unref);
    }

  if (self->records != NULL)
    {
      self->blob.records = ide_gi_parser_result_add_record (result, &g_array_index (self->records, IdeGiRecordBlob, 0));
      if (self->records->len > 1)
        for (guint i = 1; i < self->records->len; i++)
          ide_gi_parser_result_add_record (result, &g_array_index (self->records, IdeGiRecordBlob, i));

      self->blob.n_records = self->records->len;
      dzl_clear_pointer (&self->records, g_array_unref);
    }

  return &self->blob;
}

static void
ide_gi_union_builder_reset (IdeGiParserObject *parser_object)
{
  IdeGiUnionBuilder *self = IDE_GI_UNION_BUILDER (parser_object);

  dzl_clear_pointer (&self->fields, g_array_unref);
  dzl_clear_pointer (&self->functions, g_array_unref);
  dzl_clear_pointer (&self->records, g_array_unref);

  memset (self + sizeof (IdeGiParserObject),
          0,
          sizeof (IdeGiUnionBuilder) - sizeof (IdeGiParserObject));
}

IdeGiParserObject *
ide_gi_union_builder_new (void)
{
  IdeGiParserObject *self;

  self = IDE_GI_PARSER_OBJECT (g_object_new (IDE_TYPE_GI_UNION_BUILDER, NULL));

  _ide_gi_parser_object_set_element_type (self, IDE_GI_ELEMENT_TYPE_UNION);
  return self;
}

static void
ide_gi_union_builder_finalize (GObject *object)
{
  IdeGiUnionBuilder *self = IDE_GI_UNION_BUILDER (object);

  dzl_clear_pointer (&self->fields, g_array_unref);
  dzl_clear_pointer (&self->functions, g_array_unref);
  dzl_clear_pointer (&self->records, g_array_unref);

  G_OBJECT_CLASS (ide_gi_union_builder_parent_class)->finalize (object);
}

static void
ide_gi_union_builder_class_init (IdeGiUnionBuilderClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  IdeGiParserObjectClass *parent_class = IDE_GI_PARSER_OBJECT_CLASS (klass);

  object_class->finalize = ide_gi_union_builder_finalize;

  parent_class->finish = ide_gi_union_builder_finish;
  parent_class->index = ide_gi_union_builder_index;
  parent_class->parse = ide_gi_union_builder_parse;
  parent_class->reset = ide_gi_union_builder_reset;
}

static void
ide_gi_union_builder_init (IdeGiUnionBuilder *self)
{
}
