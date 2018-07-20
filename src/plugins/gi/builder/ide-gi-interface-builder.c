/* ide-gi-interface-builder.c
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

#include "ide-gi-interface-builder.h"

#include "ide-gi-constant-builder.h"
#include "ide-gi-callback-builder.h"
#include "ide-gi-doc-builder.h"
#include "ide-gi-field-builder.h"
#include "ide-gi-function-builder.h"
#include "ide-gi-property-builder.h"
#include "ide-gi-record-builder.h"
#include "ide-gi-signal-builder.h"

struct _IdeGiInterfaceBuilder
{
  IdeGiParserObject  parent_instance;

  IdeGiDocBlob       doc_blob;
  IdeGiObjectBlob    blob;

  GArray            *callbacks;
  GArray            *fields;
  GArray            *functions;
  GArray            *properties;

  guint              is_in_prerequisite      : 1;
  guint              is_buildable            : 1;
  guint              has_doc_blob            : 1;

  guint              first_signal_set        : 1;
  guint              first_constant_set      : 1;
  guint              first_prerequisites_set : 1;
};

G_DEFINE_TYPE (IdeGiInterfaceBuilder, ide_gi_interface_builder, IDE_TYPE_GI_PARSER_OBJECT)

static void
ide_gi_interface_builder_index (IdeGiParserObject *parse_object,
                                IdeGiParserResult *result,
                                gpointer           user_data)
{
  IdeGiInterfaceBuilder *self = (IdeGiInterfaceBuilder *)parse_object;
  gint32 offset = GPOINTER_TO_INT (user_data);
  const gchar *name;

  g_return_if_fail (IDE_IS_GI_PARSER_OBJECT (self));
  g_return_if_fail (IDE_IS_GI_PARSER_RESULT (result));

  name = ide_gi_parser_result_get_string (result, self->blob.common.name);
  ide_gi_parser_result_add_object_index (result, name, IDE_GI_BLOB_TYPE_INTERFACE, offset);

  name = ide_gi_parser_result_get_string (result, self->blob.g_type_name);
  ide_gi_parser_result_add_global_index (result,
                                         name,
                                         offset,
                                         IDE_GI_PREFIX_TYPE_GTYPE,
                                         IDE_GI_BLOB_TYPE_INTERFACE,
                                         FALSE);
}

static gboolean
ide_gi_interface_builder_parse_prerequisite (IdeGiParserObject    *parser_object,
                                             GMarkupParseContext  *context,
                                             IdeGiParserResult    *result,
                                             const gchar          *element_name,
                                             const gchar         **attribute_names,
                                             const gchar         **attribute_values,
                                             GError              **error)
{
  IdeGiInterfaceBuilder *self = (IdeGiInterfaceBuilder *)parser_object;
  gboolean is_local = FALSE;
  const gchar *name;
  g_autofree gchar *free_qname = NULL;
  gchar *qname;

  g_assert (IDE_IS_GI_INTERFACE_BUILDER (self));
  g_assert (IDE_IS_GI_PARSER_RESULT (result));
  g_assert (g_str_equal (element_name, "prerequisite"));

  if (!ide_gi_helper_markup_collect_attributes (result,
                                                context,
                                                element_name, attribute_names, attribute_values, error,
                                                IDE_GI_MARKUP_COLLECT_STRING | IDE_GI_MARKUP_COLLECT_OPTIONAL, "", "name", &name,
                                                IDE_GI_MARKUP_COLLECT_INVALID, NULL, NULL, NULL))
    return FALSE;

  is_local = (strchr (name, '.') == NULL);
  if (is_local)
    free_qname = qname = g_strconcat (ide_gi_parser_result_get_namespace (result), ".", name, NULL);
  else
    qname = (gchar *)name;

  self->blob.n_interfaces++;

  /* This is a partial crossref, we still need to complete the namespace version later.
   * Also, here the type can be either a class or an interface.
   * */
  if (!self->first_prerequisites_set)
    {
      self->first_prerequisites_set = TRUE;
      self->blob.interfaces = ide_gi_parser_result_add_crossref (result,
                                                                 IDE_GI_BLOB_TYPE_UNKNOW,
                                                                 qname,
                                                                 is_local);
    }
  else
    ide_gi_parser_result_add_crossref (result,
                                       IDE_GI_BLOB_TYPE_UNKNOW,
                                       qname,
                                       is_local);

  if (self->is_buildable == FALSE && dzl_str_equal0 (name, "Gtk.Buildable"))
    self->is_buildable = TRUE;

  return TRUE;
}

static void
ide_gi_interface_builder_start_element (GMarkupParseContext  *context,
                                        const gchar          *element_name,
                                        const gchar         **attribute_names,
                                        const gchar         **attribute_values,
                                        gpointer              user_data,
                                        GError              **error)
{
  IdeGiInterfaceBuilder *self = (IdeGiInterfaceBuilder *)user_data;
  IdeGiParserResult *result;
  IdeGiParserObject *parser_object;
  IdeGiPool *pool;
  IdeGiParserObject *child;
  IdeGiElementType element_type;

  g_assert (IDE_IS_GI_INTERFACE_BUILDER (self));
  g_assert (context != NULL);
  g_assert (element_name != NULL);
  g_assert (attribute_names != NULL);
  g_assert (attribute_values != NULL);

  result = ide_gi_parser_object_get_result (IDE_GI_PARSER_OBJECT (self));
  pool = ide_gi_parser_result_get_pool (result);
  parser_object = ide_gi_pool_get_current_parser_object (pool);
  self = IDE_GI_INTERFACE_BUILDER (parser_object);
  element_type = ide_gi_parser_get_element_type (element_name);

  if (self->is_in_prerequisite)
    {
      g_autoptr(GFile) file = ide_gi_parser_result_get_file (result);
      ide_gi_helper_parsing_error_custom (IDE_GI_PARSER_OBJECT (self),
                                          context,
                                          file,
                                          "We should not have sub-elements in <prerequisite>");
      return;
    }

  if (element_type & IDE_GI_PARSER_ELEMENT_MASK_INTERFACE)
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
  else if (element_type == IDE_GI_ELEMENT_TYPE_PREREQUISITE)
    {
      ide_gi_interface_builder_parse_prerequisite (parser_object,
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
ide_gi_interface_builder_end_element (GMarkupParseContext  *context,
                                      const gchar          *element_name,
                                      gpointer              user_data,
                                      GError              **error)
{
  IdeGiInterfaceBuilder *self = (IdeGiInterfaceBuilder *)user_data;
  IdeGiParserResult *result;
  IdeGiPool *pool;
  IdeGiParserObject *child;
  IdeGiElementType element_type;

  g_assert (IDE_IS_GI_INTERFACE_BUILDER (self));
  g_assert (context != NULL);
  g_assert (element_name != NULL);

  result = ide_gi_parser_object_get_result (IDE_GI_PARSER_OBJECT (self));
  pool = ide_gi_parser_result_get_pool (result);
  child = ide_gi_pool_get_current_parser_object (pool);
  element_type = ide_gi_parser_get_element_type (element_name);

  if (element_type & IDE_GI_PARSER_ELEMENT_MASK_INTERFACE)
    {
      if (element_type & IDE_GI_PARSER_ELEMENT_MASK_DOC)
        {
          g_autofree gchar *str = ide_gi_parser_object_finish (child);
          ide_gi_helper_update_doc_blob (result, &self->doc_blob, element_type, str);
          self->has_doc_blob = TRUE;
        }
      else if (element_type == IDE_GI_ELEMENT_TYPE_CALLBACK)
        {
          IdeGiCallbackBlob *blob = ide_gi_parser_object_finish (child);

          if (self->callbacks == NULL)
            self->callbacks = g_array_new (FALSE, FALSE, sizeof (IdeGiCallbackBlob));

          g_array_append_val (self->callbacks, *blob);
        }
      else if (element_type == IDE_GI_ELEMENT_TYPE_CONSTANT)
        {
          IdeGiConstantBlob *blob = ide_gi_parser_object_finish (child);

          if (!self->first_constant_set)
            {
              self->first_constant_set = TRUE;
              self->blob.constants = ide_gi_parser_result_add_constant (result, blob);
            }
          else
            ide_gi_parser_result_add_constant (result, blob);

          self->blob.n_constants++;
        }
      else if (element_type == IDE_GI_ELEMENT_TYPE_CONSTRUCTOR ||
               element_type == IDE_GI_ELEMENT_TYPE_FUNCTION ||
               element_type == IDE_GI_ELEMENT_TYPE_METHOD ||
               element_type == IDE_GI_ELEMENT_TYPE_VIRTUAL_METHOD)
        {
          IdeGiFunctionBlob *blob = ide_gi_parser_object_finish (child);

          if (self->functions == NULL)
            self->functions= g_array_new (FALSE, FALSE, sizeof (IdeGiFunctionBlob));

          g_array_append_val (self->functions, *blob);
        }
      else if (element_type == IDE_GI_ELEMENT_TYPE_BITFIELD)
        {
          IdeGiFieldBlob *blob = ide_gi_parser_object_finish (child);

          if (self->fields == NULL)
            self->fields = g_array_new (FALSE, FALSE, sizeof (IdeGiFieldBlob));

          g_array_append_val (self->fields, *blob);
        }
      else if (element_type == IDE_GI_ELEMENT_TYPE_GLIB_SIGNAL)
        {
          IdeGiSignalBlob *blob = ide_gi_parser_object_finish (child);

          if (!self->first_signal_set)
            {
              self->first_signal_set = TRUE;
              self->blob.signals = ide_gi_parser_result_add_signal (result, blob);
            }
          else
            ide_gi_parser_result_add_signal (result, blob);

          self->blob.n_signals++;
        }
      else if (element_type == IDE_GI_ELEMENT_TYPE_PROPERTY)
        {
          IdeGiPropertyBlob *blob = ide_gi_parser_object_finish (child);

          if (self->properties == NULL)
            self->properties = g_array_new (FALSE, FALSE, sizeof (IdeGiPropertyBlob));

          g_array_append_val (self->properties, *blob);
        }

      ide_gi_pool_release_object (pool);
      g_markup_parse_context_pop (context);
    }
  else if (element_type == IDE_GI_ELEMENT_TYPE_PREREQUISITE)
    {
      /* Handle in a sub-parser */
      self = IDE_GI_INTERFACE_BUILDER (child);
      self->is_in_prerequisite = FALSE;

      return;
    }
  else if (!dzl_str_equal0 (element_name, ide_gi_pool_get_unhandled_element (pool)))
    {
      g_autoptr(GFile) file = ide_gi_parser_result_get_file (result);

      ide_gi_helper_parsing_error (child, context, file);
    }
}

static const GMarkupParser markup_parser = {
  ide_gi_interface_builder_start_element,
  ide_gi_interface_builder_end_element,
  NULL,
  NULL,
  NULL,
};

static gboolean
ide_gi_interface_builder_parse (IdeGiParserObject    *parser_object,
                                GMarkupParseContext  *context,
                                IdeGiParserResult    *result,
                                const gchar          *element_name,
                                const gchar         **attribute_names,
                                const gchar         **attribute_values,
                                GError              **error)
{
  IdeGiInterfaceBuilder *self = IDE_GI_INTERFACE_BUILDER (parser_object);
  gboolean introspectable;
  gboolean deprecated;
  IdeGiStability stability;

  g_assert (IDE_IS_GI_INTERFACE_BUILDER (self));
  g_assert (IDE_IS_GI_PARSER_RESULT (result));
  g_assert (g_str_equal (element_name, "interface"));

  if (!ide_gi_helper_markup_collect_attributes (result,
                                                context,
                                                element_name, attribute_names, attribute_values, error,
                                                IDE_GI_MARKUP_COLLECT_BOOLEAN | IDE_GI_MARKUP_COLLECT_OPTIONAL, "0", "introspectable", &introspectable,
                                                IDE_GI_MARKUP_COLLECT_BOOLEAN | IDE_GI_MARKUP_COLLECT_OPTIONAL, "0", "deprecated", &deprecated,
                                                IDE_GI_MARKUP_COLLECT_STABILITY | IDE_GI_MARKUP_COLLECT_OPTIONAL, "Stable", "stability", &stability,
                                                IDE_GI_MARKUP_COLLECT_OFFSET32_STRING | IDE_GI_MARKUP_COLLECT_OPTIONAL, "", "deprecated-version", &self->blob.common.deprecated_version,
                                                IDE_GI_MARKUP_COLLECT_OFFSET32_STRING | IDE_GI_MARKUP_COLLECT_OPTIONAL, "", "version", &self->blob.common.version,
                                                IDE_GI_MARKUP_COLLECT_OFFSET32_STRING | IDE_GI_MARKUP_COLLECT_OPTIONAL, "", "name", &self->blob.common.name,
                                                IDE_GI_MARKUP_COLLECT_OFFSET32_STRING | IDE_GI_MARKUP_COLLECT_OPTIONAL, "", "glib:type-name", &self->blob.g_type_name,
                                                IDE_GI_MARKUP_COLLECT_OFFSET32_STRING | IDE_GI_MARKUP_COLLECT_OPTIONAL, "", "glib:get-type", &self->blob.g_get_type,
                                                IDE_GI_MARKUP_COLLECT_OFFSET32_STRING | IDE_GI_MARKUP_COLLECT_OPTIONAL, "", "c:symbol-prefix", &self->blob.c_symbol_prefix,
                                                IDE_GI_MARKUP_COLLECT_OFFSET32_STRING | IDE_GI_MARKUP_COLLECT_OPTIONAL, "", "c:type", &self->blob.c_type,
                                                IDE_GI_MARKUP_COLLECT_OFFSET32_STRING | IDE_GI_MARKUP_COLLECT_OPTIONAL, "", "glib:type-struct", &self->blob.g_type_struct,
                                                IDE_GI_MARKUP_COLLECT_INVALID, NULL, NULL, NULL))
    return FALSE;

  self->blob.common.blob_type = IDE_GI_BLOB_TYPE_INTERFACE;

  self->blob.common.introspectable = introspectable;
  self->blob.common.deprecated = deprecated;
  self->blob.common.stability = stability;

  ide_gi_parser_object_set_result (parser_object, result);
  g_markup_parse_context_push (context, &markup_parser, self);

  return TRUE;
}

static gpointer
ide_gi_interface_builder_finish (IdeGiParserObject *parser_object)
{
  IdeGiInterfaceBuilder *self = IDE_GI_INTERFACE_BUILDER (parser_object);
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

  return &self->blob;
}

static void
ide_gi_interface_builder_reset (IdeGiParserObject *parser_object)
{
  IdeGiInterfaceBuilder *self = IDE_GI_INTERFACE_BUILDER (parser_object);

  dzl_clear_pointer (&self->callbacks, g_array_unref);
  dzl_clear_pointer (&self->fields, g_array_unref);
  dzl_clear_pointer (&self->functions, g_array_unref);
  dzl_clear_pointer (&self->properties, g_array_unref);

  memset (self + sizeof (IdeGiParserObject),
          0,
          sizeof (IdeGiInterfaceBuilder) - sizeof (IdeGiParserObject));
}

IdeGiParserObject *
ide_gi_interface_builder_new (void)
{
  IdeGiParserObject *self;

  self = IDE_GI_PARSER_OBJECT (g_object_new (IDE_TYPE_GI_INTERFACE_BUILDER, NULL));

  _ide_gi_parser_object_set_element_type (self, IDE_GI_ELEMENT_TYPE_INTERFACE);
  return self;
}

static void
ide_gi_interface_builder_finalize (GObject *object)
{
  IdeGiInterfaceBuilder *self = IDE_GI_INTERFACE_BUILDER (object);

  dzl_clear_pointer (&self->callbacks, g_array_unref);
  dzl_clear_pointer (&self->fields, g_array_unref);
  dzl_clear_pointer (&self->functions, g_array_unref);
  dzl_clear_pointer (&self->properties, g_array_unref);

  G_OBJECT_CLASS (ide_gi_interface_builder_parent_class)->finalize (object);
}

static void
ide_gi_interface_builder_class_init (IdeGiInterfaceBuilderClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  IdeGiParserObjectClass *parent_class = IDE_GI_PARSER_OBJECT_CLASS (klass);

  object_class->finalize = ide_gi_interface_builder_finalize;

  parent_class->finish = ide_gi_interface_builder_finish;
  parent_class->index = ide_gi_interface_builder_index;
  parent_class->parse = ide_gi_interface_builder_parse;
  parent_class->reset = ide_gi_interface_builder_reset;
}

static void
ide_gi_interface_builder_init (IdeGiInterfaceBuilder *self)
{
}
