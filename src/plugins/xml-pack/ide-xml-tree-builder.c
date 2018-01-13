/* ide-xml-tree-builder.c
 *
 * Copyright © 2017 Sebastien Lafargue <slafargue@gnome.org>
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


#include <dazzle.h>
#include <glib/gi18n.h>
#include <libxml/parser.h>
#include <string.h>

#include "ide-xml-parser.h"
#include "ide-xml-rng-parser.h"
#include "ide-xml-sax.h"
#include "ide-xml-schema.h"
#include "ide-xml-service.h"
#include "ide-xml-schema-cache-entry.h"
#include "ide-xml-tree-builder-utils-private.h"
#include "ide-xml-validator.h"

#include "ide-xml-tree-builder.h"

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

static void
tree_builder_state_free (TreeBuilderState *state)
{
  g_clear_pointer (&state->content, g_bytes_unref);
  g_clear_pointer (&state->analysis, ide_xml_analysis_unref);
  g_clear_object (&state->file);
  g_slice_free (TreeBuilderState, state);
}

G_DEFINE_TYPE (IdeXmlTreeBuilder, ide_xml_tree_builder, IDE_TYPE_OBJECT)

static IdeDiagnostic *
create_diagnostic (IdeContext             *context,
                   const gchar            *msg,
                   GFile                  *file,
                   gint                    line,
                   gint                    col,
                   IdeDiagnosticSeverity   severity)
{
  IdeDiagnostic *diagnostic;
  g_autoptr(IdeSourceLocation) loc = NULL;
  g_autoptr(IdeFile) ifile = NULL;

  ifile = ide_file_new (context, file);
  loc = ide_source_location_new (ifile,
                                 line - 1,
                                 col - 1,
                                 0);

  diagnostic = ide_diagnostic_new (severity, msg, loc);

  return diagnostic;
}

static GBytes *
ide_xml_tree_builder_get_file_content (IdeXmlTreeBuilder *self,
                                       GFile             *file,
                                       gint64            *sequence)
{
  IdeContext *context;
  IdeBufferManager *manager;
  IdeUnsavedFiles *unsaved_files;
  IdeUnsavedFile *uf;
  IdeBuffer *buffer;
  gint64 sequence_tmp = -1;
  GBytes *content = NULL;

  g_assert (IDE_IS_XML_TREE_BUILDER (self));
  g_assert (G_IS_FILE (file));

  context = ide_object_get_context (IDE_OBJECT (self));
  manager = ide_context_get_buffer_manager (context);

  if (NULL != (buffer = ide_buffer_manager_find_buffer (manager, file)))
    {
      content = ide_buffer_get_content (buffer);

      unsaved_files = ide_context_get_unsaved_files (context);
      if (NULL != (uf = ide_unsaved_files_get_unsaved_file (unsaved_files, file)))
        sequence_tmp = ide_unsaved_file_get_sequence (uf);
    }

  if (sequence != NULL)
    *sequence = sequence_tmp;

  return content;
}

typedef struct _FetchSchemasState
{
  IdeXmlTreeBuilder *self;
  GTask             *task;
  GPtrArray         *schemas;
  guint              index;
} FetchSchemasState;

static void
fetch_schema_state_free (FetchSchemasState *state)
{
  g_clear_object (&state->self);
  g_clear_pointer (&state->schemas, g_ptr_array_unref);
  self->task = NULL;

  g_slice_free (FetchSchemasState, state);
}

static void
fetch_schemas_cb (GObject      *object,
                  GAsyncResult *result,
                  gpointer      user_data)
{
  DzlTaskCache *schemas_cache = (DzlTaskCache *)object;
  FetchSchemasState *state = (FetchSchemasState *)user_data;
  g_autoptr (IdeXmlSchemaCacheEntry) cache_entry = NULL;
  GTask *task = state->task;
  guint count;
  IdeXmlSchemaCacheEntry *entry;
  GError *error = NULL;

  g_assert (DZL_IS_TASK_CACHE (schemas_cache));
  g_assert (G_IS_TASK (result));
  g_assert (G_IS_TASK (task));

  cache_entry = dzl_task_cache_get_finish (schemas_cache, result, &error);
  entry = g_ptr_array_index (state->schemas, state->index);

  if (cache_entry->content != NULL)
    entry->content = g_bytes_ref (cache_entry->content);

  if (cache_entry->error_message != NULL)
    entry->error_message = g_strdup (cache_entry->error_message);

  if (cache_entry->schema != NULL)
    entry->schema = ide_xml_schema_ref (cache_entry->schema);

  entry->state = cache_entry->state;
  entry->mtime = cache_entry->mtime;

  fetch_schema_state_free (state);
  count = GPOINTER_TO_UINT (g_task_get_task_data (task));
  --count;
  g_task_set_task_data (task, GUINT_TO_POINTER (count), NULL);

  if (count == 0)
    {
      g_task_return_boolean (task, TRUE);
      g_object_unref (task);
    }
}

static gboolean
fetch_schemas_async (IdeXmlTreeBuilder   *self,
                     GPtrArray           *schemas,
                     GCancellable        *cancellable,
                     GAsyncReadyCallback  callback,
                     gpointer             user_data)
{
  IdeContext *context;
  IdeXmlService *service;
  DzlTaskCache *schemas_cache;
  GTask *task;
  guint count = 0;
  gboolean has_external_schemas = FALSE;

  g_assert (IDE_IS_XML_TREE_BUILDER (self));
  g_assert (schemas != NULL);
  g_assert (!cancellable || G_IS_CANCELLABLE (cancellable));

  if (schemas->len == 0)
    return FALSE;

  /* TODO: use a worker thread */
  task = g_task_new (self, cancellable, callback, user_data);

  context = ide_object_get_context (IDE_OBJECT (self));
  service = ide_context_get_service_typed (context, IDE_TYPE_XML_SERVICE);
  schemas_cache = ide_xml_service_get_schemas_cache (service);

  for (gint i = 0; i < schemas->len; ++i)
    {
      IdeXmlSchemaCacheEntry *entry;
      FetchSchemasState *state;

      entry = g_ptr_array_index (schemas, i);
      /* Check if it's an internal schema */
      if (entry->file == NULL)
        continue;

      state = g_slice_new0 (FetchSchemasState);
      state->self = g_object_ref (self);
      state->schemas = g_ptr_array_ref (schemas);
      state->task = task;

      ++count;
      g_task_set_task_data (task, GUINT_TO_POINTER (count), NULL);
      has_external_schemas = TRUE;

      state->index = i;
      /* TODO: peek schemas if it's already in cache */
      dzl_task_cache_get_async (schemas_cache,
                                entry->file,
                                FALSE,
                                cancellable,
                                fetch_schemas_cb,
                                state);
    }

  if (!has_external_schemas)
    g_task_return_boolean (task, TRUE);

  return TRUE;
}

static gboolean
fetch_schemas_finish (IdeXmlTreeBuilder  *self,
                      GAsyncResult       *result,
                      GError            **error)
{
  GTask *task = (GTask *)result;

  g_return_val_if_fail (IDE_IS_XML_TREE_BUILDER (self), FALSE);
  g_return_val_if_fail (G_IS_TASK (result), FALSE);
  g_return_val_if_fail (error != NULL, FALSE);

  return g_task_propagate_boolean (task, error);
}

static void
ide_xml_tree_builder_parse_worker (GTask        *task,
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
  IdeXmlSchemaKind kind;
  gint parser_flags;

  g_assert (IDE_IS_XML_TREE_BUILDER (self));
  g_assert (G_IS_TASK (task));
  g_assert (state != NULL);
  g_assert (cancellable == NULL || G_IS_CANCELLABLE (cancellable));

  if (g_task_return_error_if_cancelled (task))
    return;

  state = g_task_get_task_data (task);
  schemas = ide_xml_analysis_get_schemas (state->analysis);
  context = ide_object_get_context (IDE_OBJECT (self));

  xmlInitParser ();

  doc_data = g_bytes_get_data (state->content, &doc_size);
  parser_flags = XML_PARSE_RECOVER | XML_PARSE_NOERROR | XML_PARSE_NOWARNING | XML_PARSE_COMPACT;
  if (NULL != (doc = xmlReadMemory (doc_data,
                                    doc_size,
                                    NULL, NULL,
                                    parser_flags)))
    {
      doc->URL = (guchar *)g_file_get_uri (state->file);

      for (gint i = 0; i < schemas->len; ++i)
        {
          IdeXmlSchemaCacheEntry *entry;
          const gchar *schema_data;
          gsize schema_size;
          g_autoptr (IdeDiagnostics) diagnostics = NULL;
          g_autoptr (IdeDiagnostic) diagnostic = NULL;
          g_autofree gchar *uri = NULL;
          g_autofree gchar *msg = NULL;
          gboolean schema_ret;

          entry = g_ptr_array_index (schemas, i);
          kind = entry->kind;

          if (kind == SCHEMA_KIND_RNG || kind == SCHEMA_KIND_XML_SCHEMA)
            {
              if (entry->content != NULL)
                {
                  schema_data = g_bytes_get_data (entry->content, &schema_size);
                  schema_ret = ide_xml_validator_set_schema (self->validator,
                                                             kind,
                                                             schema_data,
                                                             schema_size);
                }
              else
                {
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
          else if (kind == SCHEMA_KIND_DTD)
            {
            schema_ret = ide_xml_validator_set_schema (self->validator, SCHEMA_KIND_DTD, NULL, 0);
            }
          else
            g_assert_not_reached ();

          if (!schema_ret)
            {
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

          if (ide_xml_validator_validate (self->validator, doc, &diagnostics) != 0)
            {
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

  g_task_return_pointer (task, state->analysis, (GDestroyNotify)ide_xml_analysis_unref);
}

static void
ide_xml_tree_builder_build_tree_cb2 (GObject      *object,
                                     GAsyncResult *result,
                                     gpointer      user_data)
{
  g_autoptr(GTask) task = user_data;
  g_autoptr(GError) error = NULL;
  IdeXmlTreeBuilder *self;

  g_assert (G_IS_TASK (result));
  g_assert (G_IS_TASK (task));

  self = g_task_get_source_object (task);
  g_assert (IDE_IS_XML_TREE_BUILDER (self));

  if (!fetch_schemas_finish (self, result, &error))
    {
      g_task_return_error (task, g_steal_pointer (&error));
      return;
    }

  g_task_run_in_thread (task, ide_xml_tree_builder_parse_worker);
}

static void
ide_xml_tree_builder_build_tree_cb (GObject      *object,
                                    GAsyncResult *result,
                                    gpointer      user_data)
{
  IdeXmlTreeBuilder *self;
  g_autoptr(GTask) task = user_data;
  g_autoptr(GError) error = NULL;
  TreeBuilderState *state;
  IdeXmlAnalysis *analysis;

  g_assert (G_IS_TASK (result));
  g_assert (G_IS_TASK (task));

  self = g_task_get_source_object (task);
  g_assert (IDE_IS_XML_TREE_BUILDER (self));

  if (NULL == (analysis = ide_xml_parser_get_analysis_finish (self->parser, result, &error)))
    {
      g_task_return_error (task, g_steal_pointer (&error));
      return;
    }

  state = g_task_get_task_data (task);
  state->analysis = ide_xml_analysis_ref (analysis);
  ide_xml_analysis_set_sequence (analysis, state->sequence);

  if (analysis->schemas != NULL &&
      fetch_schemas_async (self,
                           analysis->schemas,
                           g_task_get_cancellable (task),
                           ide_xml_tree_builder_build_tree_cb2,
                           g_object_ref (task)))
    return;

  g_task_return_pointer (task, analysis, (GDestroyNotify)ide_xml_analysis_unref);
}

void
ide_xml_tree_builder_build_tree_async (IdeXmlTreeBuilder   *self,
                                       GFile               *file,
                                       GCancellable        *cancellable,
                                       GAsyncReadyCallback  callback,
                                       gpointer             user_data)
{
  g_autoptr(GTask) task = NULL;
  TreeBuilderState *state;
  GBytes *content = NULL;
  gint64 sequence;

  g_return_if_fail (IDE_IS_XML_TREE_BUILDER (self));
  g_return_if_fail (G_IS_FILE (file));
  g_return_if_fail (!cancellable || G_IS_CANCELLABLE (cancellable));

  task = g_task_new (self, cancellable, callback, user_data);
  g_task_set_source_tag (task, ide_xml_tree_builder_build_tree_async);

  if (NULL == (content = ide_xml_tree_builder_get_file_content (self, file, &sequence)))
    {
      g_task_return_new_error (task,
                               G_IO_ERROR,
                               G_IO_ERROR_FAILED,
                               _("Failed to create the XML tree."));
      return;
    }

  state = g_slice_new0 (TreeBuilderState);
  state->file = g_object_ref (file);
  state->content = g_bytes_ref (content);
  state->sequence = sequence;
  g_task_set_task_data (task, state, (GDestroyNotify)tree_builder_state_free);

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
  GTask *task = (GTask *)result;

  g_return_val_if_fail (IDE_IS_XML_TREE_BUILDER (self), NULL);
  g_return_val_if_fail (G_IS_TASK (task), NULL);

  return g_task_propagate_pointer (task, error);
}

IdeXmlTreeBuilder *
ide_xml_tree_builder_new (DzlTaskCache *schemas)
{
  return g_object_new (IDE_TYPE_XML_TREE_BUILDER,
                       NULL);
}

static void
ide_xml_tree_builder_finalize (GObject *object)
{
  IdeXmlTreeBuilder *self = (IdeXmlTreeBuilder *)object;

  g_clear_object (&self->parser);
  g_clear_object (&self->validator);

  G_OBJECT_CLASS (ide_xml_tree_builder_parent_class)->finalize (object);
}

static void
ide_xml_tree_builder_class_init (IdeXmlTreeBuilderClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = ide_xml_tree_builder_finalize;
}

static void
ide_xml_tree_builder_init (IdeXmlTreeBuilder *self)
{
  IdeContext *context;

  context = ide_object_get_context (IDE_OBJECT (self));
  self->parser = g_object_new (IDE_TYPE_XML_PARSER,
                               "context", context,
                               NULL);

  self->validator = ide_xml_validator_new (context);
}
