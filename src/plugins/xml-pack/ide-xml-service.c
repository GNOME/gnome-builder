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
  DzlTaskCache      *modversions;
  IdeXmlTreeBuilder *tree_builder;
  GCancellable      *cancellable;
};

static void service_iface_init (IdeServiceInterface *iface);

G_DEFINE_TYPE_WITH_CODE (IdeXmlService, ide_xml_service, IDE_TYPE_OBJECT,
                         G_IMPLEMENT_INTERFACE (IDE_TYPE_SERVICE, service_iface_init))

#define MAX_EXPECTED_VERSION_LEN 20

typedef struct
{
  GTask *task;
  gchar *id;
} ModVersionState;

static void
mod_version_state_free (gpointer data)
{
  ModVersionState *state = (ModVersionState *)data;

  g_assert (state != NULL);

  dzl_clear_pointer (&state->id, g_free);
  g_clear_object (&state->task);
  g_slice_free (ModVersionState, state);
}

static void
ide_xml_service_get_modversion_cb1 (GObject      *object,
                                    GAsyncResult *result,
                                    gpointer      user_data)
{
  IdeSubprocess *subprocess = (IdeSubprocess *)object;
  ModVersionState *state = (ModVersionState *)user_data;
  g_autoptr(GBytes) stdout_buf = NULL;
  g_autoptr(GError) error = NULL;
  const gchar *version;
  gsize len;

  g_assert (IDE_IS_SUBPROCESS (subprocess));
  g_assert (G_IS_ASYNC_RESULT (result));
  g_assert (state != NULL);

  if (ide_subprocess_communicate_finish (subprocess, result, &stdout_buf, NULL, &error))
    {
      version = (const gchar *)g_bytes_get_data (stdout_buf, &len);
      if (len > 0 && len < MAX_EXPECTED_VERSION_LEN)
        g_task_return_pointer (state->task, g_strdup (version), g_free);
      else
        {
          error = g_error_new (G_IO_ERROR,
                               G_IO_ERROR_FAILED,
                               _("Package don't exist or is not installed: %s"),
                               state->id);
          g_task_return_error (state->task, g_steal_pointer (&error));
        }
    }
  else
    g_task_return_error (state->task, g_steal_pointer (&error));

  mod_version_state_free (state);
}

static void
ide_xml_service_get_modversion_cb (DzlTaskCache  *cache,
                                   gconstpointer  key,
                                   GTask         *task,
                                   gpointer       user_data)
{
  IdeXmlService *self = user_data;
  const gchar *id = (const gchar *)key;
  g_autoptr(IdeSubprocessLauncher) launcher = NULL;
  g_autoptr(IdeSubprocess) subprocess = NULL;
  g_autoptr(GError) error = NULL;
  ModVersionState *state;

  IDE_ENTRY;

  g_assert (IDE_IS_XML_SERVICE (self));
  g_assert (DZL_IS_TASK_CACHE (cache));
  g_assert (!dzl_str_empty0 (id));
  g_assert (G_IS_TASK (task));

  id = strrchr (id, '@');
  g_assert (id != NULL);
  id++;

  launcher = ide_subprocess_launcher_new (G_SUBPROCESS_FLAGS_STDOUT_PIPE);

  ide_subprocess_launcher_push_argv (launcher, "pkg-config");
  ide_subprocess_launcher_push_argv (launcher, id);
  ide_subprocess_launcher_push_argv (launcher, "--modversion");

  if ((subprocess = ide_subprocess_launcher_spawn (launcher, NULL, &error)))
    {
      state = g_slice_new0 (ModVersionState);
      state->task = g_object_ref (task);
      state->id = g_strdup (id);

      ide_subprocess_communicate_async (subprocess,
                                        NULL,
                                        NULL,
                                        ide_xml_service_get_modversion_cb1,
                                        state);
    }
  else
    g_task_return_error (task, g_steal_pointer (&error));

  IDE_EXIT;
}

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

  if (NULL == (gfile = ide_file_get_file (ifile)) || NULL == (path = g_file_get_path (gfile)))
    g_task_return_new_error (task,
                             G_IO_ERROR,
                             G_IO_ERROR_NOT_SUPPORTED,
                             _("File must be saved locally to parse."));
  else
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

static void
schema_state_free (gpointer data)
{
  SchemaState *state = (SchemaState *)data;

  g_assert (state != NULL);

  g_clear_object (&state->task);
  dzl_clear_pointer (&state->cache_entry, ide_xml_schema_cache_entry_unref);
}

/* Parse schema phase */
static void
ide_xml_service_load_schema_cb3 (GObject      *object,
                                 GAsyncResult *result,
                                 gpointer      user_data)
{
  GFile *file = (GFile *)object;
  SchemaState *state = (SchemaState *)user_data;
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
          if ((schema = ide_xml_rng_parser_parse (rng_parser, content, len, file)))
            {
              cache_entry->schema = schema;
              cache_entry->state = SCHEMA_STATE_PARSED;
            }
          else
            {
              /* TODO: get parse error ? */
              dzl_clear_pointer (&cache_entry->content, g_bytes_unref);
              cache_entry->state = SCHEMA_STATE_CANT_PARSE;
            }
        }
      else
        {
          /* TODO: set error message */
          dzl_clear_pointer (&cache_entry->content, g_bytes_unref);
          cache_entry->state = SCHEMA_STATE_WRONG_FILE_TYPE;
        }
    }

  g_task_return_pointer (state->task,
                         g_steal_pointer (&state->cache_entry),
                         (GDestroyNotify)ide_xml_schema_cache_entry_unref);
  schema_state_free (state);
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

  g_assert (G_IS_FILE (file));
  g_assert (G_IS_ASYNC_RESULT (result));
  g_assert (state != NULL);

  cache_entry = state->cache_entry;
  if ((file_info = g_file_query_info_finish (file, result, &error)))
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

      g_task_return_pointer (state->task,
                             g_steal_pointer (&state->cache_entry),
                             (GDestroyNotify)ide_xml_schema_cache_entry_unref);
      schema_state_free (state);
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
  g_autoptr(IdeTask) task = user_data;
  g_autoptr(IdeXmlAnalysis) analysis = NULL;
  g_autoptr(GError) error = NULL;

  g_assert (DZL_IS_TASK_CACHE (cache));
  g_assert (G_IS_ASYNC_RESULT (result));
  g_assert (IDE_IS_TASK (task));

  if (NULL == (analysis = dzl_task_cache_get_finish (cache, result, &error)))
    ide_task_return_error (task, g_steal_pointer (&error));
  else
    ide_task_return_pointer (task, g_steal_pointer (&analysis), (GDestroyNotify)ide_xml_analysis_unref);
}

typedef struct
{
  IdeXmlService *self;
  IdeTask       *task;
  GCancellable  *cancellable;
  IdeFile       *ifile;
  IdeBuffer     *buffer;
} TaskState;

static void
task_state_free (gpointer data)
{
  TaskState *state = (TaskState *)data;

  g_assert (state != NULL);

  g_clear_object (&state->task);
  g_clear_object (&state->ifile);
  g_clear_object (&state->buffer);
}

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

  task_state_free (state);
}

static void
ide_xml_service_get_analysis_async (IdeXmlService       *self,
                                    IdeFile             *ifile,
                                    IdeBuffer           *buffer,
                                    GCancellable        *cancellable,
                                    GAsyncReadyCallback  callback,
                                    gpointer             user_data)
{
  g_autoptr(IdeTask) task = NULL;
  IdeContext *context;
  IdeBufferManager *manager;
  GFile *gfile;

  g_assert (IDE_IS_XML_SERVICE (self));
  g_assert (IDE_IS_FILE (ifile));
  g_assert (IDE_IS_BUFFER (buffer));
  g_assert (cancellable == NULL || G_IS_CANCELLABLE (cancellable));

  task = ide_task_new (self, cancellable, callback, user_data);
  context = ide_object_get_context (IDE_OBJECT (self));
  manager = ide_context_get_buffer_manager (context);
  gfile = ide_file_get_file (ifile);

  if (!ide_buffer_manager_has_file (manager, gfile))
    {
      TaskState *state;

      if (!ide_buffer_get_loading (buffer))
        {
          ide_task_return_new_error (task,
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

static IdeXmlAnalysis *
ide_xml_service_get_analysis_finish (IdeXmlService  *self,
                                     GAsyncResult   *result,
                                     GError        **error)
{
  IdeTask *task = (IdeTask *)result;

  g_return_val_if_fail (IDE_IS_XML_SERVICE (self), NULL);
  g_return_val_if_fail (IDE_IS_TASK (task), NULL);

  return ide_task_propagate_pointer (task, error);
}

static void
ide_xml_service_get_root_node_cb (GObject      *object,
                                  GAsyncResult *result,
                                  gpointer      user_data)
{
  IdeXmlService *self = (IdeXmlService *)object;
  g_autoptr(IdeTask) task = user_data;
  g_autoptr(IdeXmlAnalysis) analysis = NULL;
  IdeXmlSymbolNode *root_node;
  g_autoptr(GError) error = NULL;

  g_assert (IDE_IS_XML_SERVICE (self));
  g_assert (G_IS_ASYNC_RESULT (result));
  g_assert (IDE_IS_TASK (task));

  if (NULL == (analysis = ide_xml_service_get_analysis_finish (self, result, &error)))
    ide_task_return_error (task, g_steal_pointer (&error));
  else
    {
      root_node = g_object_ref (ide_xml_analysis_get_root_node (analysis));
      ide_task_return_pointer (task, root_node, g_object_unref);
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
  g_autoptr(IdeTask) task = NULL;
  IdeXmlAnalysis *cached;

  g_return_if_fail (IDE_IS_XML_SERVICE (self));
  g_return_if_fail (IDE_IS_FILE (ifile));
  g_return_if_fail (IDE_IS_BUFFER (buffer));
  g_return_if_fail (!cancellable || G_IS_CANCELLABLE (cancellable));

  task = ide_task_new (self, cancellable, callback, user_data);

  /*
   * If we have a cached analysis with a valid root_node,
   * and it is new enough, then re-use it.
   */
  if ((cached = dzl_task_cache_peek (self->analyses, ifile)))
    {
      IdeContext *context;
      IdeUnsavedFiles *unsaved_files;
      IdeUnsavedFile *uf;
      IdeXmlSymbolNode *root_node;
      GFile *gfile;

      gfile = ide_file_get_file (ifile);
      context = ide_object_get_context (IDE_OBJECT (self));
      unsaved_files = ide_context_get_unsaved_files (context);

      if ((uf = ide_unsaved_files_get_unsaved_file (unsaved_files, gfile)) &&
          ide_xml_analysis_get_sequence (cached) == ide_unsaved_file_get_sequence (uf))
        {
          root_node = g_object_ref (ide_xml_analysis_get_root_node (cached));

          ide_task_return_pointer (task, root_node, g_object_unref);
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
 * Returns: (transfer full): An #IdeXmlSymbolNode or %NULL upon failure.
 */
IdeXmlSymbolNode *
ide_xml_service_get_root_node_finish (IdeXmlService  *self,
                                      GAsyncResult   *result,
                                      GError        **error)
{
  IdeTask *task = (IdeTask *)result;

  g_return_val_if_fail (IDE_IS_XML_SERVICE (self), NULL);
  g_return_val_if_fail (IDE_IS_TASK (result), NULL);
  g_return_val_if_fail (error != NULL, NULL);

  return ide_task_propagate_pointer (task, error);
}

static void
ide_xml_service_get_diagnostics_cb (GObject      *object,
                                    GAsyncResult *result,
                                    gpointer      user_data)
{
  IdeXmlService  *self = (IdeXmlService *)object;
  g_autoptr(IdeTask) task = user_data;
  g_autoptr(IdeXmlAnalysis) analysis = NULL;
  IdeDiagnostics *diagnostics;
  g_autoptr(GError) error = NULL;

  g_assert (IDE_IS_XML_SERVICE (self));
  g_assert (G_IS_ASYNC_RESULT (result));
  g_assert (IDE_IS_TASK (task));

  if (!(analysis = ide_xml_service_get_analysis_finish (self, result, &error)))
    ide_task_return_error (task, g_steal_pointer (&error));
  else
    {
      diagnostics = ide_diagnostics_ref (ide_xml_analysis_get_diagnostics (analysis));
      ide_task_return_pointer (task, diagnostics, (GDestroyNotify)ide_diagnostics_unref);
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
  g_autoptr(IdeTask) task = NULL;
  IdeXmlAnalysis *cached;

  g_return_if_fail (IDE_IS_MAIN_THREAD ());
  g_return_if_fail (IDE_IS_XML_SERVICE (self));
  g_return_if_fail (IDE_IS_FILE (ifile));
  g_return_if_fail (IDE_IS_BUFFER (buffer) || buffer == NULL);
  g_return_if_fail (!cancellable || G_IS_CANCELLABLE (cancellable));

  task = ide_task_new (self, cancellable, callback, user_data);
  ide_task_set_source_tag (task, ide_xml_service_get_diagnostics_async);

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
          ide_task_return_pointer (task,
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
  g_return_val_if_fail (IDE_IS_TASK (result), NULL);
  g_return_val_if_fail (error != NULL, NULL);

  return ide_task_propagate_pointer (IDE_TASK (result), error);
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

static IdeXmlDetail *
get_attr_value_pair (const gchar  *insert_ptr,
                     const gchar **cursor,
                     gboolean     *has_more_pairs)
{
  g_autofree gchar *name = NULL;
  g_autofree gchar *value = NULL;
  g_autofree gchar *prefix = NULL;
  const gchar *name_start = NULL;
  const gchar *name_end = NULL;
  const gchar *value_start = NULL;
  const gchar *value_end = NULL;
  IdeXmlDetailMember member = IDE_XML_DETAIL_MEMBER_NONE;
  IdeXmlDetailSide side = IDE_XML_DETAIL_SIDE_NONE;
  gboolean member_set = FALSE;
  gssize name_len = -1;
  gssize value_len = -1;
  gchar quote = 0;

  *has_more_pairs = FALSE;

  /* Skip the whitespaces before the name */
  if (!skip_whitespaces (cursor))
    return NULL;

  if (*cursor > insert_ptr)
    {
      name_start = name_end = insert_ptr;
      goto finish;
    }

  /* Attribute name */
  name_start = *cursor;
  if (!ide_xml_utils_skip_attribute_name (cursor))
    return NULL;

  name_end = *cursor;
  if (**cursor == 0)
    goto finish;

  /* Skip the whitespaces between the name and the = */
  skip_whitespaces (cursor);

  if (**cursor != '=')
    return NULL;

  /* Skip the '=' and the whitespaces after it */
  (*cursor)++;
  skip_whitespaces (cursor);
  if (**cursor == 0)
    return NULL;

  if (**cursor != '"' && **cursor != '\'')
    {
      skip_all_name (cursor);
      return NULL;
    }

  quote = **cursor;
  (*cursor)++;
  value_start = *cursor;

  /* Attribute value */
  if (!ide_xml_utils_skip_attribute_value (cursor, quote))
    goto finish;

  value_end = (*cursor) - 1;
  /* Pairs need to be space-separated */
  if (*cursor < insert_ptr)
    {
      gunichar ch = g_utf8_get_char (*cursor);

      if (g_unichar_isspace (ch))
        *has_more_pairs = TRUE;
    }

finish:
  if (name_start != NULL && name_end != NULL)
    {
      name_len = name_end - name_start;

      if (insert_ptr >= name_start && insert_ptr <= name_end)
        {
          member = IDE_XML_DETAIL_MEMBER_ATTRIBUTE_NAME;
          member_set = TRUE;

          if (insert_ptr == name_start)
            side = IDE_XML_DETAIL_SIDE_LEFT;
          else if (insert_ptr == name_end)
            side = IDE_XML_DETAIL_SIDE_RIGHT;
          else
            side = IDE_XML_DETAIL_SIDE_MIDDLE;

          prefix = g_strndup (name_start, insert_ptr - name_start);
        }
    }

  if (value_start != NULL && value_end != NULL)
    {
      value_len = value_end - value_start;

      if (!member_set)
        {
          if (insert_ptr >= value_start && insert_ptr <= value_end)
            {
              member = IDE_XML_DETAIL_MEMBER_ATTRIBUTE_VALUE;
              member_set = TRUE;

              if (insert_ptr == value_start)
                side = IDE_XML_DETAIL_SIDE_LEFT;
              else if (insert_ptr == value_end)
                side = IDE_XML_DETAIL_SIDE_RIGHT;
              else
                side = IDE_XML_DETAIL_SIDE_MIDDLE;

              prefix = g_strndup (value_start, insert_ptr - value_start);
            }
        }
    }

  if (!member_set)
    return NULL;

  if (name_len > -1)
    name = g_strndup (name_start, name_len);

  if (value_len > -1)
    value = g_strndup (value_start, value_len);

  return ide_xml_detail_new (name, value, prefix, member, side, quote);
}

static IdeXmlDetail *
get_detail (const gchar        *content,
            const gchar        *insert_ptr,
            IdeXmlPositionKind  kind)
{
  const gchar *cursor = content;
  gboolean has_more_pairs = TRUE;

  if (insert_ptr == NULL ||
      !ide_xml_utils_skip_element_name (&cursor))
    goto fail;

  if (cursor >= insert_ptr)
    {
      IdeXmlDetailSide side;
      g_autofree gchar *name = g_strndup (content, cursor - content);
      g_autofree gchar *prefix = g_strndup (content, insert_ptr - content);;

      if (insert_ptr == content)
        side = IDE_XML_DETAIL_SIDE_LEFT;
      else if (insert_ptr == cursor)
        side = IDE_XML_DETAIL_SIDE_RIGHT;
      else
        side = IDE_XML_DETAIL_SIDE_MIDDLE;

      return ide_xml_detail_new (name, NULL, prefix,
                                 IDE_XML_DETAIL_MEMBER_NAME,
                                 side,
                                 0);
    }

  if (kind == IDE_XML_POSITION_KIND_IN_START_TAG)
    {
      while (cursor <= insert_ptr && has_more_pairs)
        {
          IdeXmlDetail *detail;

          if ((detail = get_attr_value_pair (insert_ptr, &cursor, &has_more_pairs)))
            return detail;
        };
    }

fail:
  return ide_xml_detail_new (NULL, NULL, NULL,
                             IDE_XML_DETAIL_MEMBER_NONE,
                             IDE_XML_DETAIL_SIDE_NONE,
                             0);
}

static IdeXmlPosition *
get_position (IdeXmlService  *self,
              IdeXmlAnalysis *analysis,
              GtkTextBuffer  *buffer,
              gint            line,
              gint            line_offset)
{
  IdeXmlPosition *position = NULL;
  IdeXmlSymbolNode *root_node;
  IdeXmlSymbolNode *current_node, *child_node;
  IdeXmlSymbolNode *candidate_node = NULL;
  IdeXmlSymbolNode *previous_node = NULL;
  IdeXmlSymbolNode *previous_sibling_node = NULL;
  IdeXmlSymbolNode *next_sibling_node = NULL;
  IdeXmlSymbolNodeRelativePosition rel_pos;
  IdeXmlPositionKind candidate_kind = IDE_XML_POSITION_KIND_IN_CONTENT;
  g_autoptr(IdeXmlDetail) detail = NULL;
  const gchar *insert_ptr;
  GtkTextIter start, cursor, end;
  gint start_line, start_line_offset;
  gint end_line, end_line_offset;
  guint n_children;
  gint child_pos = -1;
  gint prefix_size = 0;

  g_assert (IDE_IS_XML_SERVICE (self));
  g_assert (analysis != NULL);
  g_assert (GTK_IS_TEXT_BUFFER (buffer));

  current_node = root_node = ide_xml_analysis_get_root_node (analysis);
  while (TRUE)
    {
loop:
      if (0 == (n_children = ide_xml_symbol_node_get_n_direct_children (current_node)))
        {
          child_pos = 0;
          goto result;
        }

      for (guint n = 0; n < n_children; ++n)
        {
          child_node = IDE_XML_SYMBOL_NODE (ide_xml_symbol_node_get_nth_direct_child (current_node, n));
          child_pos = n;
          rel_pos = ide_xml_symbol_node_compare_location (child_node, line, line_offset);

          switch (rel_pos)
            {
            case IDE_XML_SYMBOL_NODE_RELATIVE_POSITION_IN_START_TAG:
              candidate_node = child_node;
              candidate_kind = IDE_XML_POSITION_KIND_IN_START_TAG;
              goto result;

            case IDE_XML_SYMBOL_NODE_RELATIVE_POSITION_IN_END_TAG:
              candidate_node = child_node;
              candidate_kind = IDE_XML_POSITION_KIND_IN_END_TAG;
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
  /* Empty tree case */
  if (candidate_node == NULL)
    {
      candidate_node = root_node;
      candidate_kind = IDE_XML_POSITION_KIND_IN_CONTENT;
    }
  /* Node not closed case */
  else if (candidate_kind == IDE_XML_POSITION_KIND_IN_CONTENT)
    {
      if (previous_node != NULL &&
          ide_xml_symbol_node_get_state (previous_node) == IDE_XML_SYMBOL_NODE_STATE_NOT_CLOSED)
        {
          candidate_node = previous_node;
          /* TODO: detect the IN_END_TAG case */
          candidate_kind = IDE_XML_POSITION_KIND_IN_START_TAG;
        }
    }

  gtk_text_buffer_get_iter_at_line_index (buffer, &cursor, line - 1, line_offset - 1);

  if (candidate_kind == IDE_XML_POSITION_KIND_IN_CONTENT)
    {
      g_autofree gchar *content_prefix = NULL;

      start = cursor;
      while (gtk_text_iter_backward_char (&start))
        {
          gunichar ch = gtk_text_iter_get_char (&start);

          if (ch == '>' || g_unichar_isspace (ch))
            {
              gtk_text_iter_forward_char (&start);
              break;
            }
        }

      content_prefix = gtk_text_buffer_get_text (buffer, &start, &cursor, FALSE);
      detail = ide_xml_detail_new (NULL, NULL, content_prefix,
                                   IDE_XML_DETAIL_MEMBER_NONE,
                                   IDE_XML_DETAIL_SIDE_NONE,
                                   0);
      position = ide_xml_position_new (candidate_node,
                                       candidate_kind,
                                       detail);

      ide_xml_position_set_analysis (position, analysis);
      ide_xml_position_set_child_pos (position, child_pos);
    }
  else if (candidate_kind == IDE_XML_POSITION_KIND_IN_START_TAG ||
           candidate_kind == IDE_XML_POSITION_KIND_IN_END_TAG)
    {
      g_autofree gchar *content = NULL;
      g_autofree gchar *node_prefix = NULL;
      gsize content_len;
      gint order;

      if (candidate_kind == IDE_XML_POSITION_KIND_IN_START_TAG)
        ide_xml_symbol_node_get_location (candidate_node,
                                          &start_line,
                                          &start_line_offset,
                                          &end_line,
                                          &end_line_offset,
                                          NULL);
      else
        ide_xml_symbol_node_get_end_tag_location (candidate_node,
                                                  &start_line,
                                                  &start_line_offset,
                                                  &end_line,
                                                  &end_line_offset,
                                                  NULL);

      gtk_text_buffer_get_iter_at_line_index (buffer, &start, start_line - 1, start_line_offset - 1);
      gtk_text_buffer_get_iter_at_line_index (buffer, &end, end_line - 1, end_line_offset - 1);

      /* Exclude the opening '<' or '</' from the content */
      if (gtk_text_iter_get_char (&start) == '<')
        gtk_text_iter_forward_char (&start);

      if (candidate_kind == IDE_XML_POSITION_KIND_IN_END_TAG)
        {
          g_assert (gtk_text_iter_get_char (&start) == '/');
          gtk_text_iter_forward_char (&start);
        }

      /* We get the node's content and a pointer to the cursor position */
      insert_ptr = content = gtk_text_buffer_get_text (buffer, &start, &end, FALSE);

      /* Exclude the closing '>' or '/>' from the content */
      content_len = strlen (content);
      if (g_str_has_suffix (content, "/>"))
        content_len -= 2;
      else if (g_str_has_suffix (content, ">"))
        content_len -= 1;

      *(content + content_len) = '\0';

      order = gtk_text_iter_compare (&start, &cursor);
      if (order == -1)
        {
          node_prefix = gtk_text_buffer_get_text (buffer, &start, &cursor, FALSE);
          prefix_size = strlen (node_prefix);
          insert_ptr = content + prefix_size;
        }
      else if (order == 1)
        {
          /* Means we are in the end tag, juste before the '/',
           * we just tag this case by setting insert_ptr to NULL
           */
          g_assert (candidate_kind == IDE_XML_POSITION_KIND_IN_END_TAG);
          insert_ptr = NULL;
        }

      detail = get_detail (content, insert_ptr, candidate_kind);

      child_node = candidate_node;
      candidate_node = ide_xml_symbol_node_get_parent (child_node);

      position = ide_xml_position_new (candidate_node,
                                       candidate_kind,
                                       detail);

      ide_xml_position_set_analysis (position, analysis);
      ide_xml_position_set_child_node (position, child_node);
    }
  else
    g_assert_not_reached ();

  if (child_pos > 0)
    previous_sibling_node = IDE_XML_SYMBOL_NODE (ide_xml_symbol_node_get_nth_direct_child (candidate_node, child_pos - 1));

  if (child_pos < ide_xml_symbol_node_get_n_direct_children (candidate_node))
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
  IdeXmlAnalysis *analysis;
  g_autoptr(IdeTask) task = user_data;
  g_autoptr(GError) error = NULL;

  IDE_ENTRY;

  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (IDE_IS_XML_SERVICE (self));
  g_assert (G_IS_ASYNC_RESULT (result));
  g_assert (IDE_IS_TASK (task));

  if (!(analysis = ide_xml_service_get_analysis_finish (self, result, &error)))
    ide_task_return_error (task, g_steal_pointer (&error));
  else
    {
      PositionState *state = ide_task_get_task_data (task);
      IdeXmlPosition *position = get_position (self,
                                               analysis,
                                               GTK_TEXT_BUFFER (state->buffer),
                                               state->line,
                                               state->line_offset);

      ide_task_return_pointer (task,
                               position,
                               g_object_unref);
    }

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

  self->modversions = dzl_task_cache_new ((GHashFunc)g_str_hash,
                                          (GEqualFunc)g_str_equal,
                                          (GBoxedCopyFunc)g_strdup,
                                          (GBoxedFreeFunc)g_free,
                                          (GBoxedCopyFunc)g_strdup,
                                          (GBoxedFreeFunc)g_free,
                                          DEFAULT_EVICTION_MSEC,
                                          ide_xml_service_get_modversion_cb,
                                          self,
                                          NULL);

  dzl_task_cache_set_name (self->analyses, "pkgconfig modversion cache");

  /* TODO: populate async and track changes in:
   * - runtime/app for flatpak
   * - local typelib dir for host/jhbuild
   * - selected runtime
   */
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
  g_clear_object (&self->modversions);
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

  if ((analysis = dzl_task_cache_peek (self->analyses, gfile)) &&
      (cached = ide_xml_analysis_get_root_node (analysis)))
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

  if ((analysis = dzl_task_cache_peek (self->analyses, gfile)) &&
      (cached = ide_xml_analysis_get_diagnostics (analysis)))
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

static void
fetch_modversion_cb (GObject      *object,
                     GAsyncResult *result,
                     gpointer      user_data)
{
  DzlTaskCache *modversions = (DzlTaskCache *)object;
  g_autoptr(IdeTask) task = (IdeTask *)user_data;
  g_autoptr(GError) error = NULL;
  gchar *modversion;

  if (!(modversion = dzl_task_cache_get_finish (modversions, result, &error)))
    ide_task_return_error (task, g_steal_pointer (&error));
  else
    ide_task_return_pointer (task, modversion, g_free);
}

void
ide_xml_service_get_modversion_async (IdeXmlService       *self,
                                      const gchar         *runtime_name,
                                      const gchar         *lib,
                                      GCancellable        *cancellable,
                                      GAsyncReadyCallback  callback,
                                      gpointer             user_data)
{
  g_autofree gchar *key = NULL;
  IdeTask *task;

  g_return_if_fail (IDE_IS_XML_SERVICE (self));
  g_return_if_fail (!dzl_str_empty0 (runtime_name));
  g_return_if_fail (!dzl_str_empty0 (lib));
  g_return_if_fail (!cancellable || G_IS_CANCELLABLE (cancellable));

  task = ide_task_new (self, cancellable, callback, user_data);
  key = g_strconcat (runtime_name, "@", lib, NULL);

  dzl_task_cache_get_async (self->modversions,
                            key,
                            FALSE,
                            cancellable,
                            fetch_modversion_cb,
                            task);
}

gchar *
ide_xml_service_get_modversion_finish (IdeXmlService  *self,
                                       GAsyncResult   *result,
                                       GError        **error)
{
  IdeTask *task = (IdeTask *)result;

  g_return_val_if_fail (IDE_IS_XML_SERVICE (self), NULL);
  g_return_val_if_fail (G_IS_ASYNC_RESULT (result), NULL);

  return ide_task_propagate_pointer (task, error);
}
