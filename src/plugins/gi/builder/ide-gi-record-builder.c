/* ide-gi-record-builder.c
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

#include "ide-gi-record-builder.h"

#include "ide-gi-doc-builder.h"
#include "ide-gi-field-builder.h"
#include "ide-gi-function-builder.h"
#include "ide-gi-property-builder.h"
#include "ide-gi-record-builder.h"
#include "ide-gi-union-builder.h"

struct _IdeGiRecordBuilder
{
  IdeGiParserObject  parent_instance;

  IdeGiDocBlob       doc_blob;
  IdeGiRecordBlob    blob;

  GArray            *callbacks;
  GArray            *fields;
  GArray            *functions;
  GArray            *properties;
  GArray            *unions;

  guint              has_doc_blob : 1;
};

G_DEFINE_TYPE (IdeGiRecordBuilder, ide_gi_record_builder, IDE_TYPE_GI_PARSER_OBJECT)

static void
ide_gi_record_builder_index (IdeGiParserObject *parse_object,
                             IdeGiParserResult *result,
                             gpointer           user_data)
{
  IdeGiRecordBuilder *self = (IdeGiRecordBuilder *)parse_object;
  gint32 offset = GPOINTER_TO_INT (user_data);
  const gchar *name;

  g_return_if_fail (IDE_IS_GI_PARSER_OBJECT (self));
  g_return_if_fail (IDE_IS_GI_PARSER_RESULT (result));

  /* glib:boxed doesn't have a name */
  name = ide_gi_parser_result_get_string (result, self->blob.common.name);
  if (!dzl_str_empty0 (name))
    ide_gi_parser_result_add_object_index (result, name, IDE_GI_BLOB_TYPE_RECORD, offset);

  /* Not all records have a GType  */
  name = ide_gi_parser_result_get_string (result, self->blob.g_type_name);
  if (!dzl_str_empty0 (name))
    ide_gi_parser_result_add_global_index (result,
                                           name,
                                           offset,
                                           IDE_GI_PREFIX_TYPE_GTYPE,
                                           IDE_GI_BLOB_TYPE_RECORD,
                                           FALSE);
}

static void
ide_gi_record_builder_start_element (GMarkupParseContext  *context,
                                     const gchar          *element_name,
                                     const gchar         **attribute_names,
                                     const gchar         **attribute_values,
                                     gpointer              user_data,
                                     GError              **error)
{
  IdeGiRecordBuilder *self = (IdeGiRecordBuilder *)user_data;
  IdeGiParserResult *result;
  IdeGiPool *pool;
  IdeGiParserObject *child;
  IdeGiElementType element_type;

  g_assert (IDE_IS_GI_RECORD_BUILDER (self));
  g_assert (context != NULL);
  g_assert (element_name != NULL);
  g_assert (attribute_names != NULL);
  g_assert (attribute_values != NULL);

  result = ide_gi_parser_object_get_result (IDE_GI_PARSER_OBJECT (self));
  pool = ide_gi_parser_result_get_pool (result);
  element_type = ide_gi_parser_get_element_type (element_name);

  if (element_type & IDE_GI_PARSER_ELEMENT_MASK_RECORD)
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
ide_gi_record_builder_end_element (GMarkupParseContext  *context,
                                   const gchar          *element_name,
                                   gpointer              user_data,
                                   GError              **error)
{
  IdeGiRecordBuilder *self = (IdeGiRecordBuilder *)user_data;
  IdeGiParserResult *result;
  IdeGiPool *pool;
  IdeGiParserObject *child;
  IdeGiElementType element_type;

  g_assert (IDE_IS_GI_RECORD_BUILDER (self));
  g_assert (context != NULL);
  g_assert (element_name != NULL);

  result = ide_gi_parser_object_get_result (IDE_GI_PARSER_OBJECT (self));
  pool = ide_gi_parser_result_get_pool (result);
  child = ide_gi_pool_get_current_parser_object (pool);
  element_type = ide_gi_parser_get_element_type (element_name);

  if (element_type & IDE_GI_PARSER_ELEMENT_MASK_RECORD)
    {
      if (element_type & IDE_GI_PARSER_ELEMENT_MASK_DOC)
        {
          g_autofree gchar *str = ide_gi_parser_object_finish (child);
          ide_gi_helper_update_doc_blob (result, &self->doc_blob, element_type, str);
          self->has_doc_blob = TRUE;
        }
      else if (element_type == IDE_GI_ELEMENT_TYPE_CONSTRUCTOR ||
               element_type == IDE_GI_ELEMENT_TYPE_FUNCTION ||
               element_type == IDE_GI_ELEMENT_TYPE_METHOD ||
               element_type == IDE_GI_ELEMENT_TYPE_VIRTUAL_METHOD)
        {
          IdeGiFunctionBlob *blob = ide_gi_parser_object_finish (child);

          if (self->functions == NULL)
            self->functions = g_array_new (FALSE, FALSE, sizeof (IdeGiFunctionBlob));

          g_array_append_val (self->functions, *blob);
        }
      else if (element_type == IDE_GI_ELEMENT_TYPE_FIELD)
        {
          IdeGiFieldBlob *blob = ide_gi_parser_object_finish (child);

          if (self->fields == NULL)
            self->fields = g_array_new (FALSE, FALSE, sizeof (IdeGiFieldBlob));

          g_array_append_val (self->fields, *blob);
        }
      else if (element_type == IDE_GI_ELEMENT_TYPE_CALLBACK)
        {
          IdeGiCallbackBlob *blob = ide_gi_parser_object_finish (child);

          if (self->callbacks == NULL)
            self->callbacks = g_array_new (FALSE, FALSE, sizeof (IdeGiCallbackBlob));

          g_array_append_val (self->callbacks, *blob);
        }
      else if (element_type == IDE_GI_ELEMENT_TYPE_PROPERTY)
        {
          IdeGiPropertyBlob *blob = ide_gi_parser_object_finish (child);

          if (self->properties == NULL)
            self->properties = g_array_new (FALSE, FALSE, sizeof (IdeGiPropertyBlob));

          g_array_append_val (self->properties, *blob);
        }
      else // IDE_GI_ELEMENT_TYPE_UNION
        {
          IdeGiUnionBlob *blob = ide_gi_parser_object_finish (child);

          if (self->unions == NULL)
            self->unions = g_array_new (FALSE, FALSE, sizeof (IdeGiUnionBlob));

          g_array_append_val (self->unions, *blob);
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
  ide_gi_record_builder_start_element,
  ide_gi_record_builder_end_element,
  NULL,
  NULL,
  NULL,
};

static gboolean
ide_gi_record_builder_parse (IdeGiParserObject    *parser_object,
                             GMarkupParseContext  *context,
                             IdeGiParserResult    *result,
                             const gchar          *element_name,
                             const gchar         **attribute_names,
                             const gchar         **attribute_values,
                             GError              **error)
{
  IdeGiRecordBuilder *self = IDE_GI_RECORD_BUILDER (parser_object);
  gboolean introspectable;
  gboolean deprecated;
  gboolean  disguised;
  gboolean foreign;
  IdeGiStability stability;

  g_assert (IDE_IS_GI_RECORD_BUILDER (self));
  g_assert (IDE_IS_GI_PARSER_RESULT (result));
  g_assert (g_str_equal (element_name, "record") ||
            g_str_equal (element_name, "glib:boxed"));

  if (!ide_gi_helper_markup_collect_attributes (result,
                                                context,
                                                element_name, attribute_names, attribute_values, error,
                                                IDE_GI_MARKUP_COLLECT_BOOLEAN | IDE_GI_MARKUP_COLLECT_OPTIONAL, "0", "introspectable", &introspectable,
                                                IDE_GI_MARKUP_COLLECT_BOOLEAN | IDE_GI_MARKUP_COLLECT_OPTIONAL, "0", "deprecated", &deprecated,
                                                IDE_GI_MARKUP_COLLECT_BOOLEAN | IDE_GI_MARKUP_COLLECT_OPTIONAL, "0", "disguised", &disguised,
                                                IDE_GI_MARKUP_COLLECT_BOOLEAN | IDE_GI_MARKUP_COLLECT_OPTIONAL, "0", "foreign", &foreign,
                                                IDE_GI_MARKUP_COLLECT_STABILITY | IDE_GI_MARKUP_COLLECT_OPTIONAL, "Stable", "stability", &stability,
                                                IDE_GI_MARKUP_COLLECT_OFFSET32_STRING | IDE_GI_MARKUP_COLLECT_OPTIONAL, "", "deprecated-version", &self->blob.common.deprecated_version,
                                                IDE_GI_MARKUP_COLLECT_OFFSET32_STRING | IDE_GI_MARKUP_COLLECT_OPTIONAL, "", "version", &self->blob.common.version,
                                                IDE_GI_MARKUP_COLLECT_OFFSET32_STRING | IDE_GI_MARKUP_COLLECT_OPTIONAL, "", "name", &self->blob.common.name,
                                                IDE_GI_MARKUP_COLLECT_OFFSET32_STRING | IDE_GI_MARKUP_COLLECT_OPTIONAL, "", "c:type", &self->blob.c_type,
                                                IDE_GI_MARKUP_COLLECT_OFFSET32_STRING | IDE_GI_MARKUP_COLLECT_OPTIONAL, "", "glib:type-name", &self->blob.g_type_name,
                                                IDE_GI_MARKUP_COLLECT_OFFSET32_STRING | IDE_GI_MARKUP_COLLECT_OPTIONAL, "", "glib:get-type", &self->blob.g_get_type,
                                                IDE_GI_MARKUP_COLLECT_OFFSET32_STRING | IDE_GI_MARKUP_COLLECT_OPTIONAL, "", "c:symbol-prefix", &self->blob.c_symbol_prefix,
                                                IDE_GI_MARKUP_COLLECT_OFFSET32_STRING | IDE_GI_MARKUP_COLLECT_OPTIONAL, "", "glib:is-gtype-struct-for", &self->blob.g_is_gtype_struct_for,
                                                IDE_GI_MARKUP_COLLECT_INVALID, NULL, NULL, NULL))
    return FALSE;

  self->blob.common.blob_type = IDE_GI_BLOB_TYPE_RECORD;

  self->blob.common.introspectable = introspectable;
  self->blob.common.deprecated = deprecated;
  self->blob.common.stability = stability;
  self->blob.disguised = disguised;
  self->blob.foreign = foreign;

  ide_gi_parser_object_set_result (parser_object, result);
  g_markup_parse_context_push (context, &markup_parser, self);

  return TRUE;
}

static gpointer
ide_gi_record_builder_finish (IdeGiParserObject *parser_object)
{
  IdeGiRecordBuilder *self = IDE_GI_RECORD_BUILDER (parser_object);
  IdeGiParserResult *result = ide_gi_parser_object_get_result (IDE_GI_PARSER_OBJECT (self));

  if (self->has_doc_blob)
    {
      self->doc_blob.blob_type = IDE_GI_BLOB_TYPE_DOC;
      self->blob.common.doc = ide_gi_parser_result_add_doc (result, &self->doc_blob);
    }
  else
    self->blob.common.doc = -1;

  if (self->callbacks != NULL)
    {
      IdeGiTypeRef ref = ide_gi_parser_result_add_callback (result, &g_array_index (self->callbacks, IdeGiCallbackBlob, 0));
      self->blob.callbacks = ref.offset;
      if (self->callbacks->len > 1)
        for (guint i = 1; i < self->callbacks->len; i++)
          ide_gi_parser_result_add_callback (result, &g_array_index (self->callbacks, IdeGiCallbackBlob, i));

      self->blob.n_callbacks = self->callbacks->len;
      dzl_clear_pointer (&self->callbacks, g_array_unref);
    }

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

  if (self->properties != NULL)
    {
      self->blob.properties = ide_gi_parser_result_add_property (result, &g_array_index (self->properties, IdeGiPropertyBlob, 0));
      if (self->properties->len > 1)
        for (guint i = 1; i < self->properties->len; i++)
          ide_gi_parser_result_add_property (result, &g_array_index (self->properties, IdeGiPropertyBlob, i));

      self->blob.n_properties = self->properties->len;
      dzl_clear_pointer (&self->properties, g_array_unref);
    }

  if (self->unions != NULL)
    {
      self->blob.unions = ide_gi_parser_result_add_union (result, &g_array_index (self->unions, IdeGiUnionBlob, 0));
      if (self->unions->len > 1)
        for (guint i = 1; i < self->unions->len; i++)
          ide_gi_parser_result_add_union (result, &g_array_index (self->unions, IdeGiUnionBlob, i));

      self->blob.n_unions = self->unions->len;
      dzl_clear_pointer (&self->unions, g_array_unref);
    }

  return &self->blob;
}

static void
ide_gi_record_builder_reset (IdeGiParserObject *parser_object)
{
  IdeGiRecordBuilder *self = IDE_GI_RECORD_BUILDER (parser_object);

  dzl_clear_pointer (&self->callbacks, g_array_unref);
  dzl_clear_pointer (&self->fields, g_array_unref);
  dzl_clear_pointer (&self->functions, g_array_unref);
  dzl_clear_pointer (&self->properties, g_array_unref);
  dzl_clear_pointer (&self->unions, g_array_unref);

  memset (self + sizeof (IdeGiParserObject),
          0,
          sizeof (IdeGiRecordBuilder) - sizeof (IdeGiParserObject));
}

IdeGiParserObject *
ide_gi_record_builder_new (void)
{
  IdeGiParserObject *self;

  self = IDE_GI_PARSER_OBJECT (g_object_new (IDE_TYPE_GI_RECORD_BUILDER, NULL));

  _ide_gi_parser_object_set_element_type (self, IDE_GI_ELEMENT_TYPE_RECORD);
  return self;
}

static void
ide_gi_record_builder_finalize (GObject *object)
{
  IdeGiRecordBuilder *self = IDE_GI_RECORD_BUILDER (object);

  G_OBJECT_CLASS (ide_gi_record_builder_parent_class)->finalize (object);

  dzl_clear_pointer (&self->callbacks, g_array_unref);
  dzl_clear_pointer (&self->fields, g_array_unref);
  dzl_clear_pointer (&self->functions, g_array_unref);
  dzl_clear_pointer (&self->properties, g_array_unref);
  dzl_clear_pointer (&self->unions, g_array_unref);
}

static void
ide_gi_record_builder_class_init (IdeGiRecordBuilderClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  IdeGiParserObjectClass *parent_class = IDE_GI_PARSER_OBJECT_CLASS (klass);

  object_class->finalize = ide_gi_record_builder_finalize;

  parent_class->finish = ide_gi_record_builder_finish;
  parent_class->index = ide_gi_record_builder_index;
  parent_class->parse = ide_gi_record_builder_parse;
  parent_class->reset = ide_gi_record_builder_reset;
}

static void
ide_gi_record_builder_init (IdeGiRecordBuilder *self)
{
}
