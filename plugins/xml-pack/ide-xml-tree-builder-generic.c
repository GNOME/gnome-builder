/* ide-xml-tree-builder-generic.c
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

#include "ide-xml-stack.h"
#include "ide-xml-tree-builder-utils-private.h"

#include "ide-xml-tree-builder-generic.h"

static IdeXmlSymbolNode *
create_node_from_reader (XmlReader *reader)
{
  const gchar *name;
  GFile *file = NULL;
  guint line = 0;
  guint line_offset = 0;

  name = xml_reader_get_name (reader);

  return ide_xml_symbol_node_new (name, NULL, NULL,
                                  IDE_SYMBOL_UI_OBJECT,
                                  file, line, line_offset);
}

IdeXmlSymbolNode *
ide_xml_tree_builder_generic_create (IdeXmlSax   *parser,
                                     GFile       *file,
                                     const gchar *data,
                                     gsize        size)
{
  IdeXmlStack *stack;
  IdeXmlSymbolNode *root_node;
  IdeXmlSymbolNode *parent_node;
  IdeXmlSymbolNode *current_node;
  IdeXmlSymbolNode *previous_node = NULL;
  xmlReaderTypes type;
  gint depth = 0;
  gint current_depth = 0;
  gboolean is_empty;

  /* g_assert (XML_IS_READER (reader)); */

  /* stack = stack_new (); */

  /* parent_node = root_node = ide_xml_symbol_node_new ("root", IDE_SYMBOL_NONE, */
  /*                                                    NULL, 0, 0); */
  /* stack_push (stack, parent_node); */

  /* while (xml_reader_read (reader)) */
  /*   { */
  /*     type = xml_reader_get_node_type (reader); */
  /*     if ( type == XML_READER_TYPE_ELEMENT) */
  /*       { */
  /*         current_node = create_node_from_reader (reader); */
  /*         depth = xml_reader_get_depth (reader); */
  /*         is_empty = xml_reader_is_empty_element (reader); */
  /*         if (depth < 0) */
  /*           { */
  /*             g_warning ("Wrong xml element depth, current:%i new:%i\n", current_depth, depth); */
  /*             break; */
  /*           } */

  /*         if (depth > current_depth) */
  /*           { */
  /*             ++current_depth; */
  /*             stack_push (stack, parent_node); */

  /*             g_assert (previous_node != NULL); */
  /*             parent_node = previous_node; */
  /*             ide_xml_symbol_node_take_child (parent_node, current_node); */
  /*           } */
  /*         else if (depth < current_depth) */
  /*           { */
  /*             --current_depth; */
  /*             parent_node = stack_pop (stack); */
  /*             if (parent_node == NULL) */
  /*               { */
  /*                 g_warning ("Xml nodes stack empty\n"); */
  /*                 break; */
  /*               } */

  /*             g_assert (parent_node != NULL); */
  /*             ide_xml_symbol_node_take_child (parent_node, current_node); */
  /*           } */
  /*         else */
  /*           { */
  /*             ide_xml_symbol_node_take_child (parent_node, current_node); */
  /*           } */

  /*         previous_node = current_node; */
  /*         print_node (current_node, depth); */
  /*       } */
  /*   } */

  /* printf ("stack size:%li\n", stack_get_size (stack)); */

  /* stack_destroy (stack); */

  return root_node;
}
