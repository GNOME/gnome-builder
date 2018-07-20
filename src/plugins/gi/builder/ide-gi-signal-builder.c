/* ide-gi-signal-builder.c
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

#include "ide-gi-signal-builder.h"

#include "ide-gi-doc-builder.h"
#include "ide-gi-parameters-builder.h"
#include "ide-gi-record-builder.h"

struct _IdeGiSignalBuilder
{
  IdeGiParserObject  parent_instance;

  IdeGiDocBlob       doc_blob;
  IdeGiSignalBlob    blob;

  guint              has_doc_blob : 1;
};

G_DEFINE_TYPE (IdeGiSignalBuilder, ide_gi_signal_builder, IDE_TYPE_GI_PARSER_OBJECT)

static void
ide_gi_signal_builder_start_element (GMarkupParseContext  *context,
                                     const gchar          *element_name,
                                     const gchar         **attribute_names,
                                     const gchar         **attribute_values,
                                     gpointer              user_data,
                                     GError              **error)
{
  IdeGiSignalBuilder *self = (IdeGiSignalBuilder *)user_data;
  IdeGiParserResult *result;
  IdeGiPool *pool;
  IdeGiParserObject *child;
  IdeGiElementType element_type;

  g_assert (IDE_IS_GI_SIGNAL_BUILDER (self));
  g_assert (context != NULL);
  g_assert (element_name != NULL);
  g_assert (attribute_names != NULL);
  g_assert (attribute_values != NULL);

  result = ide_gi_parser_object_get_result (IDE_GI_PARSER_OBJECT (self));
  pool = ide_gi_parser_result_get_pool (result);
  element_type = ide_gi_parser_get_element_type (element_name);

  if (element_type & IDE_GI_PARSER_ELEMENT_MASK_GLIB_SIGNAL)
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
ide_gi_signal_builder_end_element (GMarkupParseContext  *context,
                                   const gchar          *element_name,
                                   gpointer              user_data,
                                   GError              **error)
{
  IdeGiSignalBuilder *self = (IdeGiSignalBuilder *)user_data;
  IdeGiParserResult *result;
  IdeGiPool *pool;
  IdeGiParserObject *child;
  IdeGiElementType element_type;

  g_assert (IDE_IS_GI_SIGNAL_BUILDER (self));
  g_assert (context != NULL);
  g_assert (element_name != NULL);

  result = ide_gi_parser_object_get_result (IDE_GI_PARSER_OBJECT (self));
  pool = ide_gi_parser_result_get_pool (result);
  child = ide_gi_pool_get_current_parser_object (pool);
  element_type = ide_gi_parser_get_element_type (element_name);

  if (element_type & IDE_GI_PARSER_ELEMENT_MASK_GLIB_SIGNAL)
    {
      if (element_type & IDE_GI_PARSER_ELEMENT_MASK_DOC)
        {
          g_autofree gchar *str = ide_gi_parser_object_finish (child);
          ide_gi_helper_update_doc_blob (result, &self->doc_blob, element_type, str);
          self->has_doc_blob = TRUE;
        }
      else if (element_type == IDE_GI_ELEMENT_TYPE_PARAMETERS)
        {
          IdeGiParametersEntry *entry = ide_gi_parser_object_finish (child);

          if (self->blob.n_parameters != 0)
            {
              g_autoptr(GFile) file = ide_gi_parser_result_get_file (result);
              ide_gi_helper_parsing_error_custom (IDE_GI_PARSER_OBJECT (self),
                                                  context,
                                                  file,
                                                  "parameters already set");
            }

          self->blob.n_parameters = entry->n_parameters;
          self->blob.parameters = entry->first_param_offset;
        }
      else // IDE_GI_ELEMENT_TYPE_RETURN_VALUE
        {
          IdeGiParameterBlob *blob = ide_gi_parser_object_finish (child);
          self->blob.return_value = ide_gi_parser_result_add_parameter (result, blob);
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
  ide_gi_signal_builder_start_element,
  ide_gi_signal_builder_end_element,
  NULL,
  NULL,
  NULL,
};

static gboolean
ide_gi_signal_builder_parse (IdeGiParserObject    *parser_object,
                             GMarkupParseContext  *context,
                             IdeGiParserResult    *result,
                             const gchar          *element_name,
                             const gchar         **attribute_names,
                             const gchar         **attribute_values,
                             GError              **error)
{
  IdeGiSignalBuilder *self = IDE_GI_SIGNAL_BUILDER (parser_object);
  gboolean introspectable;
  gboolean deprecated;
  gboolean detailed;
  gboolean action;
  gboolean no_hooks;
  gboolean no_recurse;
  IdeGiStability stability;
  IdeGiSignalWhen when;

  g_assert (IDE_IS_GI_SIGNAL_BUILDER (self));
  g_assert (IDE_IS_GI_PARSER_RESULT (result));
  g_assert (g_str_equal (element_name, "glib:signal"));

  if (!ide_gi_helper_markup_collect_attributes (result,
                                                context,
                                                element_name, attribute_names, attribute_values, error,
                                                IDE_GI_MARKUP_COLLECT_BOOLEAN | IDE_GI_MARKUP_COLLECT_OPTIONAL, "0", "introspectable", &introspectable,
                                                IDE_GI_MARKUP_COLLECT_BOOLEAN | IDE_GI_MARKUP_COLLECT_OPTIONAL, "0", "deprecated", &deprecated,
                                                IDE_GI_MARKUP_COLLECT_BOOLEAN | IDE_GI_MARKUP_COLLECT_OPTIONAL, "0", "action", &action,
                                                IDE_GI_MARKUP_COLLECT_BOOLEAN | IDE_GI_MARKUP_COLLECT_OPTIONAL, "0", "no-hooks", &no_hooks,
                                                IDE_GI_MARKUP_COLLECT_BOOLEAN | IDE_GI_MARKUP_COLLECT_OPTIONAL, "0", "no-recurse", &no_recurse,
                                                IDE_GI_MARKUP_COLLECT_BOOLEAN | IDE_GI_MARKUP_COLLECT_OPTIONAL, "0", "detailed", &detailed,
                                                IDE_GI_MARKUP_COLLECT_STABILITY | IDE_GI_MARKUP_COLLECT_OPTIONAL, "Stable", "stability", &stability,
                                                IDE_GI_MARKUP_COLLECT_SIGNAL_WHEN | IDE_GI_MARKUP_COLLECT_OPTIONAL, "first", "when", &when,
                                                IDE_GI_MARKUP_COLLECT_OFFSET32_STRING | IDE_GI_MARKUP_COLLECT_OPTIONAL, "", "deprecated-version", &self->blob.common.deprecated_version,
                                                IDE_GI_MARKUP_COLLECT_OFFSET32_STRING | IDE_GI_MARKUP_COLLECT_OPTIONAL, "", "version", &self->blob.common.version,
                                                IDE_GI_MARKUP_COLLECT_OFFSET32_STRING | IDE_GI_MARKUP_COLLECT_OPTIONAL, "", "name", &self->blob.common.name,
                                                IDE_GI_MARKUP_COLLECT_INVALID, NULL, NULL, NULL))
    return FALSE;

  self->blob.common.blob_type = IDE_GI_BLOB_TYPE_SIGNAL;

  self->blob.common.introspectable = introspectable;
  self->blob.common.deprecated = deprecated;
  self->blob.common.stability = stability;
  self->blob.action = action;
  self->blob.no_hooks = no_hooks;
  self->blob.no_recurse = no_recurse;
  self->blob.detailed = detailed;
  self->blob.run_when  = when;

  ide_gi_parser_object_set_result (parser_object, result);
  g_markup_parse_context_push (context, &markup_parser, self);

  return TRUE;
}

static gpointer
ide_gi_signal_builder_finish (IdeGiParserObject *parser_object)
{
  IdeGiSignalBuilder *self = IDE_GI_SIGNAL_BUILDER (parser_object);
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
ide_gi_signal_builder_reset (IdeGiParserObject *parser_object)
{
  IdeGiSignalBuilder *self = IDE_GI_SIGNAL_BUILDER (parser_object);

  memset (self + sizeof (IdeGiParserObject),
          0,
          sizeof (IdeGiSignalBuilder) - sizeof (IdeGiParserObject));
}

IdeGiParserObject *
ide_gi_signal_builder_new (void)
{
  IdeGiParserObject *self;

  self = IDE_GI_PARSER_OBJECT (g_object_new (IDE_TYPE_GI_SIGNAL_BUILDER, NULL));

  _ide_gi_parser_object_set_element_type (self, IDE_GI_ELEMENT_TYPE_GLIB_SIGNAL);
  return self;
}

static void
ide_gi_signal_builder_finalize (GObject *object)
{
  G_OBJECT_CLASS (ide_gi_signal_builder_parent_class)->finalize (object);
}

static void
ide_gi_signal_builder_class_init (IdeGiSignalBuilderClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  IdeGiParserObjectClass *parent_class = IDE_GI_PARSER_OBJECT_CLASS (klass);

  object_class->finalize = ide_gi_signal_builder_finalize;

  parent_class->finish = ide_gi_signal_builder_finish;
  parent_class->parse = ide_gi_signal_builder_parse;
  parent_class->reset = ide_gi_signal_builder_reset;
}

static void
ide_gi_signal_builder_init (IdeGiSignalBuilder *self)
{
}
