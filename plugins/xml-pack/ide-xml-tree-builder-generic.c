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
#include "ide-xml-symbol-node.h"
#include "ide-xml-tree-builder-utils-private.h"

#include "ide-xml-tree-builder-generic.h"

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
      if (callback_type == IDE_XML_SAX_CALLBACK_TYPE_END_ELEMENT)
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
    {
      ide_xml_symbol_node_take_child (state->parent_node, node);
    }

  state->current_depth = depth;
  state->current_node = node;
}

static gchar *
collect_attributes (IdeXmlTreeBuilder  *self,
                    const gchar       **attributes)
{
  GString *string;
  gchar *value;
  const gchar **l = attributes;

  g_assert (IDE_IS_XML_TREE_BUILDER (self));

  if (attributes == NULL)
    return NULL;

  string = g_string_new (NULL);
  while (l [0] != NULL)
    {
      value = ide_xml_tree_builder_get_color_tag (self, l [0], COLOR_TAG_ATTRIBUTE, TRUE, TRUE, TRUE);
      g_string_append (string, value);
      g_free (value);
      g_string_append (string, l [1]);

      l += 2;
    }

  return g_string_free (string, FALSE);
}

static void
start_element_sax_cb (ParserState    *state,
                      const xmlChar  *name,
                      const xmlChar **attributes)
{
  IdeXmlTreeBuilder *self = (IdeXmlTreeBuilder *)state->self;
  IdeXmlSymbolNode *node = NULL;
  g_autofree gchar *attr = NULL;
  g_autofree gchar *label = NULL;

  g_assert (IDE_IS_XML_TREE_BUILDER (self));

  attr = collect_attributes (self, (const gchar **)attributes);
  label = g_strconcat ((const gchar *)name, attr, NULL);

  node = ide_xml_symbol_node_new (label, NULL, NULL, IDE_SYMBOL_XML_ELEMENT, NULL, 0, 0);
  g_object_set (node, "use-markup", TRUE, NULL);

  state_processing (state, (const gchar *)name, node, IDE_XML_SAX_CALLBACK_TYPE_START_ELEMENT, FALSE);
}

static void
comment_sax_cb (ParserState   *state,
                const xmlChar *name)
{
  IdeXmlSymbolNode *node = NULL;
  g_autofree gchar *strip_name = NULL;

  strip_name = g_strstrip (g_strdup ((const gchar *)name));
  node = ide_xml_symbol_node_new (strip_name, NULL, NULL, IDE_SYMBOL_XML_COMMENT, NULL, 0, 0);
  state_processing (state, "comment", node, IDE_XML_SAX_CALLBACK_TYPE_COMMENT, FALSE);
}

static void
cdata_sax_cb (ParserState   *state,
              const xmlChar *value,
              gint           len)
{
  IdeXmlSymbolNode *node = NULL;

  node = ide_xml_symbol_node_new ("cdata", NULL, NULL, IDE_SYMBOL_XML_CDATA, NULL, 0, 0);
  state_processing (state, "cdata", node, IDE_XML_SAX_CALLBACK_TYPE_CDATA, FALSE);
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

IdeXmlAnalysis *
ide_xml_tree_builder_generic_create (IdeXmlTreeBuilder *self,
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
  ide_xml_sax_set_callback (parser, IDE_XML_SAX_CALLBACK_TYPE_COMMENT, comment_sax_cb);
  ide_xml_sax_set_callback (parser, IDE_XML_SAX_CALLBACK_TYPE_CDATA, cdata_sax_cb);
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

  analysis = g_steal_pointer (&state->analysis);
  diagnostics = ide_diagnostics_new (g_steal_pointer (&state->diagnostics_array));
  ide_xml_analysis_set_diagnostics (analysis, diagnostics);

  parser_state_free (state);
  return analysis;
}
