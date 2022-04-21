/* ide-xml-parser-ui.c
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

#define G_LOG_DOMAIN "ide-xml-parser-ui"

#include <libide-code.h>

#include "ide-xml-parser-ui.h"
#include "ide-xml-parser.h"
#include "ide-xml-sax.h"
#include "ide-xml-stack.h"
#include "ide-xml-tree-builder-utils-private.h"

static const gchar *
get_attribute (const guchar **list,
               const gchar   *name,
               const gchar   *replacement)
{
  const gchar *value = NULL;

  value = list_get_attribute (list, name);
  return ide_str_empty0 (value) ? ((replacement != NULL) ? replacement : NULL) : value;
}

static void
ide_xml_parser_ui_start_element_sax_cb (ParserState    *state,
                                        const xmlChar  *name,
                                        const xmlChar **attributes)
{
  IdeXmlParser *self = (IdeXmlParser *)state->self;
  g_autoptr(GString) string = NULL;
  g_autofree gchar *label = NULL;
  const gchar *value = NULL;
  IdeXmlSymbolNode *node = NULL;
  const gchar *parent_name;
  gboolean is_internal = FALSE;

  g_assert (IDE_IS_XML_PARSER (self));

  if (state->build_state == BUILD_STATE_GET_CONTENT)
    {
      g_warning ("Wrong xml element, waiting for content\n");
      return;
    }

  string = g_string_new (NULL);
  parent_name = ide_xml_symbol_node_get_element_name (state->parent_node);

  if (ide_str_equal0 (name, "property"))
    {
      if (ide_str_equal0 (parent_name, "object") ||
          ide_str_equal0 (parent_name, "template"))
        {
          value = get_attribute (attributes, "name", NULL);
          node = ide_xml_symbol_node_new (value, NULL, "property", IDE_SYMBOL_KIND_UI_PROPERTY);
          is_internal = TRUE;
          state->build_state = BUILD_STATE_GET_CONTENT;
        }
    }
  else if (ide_str_equal0 (name, "attribute"))
    {
      if (ide_str_equal0 (parent_name, "section") ||
          ide_str_equal0 (parent_name, "submenu") ||
          ide_str_equal0 (parent_name, "item"))
        {
          value = get_attribute (attributes, "name", NULL);
          node = ide_xml_symbol_node_new (value, NULL, "attribute", IDE_SYMBOL_KIND_UI_MENU_ATTRIBUTE);
          is_internal = TRUE;
          state->build_state = BUILD_STATE_GET_CONTENT;
        }
    }
  else if (ide_str_equal0 (name, "class") && ide_str_equal0 (parent_name, "style"))
    {
      value = get_attribute (attributes, "name", NULL);
      node = ide_xml_symbol_node_new (value, NULL, "class", IDE_SYMBOL_KIND_UI_STYLE_CLASS);
      is_internal = TRUE;
    }
  else if (ide_str_equal0 (name, "child"))
    {
      g_string_append (string, "child");

      if (NULL != (value = get_attribute (attributes, "type", NULL)))
        {
          label = ide_xml_parser_get_color_tag (self, "type", COLOR_TAG_TYPE, TRUE, TRUE, TRUE);
          g_string_append (string, label);
          g_string_append (string, value);
        }

      if (NULL != (value = get_attribute (attributes, "internal-child", NULL)))
        {
          label = ide_xml_parser_get_color_tag (self, "internal", COLOR_TAG_TYPE, TRUE, TRUE, TRUE);
          g_string_append (string, label);
          g_string_append (string, value);
        }

      node = ide_xml_symbol_node_new (string->str, NULL, "child", IDE_SYMBOL_KIND_UI_CHILD);
      g_object_set (node, "use-markup", TRUE, NULL);
    }
  else if (ide_str_equal0 (name, "object"))
    {
      value = get_attribute (attributes, "class", "?");
      label = ide_xml_parser_get_color_tag (self, "class", COLOR_TAG_CLASS, TRUE, TRUE, TRUE);
      g_string_append (string, label);
      g_string_append (string, value);

      if (NULL != (value = list_get_attribute (attributes, "id")))
        {
          g_free (label);
          label = ide_xml_parser_get_color_tag (self, "id", COLOR_TAG_ID, TRUE, TRUE, TRUE);
          g_string_append (string, label);
          g_string_append (string, value);
        }

      node = ide_xml_symbol_node_new (string->str, NULL, "object", IDE_SYMBOL_KIND_UI_OBJECT);
      g_object_set (node, "use-markup", TRUE, NULL);
    }
  else if (ide_str_equal0 (name, "template"))
    {
      value = get_attribute (attributes, "class", "?");
      label = ide_xml_parser_get_color_tag (self, "class", COLOR_TAG_CLASS, TRUE, TRUE, TRUE);
      g_string_append (string, label);
      g_string_append (string, value);
      g_free (label);

      value = get_attribute (attributes, "parent", "?");
      label = ide_xml_parser_get_color_tag (self, "parent", COLOR_TAG_PARENT, TRUE, TRUE, TRUE);
      g_string_append (string, label);
      g_string_append (string, value);

      node = ide_xml_symbol_node_new (string->str, NULL, (const gchar *)name, IDE_SYMBOL_KIND_UI_TEMPLATE);
      g_object_set (node, "use-markup", TRUE, NULL);
    }
  else if (ide_str_equal0 (name, "packing"))
    {
      node = ide_xml_symbol_node_new ("packing", NULL, "packing", IDE_SYMBOL_KIND_UI_PACKING);
    }
  else if (ide_str_equal0 (name, "style"))
    {
      node = ide_xml_symbol_node_new ("style", NULL, "style", IDE_SYMBOL_KIND_UI_STYLE);
    }
  else if (ide_str_equal0 (name, "menu"))
    {
      value = get_attribute (attributes, "id", "?");
      label = ide_xml_parser_get_color_tag (self, "id", COLOR_TAG_ID, TRUE, TRUE, TRUE);
      g_string_append (string, label);
      g_string_append (string, value);

      node = ide_xml_symbol_node_new (string->str, NULL, "menu", IDE_SYMBOL_KIND_UI_MENU);
      g_object_set (node, "use-markup", TRUE, NULL);
    }
  else if (ide_str_equal0 (name, "submenu"))
    {
      value = get_attribute (attributes, "id", "?");
      label = ide_xml_parser_get_color_tag (self, "id", COLOR_TAG_ID, TRUE, TRUE, TRUE);
      g_string_append (string, label);
      g_string_append (string, value);

      node = ide_xml_symbol_node_new (string->str, NULL, "submenu", IDE_SYMBOL_KIND_UI_SUBMENU);
      g_object_set (node, "use-markup", TRUE, NULL);
    }
  else if (ide_str_equal0 (name, "section"))
    {
      value = get_attribute (attributes, "id", "?");
      label = ide_xml_parser_get_color_tag (self, "id", COLOR_TAG_ID, TRUE, TRUE, TRUE);
      g_string_append (string, label);
      g_string_append (string, value);

      node = ide_xml_symbol_node_new (string->str, NULL, "section", IDE_SYMBOL_KIND_UI_SECTION);
      g_object_set (node, "use-markup", TRUE, NULL);
    }
  else if (ide_str_equal0 (name, "item"))
    {
      node = ide_xml_symbol_node_new ("item", NULL, "item", IDE_SYMBOL_KIND_UI_ITEM);
    }

  state->attributes = (const gchar **)attributes;
  ide_xml_parser_state_processing (self, state, (const gchar *)name, node, IDE_XML_SAX_CALLBACK_TYPE_START_ELEMENT, is_internal);
}

static const gchar *
get_menu_attribute_value (IdeXmlSymbolNode *node,
                          const gchar      *name)
{
  IdeXmlSymbolNode *child;
  gint n_children;

  g_assert (IDE_IS_XML_SYMBOL_NODE (node));

  n_children = ide_xml_symbol_node_get_n_internal_children (node);
  for (gint i = 0; i < n_children; ++i)
    {
      child = IDE_XML_SYMBOL_NODE (ide_xml_symbol_node_get_nth_internal_child (node, i));
      if (ide_symbol_node_get_kind (IDE_SYMBOL_NODE (child)) == IDE_SYMBOL_KIND_UI_MENU_ATTRIBUTE &&
          ide_str_equal0 (ide_symbol_node_get_name (IDE_SYMBOL_NODE (child)), name))
        {
          return ide_xml_symbol_node_get_value (child);
        }
    }

  return NULL;
}

static void
node_post_processing_collect_style_classes (IdeXmlParser      *self,
                                            IdeXmlSymbolNode  *node)
{
  IdeXmlSymbolNode *child;
  g_autoptr(GString) label = NULL;
  gint n_children;

  g_assert (IDE_IS_XML_PARSER (self));
  g_assert (IDE_IS_XML_SYMBOL_NODE (node));

  label = g_string_new (NULL);
  n_children = ide_xml_symbol_node_get_n_internal_children (node);
  for (gint i = 0; i < n_children; ++i)
    {
      g_autofree gchar *class_tag = NULL;
      const gchar *name;

      child = IDE_XML_SYMBOL_NODE (ide_xml_symbol_node_get_nth_internal_child (node, i));
      if (ide_symbol_node_get_kind (IDE_SYMBOL_NODE (child)) == IDE_SYMBOL_KIND_UI_STYLE_CLASS)
        {
          name = ide_symbol_node_get_name (IDE_SYMBOL_NODE (child));
          if (ide_str_empty0 (name))
            continue;

          class_tag = ide_xml_parser_get_color_tag (self, name, COLOR_TAG_STYLE_CLASS, TRUE, TRUE, TRUE);
          g_string_append (label, class_tag);
          g_string_append (label, " ");
        }
    }

  g_object_set (node,
                "name", label->str,
                "use-markup", TRUE,
                NULL);
}

static void
node_post_processing_add_label (IdeXmlParser      *self,
                                IdeXmlSymbolNode  *node)
{
  const gchar *value;
  g_autoptr(GString) name = NULL;
  g_autofree gchar *label = NULL;

  g_assert (IDE_IS_XML_PARSER (self));
  g_assert (IDE_IS_XML_SYMBOL_NODE (node));

  if (NULL != (value = get_menu_attribute_value (node, "label")))
    {
      g_object_get (node, "name", &label, NULL);
      name = g_string_new (label);
      g_free (label);

      label = ide_xml_parser_get_color_tag (self, "label", COLOR_TAG_LABEL, TRUE, TRUE, TRUE);

      g_string_append (name, label);
      g_string_append (name, value);
      g_object_set (node,
                    "name", name->str,
                    "use-markup", TRUE,
                    NULL);
    }
}

static gboolean
ide_xml_parser_ui_post_processing (IdeXmlParser      *self,
                                   IdeXmlSymbolNode  *root_node)
{
  g_autoptr(GPtrArray) ar = NULL;
  IdeXmlSymbolNode *node;
  const gchar *element_name;
  gint n_children;

  g_assert (IDE_IS_XML_PARSER (self));
  g_assert (IDE_IS_XML_SYMBOL_NODE (root_node));

  ar = g_ptr_array_new ();
  g_ptr_array_add (ar, root_node);

  while (ar->len > 0)
    {
      node = g_ptr_array_remove_index (ar, ar->len - 1);

      n_children = ide_xml_symbol_node_get_n_children (node);
      for (gint i = 0; i < n_children; ++i)
        g_ptr_array_add (ar, ide_xml_symbol_node_get_nth_child (node, i));

      element_name = ide_xml_symbol_node_get_element_name (node);

      if (ide_str_equal0 (element_name, "style"))
        node_post_processing_collect_style_classes (self, node);
      else if (ide_str_equal0 (element_name, "item") ||
               ide_str_equal0 (element_name, "submenu") ||
               ide_str_equal0 (element_name, "section"))
        node_post_processing_add_label (self, node);
    }

  return TRUE;
}

void
ide_xml_parser_ui_setup (IdeXmlParser *self,
                         ParserState  *state)
{
  g_assert (IDE_IS_XML_PARSER (self));
  g_assert (state != NULL);

  ide_xml_sax_clear (state->sax_parser);
  ide_xml_sax_set_callback (state->sax_parser, IDE_XML_SAX_CALLBACK_TYPE_START_ELEMENT, ide_xml_parser_ui_start_element_sax_cb);
  ide_xml_sax_set_callback (state->sax_parser, IDE_XML_SAX_CALLBACK_TYPE_END_ELEMENT, ide_xml_parser_end_element_sax_cb);
  ide_xml_sax_set_callback (state->sax_parser, IDE_XML_SAX_CALLBACK_TYPE_CHAR, ide_xml_parser_characters_sax_cb);

  ide_xml_sax_set_callback (state->sax_parser, IDE_XML_SAX_CALLBACK_TYPE_INTERNAL_SUBSET, ide_xml_parser_internal_subset_sax_cb);
  ide_xml_sax_set_callback (state->sax_parser, IDE_XML_SAX_CALLBACK_TYPE_EXTERNAL_SUBSET, ide_xml_parser_external_subset_sax_cb);
  ide_xml_sax_set_callback (state->sax_parser, IDE_XML_SAX_CALLBACK_TYPE_PROCESSING_INSTRUCTION, ide_xml_parser_processing_instruction_sax_cb);

  ide_xml_sax_set_callback (state->sax_parser, IDE_XML_SAX_CALLBACK_TYPE_WARNING, ide_xml_parser_warning_sax_cb);
  ide_xml_sax_set_callback (state->sax_parser, IDE_XML_SAX_CALLBACK_TYPE_ERROR, ide_xml_parser_error_sax_cb);
  ide_xml_sax_set_callback (state->sax_parser, IDE_XML_SAX_CALLBACK_TYPE_FATAL_ERROR, ide_xml_parser_fatal_error_sax_cb);

  ide_xml_parser_set_post_processing_callback (self, ide_xml_parser_ui_post_processing);
}
