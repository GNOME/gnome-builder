/* ide-xml-service.c
 *
 * Copyright 2017 Sébastien Lafargue <slafargue@gnome.org>
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

#define G_LOG_DOMAIN "ide-xml-service"

#include <glib/gi18n.h>
#include <gtksourceview/gtksource.h>
#include <math.h>

#include "ide-xml-analysis.h"
#include "ide-xml-rng-parser.h"
#include "ide-xml-schema-cache-entry.h"
#include "ide-xml-tree-builder.h"
#include "ide-xml-types.h"
#include "ide-xml-utils.h"
#include "ide-xml-service.h"
#include "ide-xml-tree-builder.h"

#define DEFAULT_EVICTION_MSEC (60 * 1000)

struct _IdeXmlService
{
  IdeObject          parent_instance;

  DzlTaskCache      *analyses;
  DzlTaskCache      *schemas;
  IdeXmlTreeBuilder *tree_builder;
  GCancellable      *cancellable;
};

static void service_iface_init (IdeServiceInterface *iface);

G_DEFINE_TYPE_WITH_CODE (IdeXmlService, ide_xml_service, IDE_TYPE_OBJECT,
                         G_IMPLEMENT_INTERFACE (IDE_TYPE_SERVICE, service_iface_init))

static void
ide_xml_service_build_tree_cb2 (GObject      *object,
                                GAsyncResult *result,
                                gpointer      user_data)
{
  IdeXmlTreeBuilder *tree_builder = (IdeXmlTreeBuilder *)object;
  g_autoptr(GTask) task = user_data;
  g_autoptr(IdeXmlAnalysis) analysis = NULL;
  g_autoptr(GError) error = NULL;

  g_assert (IDE_IS_XML_TREE_BUILDER (tree_builder));
  g_assert (G_IS_ASYNC_RESULT (result));
  g_assert (G_IS_TASK (task));

  if (NULL == (analysis = ide_xml_tree_builder_build_tree_finish (tree_builder, result, &error)))
    g_task_return_error (task, g_steal_pointer (&error));
  else
    g_task_return_pointer (task, g_steal_pointer (&analysis), (GDestroyNotify)ide_xml_analysis_unref);
}

static void
ide_xml_service_build_tree_cb (DzlTaskCache  *cache,
                               gconstpointer  key,
                               GTask         *task,
                               gpointer       user_data)
{
  IdeXmlService *self = user_data;
  g_autofree gchar *path = NULL;
  IdeFile *ifile = (IdeFile *)key;
  GFile *gfile;

  IDE_ENTRY;

  g_assert (DZL_IS_TASK_CACHE (cache));
  g_assert (IDE_IS_XML_SERVICE (self));
  g_assert (IDE_IS_FILE (ifile));
  g_assert (G_IS_TASK (task));

  if (NULL == (gfile = ide_file_get_file (ifile)) ||
      NULL == (path = g_file_get_path (gfile)))
    {
      g_task_return_new_error (task,
                               G_IO_ERROR,
                               G_IO_ERROR_NOT_SUPPORTED,
                               _("File must be saved locally to parse."));
      return;
    }

  ide_xml_tree_builder_build_tree_async (self->tree_builder,
                                         gfile,
                                         g_task_get_cancellable (task),
                                         ide_xml_service_build_tree_cb2,
                                         g_object_ref (task));

  IDE_EXIT;
}

typedef struct
{
  IdeXmlService          *self;
  GTask                  *task;
  IdeXmlSchemaCacheEntry *cache_entry;
} SchemaState;

/* Parse schema phase */
static void
ide_xml_service_load_schema_cb3 (GObject      *object,
                                 GAsyncResult *result,
                                 gpointer      user_data)
{
  GFile *file = (GFile *)object;
  SchemaState *state = (SchemaState *)user_data;
  GTask *task;
  IdeXmlSchema *schema;
  g_autoptr (IdeXmlRngParser) rng_parser = NULL;
  IdeXmlSchemaCacheEntry *cache_entry;
  IdeXmlSchemaKind kind;
  g_autoptr(GError) error = NULL;
  gchar *content;
  gsize len;

  g_assert (G_IS_FILE (file));
  g_assert (G_IS_ASYNC_RESULT (result));
  g_assert (state != NULL);

  task = state->task;
  cache_entry = state->cache_entry;
  kind = GPOINTER_TO_UINT (g_object_get_data (G_OBJECT (file), "kind"));

  if (!g_file_load_contents_finish (file, result, &content, &len, NULL, &error))
    {
      cache_entry->error_message = g_strdup (error->message);
      cache_entry->state = SCHEMA_STATE_CANT_LOAD;
    }
  else
    {
      cache_entry->content = g_bytes_new_take (content, len);
      if (kind == SCHEMA_KIND_RNG)
        {
          rng_parser = ide_xml_rng_parser_new ();
          if (NULL != (schema = ide_xml_rng_parser_parse (rng_parser, content, len, file)))
            {
              cache_entry->schema = schema;
              cache_entry->state = SCHEMA_STATE_PARSED;
            }
          else
            {
              /* TODO: get parse error ? */
              g_clear_pointer (&cache_entry->content, g_bytes_unref);
              cache_entry->state = SCHEMA_STATE_CANT_PARSE;
            }
        }
      else
        {
          /* TODO: set error message */
          g_clear_pointer (&cache_entry->content, g_bytes_unref);
          cache_entry->state = SCHEMA_STATE_WRONG_FILE_TYPE;
        }
    }

  g_object_unref (state->task);
  g_slice_free (SchemaState, state);
  g_task_return_pointer (task, cache_entry, (GDestroyNotify)ide_xml_schema_cache_entry_unref);
}

/* Get content phase */
static void
ide_xml_service_load_schema_cb2 (GObject      *object,
                                 GAsyncResult *result,
                                 gpointer      user_data)
{
  GFile *file = (GFile *)object;
  SchemaState *state = (SchemaState *)user_data;
  IdeXmlSchemaCacheEntry *cache_entry;
  g_autoptr (GFileInfo) file_info = NULL;
  g_autoptr(GError) error = NULL;
  GTask *task;

  g_assert (G_IS_FILE (file));
  g_assert (G_IS_ASYNC_RESULT (result));
  g_assert (state != NULL);

  task = state->task;
  cache_entry = state->cache_entry;
  if (NULL != (file_info = g_file_query_info_finish (file, result, &error)))
    {
      cache_entry->mtime = g_file_info_get_attribute_uint64 (file_info, G_FILE_ATTRIBUTE_TIME_MODIFIED);
      g_file_load_contents_async (file,
                                  g_task_get_cancellable (state->task),
                                  ide_xml_service_load_schema_cb3,
                                  state);
    }
  else
    {
      cache_entry->error_message = g_strdup (error->message);
      cache_entry->state = SCHEMA_STATE_CANT_LOAD;

      g_object_unref (state->task);
      g_slice_free (SchemaState, state);
      g_task_return_pointer (task, cache_entry, (GDestroyNotify)ide_xml_schema_cache_entry_unref);
    }
}

/* Get mtime phase */
static void
ide_xml_service_load_schema_cb (DzlTaskCache  *cache,
                                gconstpointer  key,
                                GTask         *task,
                                gpointer       user_data)
{
  IdeXmlService *self = user_data;
  GFile *file = (GFile *)key;
  SchemaState *state;

  IDE_ENTRY;

  g_assert (DZL_IS_TASK_CACHE (cache));
  g_assert (IDE_IS_XML_SERVICE (self));
  g_assert (G_IS_TASK (task));
  g_assert (G_IS_FILE (file));

  state = g_slice_new0 (SchemaState);
  state->self = self;
  state->task = g_object_ref (task);
  state->cache_entry = ide_xml_schema_cache_entry_new ();

  state->cache_entry->file = g_object_ref (file);

  g_file_query_info_async (file,
                           G_FILE_ATTRIBUTE_TIME_MODIFIED,
                           G_FILE_QUERY_INFO_NONE,
                           G_PRIORITY_DEFAULT,
                           g_task_get_cancellable (state->task),
                           ide_xml_service_load_schema_cb2,
                           state);

  IDE_EXIT;
}

static void
ide_xml_service_get_analysis_cb (GObject      *object,
                                 GAsyncResult *result,
                                 gpointer      user_data)
{
  DzlTaskCache *cache = (DzlTaskCache *)object;
  g_autoptr(GTask) task = user_data;
  g_autoptr(IdeXmlAnalysis) analysis = NULL;
  g_autoptr(GError) error = NULL;

  g_assert (DZL_IS_TASK_CACHE (cache));
  g_assert (G_IS_ASYNC_RESULT (result));
  g_assert (G_IS_TASK (task));

  if (NULL == (analysis = dzl_task_cache_get_finish (cache, result, &error)))
    g_task_return_error (task, g_steal_pointer (&error));
  else
    g_task_return_pointer (task, g_steal_pointer (&analysis), (GDestroyNotify)ide_xml_analysis_unref);
}

typedef struct
{
  IdeXmlService *self;
  GTask         *task;
  GCancellable  *cancellable;
  IdeFile       *ifile;
  IdeBuffer     *buffer;
} TaskState;

static void
ide_xml_service__buffer_loaded_cb (IdeBuffer *buffer,
                                   TaskState *state)
{
  IdeXmlService *self = (IdeXmlService *)state->self;

  g_assert (IDE_IS_XML_SERVICE (self));
  g_assert (G_IS_TASK (state->task));
  g_assert (state->cancellable == NULL || G_IS_CANCELLABLE (state->cancellable));
  g_assert (IDE_IS_FILE (state->ifile));
  g_assert (IDE_IS_BUFFER (state->buffer));

  g_signal_handlers_disconnect_by_func (buffer, ide_xml_service__buffer_loaded_cb, state);

  dzl_task_cache_get_async (self->analyses,
                            state->ifile,
                            TRUE,
                            state->cancellable,
                            ide_xml_service_get_analysis_cb,
                            g_steal_pointer (&state->task));

  g_object_unref (state->buffer);
  g_object_unref (state->ifile);
  g_slice_free (TaskState, state);
}

static void
ide_xml_service_get_analysis_async (IdeXmlService       *self,
                                    IdeFile             *ifile,
                                    IdeBuffer           *buffer,
                                    GCancellable        *cancellable,
                                    GAsyncReadyCallback  callback,
                                    gpointer             user_data)
{
  g_autoptr(GTask) task = NULL;
  IdeContext *context;
  IdeBufferManager *manager;
  GFile *gfile;

  g_assert (IDE_IS_XML_SERVICE (self));
  g_assert (IDE_IS_FILE (ifile));
  g_assert (IDE_IS_BUFFER (buffer));
  g_assert (cancellable == NULL || G_IS_CANCELLABLE (cancellable));

  task = g_task_new (self, cancellable, callback, user_data);
  context = ide_object_get_context (IDE_OBJECT (self));
  manager = ide_context_get_buffer_manager (context);
  gfile = ide_file_get_file (ifile);

  if (!ide_buffer_manager_has_file (manager, gfile))
    {
      TaskState *state;

      if (!ide_buffer_get_loading (buffer))
        {
          g_task_return_new_error (task,
                                   G_IO_ERROR,
                                   G_IO_ERROR_NOT_SUPPORTED,
                                   _("Buffer loaded but not in the buffer manager."));
          return;
        }

      /* Wait for the buffer to be fully loaded */
      state = g_slice_new0 (TaskState);
      state->self = self;
      state->task = g_steal_pointer (&task);
      state->cancellable = cancellable;
      state->ifile = g_object_ref (ifile);
      state->buffer = g_object_ref (buffer);

      g_signal_connect (buffer,
                        "loaded",
                        G_CALLBACK (ide_xml_service__buffer_loaded_cb),
                        state);
    }
  else
    dzl_task_cache_get_async (self->analyses,
                              ifile,
                              TRUE,
                              cancellable,
                              ide_xml_service_get_analysis_cb,
                              g_steal_pointer (&task));
}

IdeXmlAnalysis *
ide_xml_service_get_analysis_finish (IdeXmlService  *self,
                                     GAsyncResult   *result,
                                     GError        **error)
{
  GTask *task = (GTask *)result;

  g_return_val_if_fail (IDE_IS_XML_SERVICE (self), NULL);
  g_return_val_if_fail (G_IS_TASK (task), NULL);

  return g_task_propagate_pointer (task, error);
}

static void
ide_xml_service_get_root_node_cb (GObject      *object,
                                  GAsyncResult *result,
                                  gpointer      user_data)
{
  IdeXmlService *self = (IdeXmlService *)object;
  g_autoptr(GTask) task = user_data;
  g_autoptr(IdeXmlAnalysis) analysis = NULL;
  IdeXmlSymbolNode *root_node;
  g_autoptr(GError) error = NULL;

  g_assert (IDE_IS_XML_SERVICE (self));
  g_assert (G_IS_ASYNC_RESULT (result));
  g_assert (G_IS_TASK (task));

  if (NULL == (analysis = ide_xml_service_get_analysis_finish (self, result, &error)))
    g_task_return_error (task, g_steal_pointer (&error));
  else
    {
      root_node = g_object_ref (ide_xml_analysis_get_root_node (analysis));
      g_task_return_pointer (task, root_node, g_object_unref);
    }
}

/**
 * ide_xml_service_get_root_node_async:
 *
 * This function is used to asynchronously retrieve the root node for
 * a particular file.
 *
 * If the root node is up to date, then no parsing will occur and the
 * existing root node will be used.
 *
 * If the root node is out of date, then the source file(s) will be
 * parsed asynchronously.
 *
 * The xml service is meant to be used with buffers, that is,
 * by extension, loaded views.
 */
void
ide_xml_service_get_root_node_async (IdeXmlService       *self,
                                     IdeFile             *ifile,
                                     IdeBuffer           *buffer,
                                     GCancellable        *cancellable,
                                     GAsyncReadyCallback  callback,
                                     gpointer             user_data)
{
  g_autoptr(GTask) task = NULL;
  IdeXmlAnalysis *cached;

  g_return_if_fail (IDE_IS_XML_SERVICE (self));
  g_return_if_fail (IDE_IS_FILE (ifile));
  g_return_if_fail (IDE_IS_BUFFER (buffer));
  g_return_if_fail (!cancellable || G_IS_CANCELLABLE (cancellable));

  task = g_task_new (self, cancellable, callback, user_data);

  /*
   * If we have a cached analysis with a valid root_node,
   * and it is new enough, then re-use it.
   */
  if (NULL != (cached = dzl_task_cache_peek (self->analyses, ifile)))
    {
      IdeContext *context;
      IdeUnsavedFiles *unsaved_files;
      IdeUnsavedFile *uf;
      IdeXmlSymbolNode *root_node;
      GFile *gfile;

      gfile = ide_file_get_file (ifile);
      context = ide_object_get_context (IDE_OBJECT (self));
      unsaved_files = ide_context_get_unsaved_files (context);

      if (NULL != (uf = ide_unsaved_files_get_unsaved_file (unsaved_files, gfile)) &&
          ide_xml_analysis_get_sequence (cached) == ide_unsaved_file_get_sequence (uf))
        {
          root_node = g_object_ref (ide_xml_analysis_get_root_node (cached));
          g_assert (IDE_IS_XML_SYMBOL_NODE (root_node));

          g_task_return_pointer (task, root_node, g_object_unref);
          return;
        }
    }

  ide_xml_service_get_analysis_async (self,
                                      ifile,
                                      buffer,
                                      cancellable,
                                      ide_xml_service_get_root_node_cb,
                                      g_steal_pointer (&task));
}

/**
 * ide_xml_service_get_root_node_finish:
 *
 * Completes an asychronous request to get a root node for a given file.
 * See ide_xml_service_get_root_node_async() for more information.
 *
 * Returns: (transfer full): An #IdeXmlSymbolNode or %NULL up on failure.
 */
IdeXmlSymbolNode *
ide_xml_service_get_root_node_finish (IdeXmlService  *self,
                                      GAsyncResult   *result,
                                      GError        **error)
{
  GTask *task = (GTask *)result;

  g_return_val_if_fail (IDE_IS_XML_SERVICE (self), NULL);
  g_return_val_if_fail (G_IS_TASK (result), NULL);
  g_return_val_if_fail (error != NULL, NULL);

  return g_task_propagate_pointer (task, error);
}

static void
ide_xml_service_get_diagnostics_cb (GObject      *object,
                                    GAsyncResult *result,
                                    gpointer      user_data)
{
  IdeXmlService  *self = (IdeXmlService *)object;
  g_autoptr(GTask) task = user_data;
  g_autoptr(IdeXmlAnalysis) analysis = NULL;
  IdeDiagnostics *diagnostics;
  g_autoptr(GError) error = NULL;

  g_assert (IDE_IS_XML_SERVICE (self));
  g_assert (G_IS_ASYNC_RESULT (result));
  g_assert (G_IS_TASK (task));

  if (NULL == (analysis = ide_xml_service_get_analysis_finish (self, result, &error)))
    g_task_return_error (task, g_steal_pointer (&error));
  else
    {
      diagnostics = ide_diagnostics_ref (ide_xml_analysis_get_diagnostics (analysis));
      g_task_return_pointer (task, diagnostics, (GDestroyNotify)ide_diagnostics_unref);
    }
}

/**
 * ide_xml_service_get_diagnostics_async:
 *
 * This function is used to asynchronously retrieve the diagnostics
 * for a particular file.
 *
 * If the analysis is up to date, then no parsing will occur and the
 * existing diagnostics will be used.
 *
 * If the analysis is out of date, then the source file(s) will be
 * parsed asynchronously.
 *
 * The xml service is meant to be used with buffers, that is,
 * by extension, loaded views.
 */
void
ide_xml_service_get_diagnostics_async (IdeXmlService       *self,
                                       IdeFile             *ifile,
                                       IdeBuffer           *buffer,
                                       GCancellable        *cancellable,
                                       GAsyncReadyCallback  callback,
                                       gpointer             user_data)
{
  g_autoptr(GTask) task = NULL;
  IdeXmlAnalysis *cached;

  g_return_if_fail (IDE_IS_MAIN_THREAD ());
  g_return_if_fail (IDE_IS_XML_SERVICE (self));
  g_return_if_fail (IDE_IS_FILE (ifile));
  g_return_if_fail (IDE_IS_BUFFER (buffer) || buffer == NULL);
  g_return_if_fail (!cancellable || G_IS_CANCELLABLE (cancellable));

  task = g_task_new (self, cancellable, callback, user_data);
  g_task_set_source_tag (task, ide_xml_service_get_diagnostics_async);

  /*
   * If we have a cached analysis with some diagnostics,
   * and it is new enough, then re-use it.
   */
  if ((cached = dzl_task_cache_peek (self->analyses, ifile)))
    {
      IdeContext *context;
      IdeUnsavedFiles *unsaved_files;
      IdeUnsavedFile *uf;
      IdeDiagnostics *diagnostics;
      GFile *gfile;

      gfile = ide_file_get_file (ifile);
      context = ide_object_get_context (IDE_OBJECT (self));
      unsaved_files = ide_context_get_unsaved_files (context);

      if ((uf = ide_unsaved_files_get_unsaved_file (unsaved_files, gfile)) &&
          ide_xml_analysis_get_sequence (cached) == ide_unsaved_file_get_sequence (uf))
        {
          diagnostics = ide_xml_analysis_get_diagnostics (cached);
          g_assert (diagnostics != NULL);
          g_task_return_pointer (task,
                                 ide_diagnostics_ref (diagnostics),
                                 (GDestroyNotify)ide_diagnostics_unref);
          return;
        }
    }

  ide_xml_service_get_analysis_async (self,
                                      ifile,
                                      buffer,
                                      cancellable,
                                      ide_xml_service_get_diagnostics_cb,
                                      g_steal_pointer (&task));
}

/**
 * ide_xml_service_get_diagnostics_finish:
 *
 * Completes an asychronous request to get the diagnostics for a given file.
 * See ide_xml_service_get_diagnostics_async() for more information.
 *
 * Returns: (transfer full): An #IdeDiagnostics or %NULL on failure.
 */
IdeDiagnostics *
ide_xml_service_get_diagnostics_finish (IdeXmlService  *self,
                                        GAsyncResult   *result,
                                        GError        **error)
{
  g_return_val_if_fail (IDE_IS_XML_SERVICE (self), NULL);
  g_return_val_if_fail (G_IS_TASK (result), NULL);
  g_return_val_if_fail (error != NULL, NULL);

  return g_task_propagate_pointer (G_TASK (result), error);
}

static void
ide_xml_service_context_loaded (IdeService *service)
{
  IdeXmlService *self = (IdeXmlService *)service;
  IdeContext *context;

  IDE_ENTRY;

  g_assert (IDE_IS_XML_SERVICE (self));

  context = ide_object_get_context (IDE_OBJECT (self));

  if (self->tree_builder == NULL)
    self->tree_builder = g_object_new (IDE_TYPE_XML_TREE_BUILDER,
                                       "context", context,
                                       NULL);

  IDE_EXIT;
}

typedef struct
{
  IdeFile   *ifile;
  IdeBuffer *buffer;
  gint       line;
  gint       line_offset;
} PositionState;

static void
position_state_free (PositionState *state)
{
  g_assert (state != NULL);
  g_assert (IDE_IS_MAIN_THREAD ());

  g_clear_object (&state->ifile);
  g_clear_object (&state->buffer);
  g_slice_free (PositionState, state);
}

static inline gboolean
skip_whitespaces (const gchar **cursor)
{
  const gchar *p = *cursor;
  gunichar ch;

  g_assert (cursor != NULL && *cursor != NULL);

  while ((ch = g_utf8_get_char (*cursor)) && g_unichar_isspace (ch))
    *cursor = g_utf8_next_char (*cursor);

  return (p != *cursor);
}

static inline void
skip_all_name (const gchar **cursor)
{
  gunichar ch;

  g_assert (cursor != NULL && *cursor != NULL);

  while ((ch = g_utf8_get_char (*cursor)) && !g_unichar_isspace (ch))
    *cursor = g_utf8_next_char (*cursor);
}

static IdeXmlPositionDetail
get_detail (IdeXmlSymbolNode  *node,
            const gchar       *prefix,
            gunichar           next_ch,
            gchar            **name,
            gchar            **value,
            gchar             *quote)
{
  IdeXmlPositionDetail detail;
  const gchar *cursor, *start;
  const gchar *name_start;
  gsize name_size;
  gboolean has_spaces = FALSE;

  g_assert (IDE_IS_XML_SYMBOL_NODE (node));
  g_assert (prefix != NULL);
  g_assert (value != NULL);

  *name = NULL;
  *value = NULL;
  *quote = 0;

  cursor = prefix;
  detail =  IDE_XML_POSITION_DETAIL_IN_NAME;
  if (!ide_xml_utils_skip_element_name (&cursor))
    return IDE_XML_POSITION_DETAIL_NONE;

  if (*cursor == 0)
    {
      if (!g_unichar_isspace (next_ch) &&
          next_ch != '<' &&
          next_ch != '>' &&
          next_ch != '/')
        return IDE_XML_POSITION_DETAIL_NONE;
      else
        {
          *name = g_strdup (prefix);
          return detail;
        }
    }

  detail =  IDE_XML_POSITION_DETAIL_IN_ATTRIBUTE_NAME;
  /* Whitespaces after the element name */
  skip_whitespaces (&cursor);
  if (*cursor == 0)
    return detail;

  while (TRUE)
    {
      *quote = 0;
      start = cursor;
      /* Attribute name */
      if (!ide_xml_utils_skip_attribute_name (&cursor))
        continue;

      if (*cursor == 0)
        {
          if (!g_unichar_isspace (next_ch) && next_ch != '=')
            return IDE_XML_POSITION_DETAIL_NONE;

          *name = g_strndup (start, cursor - start);
          return detail;
        }

      name_start = start;
      name_size = cursor - start;
      /* Whitespaces between the name and the = */
      skip_whitespaces (&cursor);
      if (*cursor == 0)
        return detail;

      if (*cursor != '=')
        continue;
      else
        cursor++;

      /* Whitespaces after the = */
      /* TODO: at this point we need to add quoted around the value */
      detail =  IDE_XML_POSITION_DETAIL_IN_ATTRIBUTE_VALUE;
      skip_whitespaces (&cursor);
      if (*cursor == 0)
        {
          *name = g_strndup (name_start, name_size);
          return detail;
        }

      *quote = *cursor;
      start = ++cursor;
      if (*quote != '"' && *quote != '\'')
        {
          *quote = 0;
          if (!g_unichar_isspace(*(cursor -1)))
            {
              skip_all_name (&cursor);
              if (*cursor == 0)
                return IDE_XML_POSITION_DETAIL_NONE;

              skip_whitespaces (&cursor);
              if (*cursor == 0)
                return IDE_XML_POSITION_DETAIL_IN_ATTRIBUTE_NAME;
            }

          continue;
        }

      /* Attribute value */
      if (!ide_xml_utils_skip_attribute_value (&cursor, *quote))
        {
          *name = g_strndup (name_start, name_size);
          *value = g_strndup (start, cursor - start);
          return detail;
        }

      detail =  IDE_XML_POSITION_DETAIL_IN_ATTRIBUTE_NAME;
      /* Whitespaces after the attribute value */
      if (skip_whitespaces (&cursor))
        has_spaces = TRUE;

      if (*cursor == 0)
        {
          *quote = 0;
          return (has_spaces) ? IDE_XML_POSITION_DETAIL_IN_ATTRIBUTE_NAME : IDE_XML_POSITION_DETAIL_NONE;
        }

      if (!has_spaces)
        {
          skip_all_name (&cursor);
          if (*cursor == 0)
            return IDE_XML_POSITION_DETAIL_NONE;
        }
    };
}

static IdeXmlPosition *
get_position (IdeXmlService   *self,
              IdeXmlAnalysis  *analysis,
              GtkTextBuffer   *buffer,
              gint             line,
              gint             line_offset)
{
  IdeXmlPosition *position = NULL;
  IdeXmlSymbolNode *root_node;
  IdeXmlSymbolNode *current_node, *child_node;
  IdeXmlSymbolNode *candidate_node = NULL;
  IdeXmlSymbolNode *previous_node = NULL;
  IdeXmlSymbolNode *previous_sibling_node = NULL;
  IdeXmlSymbolNode *next_sibling_node = NULL;
  IdeXmlSymbolNodeRelativePosition rel_pos;
  IdeXmlPositionKind candidate_kind;
  IdeXmlPositionDetail detail = IDE_XML_POSITION_DETAIL_NONE;
  g_autofree gchar *prefix = NULL;
  g_autofree gchar *detail_name = NULL;
  g_autofree gchar *detail_value = NULL;
  GtkTextIter start, end;
  gunichar next_ch = 0;
  gint start_line, start_line_offset;
  guint n_children;
  gint child_pos = -1;
  gint n = 0;
  gchar quote = 0;
  gboolean has_prefix = FALSE;

  g_assert (IDE_IS_XML_SERVICE (self));
  g_assert (analysis != NULL);
  g_assert (GTK_IS_TEXT_BUFFER (buffer));

  current_node = root_node = ide_xml_analysis_get_root_node (analysis);
  while (TRUE)
    {
loop:
      if (0 == (n_children = ide_xml_symbol_node_get_n_direct_children (current_node)))
        goto result;

      for (n = 0; n < n_children; ++n)
        {
          child_node = IDE_XML_SYMBOL_NODE (ide_xml_symbol_node_get_nth_direct_child (current_node, n));
          child_pos = n;
          rel_pos = ide_xml_symbol_node_compare_location (child_node, line, line_offset);

          switch (rel_pos)
            {
            case IDE_XML_SYMBOL_NODE_RELATIVE_POSITION_IN_START_TAG:
              candidate_node = child_node;
              candidate_kind = IDE_XML_POSITION_KIND_IN_START_TAG;
              has_prefix = TRUE;
              goto result;

            case IDE_XML_SYMBOL_NODE_RELATIVE_POSITION_IN_END_TAG:
              candidate_node = child_node;
              candidate_kind = IDE_XML_POSITION_KIND_IN_END_TAG;
              has_prefix = TRUE;
              goto result;

            case IDE_XML_SYMBOL_NODE_RELATIVE_POSITION_BEFORE:
              candidate_node = current_node;
              candidate_kind = IDE_XML_POSITION_KIND_IN_CONTENT;
              goto result;

            case IDE_XML_SYMBOL_NODE_RELATIVE_POSITION_AFTER:
              previous_node = child_node;

              if (n == (n_children - 1))
                {
                  child_pos = n_children;
                  candidate_node = current_node;
                  candidate_kind = IDE_XML_POSITION_KIND_IN_CONTENT;
                  goto result;
                }

              break;

            case IDE_XML_SYMBOL_NODE_RELATIVE_POSITION_IN_CONTENT:
              candidate_node = current_node = previous_node = child_node;
              candidate_kind = IDE_XML_POSITION_KIND_IN_CONTENT;
              goto loop;

            case IDE_XML_POSITION_KIND_UNKNOW:
            default:
              g_assert_not_reached ();
            }
        }
    }

result:
  if (candidate_node == NULL)
    {
      /* Empty tree case */
      candidate_node = root_node;
      candidate_kind = IDE_XML_POSITION_KIND_IN_CONTENT;
    }
  else if (candidate_kind == IDE_XML_POSITION_KIND_IN_CONTENT)
    {
      if (previous_node != NULL &&
          ide_xml_symbol_node_get_state (previous_node) == IDE_XML_SYMBOL_NODE_STATE_NOT_CLOSED)
        {
          candidate_node = previous_node;
          /* TODO: detect the IN_END_TAG case */
          candidate_kind = IDE_XML_POSITION_KIND_IN_START_TAG;
          has_prefix = TRUE;
        }
    }

  if (has_prefix)
    {
      ide_xml_symbol_node_get_location (candidate_node, &start_line, &start_line_offset, NULL, NULL, NULL);
      gtk_text_buffer_get_iter_at_line_index (buffer, &start, start_line - 1, start_line_offset - 1);
      gtk_text_buffer_get_iter_at_line_index (buffer, &end, line - 1, line_offset - 1);

      if (gtk_text_iter_get_char (&start) == '<')
        gtk_text_iter_forward_char (&start);

      next_ch = gtk_text_iter_get_char (&end);
      if (!gtk_text_iter_equal (&start, &end))
        {
          prefix = gtk_text_buffer_get_text (buffer, &start, &end, FALSE);
          detail = get_detail (candidate_node, prefix, next_ch, &detail_name, &detail_value, &quote);
        }
      else
        detail = IDE_XML_POSITION_DETAIL_IN_NAME;
    }

  if (candidate_kind == IDE_XML_POSITION_KIND_IN_CONTENT)
    {
      position = ide_xml_position_new (candidate_node, prefix, candidate_kind, detail, detail_name, detail_value, quote);
      ide_xml_position_set_analysis (position, analysis);
      ide_xml_position_set_child_pos (position, child_pos);
    }
  else if (candidate_kind == IDE_XML_POSITION_KIND_IN_START_TAG ||
           candidate_kind == IDE_XML_POSITION_KIND_IN_END_TAG)
    {
      child_node = candidate_node;
      candidate_node = ide_xml_symbol_node_get_parent (child_node);

      position = ide_xml_position_new (candidate_node, prefix, candidate_kind, detail, detail_name, detail_value, quote);
      ide_xml_position_set_analysis (position, analysis);
      ide_xml_position_set_child_node (position, child_node);
    }
  else
    g_assert_not_reached ();

  if (child_pos > 0)
    previous_sibling_node = IDE_XML_SYMBOL_NODE (ide_xml_symbol_node_get_nth_direct_child (candidate_node, child_pos - 1));

  if (child_pos < n_children)
    next_sibling_node = IDE_XML_SYMBOL_NODE (ide_xml_symbol_node_get_nth_direct_child (candidate_node, child_pos));

  ide_xml_position_set_siblings (position, previous_sibling_node, next_sibling_node);

  return position;
}

static void
ide_xml_service_get_position_from_cursor_cb (GObject      *object,
                                             GAsyncResult *result,
                                             gpointer      user_data)
{
  IdeXmlService *self = (IdeXmlService *)object;
  g_autoptr(IdeXmlPosition) position = NULL;
  g_autoptr(IdeTask) task = user_data;
  g_autoptr(GError) error = NULL;
  IdeXmlAnalysis *analysis;
  PositionState *state;

  IDE_ENTRY;

  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (IDE_IS_TASK (task));
  g_assert (G_IS_ASYNC_RESULT (result));
  g_assert (IDE_IS_XML_SERVICE (self));

  if (!(analysis = ide_xml_service_get_analysis_finish (self, result, &error)))
    {
      ide_task_return_error (task, g_steal_pointer (&error));
      return;
    }

  state = ide_task_get_task_data (task);

  position = get_position (self,
                           analysis,
                           GTK_TEXT_BUFFER (state->buffer),
                           state->line,
                           state->line_offset);

  ide_task_return_pointer (task,
                           g_steal_pointer (&position),
                           g_object_unref);

  IDE_EXIT;
}

void
ide_xml_service_get_position_from_cursor_async (IdeXmlService       *self,
                                                IdeFile             *ifile,
                                                IdeBuffer           *buffer,
                                                gint                 line,
                                                gint                 line_offset,
                                                GCancellable        *cancellable,
                                                GAsyncReadyCallback  callback,
                                                gpointer             user_data)
{
  g_autoptr(IdeTask) task = NULL;
  PositionState *state;

  IDE_ENTRY;

  g_return_if_fail (IDE_IS_XML_SERVICE (self));
  g_return_if_fail (IDE_IS_FILE (ifile));
  g_return_if_fail (IDE_IS_BUFFER (buffer) || buffer == NULL);
  g_return_if_fail (!cancellable || G_IS_CANCELLABLE (cancellable));

  task = ide_task_new (self, cancellable, callback, user_data);
  ide_task_set_source_tag (task, ide_xml_service_get_position_from_cursor_async);

  state = g_slice_new0 (PositionState);
  state->ifile = g_object_ref (ifile);
  state->buffer = g_object_ref (buffer);
  state->line = line;
  state->line_offset = line_offset;

  ide_task_set_task_data (task, state, (GDestroyNotify)position_state_free);

  ide_xml_service_get_analysis_async (self,
                                      ifile,
                                      buffer,
                                      cancellable,
                                      ide_xml_service_get_position_from_cursor_cb,
                                      g_steal_pointer (&task));

  IDE_EXIT;
}

IdeXmlPosition *
ide_xml_service_get_position_from_cursor_finish (IdeXmlService  *self,
                                                 GAsyncResult   *result,
                                                 GError        **error)
{
  g_return_val_if_fail (IDE_IS_XML_SERVICE (self), NULL);
  g_return_val_if_fail (IDE_IS_TASK (result), NULL);

  return ide_task_propagate_pointer (IDE_TASK (result), error);
}

static void
ide_xml_service_start (IdeService *service)
{
  IdeXmlService *self = (IdeXmlService *)service;

  g_assert (IDE_IS_XML_SERVICE (self));

  self->analyses = dzl_task_cache_new ((GHashFunc)ide_file_hash,
                                       (GEqualFunc)ide_file_equal,
                                       g_object_ref,
                                       g_object_unref,
                                       (GBoxedCopyFunc)ide_xml_analysis_ref,
                                       (GBoxedFreeFunc)ide_xml_analysis_unref,
                                       DEFAULT_EVICTION_MSEC,
                                       ide_xml_service_build_tree_cb,
                                       self,
                                       NULL);

  dzl_task_cache_set_name (self->analyses, "xml analysis cache");

  /* There's no eviction time on this cache */
  self->schemas = dzl_task_cache_new ((GHashFunc)g_file_hash,
                                      (GEqualFunc)g_file_equal,
                                      g_object_ref,
                                      g_object_unref,
                                      (GBoxedCopyFunc)ide_xml_schema_cache_entry_ref,
                                      (GBoxedFreeFunc)ide_xml_schema_cache_entry_unref,
                                      0,
                                      ide_xml_service_load_schema_cb,
                                      self,
                                      NULL);

  dzl_task_cache_set_name (self->schemas, "xml schemas cache");
}

static void
ide_xml_service_stop (IdeService *service)
{
  IdeXmlService *self = (IdeXmlService *)service;

  g_assert (IDE_IS_XML_SERVICE (self));

  if (self->cancellable && !g_cancellable_is_cancelled (self->cancellable))
    g_cancellable_cancel (self->cancellable);

  g_clear_object (&self->cancellable);
  g_clear_object (&self->analyses);
  g_clear_object (&self->schemas);
}

static void
ide_xml_service_finalize (GObject *object)
{
  IdeXmlService *self = (IdeXmlService *)object;

  IDE_ENTRY;

  ide_xml_service_stop (IDE_SERVICE (self));
  g_clear_object (&self->tree_builder);

  G_OBJECT_CLASS (ide_xml_service_parent_class)->finalize (object);

  IDE_EXIT;
}

static void
ide_xml_service_class_init (IdeXmlServiceClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = ide_xml_service_finalize;
}

static void
service_iface_init (IdeServiceInterface *iface)
{
  iface->context_loaded = ide_xml_service_context_loaded;
  iface->start = ide_xml_service_start;
  iface->stop = ide_xml_service_stop;
}

static void
ide_xml_service_init (IdeXmlService *self)
{
}

/**
 * ide_xml_service_get_cached_root_node:
 *
 * Gets the #IdeXmlSymbolNode root node for the corresponding file.
 *
 * Returns: (transfer NULL): A xml symbol node.
 */
IdeXmlSymbolNode *
ide_xml_service_get_cached_root_node (IdeXmlService *self,
                                      GFile         *gfile)
{
  IdeXmlAnalysis *analysis;
  IdeXmlSymbolNode *cached;

  g_return_val_if_fail (IDE_IS_XML_SERVICE (self), NULL);
  g_return_val_if_fail (IDE_IS_FILE (gfile), NULL);

  if (NULL != (analysis = dzl_task_cache_peek (self->analyses, gfile)) &&
      NULL != (cached = ide_xml_analysis_get_root_node (analysis)))
    return g_object_ref (cached);

  return NULL;
}

/**
 * ide_xml_service_get_cached_diagnostics:
 *
 * Gets the #IdeDiagnostics for the corresponding file.
 *
 * Returns: (transfer NULL): an #IdeDiagnostics.
 */
IdeDiagnostics *
ide_xml_service_get_cached_diagnostics (IdeXmlService *self,
                                        GFile         *gfile)
{
  IdeXmlAnalysis *analysis;
  IdeDiagnostics *cached;

  g_return_val_if_fail (IDE_IS_XML_SERVICE (self), NULL);
  g_return_val_if_fail (IDE_IS_FILE (gfile), NULL);

  if (NULL != (analysis = dzl_task_cache_peek (self->analyses, gfile)) &&
      NULL != (cached = ide_xml_analysis_get_diagnostics (analysis)))
    return ide_diagnostics_ref (cached);

  return NULL;
}

/**
 * ide_xml_service_get_schemas_cache:
 *
 * Gets the #DzlTaskCache for the xml schemas.
 *
 * Returns: (transfer NULL): a #DzlTaskCache.
 */
DzlTaskCache *
ide_xml_service_get_schemas_cache (IdeXmlService *self)
{
  g_return_val_if_fail (IDE_IS_XML_SERVICE (self), NULL);

  return self->schemas;
}
