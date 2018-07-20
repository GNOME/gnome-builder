/* ide-gi-doc-builder.c
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

#include "ide-gi-doc-builder.h"

struct _IdeGiDocBuilder
{
  IdeGiParserObject  parent_instance;

  GString           *doc_str;
};

G_DEFINE_TYPE (IdeGiDocBuilder, ide_gi_doc_builder, IDE_TYPE_GI_PARSER_OBJECT)

static void
ide_gi_doc_builder_text (GMarkupParseContext  *context,
                         const gchar          *text,
                         gsize                 text_len,
                         gpointer              user_data,
                         GError              **error)
{
  IdeGiDocBuilder *self = (IdeGiDocBuilder *)user_data;
  IdeGiParserResult *result G_GNUC_UNUSED;

  g_assert (IDE_IS_GI_DOC_BUILDER (self));
  g_assert (context != NULL);
  g_assert (text != NULL);

  result = ide_gi_parser_object_get_result (IDE_GI_PARSER_OBJECT (self));

  g_string_append_len (self->doc_str, text, text_len);
}

static const GMarkupParser markup_parser = {
  NULL,
  NULL,
  ide_gi_doc_builder_text,
  NULL,
  NULL,
};

static gboolean
ide_gi_doc_builder_parse (IdeGiParserObject    *parser_object,
                          GMarkupParseContext  *context,
                          IdeGiParserResult    *result,
                          const gchar          *element_name,
                          const gchar         **attribute_names,
                          const gchar         **attribute_values,
                          GError              **error)
{
  IdeGiDocBuilder *self = IDE_GI_DOC_BUILDER (parser_object);
  IdeGiElementType element_type;
  const gchar *key;
  const gchar *value;

  g_assert (IDE_IS_GI_DOC_BUILDER (self));
  g_assert (IDE_IS_GI_PARSER_RESULT (result));
  g_assert (g_str_equal (element_name, "doc") ||
            g_str_equal (element_name, "doc-version") ||
            g_str_equal (element_name, "doc-stability") ||
            g_str_equal (element_name, "doc-deprecated") ||
            g_str_equal (element_name, "annotation"));

  /* if (!ide_gi_helper_markup_collect_attributes (result, */
  /*                                               context, */
  /*                                               element_name, attribute_names, attribute_values, error, */
  /*                                               IDE_GI_MARKUP_COLLECT_OFFSET32_STRING | IDE_GI_MARKUP_COLLECT_OPTIONAL, "", "xml:space", &xml_space, */
  /*                                               IDE_GI_MARKUP_COLLECT_OFFSET32_STRING | IDE_GI_MARKUP_COLLECT_OPTIONAL, "", "xml:whitespace", &xml_whitespace, */
  /*                                               IDE_GI_MARKUP_COLLECT_INVALID, NULL, NULL)) */
  /*   return FALSE; */

  /* TODO: Annotation same thing different name ? */

  element_type = ide_gi_parser_get_element_type (element_name);
  if (element_type == IDE_GI_ELEMENT_TYPE_ANNOTATION)
  {
    if (!ide_gi_helper_markup_collect_attributes (result,
                                                  context,
                                                  element_name, attribute_names, attribute_values, error,
                                                  IDE_GI_MARKUP_COLLECT_STRING | IDE_GI_MARKUP_COLLECT_OPTIONAL, "", "key", &key,
                                                  IDE_GI_MARKUP_COLLECT_STRING | IDE_GI_MARKUP_COLLECT_OPTIONAL, "", "value", &value,
                                                  IDE_GI_MARKUP_COLLECT_INVALID, NULL, NULL))
      return FALSE;

    g_string_append (self->doc_str, key);
    g_string_append_c (self->doc_str, ':');
    g_string_append (self->doc_str, value);

    /* We push a context even if <annotation> is self closed to avoid specific code in each Builder.
     * It's a rare case, no need to optimize that.
     */
  }

  ide_gi_parser_object_set_result (parser_object, result);
  g_markup_parse_context_push (context, &markup_parser, self);

  return TRUE;
}

static gpointer
ide_gi_doc_builder_finish (IdeGiParserObject *parser_object)
{
  IdeGiDocBuilder *self = IDE_GI_DOC_BUILDER (parser_object);
  gchar *str = NULL;

  if (self->doc_str->len > 0)
    {
      str = g_string_free (self->doc_str, FALSE);
      self->doc_str = NULL;
    }

  return str;
}

static void
ide_gi_doc_builder_reset (IdeGiParserObject *parser_object)
{
  IdeGiDocBuilder *self = IDE_GI_DOC_BUILDER (parser_object);

  if (self->doc_str != NULL)
    {
      g_string_free (self->doc_str, TRUE);
      self->doc_str = NULL;
    }
}

IdeGiParserObject *
ide_gi_doc_builder_new (void)
{
  IdeGiParserObject *self;

  self = IDE_GI_PARSER_OBJECT (g_object_new (IDE_TYPE_GI_DOC_BUILDER, NULL));

  _ide_gi_parser_object_set_element_type (self, IDE_GI_ELEMENT_TYPE_DOC);
  return self;
}

static void
ide_gi_doc_builder_finalize (GObject *object)
{
  IdeGiDocBuilder *self = IDE_GI_DOC_BUILDER (object);

  if (self->doc_str != NULL)
    {
      g_string_free (self->doc_str, TRUE);
      self->doc_str = NULL;
    }

  G_OBJECT_CLASS (ide_gi_doc_builder_parent_class)->finalize (object);
}

static void
ide_gi_doc_builder_class_init (IdeGiDocBuilderClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  IdeGiParserObjectClass *parent_class = IDE_GI_PARSER_OBJECT_CLASS (klass);

  object_class->finalize = ide_gi_doc_builder_finalize;

  parent_class->finish = ide_gi_doc_builder_finish;
  parent_class->parse = ide_gi_doc_builder_parse;
  parent_class->reset = ide_gi_doc_builder_reset;
}

static void
ide_gi_doc_builder_init (IdeGiDocBuilder *self)
{
  self->doc_str = g_string_new (NULL);
}
