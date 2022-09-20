/* ide-xml-parser-generic.c
 *
 * Copyright 2017 Sebastien Lafargue <slafargue@gnome.org>
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
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include <libxml/parser.h>

#include "ide-xml-parser.h"
#include "ide-xml-parser-generic.h"
#include "ide-xml-sax.h"
#include "ide-xml-stack.h"

#include "ide-xml-parser-generic.h"

static gchar *
collect_attributes (IdeXmlParser  *self,
                    const gchar  **attributes)
{
  GString *string;
  gchar *value;
  const gchar **l = attributes;

  g_assert (IDE_IS_XML_PARSER (self));

  if (attributes == NULL)
    return NULL;

  string = g_string_new (NULL);
  while (l [0] != NULL && *l [0] != '\0')
    {
      value = ide_xml_parser_get_color_tag (self, l [0], COLOR_TAG_ATTRIBUTE, TRUE, TRUE, TRUE);
      g_string_append (string, value);
      g_free (value);
      g_string_append (string, l [1]);

      l += 2;
    }

  return g_string_free (string, FALSE);
}

static void
ide_xml_parser_generic_start_element_sax_cb (ParserState    *state,
                                             const xmlChar  *name,
                                             const xmlChar **attributes)
{
  IdeXmlParser *self = (IdeXmlParser *)state->self;
  IdeXmlSymbolNode *node = NULL;
  g_autofree gchar *attr = NULL;
  g_autofree gchar *label = NULL;

  g_assert (IDE_IS_XML_PARSER (self));

  attr = collect_attributes (self, (const gchar **)attributes);
  label = g_strconcat ((const gchar *)name, attr, NULL);

  node = ide_xml_symbol_node_new (label, NULL, (gchar *)name, IDE_SYMBOL_KIND_XML_ELEMENT);
  g_object_set (node, "use-markup", TRUE, NULL);

  state->attributes = (const gchar **)attributes;
  ide_xml_parser_state_processing (self, state, (const gchar *)name, node, IDE_XML_SAX_CALLBACK_TYPE_START_ELEMENT, FALSE);
}

static void
ide_xml_parser_generic_comment_sax_cb (ParserState   *state,
                                       const xmlChar *name)
{
  IdeXmlParser *self = (IdeXmlParser *)state->self;
  IdeXmlSymbolNode *node = NULL;
  g_autofree gchar *strip_name = NULL;

  g_assert (IDE_IS_XML_PARSER (self));

  strip_name = g_strstrip (g_markup_escape_text ((const gchar *)name, -1));
  node = ide_xml_symbol_node_new (strip_name, NULL, NULL, IDE_SYMBOL_KIND_XML_COMMENT);
  ide_xml_parser_state_processing (self, state, "comment", node, IDE_XML_SAX_CALLBACK_TYPE_COMMENT, FALSE);
}

static void
ide_xml_parser_generic_cdata_sax_cb (ParserState   *state,
                                     const xmlChar *value,
                                     gint           len)
{
  IdeXmlParser *self = (IdeXmlParser *)state->self;
  IdeXmlSymbolNode *node = NULL;

  g_assert (IDE_IS_XML_PARSER (self));

  node = ide_xml_symbol_node_new ("cdata", NULL, NULL, IDE_SYMBOL_KIND_XML_CDATA);
  ide_xml_parser_state_processing (self, state, "cdata", node, IDE_XML_SAX_CALLBACK_TYPE_CDATA, FALSE);
}

void
ide_xml_parser_generic_setup (IdeXmlParser *self,
                              ParserState  *state)
{
  g_assert (IDE_IS_XML_PARSER (self));
  g_assert (state != NULL);

  ide_xml_sax_clear (state->sax_parser);
  ide_xml_sax_set_callback (state->sax_parser, IDE_XML_SAX_CALLBACK_TYPE_START_ELEMENT, ide_xml_parser_generic_start_element_sax_cb);
  ide_xml_sax_set_callback (state->sax_parser, IDE_XML_SAX_CALLBACK_TYPE_END_ELEMENT, ide_xml_parser_end_element_sax_cb);
  ide_xml_sax_set_callback (state->sax_parser, IDE_XML_SAX_CALLBACK_TYPE_COMMENT, ide_xml_parser_generic_comment_sax_cb);
  ide_xml_sax_set_callback (state->sax_parser, IDE_XML_SAX_CALLBACK_TYPE_CDATA, ide_xml_parser_generic_cdata_sax_cb);
  ide_xml_sax_set_callback (state->sax_parser, IDE_XML_SAX_CALLBACK_TYPE_CHAR, ide_xml_parser_characters_sax_cb);

  ide_xml_sax_set_callback (state->sax_parser, IDE_XML_SAX_CALLBACK_TYPE_INTERNAL_SUBSET, ide_xml_parser_internal_subset_sax_cb);
  ide_xml_sax_set_callback (state->sax_parser, IDE_XML_SAX_CALLBACK_TYPE_EXTERNAL_SUBSET, ide_xml_parser_external_subset_sax_cb);
  ide_xml_sax_set_callback (state->sax_parser, IDE_XML_SAX_CALLBACK_TYPE_PROCESSING_INSTRUCTION, ide_xml_parser_processing_instruction_sax_cb);

  ide_xml_sax_set_callback (state->sax_parser, IDE_XML_SAX_CALLBACK_TYPE_WARNING, ide_xml_parser_warning_sax_cb);
  ide_xml_sax_set_callback (state->sax_parser, IDE_XML_SAX_CALLBACK_TYPE_ERROR, ide_xml_parser_error_sax_cb);
  ide_xml_sax_set_callback (state->sax_parser, IDE_XML_SAX_CALLBACK_TYPE_FATAL_ERROR, ide_xml_parser_fatal_error_sax_cb);

  ide_xml_parser_set_post_processing_callback (self, NULL);
}
