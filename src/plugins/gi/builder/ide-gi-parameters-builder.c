/* ide-gi-parameters-builder.c
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

#include "ide-gi-parameters-builder.h"

#include "ide-gi-array-builder.h"
#include "ide-gi-type-builder.h"
#include "ide-gi-parameters-builder.h"

typedef enum {
  PARAMETER_TYPE_PARAMETER,
  PARAMETER_TYPE_INSTANCE_PARAMETER,
  PARAMETER_TYPE_RETURN_VALUE
} ParameterType;

struct _IdeGiParametersBuilder
{
  IdeGiParserObject    parent_instance;

  IdeGiDocBlob         doc_blob;
  IdeGiParameterBlob   blob;
  ParameterType        parameter_type;

  IdeGiParametersEntry parameters_entry;
  guint32              current_param_offset;

  guint                has_params : 1;
  guint                is_return_value : 1;
  guint                is_in_varargs : 1;
  guint                has_doc_blob : 1;
};

G_DEFINE_TYPE (IdeGiParametersBuilder, ide_gi_parameters_builder, IDE_TYPE_GI_PARSER_OBJECT)

static gboolean
ide_gi_parameters_builder_parse_varargs (IdeGiParserObject    *self,
                                         GMarkupParseContext  *context,
                                         IdeGiParserResult    *result,
                                         const gchar          *element_name,
                                         const gchar         **attribute_names,
                                         const gchar         **attribute_values,
                                         GError              **error)
{
  g_assert (IDE_IS_GI_PARAMETERS_BUILDER (self));
  g_assert (IDE_IS_GI_PARSER_RESULT (result));
  g_assert (g_str_equal (element_name, "varargs"));

  return TRUE;
}

static void
ide_gi_parameters_builder_parameter_start_element (GMarkupParseContext  *context,
                                                   const gchar          *element_name,
                                                   const gchar         **attribute_names,
                                                   const gchar         **attribute_values,
                                                   gpointer              user_data,
                                                   GError              **error)
{
  IdeGiParametersBuilder *self = (IdeGiParametersBuilder *)user_data;
  IdeGiParserResult *result;
  IdeGiParserObject *parser_object;
  IdeGiPool *pool;
  IdeGiElementType element_type;
  IdeGiParserObject *child;

  g_assert (IDE_IS_GI_PARAMETERS_BUILDER (self));
  g_assert (context != NULL);
  g_assert (element_name != NULL);
  g_assert (attribute_names != NULL);
  g_assert (attribute_values != NULL);

  result = ide_gi_parser_object_get_result (IDE_GI_PARSER_OBJECT (self));
  pool = ide_gi_parser_result_get_pool (result);
  parser_object = ide_gi_pool_get_current_parser_object (pool);
  //self = IDE_GI_PARAMETERS_BUILDER (parser_object);
  element_type = ide_gi_parser_get_element_type (element_name);

  g_assert ((void *)parser_object == (void *)self);

  if (self->is_in_varargs)
    {
      g_autoptr(GFile) file = ide_gi_parser_result_get_file (result);
      ide_gi_helper_parsing_error_custom (IDE_GI_PARSER_OBJECT (self),
                                          context,
                                          file,
                                          "We should not have sub-elements in <varargs>");
    }

  /* instance-parameter has type,
   * parameter has type, array and varargs,
   * return-value has type and array
   */
  if (element_type & IDE_GI_PARSER_ELEMENT_MASK_PARAMETER)
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
  else if (element_type == IDE_GI_ELEMENT_TYPE_VARARGS)
    {
      self->is_in_varargs = TRUE;
      ide_gi_parameters_builder_parse_varargs (parser_object,
                                               context,
                                               result,
                                               element_name,
                                               attribute_names,
                                               attribute_values,
                                               error);
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
ide_gi_parameters_builder_parameter_end_element (GMarkupParseContext  *context,
                                                 const gchar          *element_name,
                                                 gpointer              user_data,
                                                 GError              **error)
{
  IdeGiParametersBuilder *self = (IdeGiParametersBuilder *)user_data;
  IdeGiParserResult *result;
  IdeGiPool *pool;
  IdeGiParserObject *child;
  IdeGiElementType element_type;

  g_assert (IDE_IS_GI_PARAMETERS_BUILDER (self));
  g_assert (context != NULL);
  g_assert (element_name != NULL);

  result = ide_gi_parser_object_get_result (IDE_GI_PARSER_OBJECT (self));
  pool = ide_gi_parser_result_get_pool (result);
  child = ide_gi_pool_get_current_parser_object (pool);
  element_type = ide_gi_parser_get_element_type (element_name);

  if (element_type & IDE_GI_PARSER_ELEMENT_MASK_PARAMETER)
    {
      if (element_type & IDE_GI_PARSER_ELEMENT_MASK_DOC)
        {
          g_autofree gchar *str = ide_gi_parser_object_finish (child);
          ide_gi_helper_update_doc_blob (result, &self->doc_blob, element_type, str);
          self->has_doc_blob = TRUE;
        }
      else if (element_type == IDE_GI_ELEMENT_TYPE_TYPE)
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

      ide_gi_pool_release_object (pool);
      g_markup_parse_context_pop (context);
    }
  else if (element_type == IDE_GI_ELEMENT_TYPE_VARARGS)
    {
      /* Handle in a sub-parser */
      self = IDE_GI_PARAMETERS_BUILDER (child);

      self->is_in_varargs = FALSE;
      self->blob.flags |= IDE_GI_PARAMETER_FLAG_VARARGS;

      return;
    }
  else if (!dzl_str_equal0 (element_name, ide_gi_pool_get_unhandled_element (pool)))
    {
      g_autoptr(GFile) file = ide_gi_parser_result_get_file (result);

      ide_gi_helper_parsing_error (child, context, file);
    }
}

static const GMarkupParser markup_parameter_parser = {
  ide_gi_parameters_builder_parameter_start_element,
  ide_gi_parameters_builder_parameter_end_element,
  NULL,
  NULL,
  NULL,
};

static gboolean
ide_gi_parameters_builder_parameter_parse (IdeGiParserObject    *parser_object,
                                           GMarkupParseContext  *context,
                                           IdeGiParserResult    *result,
                                           ParameterType         parameter_type,
                                           const gchar          *element_name,
                                           const gchar         **attribute_names,
                                           const gchar         **attribute_values,
                                           GError              **error)
{
  IdeGiParametersBuilder *self = IDE_GI_PARAMETERS_BUILDER (parser_object);
  gboolean nullable = FALSE;
  gboolean allow_none = FALSE;
  gboolean introspectable = FALSE;
  gboolean caller_allocates = FALSE;
  gboolean optional = FALSE;
  gboolean skip = FALSE;
  gint64 closure = -1;
  gint64 destroy = -1;
  IdeGiDirection direction = IDE_GI_DIRECTION_IN;
  IdeGiTransferOwnership transfer_ownership = IDE_GI_TRANSFER_OWNERSHIP_NONE;
  IdeGiScope scope = IDE_GI_SCOPE_CALL;

  g_assert (IDE_IS_GI_PARAMETERS_BUILDER (parser_object));
  g_assert (IDE_IS_GI_PARSER_RESULT (result));
  g_assert (g_str_equal (element_name, "parameter") ||
            g_str_equal (element_name, "instance-parameter") ||
            g_str_equal (element_name, "return-value"));

  /* TODO: transfert ownership depend on the type */

  if (parameter_type == PARAMETER_TYPE_PARAMETER)
    {
      if (!ide_gi_helper_markup_collect_attributes (result,
                                                    context,
                                                    element_name, attribute_names, attribute_values, error,
                                                    IDE_GI_MARKUP_COLLECT_BOOLEAN | IDE_GI_MARKUP_COLLECT_OPTIONAL, "0", "nullable", &nullable,
                                                    IDE_GI_MARKUP_COLLECT_BOOLEAN | IDE_GI_MARKUP_COLLECT_OPTIONAL, "0", "allow-none", &allow_none,
                                                    IDE_GI_MARKUP_COLLECT_BOOLEAN | IDE_GI_MARKUP_COLLECT_OPTIONAL, "0", "introspectable", &introspectable,
                                                    IDE_GI_MARKUP_COLLECT_BOOLEAN | IDE_GI_MARKUP_COLLECT_OPTIONAL, "0", "caller-allocates", &caller_allocates,
                                                    IDE_GI_MARKUP_COLLECT_BOOLEAN | IDE_GI_MARKUP_COLLECT_OPTIONAL, "0", "optional", &optional,
                                                    IDE_GI_MARKUP_COLLECT_BOOLEAN | IDE_GI_MARKUP_COLLECT_OPTIONAL, "0", "skip", &skip,
                                                    IDE_GI_MARKUP_COLLECT_SCOPE | IDE_GI_MARKUP_COLLECT_OPTIONAL, "call", "scope", &scope,
                                                    IDE_GI_MARKUP_COLLECT_DIRECTION | IDE_GI_MARKUP_COLLECT_OPTIONAL, "in", "direction", &direction,
                                                    IDE_GI_MARKUP_COLLECT_TRANSFER_OWNERSHIP | IDE_GI_MARKUP_COLLECT_OPTIONAL, "none", "transfer-ownership", &transfer_ownership,
                                                    IDE_GI_MARKUP_COLLECT_INT64 | IDE_GI_MARKUP_COLLECT_OPTIONAL, "-1", "closure", &closure,
                                                    IDE_GI_MARKUP_COLLECT_INT64 | IDE_GI_MARKUP_COLLECT_OPTIONAL, "-1", "destroy", &destroy,
                                                    IDE_GI_MARKUP_COLLECT_OFFSET32_STRING | IDE_GI_MARKUP_COLLECT_OPTIONAL, "", "name", &self->blob.common.name,
                                                    IDE_GI_MARKUP_COLLECT_INVALID, NULL, NULL, NULL))
        return FALSE;
    }
  else if (parameter_type == PARAMETER_TYPE_INSTANCE_PARAMETER)
    {
      if (!ide_gi_helper_markup_collect_attributes (result,
                                                    context,
                                                    element_name, attribute_names, attribute_values, error,
                                                    IDE_GI_MARKUP_COLLECT_BOOLEAN | IDE_GI_MARKUP_COLLECT_OPTIONAL, "0", "nullable", &nullable,
                                                    IDE_GI_MARKUP_COLLECT_BOOLEAN | IDE_GI_MARKUP_COLLECT_OPTIONAL, "0", "allow-none", &allow_none,
                                                    IDE_GI_MARKUP_COLLECT_BOOLEAN | IDE_GI_MARKUP_COLLECT_OPTIONAL, "0", "caller-allocates", &caller_allocates,
                                                    IDE_GI_MARKUP_COLLECT_DIRECTION | IDE_GI_MARKUP_COLLECT_OPTIONAL, "in", "direction", &direction,
                                                    IDE_GI_MARKUP_COLLECT_TRANSFER_OWNERSHIP | IDE_GI_MARKUP_COLLECT_OPTIONAL, "none", "transfer-ownership", &transfer_ownership,
                                                    IDE_GI_MARKUP_COLLECT_OFFSET32_STRING | IDE_GI_MARKUP_COLLECT_OPTIONAL, "", "name", &self->blob.common.name,
                                                    IDE_GI_MARKUP_COLLECT_INVALID, NULL, NULL, NULL))
        return FALSE;

      self->blob.flags |= IDE_GI_PARAMETER_FLAG_INSTANCE_PARAMETER;
    }
  else if (parameter_type == PARAMETER_TYPE_RETURN_VALUE)
    {
      if (!ide_gi_helper_markup_collect_attributes (result,
                                                    context,
                                                    element_name, attribute_names, attribute_values, error,
                                                    IDE_GI_MARKUP_COLLECT_BOOLEAN | IDE_GI_MARKUP_COLLECT_OPTIONAL, "0", "nullable", &nullable,
                                                    IDE_GI_MARKUP_COLLECT_BOOLEAN | IDE_GI_MARKUP_COLLECT_OPTIONAL, "0", "allow-none", &allow_none,
                                                    IDE_GI_MARKUP_COLLECT_BOOLEAN | IDE_GI_MARKUP_COLLECT_OPTIONAL, "0", "introspectable", &introspectable,
                                                    IDE_GI_MARKUP_COLLECT_BOOLEAN | IDE_GI_MARKUP_COLLECT_OPTIONAL, "0", "skip", &skip,
                                                    IDE_GI_MARKUP_COLLECT_TRANSFER_OWNERSHIP | IDE_GI_MARKUP_COLLECT_OPTIONAL, "none", "transfer-ownership", &transfer_ownership,
                                                    IDE_GI_MARKUP_COLLECT_SCOPE | IDE_GI_MARKUP_COLLECT_OPTIONAL, "call", "scope", &scope,
                                                    IDE_GI_MARKUP_COLLECT_INT64 | IDE_GI_MARKUP_COLLECT_OPTIONAL, "-1", "closure", &closure,
                                                    IDE_GI_MARKUP_COLLECT_INT64 | IDE_GI_MARKUP_COLLECT_OPTIONAL, "-1", "destroy", &destroy,
                                                    IDE_GI_MARKUP_COLLECT_INVALID, NULL, NULL, NULL))
        return FALSE;

      self->blob.flags |= IDE_GI_PARAMETER_FLAG_RETURN_VALUE;
    }

  self->parameter_type = parameter_type;
  self->blob.common.blob_type = IDE_GI_BLOB_TYPE_PARAMETER;
  self->blob.common.introspectable = introspectable;

  if (nullable)
    self->blob.flags |= IDE_GI_PARAMETER_FLAG_NULLABLE;

  if (allow_none)
    self->blob.flags |= IDE_GI_PARAMETER_FLAG_ALLOW_NONE;

  if (caller_allocates)
    self->blob.flags |= IDE_GI_PARAMETER_FLAG_CALLER_ALLOCATES;

  if (optional)
    self->blob.flags |= IDE_GI_PARAMETER_FLAG_OPTIONAL;

  if (skip)
    self->blob.flags |= IDE_GI_PARAMETER_FLAG_SKIP;

  self->blob.direction = direction;
  self->blob.scope = scope;
  self->blob.transfer_ownership = transfer_ownership;

  if (closure > -1)
    {
      self->blob.flags |= IDE_GI_PARAMETER_FLAG_HAS_CLOSURE;
      self->blob.closure = closure;
    }

  if (destroy > -1)
    {
      self->blob.flags |= IDE_GI_PARAMETER_FLAG_HAS_DESTROY;
      self->blob.destroy = destroy;
    }

  ide_gi_parser_object_set_result (parser_object, result);
  g_markup_parse_context_push (context, &markup_parameter_parser, self);

  return TRUE;
}

static void
ide_gi_parameters_builder_start_element (GMarkupParseContext  *context,
                                         const gchar          *element_name,
                                         const gchar         **attribute_names,
                                         const gchar         **attribute_values,
                                         gpointer              user_data,
                                         GError              **error)
{
  IdeGiParametersBuilder *self = (IdeGiParametersBuilder *)user_data;
  IdeGiParserResult *result;
  IdeGiParserObject *parser_object;
  IdeGiPool *pool;
  IdeGiParserObject *child;
  IdeGiElementType element_type;
  ParameterType parameter_type;

  g_assert (IDE_IS_GI_PARAMETERS_BUILDER (self));
  g_assert (context != NULL);
  g_assert (element_name != NULL);
  g_assert (attribute_names != NULL);
  g_assert (attribute_values != NULL);

  result = ide_gi_parser_object_get_result (IDE_GI_PARSER_OBJECT (self));
  pool = ide_gi_parser_result_get_pool (result);
  element_type = ide_gi_parser_get_element_type (element_name);

  if (element_type & IDE_GI_PARSER_ELEMENT_MASK_PARAMETERS)
    {
      parser_object = ide_gi_pool_get_current_parser_object (pool);
      g_assert (IDE_IS_GI_PARAMETERS_BUILDER (parser_object));

      if (element_type == IDE_GI_ELEMENT_TYPE_PARAMETER)
        parameter_type = PARAMETER_TYPE_PARAMETER;
      else if (element_type == IDE_GI_ELEMENT_TYPE_INSTANCE_PARAMETER)
        parameter_type = PARAMETER_TYPE_INSTANCE_PARAMETER;
      else if (element_type == IDE_GI_ELEMENT_TYPE_RETURN_VALUE)
        parameter_type = PARAMETER_TYPE_RETURN_VALUE;
      else
        g_assert_not_reached ();

      ide_gi_parameters_builder_parameter_parse (parser_object,
                                                 context,
                                                 result,
                                                 parameter_type,
                                                 element_name,
                                                 attribute_names,
                                                 attribute_values,
                                                 error);
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
ide_gi_parameters_builder_end_element (GMarkupParseContext  *context,
                                       const gchar          *element_name,
                                       gpointer              user_data,
                                       GError              **error)
{
  IdeGiParametersBuilder *self = (IdeGiParametersBuilder *)user_data;
  IdeGiParserResult *result;
  IdeGiPool *pool;
  IdeGiParserObject *child;
  IdeGiElementType element_type;

  g_assert (IDE_IS_GI_PARAMETERS_BUILDER (self));
  g_assert (context != NULL);
  g_assert (element_name != NULL);

  result = ide_gi_parser_object_get_result (IDE_GI_PARSER_OBJECT (self));
  pool = ide_gi_parser_result_get_pool (result);
  child = ide_gi_pool_get_current_parser_object (pool);
  element_type = ide_gi_parser_get_element_type (element_name);

  if (element_type & IDE_GI_PARSER_ELEMENT_MASK_PARAMETERS)
    {
      guint32 offset;

      if (self->has_doc_blob)
        {
          self->doc_blob.blob_type = IDE_GI_BLOB_TYPE_DOC;
          self->blob.common.doc = ide_gi_parser_result_add_doc (result, &self->doc_blob);
        }
      else
        self->blob.common.doc = -1;

      offset = ide_gi_parser_result_add_parameter (result, &self->blob);

      /* Clear the parameter and doc blob for the next use */
      memset (&self->blob, 0, sizeof (IdeGiParameterBlob));
      memset (&self->doc_blob, 0, sizeof (IdeGiDocBlob));
      self->has_doc_blob = FALSE;

      g_assert (offset == self->current_param_offset + 1 || self->current_param_offset == 0);

      self->current_param_offset = offset;
      self->parameters_entry.n_parameters++;
      if (!self->has_params)
        {
          self->has_params = TRUE;
          self->parameters_entry.first_param_offset = offset;
        }

      g_markup_parse_context_pop (context);
    }
  else if (!dzl_str_equal0 (element_name, ide_gi_pool_get_unhandled_element (pool)))
    {
      g_autoptr(GFile) file = ide_gi_parser_result_get_file (result);

      ide_gi_helper_parsing_error (child, context, file);
    }
}

static const GMarkupParser markup_parser = {
  ide_gi_parameters_builder_start_element,
  ide_gi_parameters_builder_end_element,
  NULL,
  NULL,
  NULL,
};

static gboolean
ide_gi_parameters_builder_parse (IdeGiParserObject    *parser_object,
                                 GMarkupParseContext  *context,
                                 IdeGiParserResult    *result,
                                 const gchar          *element_name,
                                 const gchar         **attribute_names,
                                 const gchar         **attribute_values,
                                 GError              **error)
{
  IdeGiParametersBuilder *self = IDE_GI_PARAMETERS_BUILDER (parser_object);
  IdeGiElementType element_type;

  g_assert (IDE_IS_GI_PARAMETERS_BUILDER (self));
  g_assert (IDE_IS_GI_PARSER_RESULT (result));
  g_assert (g_str_equal (element_name, "parameters") ||
            g_str_equal (element_name, "return-value"));

  /* TODO: transform and integrate attributes in the blob and result */

  element_type = ide_gi_parser_get_element_type (element_name);
  if (element_type == IDE_GI_ELEMENT_TYPE_RETURN_VALUE)
    {
      self->is_return_value = TRUE;
      ide_gi_parameters_builder_parameter_parse (parser_object,
                                                 context,
                                                 result,
                                                 PARAMETER_TYPE_RETURN_VALUE,
                                                 element_name,
                                                 attribute_names,
                                                 attribute_values,
                                                 error);
    }
  else
    {
      ide_gi_parser_object_set_result (parser_object, result);
      g_markup_parse_context_push (context, &markup_parser, self);
    }

  return TRUE;
}

static gpointer
ide_gi_parameters_builder_finish (IdeGiParserObject *parser_object)
{
  IdeGiParametersBuilder *self = IDE_GI_PARAMETERS_BUILDER (parser_object);
  IdeGiParserResult *result = ide_gi_parser_object_get_result (IDE_GI_PARSER_OBJECT (self));

  if (self->is_return_value)
    {
      if (self->has_doc_blob)
        {
          self->doc_blob.blob_type = IDE_GI_BLOB_TYPE_DOC;
          self->blob.common.doc = ide_gi_parser_result_add_doc (result, &self->doc_blob);
        }
      else
        self->blob.common.doc = -1;

      return &self->blob;
    }
  else
    return &self->parameters_entry;
}

static void
ide_gi_parameters_builder_reset (IdeGiParserObject *parser_object)
{
  IdeGiParametersBuilder *self = IDE_GI_PARAMETERS_BUILDER (parser_object);

  memset (self + sizeof (IdeGiParserObject),
          0,
          sizeof (IdeGiParametersBuilder) - sizeof (IdeGiParserObject));
}

IdeGiParserObject *
ide_gi_parameters_builder_new (void)
{
  IdeGiParserObject *self;

  self = IDE_GI_PARSER_OBJECT (g_object_new (IDE_TYPE_GI_PARAMETERS_BUILDER, NULL));

  _ide_gi_parser_object_set_element_type (self, IDE_GI_ELEMENT_TYPE_PARAMETERS);
  return self;
}

static void
ide_gi_parameters_builder_finalize (GObject *object)
{
  G_OBJECT_CLASS (ide_gi_parameters_builder_parent_class)->finalize (object);
}

static void
ide_gi_parameters_builder_class_init (IdeGiParametersBuilderClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  IdeGiParserObjectClass *parent_class = IDE_GI_PARSER_OBJECT_CLASS (klass);

  object_class->finalize = ide_gi_parameters_builder_finalize;

  parent_class->finish = ide_gi_parameters_builder_finish;
  parent_class->parse = ide_gi_parameters_builder_parse;
  parent_class->reset = ide_gi_parameters_builder_reset;
}

static void
ide_gi_parameters_builder_init (IdeGiParametersBuilder *self)
{
}
