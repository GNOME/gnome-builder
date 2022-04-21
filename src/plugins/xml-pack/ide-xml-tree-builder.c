/* ide-xml-tree-builder.c
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

#define G_LOG_DOMAIN "ide-xml-tree-builder"

#include "config.h"

#include <glib/gi18n.h>
#include <libxml/parser.h>
#include <string.h>

#include "ide-xml-parser.h"
#include "ide-xml-rng-parser.h"
#include "ide-xml-sax.h"
#include "ide-xml-schema-cache-entry.h"
#include "ide-xml-schema.h"
#include "ide-xml-service.h"
#include "ide-xml-tree-builder.h"
#include "ide-xml-tree-builder-utils-private.h"
#include "ide-xml-validator.h"

struct _IdeXmlTreeBuilder
{
  IdeObject        parent_instance;

  IdeXmlParser    *parser;
  IdeXmlValidator *validator;
};

typedef struct
{
  GBytes         *content;
  GFile          *file;
  IdeXmlAnalysis *analysis;
  gint64          sequence;
} TreeBuilderState;

typedef struct
{
  IdeXmlTreeBuilder *self;
  IdeTask           *task;
  GPtrArray         *schemas;
  guint              index;
} FetchSchemasState;

static void tree_builder_state_free (TreeBuilderState  *state);
static void fetch_schema_state_free (FetchSchemasState *state);

G_DEFINE_AUTOPTR_CLEANUP_FUNC (FetchSchemasState, fetch_schema_state_free)
G_DEFINE_AUTOPTR_CLEANUP_FUNC (TreeBuilderState, tree_builder_state_free)

G_DEFINE_FINAL_TYPE (IdeXmlTreeBuilder, ide_xml_tree_builder, IDE_TYPE_OBJECT)

static void
tree_builder_state_free (TreeBuilderState *state)
{
  g_assert (state != NULL);

  g_clear_pointer (&state->content, g_bytes_unref);
  g_clear_pointer (&state->analysis, ide_xml_analysis_unref);
  g_clear_object (&state->file);
  g_slice_free (TreeBuilderState, state);
}

static IdeDiagnostic *
create_diagnostic (IdeContext            *context,
                   const gchar           *msg,
                   GFile                 *file,
                   gint                   line,
                   gint                   col,
                   IdeDiagnosticSeverity  severity)
{
  g_autoptr(IdeLocation) loc = NULL;

  g_assert (IDE_IS_CONTEXT (context));
  g_assert (G_IS_FILE (file));

  loc = ide_location_new (file, line - 1, col - 1);

  return ide_diagnostic_new (severity, msg, loc);
}

static GBytes *
ide_xml_tree_builder_get_file_content (IdeXmlTreeBuilder *self,
                                       GFile             *file,
                                       gint64            *sequence)
{
  g_autoptr(GBytes) content = NULL;
  IdeBufferManager *manager;
  IdeContext *context;
  IdeBuffer *buffer;
  gint64 sequence_tmp = -1;

  g_assert (IDE_IS_XML_TREE_BUILDER (self));
  g_assert (G_IS_FILE (file));

  context = ide_object_get_context (IDE_OBJECT (self));
  manager = ide_buffer_manager_from_context (context);
  buffer = ide_buffer_manager_find_buffer (manager, file);

  if (buffer != NULL)
    {
      content = ide_buffer_dup_content (buffer);
      sequence_tmp = ide_buffer_get_change_count (buffer);
    }

  if (sequence != NULL)
    *sequence = sequence_tmp;

  return g_steal_pointer (&content);
}

static void
fetch_schema_state_free (FetchSchemasState *state)
{
  g_assert (state != NULL);

  g_clear_pointer (&state->schemas, g_ptr_array_unref);
  g_clear_object (&state->self);
  g_clear_object (&state->task);
  g_slice_free (FetchSchemasState, state);
}

static void
fetch_schemas_cb (GObject      *object,
                  GAsyncResult *result,
                  gpointer      user_data)
{
  IdeTaskCache *schemas_cache = (IdeTaskCache *)object;
  g_autoptr(FetchSchemasState) state = user_data;
  g_autoptr(IdeXmlSchemaCacheEntry) cache_entry = NULL;
  IdeXmlSchemaCacheEntry *entry;
  g_autoptr(GError) error = NULL;
  guint *count;

  g_assert (IDE_IS_TASK_CACHE (schemas_cache));
  g_assert (G_IS_ASYNC_RESULT (result));
  g_assert (state != NULL);
  g_assert (IDE_IS_TASK (state->task));
  g_assert (IDE_IS_XML_TREE_BUILDER (state->self));
  g_assert (state->schemas != NULL);
  g_assert (state->index < state->schemas->len);

  entry = g_ptr_array_index (state->schemas, state->index);
  cache_entry = ide_task_cache_get_finish (schemas_cache, result, &error);

  if (cache_entry != NULL)
    {
      if (cache_entry->content != NULL &&
          entry->content != cache_entry->content)
        {
          g_clear_pointer (&entry->content, g_bytes_unref);
          entry->content = g_bytes_ref (cache_entry->content);
        }

      if (cache_entry->error_message != NULL &&
          entry->error_message != cache_entry->error_message)
        {
          g_free (error->message);
          entry->error_message = g_strdup (cache_entry->error_message);
        }

      if (cache_entry->schema != NULL &&
          entry->schema != cache_entry->schema)
        {
          g_clear_pointer (&entry->schema, ide_xml_schema_unref);
          entry->schema = ide_xml_schema_ref (cache_entry->schema);
        }

      entry->state = cache_entry->state;
      entry->mtime = cache_entry->mtime;
    }

  count = ide_task_get_task_data (state->task);
  g_assert (count != NULL);

  (*count)--;

  if (*count == 0)
    ide_task_return_boolean (state->task, TRUE);
}

static void
fetch_schemas_async (IdeXmlTreeBuilder   *self,
                     GPtrArray           *schemas,
                     GCancellable        *cancellable,
                     GAsyncReadyCallback  callback,
                     gpointer             user_data)
{
  g_autoptr(IdeTask) task = NULL;
  g_autoptr(GPtrArray) schemas_copy = NULL;
  IdeXmlService *service;
  IdeTaskCache *schemas_cache;
  IdeContext *context;
  guint *count = 0;

  g_assert (IDE_IS_XML_TREE_BUILDER (self));
  g_assert (schemas != NULL);
  g_assert (!cancellable || G_IS_CANCELLABLE (cancellable));

  /* TODO: use a worker thread */
  task = ide_task_new (self, cancellable, callback, user_data);
  ide_task_set_source_tag (task, fetch_schemas_async);
  ide_task_set_priority (task, G_PRIORITY_LOW);

  count = g_new0 (guint, 1);
  ide_task_set_task_data (task, count, g_free);

  /* Make a copy of schemas to ensure they cannot be changed
   * during the lifetime of the operation, as the index within
   * the array is stashed by the FetchSchemasState.
   */
  schemas_copy = g_ptr_array_new_with_free_func ((GDestroyNotify)ide_xml_schema_cache_entry_unref);

  context = ide_object_get_context (IDE_OBJECT (self));
  service = ide_xml_service_from_context (context);
  schemas_cache = ide_xml_service_get_schemas_cache (service);

  for (guint i = 0; i < schemas->len; i++)
    {
      IdeXmlSchemaCacheEntry *entry = g_ptr_array_index (schemas, i);
      FetchSchemasState *state;

      /* Check if it's an internal schema */
      if (entry->file == NULL)
        continue;

      state = g_slice_new0 (FetchSchemasState);
      state->self = g_object_ref (self);
      state->schemas = g_ptr_array_ref (schemas_copy);
      state->task = g_object_ref (task);

      (*count)++;

      g_ptr_array_add (schemas_copy, ide_xml_schema_cache_entry_ref (entry));
      state->index = schemas_copy->len - 1;

      /* TODO: peek schemas if it's already in cache */
      ide_task_cache_get_async (schemas_cache,
                                entry->file,
                                FALSE,
                                cancellable,
                                fetch_schemas_cb,
                                state);
    }

  if (*count == 0)
    ide_task_return_boolean (task, TRUE);
}

static gboolean
fetch_schemas_finish (IdeXmlTreeBuilder  *self,
                      GAsyncResult       *result,
                      GError            **error)
{
  IdeTask *task = (IdeTask *)result;

  g_return_val_if_fail (IDE_IS_XML_TREE_BUILDER (self), FALSE);
  g_return_val_if_fail (IDE_IS_TASK (result), FALSE);
  g_return_val_if_fail (error != NULL, FALSE);

  return ide_task_propagate_boolean (task, error);
}

static void
ide_xml_tree_builder_parse_worker (IdeTask      *task,
                                   gpointer      source_object,
                                   gpointer      task_data,
                                   GCancellable *cancellable)
{
  IdeXmlTreeBuilder *self = (IdeXmlTreeBuilder *)source_object;
  TreeBuilderState *state = (TreeBuilderState *)task_data;
  g_autoptr (GPtrArray) schemas = NULL;
  IdeContext *context;
  const gchar *doc_data;
  xmlDoc *doc;
  gsize doc_size;
  gint parser_flags;

  g_assert (IDE_IS_XML_TREE_BUILDER (self));
  g_assert (IDE_IS_TASK (task));
  g_assert (state != NULL);
  g_assert (cancellable == NULL || G_IS_CANCELLABLE (cancellable));

  if (ide_task_return_error_if_cancelled (task))
    return;

  state = ide_task_get_task_data (task);
  g_assert (state != NULL);
  g_assert (state->analysis != NULL);

  schemas = ide_xml_analysis_get_schemas (state->analysis);
  g_assert (schemas != NULL);

  context = ide_object_get_context (IDE_OBJECT (self));
  g_assert (IDE_IS_CONTEXT (context));

  xmlInitParser ();

  doc_data = g_bytes_get_data (state->content, &doc_size);
  parser_flags = XML_PARSE_RECOVER | XML_PARSE_NOERROR | XML_PARSE_NOWARNING | XML_PARSE_COMPACT;

  doc = xmlReadMemory (doc_data, doc_size, NULL, NULL, parser_flags);

  if (doc != NULL)
    {
      doc->URL = (guchar *)g_file_get_uri (state->file);

      for (guint i = 0; i < schemas->len; ++i)
        {
          IdeXmlSchemaCacheEntry *entry = g_ptr_array_index (schemas, i);
          const gchar *schema_data;
          gsize schema_size;
          g_autoptr (IdeDiagnostics) diagnostics = NULL;
          gboolean schema_ret = FALSE;

          if (entry->kind == SCHEMA_KIND_RNG || entry->kind == SCHEMA_KIND_XML_SCHEMA)
            {
              if (entry->content != NULL)
                {
                  schema_data = g_bytes_get_data (entry->content, &schema_size);
                  schema_ret = ide_xml_validator_set_schema (self->validator,
                                                             entry->kind,
                                                             schema_data,
                                                             schema_size);
                }
              else
                {
                  g_autoptr(IdeDiagnostic) diagnostic = NULL;

                  g_assert (entry->error_message != NULL);

                  diagnostic = create_diagnostic (context,
                                                  entry->error_message,
                                                  state->file,
                                                  entry->line + 1,
                                                  entry->col + 1,
                                                  IDE_DIAGNOSTIC_ERROR);
                  ide_diagnostics_add (state->analysis->diagnostics, diagnostic);
                  continue;
                }
            }
          else if (entry->kind == SCHEMA_KIND_DTD)
            {
              schema_ret = ide_xml_validator_set_schema (self->validator, SCHEMA_KIND_DTD, NULL, 0);
            }
          else
            g_assert_not_reached ();

          if (!schema_ret)
            {
              g_autoptr(IdeDiagnostic) diagnostic = NULL;
              g_autofree gchar *uri = NULL;
              g_autofree gchar *msg = NULL;

              uri = g_file_get_uri (entry->file);
              msg = g_strdup_printf ("Can't parse the schema: '%s'", uri);

              diagnostic = create_diagnostic (context,
                                              msg,
                                              state->file,
                                              entry->line + 1,
                                              entry->col + 1,
                                              IDE_DIAGNOSTIC_ERROR);
              ide_diagnostics_add (state->analysis->diagnostics, diagnostic);
              continue;
            }

          if (!ide_xml_validator_validate (self->validator, doc, &diagnostics))
            {
              g_autoptr(IdeDiagnostic) diagnostic = NULL;
              g_autofree gchar *uri = NULL;
              g_autofree gchar *msg = NULL;

              if (entry->file == NULL)
                msg = g_strdup_printf ("Can't validate the internal schema");
              else
                {
                  uri = g_file_get_uri (entry->file);
                  msg = g_strdup_printf ("Can't validate the schema: '%s'", uri);
                }

              diagnostic = create_diagnostic (context,
                                              msg,
                                              state->file,
                                              entry->line + 1,
                                              entry->col + 1,
                                              IDE_DIAGNOSTIC_ERROR);
              ide_diagnostics_add (state->analysis->diagnostics, diagnostic);
            }

          ide_diagnostics_merge (state->analysis->diagnostics, diagnostics);
        }

      xmlFreeDoc (doc);
    }
  else
    {
      /* TODO: set error */
      g_debug ("can't create xmlDoc\n");
    }

  ide_task_return_pointer (task,
                           g_steal_pointer (&state->analysis),
                           ide_xml_analysis_unref);
}

static void
ide_xml_tree_builder_build_tree_cb2 (GObject      *object,
                                     GAsyncResult *result,
                                     gpointer      user_data)
{
  IdeXmlTreeBuilder *self = (IdeXmlTreeBuilder *)object;
  g_autoptr(GError) error = NULL;
  g_autoptr(IdeTask) task = user_data;

  g_assert (IDE_IS_XML_TREE_BUILDER (self));
  g_assert (G_IS_ASYNC_RESULT (result));
  g_assert (IDE_IS_TASK (task));

  if (!fetch_schemas_finish (self, result, &error))
    ide_task_return_error (task, g_steal_pointer (&error));
  else
    ide_task_run_in_thread (task, ide_xml_tree_builder_parse_worker);
}

static void
ide_xml_tree_builder_build_tree_cb (GObject      *object,
                                    GAsyncResult *result,
                                    gpointer      user_data)
{
  g_autoptr(IdeXmlAnalysis) analysis = NULL;
  g_autoptr(IdeTask) task = user_data;
  g_autoptr(GError) error = NULL;
  IdeXmlTreeBuilder *self;
  TreeBuilderState *state;
  GCancellable *cancellable;

  g_assert (IDE_IS_XML_PARSER (object));
  g_assert (G_IS_ASYNC_RESULT (result));
  g_assert (IDE_IS_TASK (task));

  self = ide_task_get_source_object (task);
  g_assert (IDE_IS_XML_TREE_BUILDER (self));

  state = ide_task_get_task_data (task);
  g_assert (state != NULL);

  cancellable = ide_task_get_cancellable (task);
  g_assert (!cancellable || G_IS_CANCELLABLE (cancellable));

  analysis = ide_xml_parser_get_analysis_finish (self->parser, result, &error);

  if (analysis == NULL)
    {
      ide_task_return_error (task, g_steal_pointer (&error));
      return;
    }

  state->analysis = ide_xml_analysis_ref (analysis);
  ide_xml_analysis_set_sequence (analysis, state->sequence);

  if (analysis->schemas == NULL)
    {
      ide_task_return_pointer (task,
                               g_steal_pointer (&analysis),
                               ide_xml_analysis_unref);
      return;
    }

  fetch_schemas_async (self,
                       analysis->schemas,
                       cancellable,
                       ide_xml_tree_builder_build_tree_cb2,
                       g_steal_pointer (&task));
}

void
ide_xml_tree_builder_build_tree_async (IdeXmlTreeBuilder   *self,
                                       GFile               *file,
                                       GCancellable        *cancellable,
                                       GAsyncReadyCallback  callback,
                                       gpointer             user_data)
{
  g_autoptr(IdeTask) task = NULL;
  g_autoptr(TreeBuilderState) state = NULL;
  g_autoptr(GBytes) content = NULL;
  gint64 sequence = 0;

  g_return_if_fail (IDE_IS_XML_TREE_BUILDER (self));
  g_return_if_fail (G_IS_FILE (file));
  g_return_if_fail (!cancellable || G_IS_CANCELLABLE (cancellable));

  task = ide_task_new (self, cancellable, callback, user_data);
  ide_task_set_source_tag (task, ide_xml_tree_builder_build_tree_async);
  ide_task_set_priority (task, G_PRIORITY_LOW);

  content = ide_xml_tree_builder_get_file_content (self, file, &sequence);

  if (content == NULL || g_bytes_get_size (content) == 0)
    {
      ide_task_return_new_error (task,
                                 G_IO_ERROR,
                                 G_IO_ERROR_FAILED,
                                 _("Failed to create the XML tree."));
      return;
    }

  state = g_slice_new0 (TreeBuilderState);
  state->file = g_object_ref (file);
  state->content = g_bytes_ref (content);
  state->sequence = sequence;

  ide_task_set_task_data (task,
                          g_steal_pointer (&state),
                          tree_builder_state_free);

  ide_xml_parser_get_analysis_async (self->parser,
                                     file,
                                     content,
                                     sequence,
                                     cancellable,
                                     ide_xml_tree_builder_build_tree_cb,
                                     g_steal_pointer (&task));
}

IdeXmlAnalysis *
ide_xml_tree_builder_build_tree_finish (IdeXmlTreeBuilder  *self,
                                        GAsyncResult       *result,
                                        GError            **error)
{
  g_return_val_if_fail (IDE_IS_XML_TREE_BUILDER (self), NULL);
  g_return_val_if_fail (G_IS_ASYNC_RESULT (result), NULL);

  return ide_task_propagate_pointer (IDE_TASK (result), error);
}

static void
ide_xml_tree_builder_destroy (IdeObject *object)
{
  IdeXmlTreeBuilder *self = (IdeXmlTreeBuilder *)object;

  ide_clear_and_destroy_object (&self->parser);
  ide_clear_and_destroy_object (&self->validator);

  IDE_OBJECT_CLASS (ide_xml_tree_builder_parent_class)->destroy (object);
}

static void
ide_xml_tree_builder_parent_set (IdeObject *object,
                                 IdeObject *parent)
{
  IdeXmlTreeBuilder *self = (IdeXmlTreeBuilder *)object;

  g_assert (IDE_IS_XML_TREE_BUILDER (self));
  g_assert (!parent || IDE_IS_OBJECT (parent));

  if (parent == NULL)
    return;

  self->parser = g_object_new (IDE_TYPE_XML_PARSER,
                               "parent", self,
                               NULL);
  self->validator = g_object_new (IDE_TYPE_XML_VALIDATOR,
                                  "parent", self,
                                  NULL);
}

static void
ide_xml_tree_builder_class_init (IdeXmlTreeBuilderClass *klass)
{
  IdeObjectClass *i_object_class = IDE_OBJECT_CLASS (klass);

  i_object_class->parent_set = ide_xml_tree_builder_parent_set;
  i_object_class->destroy = ide_xml_tree_builder_destroy;
}

static void
ide_xml_tree_builder_init (IdeXmlTreeBuilder *self)
{
}
