/* ide-xml-tree-builder-ui.c
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
#include "ide-xml-symbol-node.h"
#include "ide-xml-tree-builder-utils-private.h"

#include "ide-xml-tree-builder-ui.h"

typedef struct _ParserState
{
  IdeXmlSax        *parser;
  IdeXmlStack      *stack;
  GFile            *file;
  IdeXmlSymbolNode *root_node;
  IdeXmlSymbolNode *parent_node;
  IdeXmlSymbolNode *current_node;
  gint              current_depth;
} ParserState;

static void
parser_state_free (ParserState *state)
{
  g_clear_object (&state->stack);
  g_clear_object (&state->file);
  g_clear_object (&state->parser);
}

static void
state_processing (ParserState           *state,
                  const gchar           *element_name,
                  IdeXmlSymbolNode      *node,
                  IdeXmlSaxCallbackType  callback_type)
{
  IdeXmlSymbolNode *parent_node;
  IdeXmlSymbolNode *popped_node;
  g_autofree gchar *popped_element_name = NULL;
  gint line;
  gint line_offset;
  gint depth;

  depth = ide_xml_sax_get_depth (state->parser);
  printf ("[name:%s depth:%i] ", element_name, depth);

  if (node == NULL)
    {
      if (callback_type == IDE_XML_SAX_CALLBACK_TYPE_START_ELEMENT)
        {
          ide_xml_stack_push (state->stack, element_name, NULL, state->parent_node, depth);

          printf ("no node PUSH %i->%i\n", state->current_depth, depth);
        }
      else if (callback_type == IDE_XML_SAX_CALLBACK_TYPE_END_ELEMENT)
        {
          /* TODO: compare current with popped */
          if (ide_xml_stack_is_empty (state->stack))
            {
              g_warning ("Xml nodes stack empty\n");
              return;
            }

          popped_node = ide_xml_stack_pop (state->stack, &popped_element_name, &parent_node, &depth);
          state->parent_node = parent_node;
          g_assert (state->parent_node != NULL);

          printf ("no node POP %i->%i name:%s parent:%p\n",
                  state->current_depth, depth, popped_element_name, state->parent_node);
        }

      state->current_depth = depth;
      state->current_node = NULL;
      return;
    }

  ide_xml_sax_get_position (state->parser, &line, &line_offset);
  ide_xml_symbol_node_set_location (node, g_object_ref (state->file), line, line_offset);

  /* TODO: take end elements into account and use:
   * || ABS (depth - current_depth) > 1
   */
  if (depth < 0)
    {
      g_warning ("Wrong xml element depth, current:%i new:%i\n", state->current_depth, depth);
      return;
    }

  if (callback_type == IDE_XML_SAX_CALLBACK_TYPE_START_ELEMENT)
    {
      ide_xml_stack_push (state->stack, element_name, node, state->parent_node, depth);
      ide_xml_symbol_node_take_child (state->parent_node, node);

      printf ("PUSH %i->%i current:%p parent:%p\n",
              state->current_depth, depth, node, state->parent_node);

      state->parent_node = node;
    }
  else if (callback_type == IDE_XML_SAX_CALLBACK_TYPE_END_ELEMENT)
    {
      /* TODO: compare current with popped */
      if (ide_xml_stack_is_empty (state->stack))
        {
          g_warning ("Xml nodes stack empty\n");
          return;
        }

      popped_node = ide_xml_stack_pop (state->stack, &popped_element_name, &parent_node, &depth);
      state->parent_node = parent_node;
      g_assert (state->parent_node != NULL);

      printf ("POP %i->%i name:%s parent node:%p\n",
              state->current_depth, depth, popped_element_name, parent_node);
    }
  else
    {
      printf ("---- %i->%i\n", state->current_depth, depth);
      ide_xml_symbol_node_take_child (state->parent_node, node);
    }

  state->current_depth = depth;
  state->current_node = node;
  print_node (node, depth);
}

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
start_element_sax_cb (ParserState    *state,
                      const xmlChar  *name,
                      const xmlChar **atttributes)
{
  g_autoptr (GString) string = NULL;
  const gchar *value = NULL;
  IdeXmlSymbolNode *node = NULL;

  string = g_string_new (NULL);

  if (ide_str_equal0 (name, "child"))
    {
      g_string_append (string, "child");

      if (NULL != (value = get_attribute (atttributes, "type", NULL)))
        {
          g_string_append (string, " type: ");
          g_string_append (string, value);
        }

      if (NULL != (value = get_attribute (atttributes, "internal-child", NULL)))
        {
          g_string_append (string, " internal: ");
          g_string_append (string, value);
        }

      node = ide_xml_symbol_node_new (string->str, "child",
                                      IDE_SYMBOL_UI_CHILD, NULL, 0, 0);
    }
  else if (ide_str_equal0 (name, "object"))
    {
      value = get_attribute (atttributes, "class", "?");
      g_string_append (string, value);

      if (NULL != (value = list_get_attribute (atttributes, "id")))
        {
          g_string_append (string, " id: ");
          g_string_append (string, value);
        }

      node = ide_xml_symbol_node_new (string->str, "object",
                                      IDE_SYMBOL_UI_OBJECT, NULL, 0, 0);
    }
  else if (ide_str_equal0 (name, "template"))
    {
      value = get_attribute (atttributes, "class", "?");
      g_string_append (string, value);

      value = get_attribute (atttributes, "parent", "?");
      g_string_append (string, " parent: ");
      g_string_append (string, value);

      node = ide_xml_symbol_node_new (string->str, (const gchar *)name,
                                      IDE_SYMBOL_UI_TEMPLATE, NULL, 0, 0);
    }
  else if (ide_str_equal0 (name, "packing"))
    {
      node = ide_xml_symbol_node_new ("packing", "packing",
                                      IDE_SYMBOL_UI_PACKING, NULL, 0, 0);
    }
  else if (ide_str_equal0 (name, "style"))
    {
      /* TODO: collect style classes names */
      node = ide_xml_symbol_node_new ("style", "style",
                                      IDE_SYMBOL_UI_STYLE, NULL, 0, 0);
    }
  else if (ide_str_equal0 (name, "menu"))
    {
      value = get_attribute (atttributes, "id", "?");
      g_string_append (string, value);

      node = ide_xml_symbol_node_new (string->str, "menu",
                                      IDE_SYMBOL_UI_MENU, NULL, 0, 0);
    }
  else if (ide_str_equal0 (name, "submenu"))
    {
      /* TODO: show content of attribute name="label" */
      value = get_attribute (atttributes, "id", "?");
      g_string_append (string, value);

      node = ide_xml_symbol_node_new (string->str, "submenu",
                                      IDE_SYMBOL_UI_SUBMENU, NULL, 0, 0);
    }
  else if (ide_str_equal0 (name, "section"))
    {
      value = get_attribute (atttributes, "id", "?");
      g_string_append (string, value);

      node = ide_xml_symbol_node_new (string->str, "section",
                                      IDE_SYMBOL_UI_SECTION, NULL, 0, 0);
    }
  else if (ide_str_equal0 (name, "item"))
    {
      /* TODO: show content of attribute name="label" */
      node = ide_xml_symbol_node_new ("item", "item",
                                      IDE_SYMBOL_UI_ITEM, NULL, 0, 0);
    }

  state_processing (state, (const gchar *)name, node, IDE_XML_SAX_CALLBACK_TYPE_START_ELEMENT);
}

static void
end_element_sax_cb (ParserState    *state,
                    const xmlChar  *name)
{
  printf ("end element:%s\n", name);

  state_processing (state, (const gchar *)name, NULL, IDE_XML_SAX_CALLBACK_TYPE_END_ELEMENT);
}

IdeXmlSymbolNode *
ide_xml_tree_builder_ui_create (IdeXmlSax   *parser,
                                GFile       *file,
                                const gchar *data,
                                gsize        length)
{
  ParserState *state;
  g_autofree gchar *uri = NULL;

  g_assert (IDE_IS_XML_SAX (parser));

  state = g_slice_new0 (ParserState);
  state->parser = g_object_ref (parser);
  state->stack = ide_xml_stack_new ();
  state->file = g_object_ref (file);

  ide_xml_sax_clear (parser);
  ide_xml_sax_set_callback (parser, IDE_XML_SAX_CALLBACK_TYPE_START_ELEMENT, start_element_sax_cb);
  ide_xml_sax_set_callback (parser, IDE_XML_SAX_CALLBACK_TYPE_END_ELEMENT, end_element_sax_cb);

  state->root_node = ide_xml_symbol_node_new ("root", "root", IDE_SYMBOL_NONE, NULL, 0, 0);
  ide_xml_stack_push (state->stack, "root", state->root_node, NULL, 0);

  state->parent_node = state->root_node;
  printf ("root node:%p\n", state->root_node);

  uri = g_file_get_uri (file);
  ide_xml_sax_parse (parser, data, length, uri, state);
  printf ("stack size:%li\n", ide_xml_stack_get_size (state->stack));

  parser_state_free (state);

  return state->root_node;
}
