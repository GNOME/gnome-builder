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

typedef enum _BuildState
{
  BUILD_STATE_NORMAL,
  BUILD_STATE_WAIT_END_ELEMENT,
  BUILD_STATE_GET_CONTENT,
} BuildState;

typedef struct _ParserState
{
  IdeXmlTreeBuilder *self;
  IdeXmlSax         *parser;
  IdeXmlStack       *stack;
  GFile             *file;
  IdeXmlAnalysis    *analysis;
  GPtrArray         *diagnostics_array;
  IdeXmlSymbolNode  *root_node;
  IdeXmlSymbolNode  *parent_node;
  IdeXmlSymbolNode  *current_node;
  BuildState         build_state;
  gint               current_depth;
} ParserState;

static void
parser_state_free (ParserState *state)
{
  g_clear_pointer (&state->analysis, ide_xml_analysis_unref);
  g_clear_pointer (&state->diagnostics_array, g_ptr_array_unref);
  g_clear_object (&state->stack);
  g_clear_object (&state->file);
  g_clear_object (&state->parser);
  g_clear_object (&state->root_node);
}

static void
state_processing (ParserState           *state,
                  const gchar           *element_name,
                  IdeXmlSymbolNode      *node,
                  IdeXmlSaxCallbackType  callback_type,
                  gboolean               is_internal)
{
  IdeXmlSymbolNode *parent_node;
  IdeXmlSymbolNode *popped_node G_GNUC_UNUSED;
  g_autofree gchar *popped_element_name = NULL;
  gint line;
  gint line_offset;
  gint depth;

  g_assert (IDE_IS_XML_SYMBOL_NODE (node) || node == NULL);

  if (callback_type == IDE_XML_SAX_CALLBACK_TYPE_CHAR)
    {
      ide_xml_symbol_node_set_value (state->current_node, element_name);
      return;
    }

  depth = ide_xml_sax_get_depth (state->parser);

  if (node == NULL)
    {
      if (callback_type == IDE_XML_SAX_CALLBACK_TYPE_START_ELEMENT)
        ide_xml_stack_push (state->stack, element_name, NULL, state->parent_node, depth);
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
      if (is_internal)
        ide_xml_symbol_node_take_internal_child (state->parent_node, node);
      else
        ide_xml_symbol_node_take_child (state->parent_node, node);

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
    }
  else
    ide_xml_symbol_node_take_child (state->parent_node, node);

  state->current_depth = depth;
  state->current_node = node;
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
                      const xmlChar **attributes)
{
  IdeXmlTreeBuilder *self = (IdeXmlTreeBuilder *)state->self;
  g_autoptr(GString) string = NULL;
  g_autofree gchar *label = NULL;
  const gchar *value = NULL;
  IdeXmlSymbolNode *node = NULL;
  const gchar *parent_name;
  gboolean is_internal = FALSE;

  g_assert (IDE_IS_XML_TREE_BUILDER (self));

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
          node = ide_xml_symbol_node_new (value, NULL, "property",
                                          IDE_SYMBOL_UI_PROPERTY, NULL, 0, 0);
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
          node = ide_xml_symbol_node_new (value, NULL, "attribute",
                                          IDE_SYMBOL_UI_MENU_ATTRIBUTE, NULL, 0, 0);
          is_internal = TRUE;
          state->build_state = BUILD_STATE_GET_CONTENT;
        }
    }
  else if (ide_str_equal0 (name, "class") && ide_str_equal0 (parent_name, "style"))
    {
      value = get_attribute (attributes, "name", NULL);
      node = ide_xml_symbol_node_new (value, NULL, "class",
                                      IDE_SYMBOL_UI_STYLE_CLASS, NULL, 0, 0);
      is_internal = TRUE;
    }
  else if (ide_str_equal0 (name, "child"))
    {
      g_string_append (string, "child");

      if (NULL != (value = get_attribute (attributes, "type", NULL)))
        {
          label = ide_xml_tree_builder_get_color_tag (self, "type", COLOR_TAG_TYPE, TRUE, TRUE, TRUE);
          g_string_append (string, label);
          g_string_append (string, value);
        }

      if (NULL != (value = get_attribute (attributes, "internal-child", NULL)))
        {
          label = ide_xml_tree_builder_get_color_tag (self, "internal", COLOR_TAG_TYPE, TRUE, TRUE, TRUE);
          g_string_append (string, label);
          g_string_append (string, value);
        }

      node = ide_xml_symbol_node_new (string->str, NULL, "child",
                                      IDE_SYMBOL_UI_CHILD, NULL, 0, 0);
      g_object_set (node, "use-markup", TRUE, NULL);
    }
  else if (ide_str_equal0 (name, "object"))
    {
      value = get_attribute (attributes, "class", "?");
      label = ide_xml_tree_builder_get_color_tag (self, "class", COLOR_TAG_CLASS, TRUE, TRUE, TRUE);
      g_string_append (string, label);
      g_string_append (string, value);

      if (NULL != (value = list_get_attribute (attributes, "id")))
        {
          g_free (label);
          label = ide_xml_tree_builder_get_color_tag (self, "id", COLOR_TAG_ID, TRUE, TRUE, TRUE);
          g_string_append (string, label);
          g_string_append (string, value);
        }

      node = ide_xml_symbol_node_new (string->str, NULL, "object",
                                      IDE_SYMBOL_UI_OBJECT, NULL, 0, 0);
      g_object_set (node, "use-markup", TRUE, NULL);
    }
  else if (ide_str_equal0 (name, "template"))
    {
      value = get_attribute (attributes, "class", "?");
      label = ide_xml_tree_builder_get_color_tag (self, "class", COLOR_TAG_CLASS, TRUE, TRUE, TRUE);
      g_string_append (string, label);
      g_string_append (string, value);
      g_free (label);

      value = get_attribute (attributes, "parent", "?");
      label = ide_xml_tree_builder_get_color_tag (self, "parent", COLOR_TAG_PARENT, TRUE, TRUE, TRUE);
      g_string_append (string, label);
      g_string_append (string, value);

      node = ide_xml_symbol_node_new (string->str, NULL, (const gchar *)name,
                                      IDE_SYMBOL_UI_TEMPLATE, NULL, 0, 0);
      g_object_set (node, "use-markup", TRUE, NULL);
    }
  else if (ide_str_equal0 (name, "packing"))
    {
      node = ide_xml_symbol_node_new ("packing", NULL, "packing",
                                      IDE_SYMBOL_UI_PACKING, NULL, 0, 0);
    }
  else if (ide_str_equal0 (name, "style"))
    {
      node = ide_xml_symbol_node_new ("style", NULL, "style",
                                      IDE_SYMBOL_UI_STYLE, NULL, 0, 0);
    }
  else if (ide_str_equal0 (name, "menu"))
    {
      value = get_attribute (attributes, "id", "?");
      label = ide_xml_tree_builder_get_color_tag (self, "id", COLOR_TAG_ID, TRUE, TRUE, TRUE);
      g_string_append (string, label);
      g_string_append (string, value);

      node = ide_xml_symbol_node_new (string->str, NULL, "menu",
                                      IDE_SYMBOL_UI_MENU, NULL, 0, 0);
      g_object_set (node, "use-markup", TRUE, NULL);
    }
  else if (ide_str_equal0 (name, "submenu"))
    {
      value = get_attribute (attributes, "id", "?");
      label = ide_xml_tree_builder_get_color_tag (self, "id", COLOR_TAG_ID, TRUE, TRUE, TRUE);
      g_string_append (string, label);
      g_string_append (string, value);

      node = ide_xml_symbol_node_new (string->str, NULL, "submenu",
                                      IDE_SYMBOL_UI_SUBMENU, NULL, 0, 0);
      g_object_set (node, "use-markup", TRUE, NULL);
    }
  else if (ide_str_equal0 (name, "section"))
    {
      value = get_attribute (attributes, "id", "?");
      label = ide_xml_tree_builder_get_color_tag (self, "id", COLOR_TAG_ID, TRUE, TRUE, TRUE);
      g_string_append (string, label);
      g_string_append (string, value);

      node = ide_xml_symbol_node_new (string->str, NULL, "section",
                                      IDE_SYMBOL_UI_SECTION, NULL, 0, 0);
      g_object_set (node, "use-markup", TRUE, NULL);
    }
  else if (ide_str_equal0 (name, "item"))
    {
      node = ide_xml_symbol_node_new ("item", NULL, "item",
                                      IDE_SYMBOL_UI_ITEM, NULL, 0, 0);
    }

  state_processing (state, (const gchar *)name, node, IDE_XML_SAX_CALLBACK_TYPE_START_ELEMENT, is_internal);
}

static void
end_element_sax_cb (ParserState    *state,
                    const xmlChar  *name)
{
  IdeXmlTreeBuilder *self = (IdeXmlTreeBuilder *)state->self;

  g_assert (IDE_IS_XML_TREE_BUILDER (self));

  state_processing (state, (const gchar *)name, NULL, IDE_XML_SAX_CALLBACK_TYPE_END_ELEMENT, FALSE);
}

static IdeDiagnostic *
create_diagnostic (ParserState            *state,
                   const gchar            *msg,
                   IdeDiagnosticSeverity   severity)
{
  IdeContext *context;
  IdeDiagnostic *diagnostic;
  g_autoptr(IdeSourceLocation) loc = NULL;
  g_autoptr(IdeFile) ifile = NULL;
  gint line;
  gint line_offset;

  context = ide_object_get_context (IDE_OBJECT (state->self));

  ide_xml_sax_get_position (state->parser, &line, &line_offset);
  ifile = ide_file_new (context, state->file);
  loc = ide_source_location_new (ifile,
                                 line - 1,
                                 line_offset - 1,
                                 0);

  diagnostic = ide_diagnostic_new (severity, msg, loc);

  return diagnostic;
}

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wformat-nonliteral"

static void
warning_sax_cb (ParserState    *state,
                const xmlChar  *name,
                ...)
{
  IdeXmlTreeBuilder *self = (IdeXmlTreeBuilder *)state->self;
  IdeDiagnostic *diagnostic;
  g_autofree gchar *msg = NULL;
  va_list var_args;

  g_assert (IDE_IS_XML_TREE_BUILDER (self));

  va_start (var_args, name);
  msg = g_strdup_vprintf ((const gchar *)name, var_args);
  va_end (var_args);

  diagnostic = create_diagnostic (state, msg, IDE_DIAGNOSTIC_WARNING);
  g_ptr_array_add (state->diagnostics_array, diagnostic);
}

static void
error_sax_cb (ParserState    *state,
              const xmlChar  *name,
              ...)
{
  IdeXmlTreeBuilder *self = (IdeXmlTreeBuilder *)state->self;
  IdeDiagnostic *diagnostic;
  g_autofree gchar *msg = NULL;
  va_list var_args;

  g_assert (IDE_IS_XML_TREE_BUILDER (self));

  va_start (var_args, name);
  msg = g_strdup_vprintf ((const gchar *)name, var_args);
  va_end (var_args);

  diagnostic = create_diagnostic (state, msg, IDE_DIAGNOSTIC_ERROR);
  g_ptr_array_add (state->diagnostics_array, diagnostic);
}

static void
fatal_error_sax_cb (ParserState    *state,
                    const xmlChar  *name,
                    ...)
{
  IdeXmlTreeBuilder *self = (IdeXmlTreeBuilder *)state->self;
  IdeDiagnostic *diagnostic;
  g_autofree gchar *msg = NULL;
  va_list var_args;

  g_assert (IDE_IS_XML_TREE_BUILDER (self));

  va_start (var_args, name);
  msg = g_strdup_vprintf ((const gchar *)name, var_args);
  va_end (var_args);

  diagnostic = create_diagnostic (state, msg, IDE_DIAGNOSTIC_FATAL);
  g_ptr_array_add (state->diagnostics_array, diagnostic);
}

#pragma GCC diagnostic pop

static void
characters_sax_cb (ParserState    *state,
                   const xmlChar  *name,
                   gint            len)
{
  IdeXmlTreeBuilder *self = (IdeXmlTreeBuilder *)state->self;
  g_autofree gchar *element_value = NULL;

  g_assert (IDE_IS_XML_TREE_BUILDER (self));

  if (state->build_state != BUILD_STATE_GET_CONTENT)
    return;

  element_value = g_strndup ((gchar *)name, len);
  state->build_state = BUILD_STATE_NORMAL;

  state_processing (state, element_value, NULL, IDE_XML_SAX_CALLBACK_TYPE_CHAR, FALSE);
}

const gchar *
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
      if (ide_symbol_node_get_kind (IDE_SYMBOL_NODE (child)) == IDE_SYMBOL_UI_MENU_ATTRIBUTE &&
          ide_str_equal0 (ide_symbol_node_get_name (IDE_SYMBOL_NODE (child)), name))
        {
          return ide_xml_symbol_node_get_value (child);
        }
    }

  return NULL;
}

static void
node_post_processing_collect_style_classes (IdeXmlTreeBuilder *self,
                                            IdeXmlSymbolNode  *node)
{
  IdeXmlSymbolNode *child;
  g_autoptr(GString) label = NULL;
  gint n_children;

  g_assert (IDE_IS_XML_SYMBOL_NODE (node));

  label = g_string_new (NULL);
  n_children = ide_xml_symbol_node_get_n_internal_children (node);
  for (gint i = 0; i < n_children; ++i)
    {
      g_autofree gchar *class_tag = NULL;

      child = IDE_XML_SYMBOL_NODE (ide_xml_symbol_node_get_nth_internal_child (node, i));
      if (ide_symbol_node_get_kind (IDE_SYMBOL_NODE (child)) == IDE_SYMBOL_UI_STYLE_CLASS)
        {
          class_tag = ide_xml_tree_builder_get_color_tag (self, ide_symbol_node_get_name (IDE_SYMBOL_NODE (child)),
                                                          COLOR_TAG_STYLE_CLASS, TRUE, TRUE, TRUE);
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
node_post_processing_add_label (IdeXmlTreeBuilder *self,
                                IdeXmlSymbolNode  *node)
{
  const gchar *value;
  g_autoptr(GString) name = NULL;
  g_autofree gchar *label = NULL;

  g_assert (IDE_IS_XML_SYMBOL_NODE (node));

  if (NULL != (value = get_menu_attribute_value (node, "label")))
    {
      g_object_get (node, "name", &label, NULL);
      name = g_string_new (label);
      g_free (label);

      label = ide_xml_tree_builder_get_color_tag (self, "label", COLOR_TAG_LABEL, TRUE, TRUE, TRUE);

      g_string_append (name, label);
      g_string_append (name, value);
      g_object_set (node,
                    "name", name->str,
                    "use-markup", TRUE,
                    NULL);
    }
}

static gboolean
ide_xml_tree_builder_post_processing (IdeXmlTreeBuilder *self,
                                      IdeXmlSymbolNode  *root_node)
{
  g_autoptr(GPtrArray) ar = NULL;
  IdeXmlSymbolNode *node;
  const gchar *element_name;
  gint n_children;

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

IdeXmlAnalysis *
ide_xml_tree_builder_ui_create (IdeXmlTreeBuilder *self,
                                IdeXmlSax         *parser,
                                GFile             *file,
                                const gchar       *data,
                                gsize              length)
{
  ParserState *state;
  IdeXmlAnalysis *analysis;
  g_autoptr(IdeDiagnostics) diagnostics = NULL;
  g_autofree gchar *uri = NULL;

  g_return_val_if_fail (IDE_IS_XML_SAX (parser), NULL);
  g_return_val_if_fail (G_IS_FILE (file), NULL);
  g_return_val_if_fail (data != NULL, NULL);
  g_return_val_if_fail (length > 0, NULL);

  state = g_slice_new0 (ParserState);
  state->self = self;
  state->parser = g_object_ref (parser);
  state->stack = ide_xml_stack_new ();
  state->file = g_object_ref (file);
  state->diagnostics_array = g_ptr_array_new_with_free_func ((GDestroyNotify)ide_diagnostic_unref);
  state->build_state = BUILD_STATE_NORMAL;

  ide_xml_sax_clear (parser);
  ide_xml_sax_set_callback (parser, IDE_XML_SAX_CALLBACK_TYPE_START_ELEMENT, start_element_sax_cb);
  ide_xml_sax_set_callback (parser, IDE_XML_SAX_CALLBACK_TYPE_END_ELEMENT, end_element_sax_cb);
  ide_xml_sax_set_callback (parser, IDE_XML_SAX_CALLBACK_TYPE_CHAR, characters_sax_cb);

  ide_xml_sax_set_callback (parser, IDE_XML_SAX_CALLBACK_TYPE_WARNING, warning_sax_cb);
  ide_xml_sax_set_callback (parser, IDE_XML_SAX_CALLBACK_TYPE_ERROR, error_sax_cb);
  ide_xml_sax_set_callback (parser, IDE_XML_SAX_CALLBACK_TYPE_FATAL_ERROR, fatal_error_sax_cb);

  state->analysis = ide_xml_analysis_new (-1);
  state->root_node = ide_xml_symbol_node_new ("root", NULL, "root", IDE_SYMBOL_NONE, NULL, 0, 0);
  ide_xml_analysis_set_root_node (state->analysis, state->root_node);

  state->parent_node = state->root_node;
  ide_xml_stack_push (state->stack, "root", state->root_node, NULL, 0);

  uri = g_file_get_uri (file);
  ide_xml_sax_parse (parser, data, length, uri, state);

  ide_xml_tree_builder_post_processing (self, state->root_node);

  analysis = g_steal_pointer (&state->analysis);
  diagnostics = ide_diagnostics_new (g_steal_pointer (&state->diagnostics_array));
  ide_xml_analysis_set_diagnostics (analysis, diagnostics);

  parser_state_free (state);
  return analysis;
}
