/* ide-code-index-service.c
 *
 * Copyright © 2017 Anoop Chandu <anoopchandu96@gmail.com>
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

#define G_LOG_DOMAIN "ide-code-index-service"

#include <glib/gi18n.h>
#include <libpeas/peas.h>
#include <plugins/ide-extension-adapter.h>
#include <stdlib.h>

#include "ide-code-index-service.h"
#include "ide-code-index-builder.h"

#define DEFAULT_INDEX_TIMEOUT_SECS 5
#define MAX_TRIALS 3

/*
 * This is a start and stop service which monitors file changes and
 * reindexes directories using IdeCodeIndexBuilder.
 */
struct _IdeCodeIndexService
{
  IdeObject               parent;

  /* The builder to build & update index */
  IdeCodeIndexBuilder    *builder;

  /* The Index which will store all declarations */
  IdeCodeIndexIndex      *index;

  /* Queue of directories which needs to be indexed */
  GQueue                  build_queue;
  GHashTable             *build_dirs;

  GHashTable             *code_indexers;

  IdePausable            *pausable;
  GCancellable           *cancellable;

  guint                   stopped : 1;
  guint                   delayed_build_reqeusted : 1;
};

typedef struct
{
  volatile gint        ref_count;
  IdeCodeIndexService *self;
  GFile               *directory;
  guint                n_trial;
  guint                recursive : 1;
} BuildData;

static void service_iface_init           (IdeServiceInterface  *iface);
static void ide_code_index_service_build (IdeCodeIndexService  *self,
                                          GFile                *directory,
                                          gboolean              recursive,
                                          guint                 n_trial);

G_DEFINE_TYPE_EXTENDED (IdeCodeIndexService, ide_code_index_service, IDE_TYPE_OBJECT, 0,
                        G_IMPLEMENT_INTERFACE (IDE_TYPE_SERVICE, service_iface_init))

static void
build_data_unref (BuildData *data)
{
  g_assert (data != NULL);
  g_assert (data->ref_count > 0);

  if (g_atomic_int_dec_and_test (&data->ref_count))
    {
      g_clear_object (&data->self);
      g_clear_object (&data->directory);
      g_slice_free (BuildData, data);
    }
}

static BuildData *
build_data_ref (BuildData *data)
{
  g_assert (data != NULL);
  g_assert (data->ref_count > 0);
  g_atomic_int_inc (&data->ref_count);
  return data;
}

G_DEFINE_AUTOPTR_CLEANUP_FUNC (BuildData, build_data_unref)

static void
remove_source (gpointer source_id)
{
  if (source_id != NULL)
    g_source_remove (GPOINTER_TO_UINT (source_id));
}

static void
register_pausable (IdeCodeIndexService *self)
{
  IdeContext *context;

  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (IDE_IS_CODE_INDEX_SERVICE (self));

  context = ide_object_get_context (IDE_OBJECT (self));
  g_assert (!context || IDE_IS_CONTEXT (context));

  if (context != NULL && self->pausable != NULL)
    ide_context_add_pausable (context, self->pausable);
}

static void
unregister_pausable (IdeCodeIndexService *self)
{
  IdeContext *context;

  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (IDE_IS_CODE_INDEX_SERVICE (self));

  context = ide_object_get_context (IDE_OBJECT (self));
  g_assert (!context || IDE_IS_CONTEXT (context));

  if (context != NULL && self->pausable != NULL)
    ide_context_remove_pausable (context, self->pausable);
}

static gboolean
delay_until_build_completes (IdeCodeIndexService *self)
{
  IdeBuildPipeline *pipeline;
  IdeBuildManager *build_manager;
  IdeContext *context;

  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (IDE_IS_CODE_INDEX_SERVICE (self));

  if (self->delayed_build_reqeusted)
    return TRUE;

  context = ide_object_get_context (IDE_OBJECT (self));
  g_assert (IDE_IS_CONTEXT (context));

  build_manager = ide_context_get_build_manager (context);
  g_assert (IDE_IS_BUILD_MANAGER (build_manager));

  pipeline = ide_build_manager_get_pipeline (build_manager);
  g_assert (IDE_IS_BUILD_PIPELINE (pipeline));

  if (pipeline == NULL || !ide_build_pipeline_has_configured (pipeline))
    {
      self->delayed_build_reqeusted = TRUE;
      return TRUE;
    }

  return FALSE;
}

static void
ide_code_index_service_build_cb (GObject      *object,
                                 GAsyncResult *result,
                                 gpointer      user_data)
{
  g_autoptr(IdeCodeIndexService) self = user_data;
  IdeCodeIndexBuilder *builder = (IdeCodeIndexBuilder *)object;
  g_autoptr(BuildData) bdata = NULL;
  g_autoptr(GError) error = NULL;

  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (IDE_IS_CODE_INDEX_SERVICE (self));
  g_assert (G_IS_ASYNC_RESULT (result));
  g_assert (IDE_IS_CODE_INDEX_BUILDER (builder));

  if (ide_code_index_builder_build_finish (builder, result, &error))
    g_debug ("Finished building code index");
  else if (!g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CANCELLED))
    g_warning ("Failed to build code index: %s", error->message);

  if (self->stopped)
    return;

  bdata = g_queue_pop_head (&self->build_queue);

  /*
   * If we're paused, push this item back on the queue to
   * be processed when we unpause.
   */
  if (ide_pausable_get_paused (self->pausable))
    {
      g_queue_push_head (&self->build_queue, g_steal_pointer (&bdata));
      return;
    }

  if (error != NULL &&
      !g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CANCELLED))
    {
      ide_code_index_service_build (self,
                                    bdata->directory,
                                    bdata->recursive,
                                    bdata->n_trial + 1);
    }

  /* Index next directory */
  if (!g_queue_is_empty (&self->build_queue))
    {
      BuildData *peek = g_queue_peek_head (&self->build_queue);

      g_clear_object (&self->cancellable);
      self->cancellable = g_cancellable_new ();

      ide_code_index_builder_build_async (builder,
                                          peek->directory,
                                          peek->recursive,
                                          self->cancellable,
                                          ide_code_index_service_build_cb,
                                          g_object_ref (self));
    }
  else
    {
      unregister_pausable (self);
    }
}

static gboolean
ide_code_index_serivce_push (BuildData *bdata)
{
  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (bdata != NULL);
  g_assert (IDE_IS_CODE_INDEX_SERVICE (bdata->self));
  g_assert (G_IS_FILE (bdata->directory));

  if (g_queue_is_empty (&bdata->self->build_queue))
    {
      g_queue_push_tail (&bdata->self->build_queue, build_data_ref (bdata));

      g_clear_object (&bdata->self->cancellable);
      bdata->self->cancellable = g_cancellable_new ();

      register_pausable (bdata->self);

      ide_code_index_builder_build_async (bdata->self->builder,
                                          bdata->directory,
                                          bdata->recursive,
                                          bdata->self->cancellable,
                                          ide_code_index_service_build_cb,
                                          g_object_ref (bdata->self));
    }
  else
    {
      g_queue_push_tail (&bdata->self->build_queue, build_data_ref (bdata));
    }

  if (bdata->self->build_dirs != NULL)
    g_hash_table_remove (bdata->self->build_dirs, bdata->directory);

  return G_SOURCE_REMOVE;
}

static void
ide_code_index_service_build (IdeCodeIndexService *self,
                              GFile               *directory,
                              gboolean             recursive,
                              guint                n_trial)
{
  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (IDE_IS_CODE_INDEX_SERVICE (self));
  g_assert (G_IS_FILE (directory));

  if (n_trial > MAX_TRIALS)
    return;

  /*
   * If the build system is currently failed, then don't try to
   * do any indexing now. We'll wait for a successful build that
   * at least reaches IDE_BUILD_PHASE_CONFIGURE and then trigger
   * after that.
   */
  if (delay_until_build_completes (self))
    return;

  if (!g_hash_table_lookup (self->build_dirs, directory))
    {
      g_autoptr(BuildData) bdata = NULL;
      guint source_id;

      bdata = g_slice_new0 (BuildData);
      bdata->ref_count = 1;
      bdata->self = g_object_ref (self);
      bdata->directory = g_object_ref (directory);
      bdata->recursive = recursive;
      bdata->n_trial = n_trial;

      source_id = g_timeout_add_seconds_full (G_PRIORITY_LOW,
                                              DEFAULT_INDEX_TIMEOUT_SECS,
                                              (GSourceFunc) ide_code_index_serivce_push,
                                              g_steal_pointer (&bdata),
                                              (GDestroyNotify) build_data_unref);

      g_hash_table_insert (self->build_dirs,
                           g_object_ref (directory),
                           GUINT_TO_POINTER (source_id));
    }
}

static void
ide_code_index_service_vcs_changed (IdeCodeIndexService *self,
                                    IdeVcs              *vcs)
{
  GFile *workdir;

  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (IDE_IS_CODE_INDEX_SERVICE (self));
  g_assert (IDE_IS_VCS (vcs));

  workdir = ide_vcs_get_working_directory (vcs);
  ide_code_index_service_build (self, workdir, TRUE, 1);
}

static void
ide_code_index_service_buffer_saved (IdeCodeIndexService *self,
                                     IdeBuffer           *buffer,
                                     IdeBufferManager    *buffer_manager)
{
  GFile *file;
  g_autofree gchar *file_name = NULL;

  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (IDE_IS_CODE_INDEX_SERVICE (self));
  g_assert (IDE_IS_BUFFER (buffer));

  file = ide_file_get_file (ide_buffer_get_file (buffer));
  file_name = g_file_get_uri (file);

  if (NULL != ide_code_index_service_get_code_indexer (self, file_name))
    {
      g_autoptr(GFile) parent = NULL;

      parent = g_file_get_parent (file);
      ide_code_index_service_build (self, parent, FALSE, 1);
    }
}

static void
ide_code_index_service_file_trashed (IdeCodeIndexService *self,
                                     GFile               *file,
                                     IdeProject          *project)
{
  g_autofree gchar *file_name = NULL;

  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (IDE_IS_CODE_INDEX_SERVICE (self));
  g_assert (G_IS_FILE (file));

  file_name = g_file_get_uri (file);

  if (NULL != ide_code_index_service_get_code_indexer (self, file_name))
    {
      g_autoptr(GFile) parent = NULL;

      parent = g_file_get_parent (file);
      ide_code_index_service_build (self, parent, FALSE, 1);
    }
}

static void
ide_code_index_service_file_renamed (IdeCodeIndexService *self,
                                     GFile               *src_file,
                                     GFile               *dst_file,
                                     IdeProject          *project)
{
  g_autofree gchar *src_file_name = NULL;
  g_autofree gchar *dst_file_name = NULL;
  g_autoptr(GFile) src_parent = NULL;
  g_autoptr(GFile) dst_parent = NULL;

  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (IDE_IS_CODE_INDEX_SERVICE (self));
  g_assert (G_IS_FILE (src_file));
  g_assert (G_IS_FILE (dst_file));

  src_file_name = g_file_get_uri (src_file);
  dst_file_name = g_file_get_uri (dst_file);

  src_parent = g_file_get_parent (src_file);
  dst_parent = g_file_get_parent (dst_file);

  if (g_file_equal (src_parent, dst_parent))
    {
      if (NULL != ide_code_index_service_get_code_indexer (self, src_file_name) ||
          NULL != ide_code_index_service_get_code_indexer (self, dst_file_name))
        ide_code_index_service_build (self, src_parent, FALSE, 1);
    }
  else
    {
      if (NULL != ide_code_index_service_get_code_indexer (self, src_file_name))
        ide_code_index_service_build (self, src_parent, FALSE, 1);

      if (NULL != ide_code_index_service_get_code_indexer (self, dst_file_name))
        ide_code_index_service_build (self, dst_parent, FALSE, 1);
    }
}

static void
ide_code_index_service_build_finished (IdeCodeIndexService *self,
                                       IdeBuildPipeline    *pipeline,
                                       IdeBuildManager     *build_manager)
{
  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (IDE_IS_CODE_INDEX_SERVICE (self));
  g_assert (IDE_IS_BUILD_PIPELINE (pipeline));
  g_assert (IDE_IS_BUILD_MANAGER (build_manager));

  if (self->delayed_build_reqeusted &&
      ide_build_pipeline_has_configured (pipeline))
    {
      IdeContext *context;
      IdeVcs *vcs;
      GFile *workdir;

      self->delayed_build_reqeusted = FALSE;

      context = ide_object_get_context (IDE_OBJECT (self));
      vcs = ide_context_get_vcs (context);
      workdir = ide_vcs_get_working_directory (vcs);

      ide_code_index_service_build (self, workdir, TRUE, 1);
    }
}

static void
ide_code_index_service_context_loaded (IdeService *service)
{
  IdeCodeIndexService *self = (IdeCodeIndexService *)service;
  IdeBufferManager *bufmgr;
  IdeBuildManager *buildmgr;
  IdeContext *context;
  IdeProject *project;
  IdeVcs *vcs;
  GFile *workdir;

  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (IDE_IS_CODE_INDEX_SERVICE (self));

  context = ide_object_get_context (IDE_OBJECT (self));
  project = ide_context_get_project (context);
  bufmgr = ide_context_get_buffer_manager (context);
  buildmgr = ide_context_get_build_manager (context);
  vcs = ide_context_get_vcs (context);
  workdir = ide_vcs_get_working_directory (vcs);

  self->code_indexers = g_hash_table_new_full (NULL, NULL, NULL, g_object_unref);
  self->index = ide_code_index_index_new (context);
  self->builder = ide_code_index_builder_new (context, self, self->index);
  self->build_dirs = g_hash_table_new_full (g_file_hash,
                                            (GEqualFunc)g_file_equal,
                                            g_object_unref,
                                            remove_source);

  g_signal_connect_object (vcs,
                           "changed",
                           G_CALLBACK (ide_code_index_service_vcs_changed),
                           self,
                           G_CONNECT_SWAPPED);

  g_signal_connect_object (bufmgr,
                           "buffer-saved",
                           G_CALLBACK (ide_code_index_service_buffer_saved),
                           self,
                           G_CONNECT_SWAPPED);

  g_signal_connect_object (buildmgr,
                           "build-finished",
                           G_CALLBACK (ide_code_index_service_build_finished),
                           self,
                           G_CONNECT_SWAPPED);

  g_signal_connect_object (project,
                           "file-trashed",
                           G_CALLBACK (ide_code_index_service_file_trashed),
                           self,
                           G_CONNECT_SWAPPED);

  g_signal_connect_object (project,
                           "file-renamed",
                           G_CALLBACK (ide_code_index_service_file_renamed),
                           self,
                           G_CONNECT_SWAPPED);

  ide_code_index_service_build (self, workdir, TRUE, 1);
}

static void
ide_code_index_service_start (IdeService *service)
{
  IdeCodeIndexService *self = (IdeCodeIndexService *)service;

  self->stopped = FALSE;
}

static void
ide_code_index_service_paused (IdeCodeIndexService *self,
                               IdePausable         *pausable)
{
  g_assert (IDE_IS_CODE_INDEX_SERVICE (self));
  g_assert (IDE_IS_PAUSABLE (pausable));

  if (self->stopped)
    return;

  /*
   * To pause things, we need to cancel our current task. The completion of
   * the async task will check for cancelled and leave the build task for
   * another pass.
   */

  g_cancellable_cancel (self->cancellable);
}

static void
ide_code_index_service_unpaused (IdeCodeIndexService *self,
                                 IdePausable         *pausable)
{
  BuildData *peek;

  g_assert (IDE_IS_CODE_INDEX_SERVICE (self));
  g_assert (IDE_IS_PAUSABLE (pausable));

  if (self->stopped)
    return;

  peek = g_queue_peek_head (&self->build_queue);

  if (peek != NULL)
    {
      GCancellable *cancellable;

      g_clear_object (&self->cancellable);
      self->cancellable = cancellable = g_cancellable_new ();

      ide_code_index_builder_build_async (self->builder,
                                          peek->directory,
                                          peek->recursive,
                                          cancellable,
                                          ide_code_index_service_build_cb,
                                          g_object_ref (self));
    }
}

static void
ide_code_index_service_stop (IdeService *service)
{
  IdeCodeIndexService *self = (IdeCodeIndexService *)service;

  g_cancellable_cancel (self->cancellable);
  g_clear_object (&self->cancellable);

  self->stopped = TRUE;

  g_clear_object (&self->index);
  g_clear_object (&self->builder);
  g_queue_foreach (&self->build_queue, (GFunc)build_data_unref, NULL);
  g_queue_clear (&self->build_queue);
  g_clear_pointer (&self->build_dirs, g_hash_table_unref);
  g_clear_pointer (&self->code_indexers, g_hash_table_unref);

  unregister_pausable (self);
}

static void
ide_code_index_service_finalize (GObject *object)
{
  IdeCodeIndexService *self = (IdeCodeIndexService *)object;

  g_assert (self->stopped == TRUE);
  g_assert (self->index == NULL);
  g_assert (self->builder == NULL);
  g_assert (self->build_queue.length == 0);
  g_assert (self->build_dirs == NULL);
  g_assert (self->code_indexers== NULL);

  g_clear_object (&self->pausable);

  G_OBJECT_CLASS (ide_code_index_service_parent_class)->finalize (object);
}

static void
ide_code_index_service_class_init (IdeCodeIndexServiceClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = ide_code_index_service_finalize;
}

static void
service_iface_init (IdeServiceInterface *iface)
{
  iface->start = ide_code_index_service_start;
  iface->context_loaded = ide_code_index_service_context_loaded;
  iface->stop = ide_code_index_service_stop;
}

static void
ide_code_index_service_init (IdeCodeIndexService *self)
{
  self->pausable = g_object_new (IDE_TYPE_PAUSABLE,
                                 "paused", FALSE,
                                 "title", _("Indexing Source Code"),
                                 "subtitle", _("Search, diagnostics and autocompletion may be limited until complete."),
                                 NULL);

  g_signal_connect_object (self->pausable,
                           "paused",
                           G_CALLBACK (ide_code_index_service_paused),
                           self,
                           G_CONNECT_SWAPPED);

  g_signal_connect_object (self->pausable,
                           "unpaused",
                           G_CALLBACK (ide_code_index_service_unpaused),
                           self,
                           G_CONNECT_SWAPPED);
}

IdeCodeIndexIndex *
ide_code_index_service_get_index (IdeCodeIndexService *self)
{
  g_return_val_if_fail (IDE_IS_MAIN_THREAD (), NULL);
  g_return_val_if_fail (IDE_IS_CODE_INDEX_SERVICE (self), NULL);

  return self->index;
}

/**
 * ide_code_index_service_get_code_indexer:
 * @self: a #IdeCodeIndexService
 * @file_name: the name of the file to index
 *
 * Gets an #IdeCodeIndexer suitable for @file_name.
 *
 * Returns: (transfer none) (nullable): an #IdeCodeIndexer or %NULL
 *
 * Since: 3.26
 */
IdeCodeIndexer *
ide_code_index_service_get_code_indexer (IdeCodeIndexService *self,
                                         const gchar         *file_name)
{
  GtkSourceLanguageManager *manager;
  IdeExtensionAdapter *adapter;
  GtkSourceLanguage *language;
  const gchar *lang_id;

  g_return_val_if_fail (IDE_IS_MAIN_THREAD (), NULL);
  g_return_val_if_fail (IDE_IS_CODE_INDEX_SERVICE (self), NULL);
  g_return_val_if_fail (file_name != NULL, NULL);

  if (self->code_indexers == NULL ||
      !(manager = gtk_source_language_manager_get_default ()) ||
      !(language = gtk_source_language_manager_guess_language (manager, file_name, NULL)) ||
      !(lang_id = gtk_source_language_get_id (language)))
    return NULL;

  lang_id = g_intern_string (lang_id);
  adapter = g_hash_table_lookup (self->code_indexers, lang_id);

  g_assert (!adapter || IDE_IS_EXTENSION_ADAPTER (adapter));

  if (adapter == NULL)
    {
      IdeContext *context = ide_object_get_context (IDE_OBJECT (self));

      adapter = ide_extension_adapter_new (context,
                                           peas_engine_get_default (),
                                           IDE_TYPE_CODE_INDEXER,
                                           "Code-Indexer-Languages",
                                           lang_id);
      g_hash_table_insert (self->code_indexers, (gchar *)lang_id, adapter);
    }

  g_assert (IDE_IS_EXTENSION_ADAPTER (adapter));

  return ide_extension_adapter_get_extension (adapter);
}
