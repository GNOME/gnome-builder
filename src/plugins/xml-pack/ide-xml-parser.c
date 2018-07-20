/* ide-xml-parser.c
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
 */

#define G_LOG_DOMAIN "ide-xml-parser"

#include <glib/gi18n.h>
#include <glib-object.h>

#include <libxml/xmlerror.h>

#include "ide-xml-parser.h"
#include "ide-xml-parser-generic.h"
#include "ide-xml-parser-ui.h"
#include "ide-xml-parser-private.h"
#include "ide-xml-sax.h"
#include "ide-xml-service.h"
#include "ide-xml-schema-cache-entry.h"
#include "ide-xml-stack.h"
#include "ide-xml-tree-builder-utils-private.h"
#include "ide-xml-utils.h"

#include "../gi/ide-gi-service.h"

typedef struct _ColorTag
{
  gchar *name;
  gchar *fg;
  gchar *bg;
} ColorTag;

G_DEFINE_TYPE (IdeXmlParser, ide_xml_parser, IDE_TYPE_OBJECT)

static void
color_tag_free (gpointer *data)
{
  ColorTag *tag = (ColorTag *)data;

  g_free (tag->name);
  g_free (tag->fg);
  g_free (tag->bg);
}

#define STATIC_COLOR_TAG(a,b,c) \
  { (gchar*)a, (gchar *)b, (gchar *)c }

/* Keep it in sync with ColorTagId enum */
static ColorTag default_color_tags [] = {
  STATIC_COLOR_TAG ("label",       "#000000", "#D5E7FC" ), // COLOR_TAG_LABEL
  STATIC_COLOR_TAG ("id",          "#000000", "#D9E7BD" ), // COLOR_TAG_ID
  STATIC_COLOR_TAG ("style-class", "#000000", "#DFCD9B" ), // COLOR_TAG_STYLE_CLASS
  STATIC_COLOR_TAG ("type",        "#000000", "#F4DAC3" ), // COLOR_TAG_TYPE
  STATIC_COLOR_TAG ("parent",      "#000000", "#DEBECF" ), // COLOR_TAG_PARENT
  STATIC_COLOR_TAG ("class",       "#000000", "#FFEF98" ), // COLOR_TAG_CLASS
  STATIC_COLOR_TAG ("attribute",   "#000000", "#F0E68C" ), // COLOR_TAG_ATTRIBUTE
  { NULL },
};

static void
require_entry_free (gpointer *data)
{
  RequireEntry *entry = (RequireEntry *)data;

  g_free (entry->runtime_id);
  g_free (entry->package);
  g_free (entry->ns_name);

  g_slice_free (RequireEntry, entry);
}

static void
parser_state_free (ParserState *state)
{
  dzl_clear_pointer (&state->analysis, ide_xml_analysis_unref);
  dzl_clear_pointer (&state->diagnostics_array, g_ptr_array_unref);
  dzl_clear_pointer (&state->requires_array, g_ptr_array_unref);
  g_clear_object (&state->file);
  g_clear_object (&state->root_node);
  g_clear_object (&state->sax_parser);
  g_clear_object (&state->stack);

  dzl_clear_pointer (&state->content, g_bytes_unref);
  dzl_clear_pointer (&state->schemas, g_ptr_array_unref);

  dzl_clear_pointer (&state->error_data_str, g_free);

  g_slice_free (ParserState, state);
}

static gboolean
ide_xml_parser_file_is_ui (GFile       *file,
                           const gchar *data,
                           gsize        size)
{
  g_autofree gchar *path = NULL;
  g_autofree gchar *buffer = NULL;
  gsize buffer_size;

  g_assert (G_IS_FILE (file));
  g_assert (data != NULL);
  g_assert (size > 0);

  /* Very relaxed GtkBuilder ui files detection
   * so that we can get completion from the start
   */

  path = g_file_get_path (file);
  if (g_str_has_suffix (path, ".ui") || g_str_has_suffix (path, ".glade"))
    {
      buffer_size = (size < 256) ? size : 256;
      if (size == 0)
        return TRUE;

      buffer = g_strndup (data, buffer_size);
      if (*buffer == '<')
        return TRUE;
    }

  return FALSE;
}

static IdeDiagnostic *
ide_xml_parser_create_custom_diagnostic (IdeXmlParser          *self,
                                         GFile                 *file,
                                         const gchar           *msg,
                                         gint                   start_line,
                                         gint                   start_line_offset,
                                         gint                   end_line,
                                         gint                   end_line_offset,
                                         IdeDiagnosticSeverity  severity)
{
  IdeDiagnostic *diagnostic;
  IdeContext *context;
  g_autoptr(IdeSourceLocation) start_loc = NULL;
  g_autoptr(IdeSourceLocation) end_loc = NULL;
  g_autoptr(IdeFile) ifile = NULL;

  g_assert (IDE_IS_XML_PARSER (self));
  g_assert (G_IS_FILE (file));

  context = ide_object_get_context (IDE_OBJECT (self));
  ifile = ide_file_new (context, file);
  start_loc = ide_source_location_new (ifile,
                                       start_line - 1,
                                       start_line_offset - 1,
                                       0);

  if (end_line > -1)
    {
      IdeSourceRange *range;

      end_loc = ide_source_location_new (ifile,
                                         end_line - 1,
                                         end_line_offset - 1,
                                         0);

      range = ide_source_range_new (start_loc, end_loc);
      diagnostic = ide_diagnostic_new (severity, msg, NULL);
      ide_diagnostic_take_range (diagnostic, range);
    }
  else
    diagnostic = ide_diagnostic_new (severity, msg, start_loc);

  return diagnostic;
}

IdeDiagnostic *
_ide_xml_parser_create_diagnostic (ParserState            *state,
                                   const gchar            *msg,
                                   IdeDiagnosticSeverity   severity)
{
  IdeXmlParser *self = (IdeXmlParser *)state->self;
  gint start_line;
  gint start_line_offset;
  gint end_line;
  gint end_line_offset;
  gsize size;

  g_assert (IDE_IS_XML_PARSER (self));

  ide_xml_sax_get_location (state->sax_parser,
                            &start_line, &start_line_offset,
                            &end_line, &end_line_offset,
                            NULL,
                            &size);

  if (size == 0)
    end_line = -1;

  return ide_xml_parser_create_custom_diagnostic (self,
                                                  state->file,
                                                  msg,
                                                  start_line, start_line_offset,
                                                  end_line, end_line_offset,
                                                  severity);
}

static void
set_error_state (ParserState *state,
                 ErrorState   error_state,
                 gchar       *data_str)
{
  g_assert (state != NULL);

  state->error_state = error_state;
  state->error_data_str = g_strdup (data_str);
}

static void
clear_error_state (ParserState *state)
{
  g_assert (state != NULL);

  state->error_state = ERROR_STATE_NONE;
  dzl_clear_pointer (&state->error_data_str,g_free);
}

void
_ide_xml_parser_state_processing (IdeXmlParser          *self,
                                  ParserState           *state,
                                  const gchar           *element_name,
                                  IdeXmlSymbolNode      *node,
                                  IdeXmlSaxCallbackType  callback_type,
                                  gboolean               is_internal)
{
  IdeXmlSymbolNode *parent_node;
  G_GNUC_UNUSED IdeXmlSymbolNode *popped_node;
  g_autofree gchar *popped_element_name = NULL;
  const gchar *erroneous_element_name;
  const gchar *content;
  gint start_line;
  gint start_line_offset;
  gint end_line;
  gint end_line_offset;
  gsize size;
  gint depth;

  g_assert (IDE_IS_XML_SYMBOL_NODE (node) || node == NULL);

  state->build_state = BUILD_STATE_NORMAL;

  if (callback_type == IDE_XML_SAX_CALLBACK_TYPE_CHAR && IDE_IS_XML_SYMBOL_NODE (node))
    {
      ide_xml_symbol_node_set_value (state->current_node, element_name);
      return;
    }

  depth = ide_xml_sax_get_depth (state->sax_parser);

  if (state->error_state == ERROR_STATE_MISSING_TAG_END)
    {
      erroneous_element_name = ide_xml_symbol_node_get_element_name (state->parent_node);
      /* TODO: we need better node comparaison (ns) here */
      if (!dzl_str_equal0 (erroneous_element_name, element_name))
        {
          if (!(popped_node = ide_xml_stack_pop (state->stack, &popped_element_name, &parent_node, &depth)))
            goto error;

          ide_xml_symbol_node_set_state (popped_node, IDE_XML_SYMBOL_NODE_STATE_NOT_CLOSED);
          dzl_clear_pointer (&popped_element_name, g_free);

          state->parent_node = parent_node;
          g_assert (state->parent_node != NULL);
        }
    }
  /* else if (state->error_state == ERROR_STATE_MISSING_END_TAG_MISSMATCH) */
  /*   { */
  /*     g_assert (callback_type == IDE_XML_SAX_CALLBACK_TYPE_END_ELEMENT); */

      /* Remove the unpaired node from the stack */
  /*     if (!(popped_node = ide_xml_stack_pop (state->stack, &popped_element_name, &parent_node, &depth))) */
  /*       goto error; */

  /*     g_assert (dzl_str_equal0 (popped_element_name, element_name)); */
  /*     ide_xml_symbol_node_set_state (popped_node, IDE_XML_SYMBOL_NODE_STATE_MISSING_END_TAG); */
  /*     dzl_clear_pointer (&popped_element_name, g_free); */
  /*   } */

  ide_xml_sax_get_location (state->sax_parser,
                            &start_line,
                            &start_line_offset,
                            &end_line,
                            &end_line_offset,
                            &content,
                            &size);

  /* No node means not created by one of the specific parser */
  if (node == NULL && callback_type != IDE_XML_SAX_CALLBACK_TYPE_END_ELEMENT)
    node = ide_xml_symbol_node_new ("internal", NULL, element_name, IDE_SYMBOL_XML_ELEMENT);

  if (node != NULL)
    ide_xml_symbol_node_set_location (node,
                                      g_object_ref (state->file),
                                      start_line, start_line_offset,
                                      end_line, end_line_offset,
                                      size);

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
      ide_xml_symbol_node_set_attributes (node, state->attributes);
      state->attributes = NULL;
    }
  else if (callback_type == IDE_XML_SAX_CALLBACK_TYPE_END_ELEMENT)
    {
      /* In case of missmatch, element_name is the expected end element,
       * the found end element is in state->error_data_str.
       */

      while (TRUE)
        {
          if (!(popped_node = ide_xml_stack_pop (state->stack, &popped_element_name, &parent_node, &depth)))
            goto  error;

          if (dzl_str_equal0 (popped_element_name, element_name))
            {
              if (state->error_state == ERROR_STATE_MISSING_END_TAG_MISSMATCH)
                ide_xml_symbol_node_set_state (popped_node, IDE_XML_SYMBOL_NODE_STATE_END_TAG_MISSMATCH);

              ide_xml_symbol_node_set_end_tag_location (popped_node,
                                                        start_line, start_line_offset,
                                                        end_line, end_line_offset,
                                                        size);
              state->parent_node = parent_node;
              g_assert (state->parent_node != NULL);

              break;
            }
        }
    }
  else
    {
      if (is_internal)
        ide_xml_symbol_node_take_internal_child (state->parent_node, node);
      else
        ide_xml_symbol_node_take_child (state->parent_node, node);
    }

  clear_error_state (state);
  state->current_depth = depth;
  state->current_node = node;

  return;

error:
  clear_error_state (state);
  g_warning ("Xml nodes stack empty\n");
  return;
}

void
_ide_xml_parser_end_element_sax_cb (ParserState   *state,
                                    const xmlChar *name)
{
  IdeXmlParser *self = (IdeXmlParser *)state->self;

  g_assert (IDE_IS_XML_PARSER (self));

  _ide_xml_parser_state_processing (self,
                                    state,
                                    (const gchar *)name,
                                    NULL,
                                    IDE_XML_SAX_CALLBACK_TYPE_END_ELEMENT,
                                    FALSE);
}

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wformat-nonliteral"

void
_ide_xml_parser_warning_sax_cb (ParserState   *state,
                                const xmlChar *name,
                                ...)
{
  IdeDiagnostic *diagnostic;
  g_autofree gchar *msg = NULL;
  va_list var_args;

  g_assert (state != NULL);
  g_assert (IDE_IS_XML_PARSER (state->self));

  va_start (var_args, name);
  msg = g_strdup_vprintf ((const gchar *)name, var_args);
  va_end (var_args);

  diagnostic = _ide_xml_parser_create_diagnostic (state, msg, IDE_DIAGNOSTIC_WARNING);
  g_ptr_array_add (state->diagnostics_array, diagnostic);
}

void
_ide_xml_parser_error_sax_cb (ParserState   *state,
                              const xmlChar *name,
                              ...)
{
  IdeDiagnostic *diagnostic;
  xmlParserCtxt *context;
  xmlError *error;
  const gchar *base;
  const gchar *current;
  g_autofree gchar *msg = NULL;
  va_list var_args;

  g_assert (state != NULL);
  g_assert (IDE_IS_XML_PARSER (state->self));

  va_start (var_args, name);
  msg = g_strdup_vprintf ((const gchar *)name, var_args);
  va_end (var_args);

  diagnostic = _ide_xml_parser_create_diagnostic (state, msg, IDE_DIAGNOSTIC_ERROR);
  g_ptr_array_add (state->diagnostics_array, diagnostic);

  context = ide_xml_sax_get_context (state->sax_parser);
  base = (const gchar *)context->input->base;
  current = (const gchar *)context->input->cur;
  error = xmlCtxtGetLastError (context);
  if (error != NULL && error->domain == XML_FROM_PARSER)
    {
      if (error->code == XML_ERR_GT_REQUIRED)
        {
          /* If a tag is not closed, we want the following nodes to be siblings, not children */
          set_error_state (state, ERROR_STATE_MISSING_TAG_END, NULL);
        }
      else if (error->code == XML_ERR_NAME_REQUIRED && context->instate == XML_PARSER_CONTENT)
        {
          const gchar *prev;
          IdeXmlSymbolNode *node;
          gint start_line, start_line_offset;
          gint end_line, end_line_offset;
          gsize size;

          prev = current - 1;
          if (prev >= base && *prev == '<')
            {
              /* '<' only, no name tag, or '<>', empty node, node not created, we need to do it ourself*/
              node = ide_xml_symbol_node_new ("internal", NULL, NULL, IDE_SYMBOL_XML_ELEMENT);

              if (*current == '>')
                ide_xml_symbol_node_set_state (node, IDE_XML_SYMBOL_NODE_STATE_EMPTY);
              else
                ide_xml_symbol_node_set_state (node, IDE_XML_SYMBOL_NODE_STATE_NOT_CLOSED);

              ide_xml_symbol_node_take_internal_child (state->parent_node, node);

              ide_xml_sax_get_location (state->sax_parser,
                                        &start_line, &start_line_offset,
                                        &end_line, &end_line_offset,
                                        NULL, &size);
              ide_xml_symbol_node_set_location (node, g_object_ref (state->file),
                                                start_line, start_line_offset,
                                                end_line, end_line_offset,
                                                size);
            }
        }
      else if (error->code == XML_ERR_TAG_NAME_MISMATCH)
        set_error_state (state, ERROR_STATE_MISSING_END_TAG_MISSMATCH, error->str2);
    }
}

void
_ide_xml_parser_fatal_error_sax_cb (ParserState   *state,
                                    const xmlChar *name,
                                    ...)
{
  IdeDiagnostic *diagnostic;
  g_autofree gchar *msg = NULL;
  va_list var_args;

  g_assert (state != NULL);
  g_assert (IDE_IS_XML_PARSER (state->self));

  va_start (var_args, name);
  msg = g_strdup_vprintf ((const gchar *)name, var_args);
  va_end (var_args);

  diagnostic = _ide_xml_parser_create_diagnostic (state, msg, IDE_DIAGNOSTIC_FATAL);
  g_ptr_array_add (state->diagnostics_array, diagnostic);
}

#pragma GCC diagnostic pop

void
_ide_xml_parser_internal_subset_sax_cb (ParserState   *state,
                                        const xmlChar *name,
                                        const xmlChar *external_id,
                                        const xmlChar *system_id)
{
  IdeXmlSchemaCacheEntry *entry;

  g_assert (state != NULL);
  g_assert (IDE_IS_XML_PARSER (state->self));

  if (dzl_str_empty0 ((gchar *)external_id) || dzl_str_empty0 ((gchar *)system_id))
    return;

  entry = ide_xml_schema_cache_entry_new ();
  entry->kind = SCHEMA_KIND_DTD;
  ide_xml_sax_get_location (state->sax_parser, &entry->line, &entry->col, NULL, NULL, NULL, NULL);
  g_ptr_array_add (state->schemas, entry);
}

void
_ide_xml_parser_external_subset_sax_cb (ParserState   *state,
                                        const xmlChar *name,
                                        const xmlChar *external_id,
                                        const xmlChar *system_id)
{
  g_assert (state != NULL);
  g_assert (IDE_IS_XML_PARSER (state->self));
}

static GFile *
get_absolute_schema_file (GFile       *file,
                          const gchar *schema_url)
{
  g_autoptr (GFile) parent = NULL;
  GFile *abs_file = NULL;
  g_autofree gchar *scheme = NULL;

  abs_file = g_file_new_for_uri (schema_url);
  scheme = g_file_get_uri_scheme (abs_file);
  if (scheme == NULL)
    {
      parent = g_file_get_parent (file);
      if (NULL == (abs_file = g_file_resolve_relative_path (parent, schema_url)))
        abs_file = g_file_new_for_path (schema_url);
    }

  return abs_file;
}

void
_ide_xml_parser_processing_instruction_sax_cb (ParserState   *state,
                                               const xmlChar *target,
                                               const xmlChar *data)
{
  IdeDiagnostic *diagnostic;
  g_autofree gchar *schema_url = NULL;
  const gchar *extension;
  IdeXmlSchemaCacheEntry *entry;
  IdeXmlSchemaKind kind;

  g_assert (state != NULL);
  g_assert (IDE_IS_XML_PARSER (state->self));

  if ((schema_url = get_schema_url ((const gchar *)data)))
    {
      if ((extension = strrchr (schema_url, '.')))
        {
          ++extension;
          if (dzl_str_equal0 (extension, "rng"))
            kind = SCHEMA_KIND_RNG;
          else if (dzl_str_equal0 (extension, "xsd"))
            kind = SCHEMA_KIND_XML_SCHEMA;
          else
            goto fail;

          /* We skip adding gtkbuilder.rng here and add it from gresources after the parsing */
          if (g_str_has_suffix (schema_url, "gtkbuilder.rng"))
            return;

          entry = ide_xml_schema_cache_entry_new ();
          entry->file = get_absolute_schema_file (state->file, schema_url);;
          entry->kind = kind;

          ide_xml_sax_get_location (state->sax_parser,
                                    &entry->line,
                                    &entry->col,
                                    NULL, NULL, NULL, NULL);

          /* Needed to pass the kind to the service schema fetcher */
          g_object_set_data (G_OBJECT (entry->file), "kind", GUINT_TO_POINTER (entry->kind));

          g_ptr_array_add (state->schemas, entry);

          return;
        }
fail:
      diagnostic = _ide_xml_parser_create_diagnostic (state,
                                                     _("Schema type not supported"),
                                                     IDE_DIAGNOSTIC_WARNING);
      g_ptr_array_add (state->diagnostics_array, diagnostic);
    }
}

void
_ide_xml_parser_characters_sax_cb (ParserState   *state,
                                   const xmlChar *name,
                                   gint           len)
{
  IdeXmlParser *self = (IdeXmlParser *)state->self;
  g_autofree gchar *element_value = NULL;

  g_assert (IDE_IS_XML_PARSER (self));

  if (state->build_state != BUILD_STATE_GET_CONTENT)
    return;

  element_value = g_strndup ((gchar *)name, len);
  _ide_xml_parser_state_processing (self,
                                    state,
                                    element_value,
                                    NULL,
                                    IDE_XML_SAX_CALLBACK_TYPE_CHAR,
                                    FALSE);
}

/* Here name can be either a package name or a namespace */
static gchar *
split_package_name (const gchar *fullname,
                    guint16     *package_major_version,
                    guint16     *package_minor_version,
                    gboolean    *has_version)
{
  gchar *name = NULL;
  const gchar *cursor = fullname;
  guint16 tmp_major_version;
  guint16 tmp_minor_version;

  *package_major_version = 0;
  *package_minor_version = 0;
  *has_version = FALSE;

  if ((cursor = strrchr (cursor, '-')) &&
      g_ascii_isdigit (*(cursor + 1)))
    {
      name = g_strndup (fullname, cursor - fullname);
      if  (ide_xml_utils_parse_version (cursor + 1, &tmp_major_version, &tmp_minor_version, NULL))
        {
          *package_major_version = tmp_major_version;
          *package_minor_version = tmp_minor_version;
          *has_version = TRUE;
        }

      return name;
    }

  return strdup (fullname);
}

static void
analysis_process (IdeXmlParser *self,
                  ParserState  *state)
{
  IdeXmlSchemaCacheEntry *entry;
  g_autoptr(IdeDiagnostics) diagnostics = NULL;

  g_assert (IDE_IS_XML_PARSER (self));

  diagnostics = ide_diagnostics_new (IDE_PTR_ARRAY_STEAL_FULL (&state->diagnostics_array));
  ide_xml_analysis_set_diagnostics (state->analysis, diagnostics);

  if (state->file_is_ui)
    {
      entry = ide_xml_schema_cache_entry_new ();
      entry->kind = SCHEMA_KIND_RNG;
      entry->file = g_file_new_for_uri ("resource:///org/gnome/builder/plugins/xml-pack-plugin/schemas/gtkbuilder.rng");
      g_object_set_data (G_OBJECT (entry->file), "kind", GUINT_TO_POINTER (entry->kind));
      g_ptr_array_add (state->schemas, entry);
    }

  if (state->schemas != NULL && state->schemas->len > 0)
    ide_xml_analysis_set_schemas (state->analysis, g_steal_pointer (&state->schemas));

  ide_xml_analysis_set_sequence (state->analysis, state->sequence);

  ide_task_return_pointer (state->analysis_task,
                           g_steal_pointer (&state->analysis),
                           (GDestroyNotify)ide_xml_analysis_unref);
}

static void
ide_xml_parser_get_analysis_worker (IdeTask      *task,
                                    gpointer      source_object,
                                    gpointer      task_data,
                                    GCancellable *cancellable)
{
  IdeXmlParser *self = (IdeXmlParser *)source_object;
  ParserState *state = (ParserState *)task_data;
  g_autofree gchar *uri = NULL;
  const gchar *doc_data;
  gsize doc_size;

  g_assert (IDE_IS_XML_PARSER (self));
  g_assert (IDE_IS_TASK (task));
  g_assert (state != NULL);
  g_assert (cancellable == NULL || G_IS_CANCELLABLE (cancellable));

  if (ide_task_return_error_if_cancelled (task))
    return;

  doc_data = g_bytes_get_data (state->content, &doc_size);

  state->file_is_ui = ide_xml_parser_file_is_ui (state->file, doc_data, doc_size);
  ide_xml_analysis_set_is_ui (state->analysis, state->file_is_ui);

  if (state->file_is_ui)
    ide_xml_parser_ui_setup (self, state);
  else
    ide_xml_parser_generic_setup (self, state);

  uri = g_file_get_uri (state->file);
  if (!ide_xml_sax_parse (state->sax_parser, doc_data, doc_size, uri, state))
    {
      ide_task_return_new_error (task,
                                 G_IO_ERROR,
                                 G_IO_ERROR_FAILED,
                                 _("Failed to create the XML tree."));
      return;
    }

  if (self->post_processing_callback != NULL)
    (self->post_processing_callback)(self, state->root_node);

  ide_task_return_boolean (task, TRUE);
}

typedef struct
{
  IdeXmlParser *parser;
  ParserState  *parser_state;
  RequireEntry *entry;
} RequireState;

static void
add_warning_at_entry (IdeXmlParser *self,
                      ParserState  *state,
                      RequireEntry *entry,
                      const gchar  *msg)
{
  g_ptr_array_add (state->diagnostics_array,
                   ide_xml_parser_create_custom_diagnostic (self,
                                                            state->file,
                                                            msg,
                                                            entry->start_line,
                                                            entry->start_line_offset,
                                                            entry->end_line,
                                                            entry->end_line_offset,
                                                            IDE_DIAGNOSTIC_WARNING));
}

static void
analysis_requires_cb (GObject      *object,
                      GAsyncResult *result,
                      gpointer      user_data)
{
  RequireState *req_state = (RequireState *)user_data;
  IdeXmlParser *self = req_state->parser;
  ParserState *state = req_state->parser_state;
  RequireEntry *entry = req_state->entry;
  g_autoptr(GError) error = NULL;
  gchar *modversion;

  g_assert (req_state != NULL);
  g_assert (IDE_IS_XML_PARSER (self));
  g_assert (G_IS_ASYNC_RESULT (result));
  g_assert (IDE_IS_MAIN_THREAD ());

  if ((modversion = ide_task_propagate_pointer (IDE_TASK (result), &error)))
    {
      g_autofree gchar *package_name = NULL;
      IdeGiRequire *require = ide_xml_analysis_get_require (state->analysis);
      IdeGiRequireBound bound;
      guint16 package_major_version;
      guint16 package_minor_version;
      guint16 pkg_mod_major_version;
      guint16 pkg_mod_minor_version;
      gboolean has_version;

      package_name = split_package_name (entry->package,
                                         &package_major_version,
                                         &package_minor_version,
                                         &has_version);

      ide_xml_utils_parse_version (modversion, &pkg_mod_major_version, &pkg_mod_minor_version, NULL);

      bound = (IdeGiRequireBound) {IDE_GI_REQUIRE_COMP_GREATER_OR_EQUAL,
                                   entry->ns_major_version,
                                   entry->ns_minor_version};

      ide_gi_require_add (require, entry->ns_name, bound);

      if (entry->mod_major_version != pkg_mod_major_version ||
          entry->mod_minor_version > pkg_mod_minor_version)
        {
          g_autofree gchar *msg = g_strdup_printf (_("The highest version found is: %s"), modversion);
          add_warning_at_entry (self, state, entry, msg);
        }
    }
  else
    add_warning_at_entry (self, state, entry, error->message);

  g_slice_free (RequireState, req_state);
  if (--state->modversion_count == 0)
    analysis_process (self, state);
}

static void
analysis_worker_cb (GObject      *object,
                    GAsyncResult *result,
                    gpointer      user_data)
{
  IdeXmlParser *self = (IdeXmlParser *)object;
  ParserState *state = (ParserState *)user_data;
  IdeContext *context;
  IdeXmlService *xml_service;
  g_autoptr(GError) error = NULL;

  g_assert (IDE_IS_XML_PARSER (self));
  g_assert (G_IS_ASYNC_RESULT (result));
  g_assert (state != NULL);
  g_assert (IDE_IS_MAIN_THREAD ());

  if (!ide_task_propagate_boolean (IDE_TASK (result), &error))
    {
      g_autoptr(IdeTask) analysis_task = g_steal_pointer (&state->analysis_task);

      ide_task_return_error (analysis_task, g_steal_pointer (&error));
      return;
    }

  if (0 == (state->modversion_count = state->requires_array->len))
    {
      analysis_process (self, state);
      return;
    }

  context = ide_object_get_context (IDE_OBJECT (self));
  xml_service = ide_context_get_service_typed (context, IDE_TYPE_XML_SERVICE);

  for (guint i = 0; i < state->requires_array->len; i++)
    {
      RequireEntry *entry = g_ptr_array_index (state->requires_array, i);
      RequireState *req_state = g_slice_new0 (RequireState);

      req_state->parser = self;
      req_state->parser_state = state;
      req_state->entry = entry;

      ide_xml_service_get_modversion_async (xml_service,
                                            entry->runtime_id,
                                            entry->package,
                                            NULL,
                                            analysis_requires_cb,
                                            req_state);
    }
}

void
ide_xml_parser_get_analysis_async (IdeXmlParser        *self,
                                   GFile               *file,
                                   GBytes              *content,
                                   gint64               sequence,
                                   GCancellable        *cancellable,
                                   GAsyncReadyCallback  callback,
                                   gpointer             user_data)
{
  g_autoptr(IdeTask) worker_task = NULL;
  IdeTask *analysis_task;
  ParserState *state;

  g_return_if_fail (IDE_IS_XML_PARSER (self));
  g_return_if_fail (G_IS_FILE (file));
  g_return_if_fail (content != NULL);
  g_return_if_fail (!cancellable || G_IS_CANCELLABLE (cancellable));
  g_return_if_fail (IDE_IS_MAIN_THREAD ());

  analysis_task = ide_task_new (self, cancellable, callback, user_data);

  state = g_slice_new0 (ParserState);
  state->self = self;
  state->file = g_object_ref (file);
  state->content = g_bytes_ref (content);
  state->sequence = sequence;
  state->diagnostics_array = g_ptr_array_new_with_free_func ((GDestroyNotify)ide_diagnostic_unref);
  state->requires_array = g_ptr_array_new_with_free_func ((GDestroyNotify)require_entry_free);
  state->schemas = g_ptr_array_new_with_free_func (g_object_unref);
  state->sax_parser = ide_xml_sax_new ();
  state->stack = ide_xml_stack_new ();
  state->analysis_task = analysis_task;

  state->build_state = BUILD_STATE_NORMAL;

  state->analysis = ide_xml_analysis_new (-1);
  state->root_node = ide_xml_symbol_node_new ("root", NULL, "root", IDE_SYMBOL_NONE);
  ide_xml_symbol_node_set_is_root (state->root_node, TRUE);
  ide_xml_analysis_set_root_node (state->analysis, state->root_node);

  state->parent_node = state->root_node;
  ide_xml_stack_push (state->stack, "root", state->root_node, NULL, 0);

  ide_task_set_task_data (analysis_task, state, (GDestroyNotify)parser_state_free);
  ide_task_set_source_tag (analysis_task, ide_xml_parser_get_analysis_async);

  worker_task = ide_task_new (self, cancellable, analysis_worker_cb, state);
  ide_task_set_task_data (worker_task, state, NULL);
  ide_task_set_source_tag (worker_task, ide_xml_parser_get_analysis_async);
  ide_task_set_kind (worker_task, IDE_TASK_KIND_INDEXER);

  ide_task_run_in_thread (worker_task, ide_xml_parser_get_analysis_worker);
}

IdeXmlAnalysis *
ide_xml_parser_get_analysis_finish (IdeXmlParser  *self,
                                    GAsyncResult  *result,
                                    GError       **error)
{
  g_return_val_if_fail (IDE_IS_XML_PARSER (self), NULL);
  g_return_val_if_fail (IDE_IS_TASK (result), NULL);

  return ide_task_propagate_pointer (IDE_TASK (result), error);
}

gchar *
_ide_xml_parser_get_color_tag (IdeXmlParser *self,
                               const gchar  *str,
                               ColorTagId    id,
                               gboolean      space_before,
                               gboolean      space_after,
                               gboolean      space_inside)
{
  ColorTag *tag;

  g_assert (IDE_IS_XML_PARSER (self));
  g_assert (self->color_tags != NULL);
  g_assert (!dzl_str_empty0 (str));

  tag = &g_array_index (self->color_tags, ColorTag, id);
  return g_strdup_printf ("%s<span foreground=\"%s\" background=\"%s\">%s%s%s</span>%s",
                          space_before ? " " : "",
                          tag->fg,
                          tag->bg,
                          space_inside ? " " : "",
                          str,
                          space_inside ? " " : "",
                          space_after ? " " : "");
}

static void
init_color_tags (IdeXmlParser *self)
{
  g_autofree gchar *scheme_name = NULL;
  GtkSourceStyleSchemeManager *manager;
  GtkSourceStyleScheme *scheme;
  GtkSourceStyle *style;
  ColorTag tag;
  ColorTag *tag_ptr;
  gboolean tag_set;

  g_assert (IDE_IS_XML_PARSER (self));

  scheme_name = g_settings_get_string (self->settings, "style-scheme-name");
  manager = gtk_source_style_scheme_manager_get_default ();
  scheme = gtk_source_style_scheme_manager_get_scheme (manager, scheme_name);

  g_array_remove_range (self->color_tags, 0, self->color_tags->len);
  tag_ptr = default_color_tags;
  while (tag_ptr->fg != NULL)
    {
      tag_set = FALSE;
      if (scheme != NULL)
        {
          g_autofree gchar *tag_name = g_strconcat ("symboltree::", tag_ptr->name, NULL);

          if ((style = gtk_source_style_scheme_get_style (scheme, tag_name)))
            {
              g_autofree gchar *foreground = NULL;
              g_autofree gchar *background = NULL;

              g_object_get (style, "foreground", &foreground, NULL);
              g_object_get (style, "background", &background, NULL);
              if (foreground != NULL && background != NULL)
                {
                  tag_set = TRUE;
                  tag.name = g_strdup (tag_ptr->name);
                  tag.fg = g_steal_pointer (&foreground);
                  tag.bg = g_steal_pointer (&background);
                }
            }
        }

      if (!tag_set)
        {
          tag.name = g_strdup (tag_ptr->name);
          tag.fg = g_strdup (tag_ptr->fg);
          tag.bg = g_strdup (tag_ptr->bg);
        }

      g_array_append_val (self->color_tags, tag);
      ++tag_ptr;
    }
}

void
_ide_xml_parser_set_post_processing_callback (IdeXmlParser           *self,
                                              PostProcessingCallback  callback)
{
  g_return_if_fail (IDE_IS_XML_PARSER (self));

  self->post_processing_callback = callback;
}

static void
editor_settings_changed_cb (IdeXmlParser *self,
                            gchar        *key,
                            GSettings    *settings)
{
  g_assert (IDE_IS_XML_PARSER (self));

  if (dzl_str_equal0 (key, "style-scheme-name"))
    init_color_tags (self);
}

IdeXmlParser *
ide_xml_parser_new (void)
{
  return g_object_new (IDE_TYPE_XML_PARSER, NULL);
}

static void
ide_xml_parser_finalize (GObject *object)
{
  IdeXmlParser *self = (IdeXmlParser *)object;

  dzl_clear_pointer (&self->color_tags, g_array_unref);
  g_clear_object (&self->settings);

  G_OBJECT_CLASS (ide_xml_parser_parent_class)->finalize (object);
}

static void
ide_xml_parser_class_init (IdeXmlParserClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = ide_xml_parser_finalize;
}

static void
ide_xml_parser_init (IdeXmlParser *self)
{
  self->color_tags = g_array_new (TRUE, TRUE, sizeof (ColorTag));
  g_array_set_clear_func (self->color_tags, (GDestroyNotify)color_tag_free);

  self->settings = g_settings_new ("org.gnome.builder.editor");
  g_signal_connect_swapped (self->settings,
                            "changed",
                            G_CALLBACK (editor_settings_changed_cb),
                            self);

  init_color_tags (self);
}
