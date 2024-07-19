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
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include <glib/gi18n.h>
#include <glib-object.h>

#include <libxml/xmlerror.h>

#include "ide-xml-parser.h"
#include "ide-xml-parser-generic.h"
#include "ide-xml-parser-ui.h"
#include "ide-xml-parser-private.h"
#include "ide-xml-sax.h"
#include "ide-xml-schema-cache-entry.h"
#include "ide-xml-stack.h"
#include "ide-xml-tree-builder-utils-private.h"

typedef struct _ColorTag
{
  gchar *name;
  gchar *fg;
  gchar *bg;
} ColorTag;

G_DEFINE_FINAL_TYPE (IdeXmlParser, ide_xml_parser, IDE_TYPE_OBJECT)

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
parser_state_free (ParserState *state)
{
  g_clear_pointer (&state->analysis, ide_xml_analysis_unref);
  g_clear_pointer (&state->diagnostics_array, g_ptr_array_unref);
  g_clear_object (&state->file);
  g_clear_object (&state->root_node);
  g_clear_object (&state->sax_parser);
  g_clear_object (&state->stack);

  g_clear_pointer (&state->content, g_bytes_unref);
  g_clear_pointer (&state->schemas, g_ptr_array_unref);

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

  path = g_file_get_path (file);
  if (g_str_has_suffix (path, ".ui") || g_str_has_suffix (path, ".glade"))
    {
      buffer_size = (size < 256) ? size : 256;
      buffer = g_strndup (data, buffer_size);
      if (NULL != (strstr (buffer, "<interface>")))
        return TRUE;
    }

  return FALSE;
}

static gboolean
ide_xml_parser_file_is_gtk3 (GFile       *file,
                             const gchar *data,
                             gsize        size)
{
  g_autofree gchar *buffer = NULL;
  gsize buffer_size;

  g_assert (G_IS_FILE (file));
  g_assert (data != NULL);
  g_assert (size > 0);

  buffer_size = (size < 512) ? size : 512;
  buffer = g_strndup (data, buffer_size);

  if (strstr (buffer, "<requires lib=\"gtk\" version=\"3") != NULL ||
      strstr (buffer, "<requires lib='gtk' version='3") != NULL)
    return TRUE;

  return FALSE;
}

IdeDiagnostic *
ide_xml_parser_create_diagnostic (ParserState            *state,
                                  const gchar            *msg,
                                  IdeDiagnosticSeverity   severity)
{
  g_autoptr(IdeLocation) start_loc = NULL;
  g_autoptr(IdeLocation) end_loc = NULL;
  IdeDiagnostic *diagnostic;
  gint start_line;
  gint start_line_offset;
  gint end_line;
  gint end_line_offset;
  gsize size;

  g_assert (state != NULL);
  g_assert (IDE_IS_XML_PARSER (state->self));

  ide_xml_sax_get_location (state->sax_parser,
                            &start_line, &start_line_offset,
                            &end_line, &end_line_offset,
                            NULL,
                            &size);

  start_loc = ide_location_new (state->file,
                                start_line - 1,
                                start_line_offset - 1);

  if (size > 0)
    {
      IdeRange *range;

      end_loc = ide_location_new (state->file,
                                  end_line - 1,
                                  end_line_offset - 1);

      range = ide_range_new (start_loc, end_loc);
      diagnostic = ide_diagnostic_new (severity, msg, NULL);
      ide_diagnostic_take_range (diagnostic, range);
    }
  else
    diagnostic = ide_diagnostic_new (severity, msg, start_loc);

  return diagnostic;
}

void
ide_xml_parser_state_processing (IdeXmlParser          *self,
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

  if (callback_type == IDE_XML_SAX_CALLBACK_TYPE_CHAR && IDE_IS_XML_SYMBOL_NODE (node))
    {
      ide_xml_symbol_node_set_value (state->current_node, element_name);
      return;
    }

  if (state->error_missing_tag_end)
    {
      erroneous_element_name = ide_xml_symbol_node_get_element_name (state->parent_node);
      /* TODO: we need better node comparaison (ns) here */
      if (!ide_str_equal0 (erroneous_element_name, element_name))
        {
          if (ide_xml_stack_is_empty (state->stack))
            goto error;

          popped_node = ide_xml_stack_pop (state->stack, &popped_element_name, &parent_node, &depth);
          ide_xml_symbol_node_set_state (popped_node, IDE_XML_SYMBOL_NODE_STATE_NOT_CLOSED);
          g_clear_pointer (&popped_element_name, g_free);

          state->parent_node = parent_node;
          g_assert (state->parent_node != NULL);
        }
    }

  depth = ide_xml_sax_get_depth (state->sax_parser);
  ide_xml_sax_get_location (state->sax_parser, &start_line, &start_line_offset, &end_line, &end_line_offset, &content, &size);

  /* No node mean not created by one of the specific parser */
  if (node == NULL)
    {
      if (callback_type == IDE_XML_SAX_CALLBACK_TYPE_START_ELEMENT)
        {
          node = ide_xml_symbol_node_new ("internal", NULL, element_name, IDE_SYMBOL_KIND_XML_ELEMENT);
          ide_xml_symbol_node_set_location (node, g_object_ref (state->file),
                                            start_line, start_line_offset,
                                            end_line, end_line_offset,
                                            size);

          ide_xml_stack_push (state->stack, element_name, node, state->parent_node, depth);
          ide_xml_symbol_node_take_internal_child (state->parent_node, node);
          state->parent_node = node;

          ide_xml_symbol_node_set_attributes (node, state->attributes);
          state->attributes = NULL;
        }
      else if (callback_type == IDE_XML_SAX_CALLBACK_TYPE_END_ELEMENT)
        {
          while (TRUE)
            {
              if (ide_xml_stack_is_empty (state->stack))
                goto error;

              popped_node = ide_xml_stack_pop (state->stack, &popped_element_name, &parent_node, &depth);
              if (ide_str_equal0 (popped_element_name, element_name))
                {
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

      state->current_depth = depth;
      state->current_node = node;
      state->error_missing_tag_end = FALSE;

      return;
    }

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
      /* TODO: compare current with popped */
      if (ide_xml_stack_is_empty (state->stack))
        goto error;

      popped_node = ide_xml_stack_pop (state->stack, &popped_element_name, &parent_node, &depth);
      state->parent_node = parent_node;
      g_assert (state->parent_node != NULL);
    }
  else
    ide_xml_symbol_node_take_child (state->parent_node, node);

  state->current_depth = depth;
  state->current_node = node;
  state->error_missing_tag_end = FALSE;

  return;

error:
  g_warning ("Xml nodes stack empty\n");
  return;
}

void
ide_xml_parser_end_element_sax_cb (ParserState    *state,
                                   const xmlChar  *name)
{
  IdeXmlParser *self = (IdeXmlParser *)state->self;

  g_assert (IDE_IS_XML_PARSER (self));

  ide_xml_parser_state_processing (self, state, (const gchar *)name, NULL, IDE_XML_SAX_CALLBACK_TYPE_END_ELEMENT, FALSE);
}

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wformat-nonliteral"

void
ide_xml_parser_warning_sax_cb (ParserState    *state,
                               const xmlChar  *name,
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

  diagnostic = ide_xml_parser_create_diagnostic (state, msg, IDE_DIAGNOSTIC_WARNING);
  g_ptr_array_add (state->diagnostics_array, diagnostic);
}

void
ide_xml_parser_error_sax_cb (ParserState    *state,
                             const xmlChar  *name,
                             ...)
{
  IdeDiagnostic *diagnostic;
  xmlParserCtxt *context;
  const xmlError *error;
  const gchar *base;
  const gchar *current;
  g_autofree gchar *msg = NULL;
  va_list var_args;

  g_assert (state != NULL);
  g_assert (IDE_IS_XML_PARSER (state->self));

  va_start (var_args, name);
  msg = g_strdup_vprintf ((const gchar *)name, var_args);
  va_end (var_args);

  diagnostic = ide_xml_parser_create_diagnostic (state, msg, IDE_DIAGNOSTIC_ERROR);
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
          state->error_missing_tag_end = TRUE;
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
              /* '<' only case, no name tag, node not created, we need to do it ourself */
              node = ide_xml_symbol_node_new ("internal", NULL, NULL, IDE_SYMBOL_KIND_XML_ELEMENT);
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
    }
}

void
ide_xml_parser_fatal_error_sax_cb (ParserState    *state,
                                   const xmlChar  *name,
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

  diagnostic = ide_xml_parser_create_diagnostic (state, msg, IDE_DIAGNOSTIC_FATAL);
  g_ptr_array_add (state->diagnostics_array, diagnostic);
}

#pragma GCC diagnostic pop

void
ide_xml_parser_internal_subset_sax_cb (ParserState   *state,
                                       const xmlChar *name,
                                       const xmlChar *external_id,
                                       const xmlChar *system_id)
{
  IdeXmlSchemaCacheEntry *entry;

  g_assert (state != NULL);
  g_assert (IDE_IS_XML_PARSER (state->self));

  if (ide_str_empty0 ((gchar *)external_id) || ide_str_empty0 ((gchar *)system_id))
    return;

  entry = ide_xml_schema_cache_entry_new ();
  entry->kind = SCHEMA_KIND_DTD;
  ide_xml_sax_get_location (state->sax_parser, &entry->line, &entry->col, NULL, NULL, NULL, NULL);
  g_ptr_array_add (state->schemas, entry);
}

void
ide_xml_parser_external_subset_sax_cb (ParserState   *state,
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
ide_xml_parser_processing_instruction_sax_cb (ParserState   *state,
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

  if (!data)
    return;

  if (NULL != (schema_url = get_schema_url ((const gchar *)data)))
    {
      if (NULL != (extension = strrchr (schema_url, '.')))
        {
          ++extension;
          if (ide_str_equal0 (extension, "rng"))
            kind = SCHEMA_KIND_RNG;
          else if (ide_str_equal0 (extension, "xsd"))
            kind = SCHEMA_KIND_XML_SCHEMA;
          else
            goto fail;

          /* We skip adding gtkbuilder.rng and gtk4builder.rng here and add it from gresources after the parsing */
          if (g_str_has_suffix (schema_url, "gtkbuilder.rng") || g_str_has_suffix (schema_url, "gtk4builder.rng"))
            return;

          entry = ide_xml_schema_cache_entry_new ();
          entry->file = get_absolute_schema_file (state->file, schema_url);;
          entry->kind = kind;

          ide_xml_sax_get_location (state->sax_parser, &entry->line, &entry->col, NULL, NULL, NULL, NULL);
          /* Needed to pass the kind to the service schema fetcher */
          g_object_set_data (G_OBJECT (entry->file), "kind", GUINT_TO_POINTER (entry->kind));

          g_ptr_array_add (state->schemas, entry);

          return;
        }
fail:
      diagnostic = ide_xml_parser_create_diagnostic (state,
                                                     "Schema type not supported",
                                                     IDE_DIAGNOSTIC_WARNING);
      g_ptr_array_add (state->diagnostics_array, diagnostic);
    }
}

void
ide_xml_parser_characters_sax_cb (ParserState    *state,
                                  const xmlChar  *name,
                                  gint            len)
{
  IdeXmlParser *self = (IdeXmlParser *)state->self;
  g_autofree gchar *element_value = NULL;

  g_assert (IDE_IS_XML_PARSER (self));

  if (state->build_state != BUILD_STATE_GET_CONTENT)
    return;

  element_value = g_strndup ((gchar *)name, len);
  state->build_state = BUILD_STATE_NORMAL;

  ide_xml_parser_state_processing (self, state, element_value, NULL, IDE_XML_SAX_CALLBACK_TYPE_CHAR, FALSE);
}

static void
ide_xml_parser_get_analysis_worker (IdeTask      *task,
                                    gpointer      source_object,
                                    gpointer      task_data,
                                    GCancellable *cancellable)
{
  IdeXmlParser *self = (IdeXmlParser *)source_object;
  ParserState *state = (ParserState *)task_data;
  IdeXmlAnalysis *analysis;
  IdeXmlSchemaCacheEntry *entry;
  g_autoptr(IdeDiagnostics) diagnostics = NULL;
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
  if (state->file_is_ui)
    ide_xml_parser_ui_setup (self, state);
  else
    ide_xml_parser_generic_setup (self, state);

  uri = g_file_get_uri (state->file);
  ide_xml_sax_parse (state->sax_parser, doc_data, doc_size, uri, state);

  if (self->post_processing_callback != NULL)
    (self->post_processing_callback)(self, state->root_node);

  if (!(analysis = g_steal_pointer (&state->analysis)))
    {
      ide_task_return_new_error (task,
                                 G_IO_ERROR,
                                 G_IO_ERROR_FAILED,
                                 _("Failed to create the XML tree."));
      return;
    }

  diagnostics = ide_diagnostics_new ();
  if (state->diagnostics_array)
    {
      for (guint i = 0; i < state->diagnostics_array->len; i++)
        ide_diagnostics_add (diagnostics, g_ptr_array_index (state->diagnostics_array, i));
    }
  ide_xml_analysis_set_diagnostics (analysis, diagnostics);

  /* by default use gtk4builder.rng and only gtkbuilder.rng if explicitly stated in the ui file.
   * As gtkbuilder.rng is a subset of gtk4builder.rng this probably never makes problems.
   */
  if (state->file_is_ui && ide_xml_parser_file_is_gtk3 (state->file, doc_data, doc_size))
    {
      entry = ide_xml_schema_cache_entry_new ();
      entry->kind = SCHEMA_KIND_RNG;
      entry->file = g_file_new_for_uri ("resource:///plugins/xml-pack/schemas/gtkbuilder.rng");
      g_object_set_data (G_OBJECT (entry->file), "kind", GUINT_TO_POINTER (entry->kind));
      g_ptr_array_add (state->schemas, entry);
    }
  else if (state->file_is_ui)
    {
      entry = ide_xml_schema_cache_entry_new ();
      entry->kind = SCHEMA_KIND_RNG;
      entry->file = g_file_new_for_uri ("resource:///plugins/xml-pack/schemas/gtk4builder.rng");
      g_object_set_data (G_OBJECT (entry->file), "kind", GUINT_TO_POINTER (entry->kind));
      g_ptr_array_add (state->schemas, entry);
    }

  if (state->schemas != NULL && state->schemas->len > 0)
    ide_xml_analysis_set_schemas (analysis, g_steal_pointer (&state->schemas));

  ide_xml_analysis_set_sequence (analysis, state->sequence);
  ide_task_return_pointer (task,
                           g_steal_pointer (&analysis),
                           ide_xml_analysis_unref);
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
  g_autoptr(IdeTask) task = NULL;
  ParserState *state;

  g_return_if_fail (IDE_IS_XML_PARSER (self));
  g_return_if_fail (!cancellable || G_IS_CANCELLABLE (cancellable));

  task = ide_task_new (self, cancellable, callback, user_data);
  ide_task_set_source_tag (task, ide_xml_parser_get_analysis_async);
  ide_task_set_kind (task, IDE_TASK_KIND_INDEXER);

  state = g_slice_new0 (ParserState);
  state->self = self;
  state->file = g_object_ref (file);
  state->content = g_bytes_ref (content);
  state->sequence = sequence;
  state->diagnostics_array = g_ptr_array_new_with_free_func (g_object_unref);
  state->schemas = g_ptr_array_new_with_free_func (g_object_unref);
  state->sax_parser = ide_xml_sax_new ();
  state->stack = ide_xml_stack_new ();

  state->build_state = BUILD_STATE_NORMAL;

  state->analysis = ide_xml_analysis_new (-1);
  state->root_node = ide_xml_symbol_node_new ("root", NULL, "root", IDE_SYMBOL_KIND_NONE);
  ide_xml_analysis_set_root_node (state->analysis, state->root_node);

  state->parent_node = state->root_node;
  ide_xml_stack_push (state->stack, "root", state->root_node, NULL, 0);

  ide_task_set_task_data (task, state, parser_state_free);
  ide_task_run_in_thread (task, ide_xml_parser_get_analysis_worker);
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
ide_xml_parser_get_color_tag (IdeXmlParser *self,
                              const gchar  *str,
                              ColorTagId    id,
                              gboolean      space_before,
                              gboolean      space_after,
                              gboolean      space_inside)
{
  ColorTag *tag;

  g_assert (IDE_IS_XML_PARSER (self));
  g_assert (self->color_tags != NULL);
  g_assert (!ide_str_empty0 (str));

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
  gchar *tag_name;
  GtkSourceStyle *style;
  gchar *foreground;
  gchar *background;
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
          tag_name = g_strconcat ("-Builder:", tag_ptr->name, NULL);
          if (NULL != (style = gtk_source_style_scheme_get_style (scheme, tag_name)))
            {
              g_object_get (style, "foreground", &foreground, NULL);
              g_object_get (style, "background", &background, NULL);
              if (foreground != NULL && background != NULL)
                {
                  tag_set = TRUE;
                  tag.name = g_strdup (tag_ptr->name);
                  tag.fg = g_steal_pointer (&foreground);
                  tag.bg = g_steal_pointer (&background);
                }

              g_free (foreground);
              g_free (background);
            }

          g_free (tag_name);
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
ide_xml_parser_set_post_processing_callback (IdeXmlParser           *self,
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

  if (ide_str_equal0 (key, "style-scheme-name"))
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

  g_clear_pointer (&self->color_tags, g_array_unref);
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
