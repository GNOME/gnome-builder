/* ide-gi-property-builder.c
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

#include "ide-gi-property-builder.h"

#include "ide-gi-array-builder.h"
#include "ide-gi-type-builder.h"

struct _IdeGiPropertyBuilder
{
  IdeGiParserObject  parent_instance;

  IdeGiDocBlob       doc_blob;
  IdeGiPropertyBlob  blob;

  guint              has_doc_blob : 1;
};

G_DEFINE_TYPE (IdeGiPropertyBuilder, ide_gi_property_builder, IDE_TYPE_GI_PARSER_OBJECT)

static void
ide_gi_property_builder_start_element (GMarkupParseContext  *context,
                                       const gchar          *element_name,
                                       const gchar         **attribute_names,
                                       const gchar         **attribute_values,
                                       gpointer              user_data,
                                       GError              **error)
{
  IdeGiPropertyBuilder *self = (IdeGiPropertyBuilder *)user_data;
  IdeGiParserResult *result;
  IdeGiPool *pool;
  IdeGiParserObject *child;
  IdeGiElementType element_type;

  g_assert (IDE_IS_GI_PROPERTY_BUILDER (self));
  g_assert (context != NULL);
  g_assert (element_name != NULL);
  g_assert (attribute_names != NULL);
  g_assert (attribute_values != NULL);

  result = ide_gi_parser_object_get_result (IDE_GI_PARSER_OBJECT (self));
  pool = ide_gi_parser_result_get_pool (result);
  element_type = ide_gi_parser_get_element_type (element_name);

  if (element_type & IDE_GI_PARSER_ELEMENT_MASK_PROPERTY)
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
ide_gi_property_builder_end_element (GMarkupParseContext  *context,
                                     const gchar          *element_name,
                                     gpointer              user_data,
                                     GError              **error)
{
  IdeGiPropertyBuilder *self = (IdeGiPropertyBuilder *)user_data;
  IdeGiParserResult *result;
  IdeGiPool *pool;
  IdeGiParserObject *child;
  IdeGiElementType element_type;

  g_assert (IDE_IS_GI_PROPERTY_BUILDER (self));
  g_assert (context != NULL);
  g_assert (element_name != NULL);

  result = ide_gi_parser_object_get_result (IDE_GI_PARSER_OBJECT (self));
  pool = ide_gi_parser_result_get_pool (result);
  child = ide_gi_pool_get_current_parser_object (pool);
  element_type = ide_gi_parser_get_element_type (element_name);

  if (element_type & IDE_GI_PARSER_ELEMENT_MASK_PROPERTY)
    {
      if (element_type & IDE_GI_PARSER_ELEMENT_MASK_DOC)
        {
          g_autofree gchar *str = ide_gi_parser_object_finish (child);
          ide_gi_helper_update_doc_blob (result, &self->doc_blob, element_type, str);
          self->has_doc_blob = TRUE;
        }
      else if (element_type == IDE_GI_ELEMENT_TYPE_ARRAY)
        {
          IdeGiArrayBlob *blob = ide_gi_parser_object_finish (child);

          if (self->blob.type_ref.type != IDE_GI_BASIC_TYPE_NONE)
            {
              g_autoptr(GFile) file = ide_gi_parser_result_get_file (result);
              ide_gi_helper_parsing_error_custom (IDE_GI_PARSER_OBJECT (self),
                                                  context,
                                                  file,
                                                  "type_ref already set");
            }

          self->blob.type_ref = ide_gi_parser_result_add_array (result, blob);
        }
      else // IDE_GI_ELEMENT_TYPE_TYPE
        {
          IdeGiTypeBlob *blob = ide_gi_parser_object_finish (child);

          if (self->blob.type_ref.type != IDE_GI_BASIC_TYPE_NONE)
            {
              g_autoptr(GFile) file = ide_gi_parser_result_get_file (result);
              ide_gi_helper_parsing_error_custom (IDE_GI_PARSER_OBJECT (self),
                                                  context,
                                                  file,
                                                  "type_ref already set");
            }

          self->blob.type_ref = ide_gi_parser_result_add_type (result, blob);
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
  ide_gi_property_builder_start_element,
  ide_gi_property_builder_end_element,
  NULL,
  NULL,
  NULL,
};

static gboolean
ide_gi_property_builder_parse (IdeGiParserObject    *parser_object,
                               GMarkupParseContext  *context,
                               IdeGiParserResult    *result,
                               const gchar          *element_name,
                               const gchar         **attribute_names,
                               const gchar         **attribute_values,
                               GError              **error)
{
  IdeGiPropertyBuilder *self = IDE_GI_PROPERTY_BUILDER (parser_object);
  gboolean introspectable;
  gboolean deprecated;
  gboolean writable;
  gboolean readable;
  gboolean construct;
  gboolean construct_only;
  IdeGiStability stability;
  IdeGiTransferOwnership transfer_ownership;

  g_assert (IDE_IS_GI_PROPERTY_BUILDER (self));
  g_assert (IDE_IS_GI_PARSER_RESULT (result));
  g_assert (g_str_equal (element_name, "property"));

  if (!ide_gi_helper_markup_collect_attributes (result,
                                                context,
                                                element_name, attribute_names, attribute_values, error,
                                                IDE_GI_MARKUP_COLLECT_BOOLEAN | IDE_GI_MARKUP_COLLECT_OPTIONAL, "0", "introspectable", &introspectable,
                                                IDE_GI_MARKUP_COLLECT_BOOLEAN | IDE_GI_MARKUP_COLLECT_OPTIONAL, "0", "deprecated", &deprecated,
                                                IDE_GI_MARKUP_COLLECT_BOOLEAN | IDE_GI_MARKUP_COLLECT_OPTIONAL, "0", "writable", &writable,
                                                IDE_GI_MARKUP_COLLECT_BOOLEAN | IDE_GI_MARKUP_COLLECT_OPTIONAL, "0", "readable", &readable,
                                                IDE_GI_MARKUP_COLLECT_BOOLEAN | IDE_GI_MARKUP_COLLECT_OPTIONAL, "0", "construct", &construct,
                                                IDE_GI_MARKUP_COLLECT_BOOLEAN | IDE_GI_MARKUP_COLLECT_OPTIONAL, "0", "construct-only", &construct_only,
                                                IDE_GI_MARKUP_COLLECT_STABILITY | IDE_GI_MARKUP_COLLECT_OPTIONAL, "Stable", "stability", &stability,
                                                IDE_GI_MARKUP_COLLECT_TRANSFER_OWNERSHIP | IDE_GI_MARKUP_COLLECT_OPTIONAL, "none", "transfer-ownership", &transfer_ownership,
                                                IDE_GI_MARKUP_COLLECT_OFFSET32_STRING | IDE_GI_MARKUP_COLLECT_OPTIONAL, "", "version", &self->blob.common.version,
                                                IDE_GI_MARKUP_COLLECT_OFFSET32_STRING | IDE_GI_MARKUP_COLLECT_OPTIONAL, "", "deprecated-version", &self->blob.common.deprecated_version,
                                                IDE_GI_MARKUP_COLLECT_OFFSET32_STRING | IDE_GI_MARKUP_COLLECT_OPTIONAL, "", "name", &self->blob.common.name,
                                                IDE_GI_MARKUP_COLLECT_INVALID, NULL, NULL, NULL))
    return FALSE;

  self->blob.common.blob_type = IDE_GI_BLOB_TYPE_PROPERTY;

  self->blob.common.introspectable = introspectable;
  self->blob.common.deprecated = deprecated;
  self->blob.common.stability = stability;
  self->blob.writable = writable;
  self->blob.readable = readable;
  self->blob.construct = construct;
  self->blob.construct_only = construct_only;
  self->blob.transfer_ownership = transfer_ownership;

  ide_gi_parser_object_set_result (parser_object, result);
  g_markup_parse_context_push (context, &markup_parser, self);

  return TRUE;
}

static gpointer
ide_gi_property_builder_finish (IdeGiParserObject *parser_object)
{
  IdeGiPropertyBuilder *self = IDE_GI_PROPERTY_BUILDER (parser_object);
  IdeGiParserResult *result = ide_gi_parser_object_get_result (IDE_GI_PARSER_OBJECT (self));

  if (self->has_doc_blob)
    {
      self->doc_blob.blob_type = IDE_GI_BLOB_TYPE_DOC;
      self->blob.common.doc = ide_gi_parser_result_add_doc (result, &self->doc_blob);
    }
  else
    self->blob.common.doc = -1;

  return &self->blob;
}

static void
ide_gi_property_builder_reset (IdeGiParserObject *parser_object)
{
  IdeGiPropertyBuilder *self = IDE_GI_PROPERTY_BUILDER (parser_object);

  memset (self + sizeof (IdeGiParserObject),
          0,
          sizeof (IdeGiPropertyBuilder) - sizeof (IdeGiParserObject));
}

IdeGiParserObject *
ide_gi_property_builder_new (void)
{
  IdeGiParserObject *self;

  self = IDE_GI_PARSER_OBJECT (g_object_new (IDE_TYPE_GI_PROPERTY_BUILDER, NULL));

  _ide_gi_parser_object_set_element_type (self, IDE_GI_ELEMENT_TYPE_PROPERTY);
  return self;
}

static void
ide_gi_property_builder_finalize (GObject *object)
{
  G_OBJECT_CLASS (ide_gi_property_builder_parent_class)->finalize (object);
}

static void
ide_gi_property_builder_class_init (IdeGiPropertyBuilderClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  IdeGiParserObjectClass *parent_class = IDE_GI_PARSER_OBJECT_CLASS (klass);

  object_class->finalize = ide_gi_property_builder_finalize;

  parent_class->finish = ide_gi_property_builder_finish;
  parent_class->parse = ide_gi_property_builder_parse;
  parent_class->reset = ide_gi_property_builder_reset;
}

static void
ide_gi_property_builder_init (IdeGiPropertyBuilder *self)
{
}
