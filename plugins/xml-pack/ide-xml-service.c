/* ide-xml-service.c
 *
 * Copyright (C) 2017 Sébastien Lafargue <slafargue@gnome.org>
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

#include <dazzle.h>
#include <glib/gi18n.h>
#include <gtksourceview/gtksource.h>
#include <math.h>

#include "ide-xml-analysis.h"
#include "ide-xml-service.h"
#include "ide-xml-tree-builder.h"

gboolean _ide_buffer_get_loading (IdeBuffer *self);

#define DEFAULT_EVICTION_MSEC (60 * 1000)

struct _IdeXmlService
{
  IdeObject          parent_instance;

  DzlTaskCache      *analyses;
  IdeXmlTreeBuilder *tree_builder;
  GCancellable      *cancellable;
};

static void service_iface_init (IdeServiceInterface *iface);

G_DEFINE_DYNAMIC_TYPE_EXTENDED (IdeXmlService, ide_xml_service, IDE_TYPE_OBJECT, 0,
                                G_IMPLEMENT_INTERFACE (IDE_TYPE_SERVICE, service_iface_init))

static void
ide_xml_service_build_tree_cb2 (GObject      *object,
                                GAsyncResult *result,
                                gpointer      user_data)
{
  IdeXmlTreeBuilder *tree_builder = (IdeXmlTreeBuilder *)object;
  g_autoptr(GTask) task = user_data;
  g_autoptr(IdeXmlAnalysis) analysis = NULL;
  GError *error = NULL;

  g_assert (IDE_IS_XML_TREE_BUILDER (tree_builder));
  g_assert (G_IS_TASK (result));
  g_assert (G_IS_TASK (task));

  if (NULL == (analysis = ide_xml_tree_builder_build_tree_finish (tree_builder, result, &error)))
    g_task_return_error (task, error);
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

static void
ide_xml_service_get_analysis_cb (GObject      *object,
                                 GAsyncResult *result,
                                 gpointer      user_data)
{
  DzlTaskCache *cache = (DzlTaskCache *)object;
  g_autoptr(GTask) task = user_data;
  g_autoptr(IdeXmlAnalysis) analysis = NULL;
  GError *error = NULL;

  g_assert (DZL_IS_TASK_CACHE (cache));
  g_assert (G_IS_TASK (result));
  g_assert (G_IS_TASK (task));

  if (NULL == (analysis = dzl_task_cache_get_finish (cache, result, &error)))
    g_task_return_error (task, error);
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

      if (!_ide_buffer_get_loading (buffer))
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
  GError *error = NULL;

  g_assert (IDE_IS_XML_SERVICE (self));
  g_assert (G_IS_TASK (result));
  g_assert (G_IS_TASK (task));

  if (NULL == (analysis = ide_xml_service_get_analysis_finish (self, result, &error)))
    g_task_return_error (task, error);
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
  GError *error = NULL;

  g_assert (IDE_IS_XML_SERVICE (self));
  g_assert (G_IS_TASK (result));
  g_assert (G_IS_TASK (task));

  if (NULL == (analysis = ide_xml_service_get_analysis_finish (self, result, &error)))
    g_task_return_error (task, error);
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

  g_return_if_fail (IDE_IS_XML_SERVICE (self));
  g_return_if_fail (IDE_IS_FILE (ifile));
  g_return_if_fail (IDE_IS_BUFFER (buffer) || buffer == NULL);
  g_return_if_fail (!cancellable || G_IS_CANCELLABLE (cancellable));

  task = g_task_new (self, cancellable, callback, user_data);

  /*
   * If we have a cached analysis with some diagnostics,
   * and it is new enough, then re-use it.
   */
  if (NULL != (cached = dzl_task_cache_peek (self->analyses, ifile)))
    {
      IdeContext *context;
      IdeUnsavedFiles *unsaved_files;
      IdeUnsavedFile *uf;
      IdeDiagnostics *diagnostics;
      GFile *gfile;

      gfile = ide_file_get_file (ifile);
      context = ide_object_get_context (IDE_OBJECT (self));
      unsaved_files = ide_context_get_unsaved_files (context);

      if (NULL != (uf = ide_unsaved_files_get_unsaved_file (unsaved_files, gfile)) &&
          ide_xml_analysis_get_sequence (cached) == ide_unsaved_file_get_sequence (uf))
        {
          diagnostics = ide_diagnostics_ref (ide_xml_analysis_get_diagnostics (cached));
          g_assert (diagnostics != NULL);

          g_task_return_pointer (task, diagnostics, (GDestroyNotify)ide_diagnostics_unref);
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
  GTask *task = (GTask *)result;

  g_return_val_if_fail (IDE_IS_XML_SERVICE (self), NULL);
  g_return_val_if_fail (G_IS_TASK (result), NULL);
  g_return_val_if_fail (error != NULL, NULL);

  return g_task_propagate_pointer (task, error);
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
ide_xml_service_class_finalize (IdeXmlServiceClass *klass)
{
}

static void
ide_xml_service_init (IdeXmlService *self)
{
}

void
_ide_xml_service_register_type (GTypeModule *module)
{
  ide_xml_service_register_type (module);
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
 * Returns: (transfer NULL): A #IdeDiagnostics.
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
