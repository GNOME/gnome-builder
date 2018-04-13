/* ide-ctags-service.c
 *
 * Copyright 2015 Christian Hergert <christian@hergert.me>
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

#define G_LOG_DOMAIN "ide-ctags-service"

#include <dazzle.h>
#include <glib/gi18n.h>
#include <gtksourceview/gtksource.h>

#include "ide-ctags-builder.h"
#include "ide-ctags-completion-provider.h"
#include "ide-ctags-highlighter.h"
#include "ide-ctags-index.h"
#include "ide-ctags-service.h"

#define QUEUED_BUILD_TIMEOUT_SECS 5

struct _IdeCtagsService
{
  IdeObject         parent_instance;

  DzlTaskCache     *indexes;
  GCancellable     *cancellable;
  GPtrArray        *highlighters;
  GPtrArray        *completions;
  GHashTable       *build_timeout_by_dir;

  guint             queued_miner_handler;
  guint             miner_active : 1;
};

typedef struct
{
  gchar *path;
  guint  recursive;
} MineInfo;

typedef struct
{
  IdeCtagsService *self;
  GFile           *directory;
  gboolean         recursive;
} QueuedRequest;

static void service_iface_init (IdeServiceInterface *iface);

G_DEFINE_DYNAMIC_TYPE_EXTENDED (IdeCtagsService, ide_ctags_service, IDE_TYPE_OBJECT, 0,
                                G_IMPLEMENT_INTERFACE (IDE_TYPE_SERVICE, service_iface_init))

static void
queued_request_free (gpointer data)
{
  QueuedRequest *qr = data;

  g_clear_object (&qr->self);
  g_clear_object (&qr->directory);
  g_slice_free (QueuedRequest, qr);
}

static void
ide_ctags_service_build_index_init_cb (GObject      *object,
                                       GAsyncResult *result,
                                       gpointer      user_data)
{
  IdeCtagsIndex *index = (IdeCtagsIndex *)object;
  g_autoptr(GTask) task = user_data;
  g_autoptr(GError) error = NULL;

  g_assert (IDE_IS_CTAGS_INDEX (index));
  g_assert (G_IS_TASK (task));

  if (!g_async_initable_init_finish (G_ASYNC_INITABLE (index), result, &error))
    g_task_return_error (task, g_steal_pointer (&error));
  else if (ide_ctags_index_get_is_empty (index))
    g_task_return_new_error (task,
                             G_IO_ERROR,
                             G_IO_ERROR_NONE,
                             "tags file is empty");
  else
    g_task_return_pointer (task, g_object_ref (index), g_object_unref);
}

static guint64
get_file_mtime (GFile *file)
{
  g_autoptr(GFileInfo) info = NULL;
  g_autofree gchar *path = NULL;

  if ((info = g_file_query_info (file, G_FILE_ATTRIBUTE_TIME_MODIFIED,
                                 G_FILE_QUERY_INFO_NONE, NULL, NULL)))
    return g_file_info_get_attribute_uint64 (info, G_FILE_ATTRIBUTE_TIME_MODIFIED);

  path = g_file_get_uri (file);
  g_warning ("Failed to get mtime for %s", path);

  return 0;
}

static gchar *
resolve_path_root (IdeCtagsService *self,
                   GFile           *file)
{
  g_autoptr(GFile) parent = NULL;
  g_autoptr(GFile) cache_file = NULL;
  IdeContext *context;
  IdeVcs *vcs;
  GFile *workdir;

  context = ide_object_get_context (IDE_OBJECT (self));
  vcs = ide_context_get_vcs (context);
  workdir = ide_vcs_get_working_directory (vcs);
  parent = g_file_get_parent (file);

  /*
   * If we are inside the local cache dir, we are relative to the project
   * working directory.
   */
  cache_file = ide_context_cache_file (context, "ctags", NULL);
  if (g_file_has_prefix (file, cache_file) || g_file_equal (file, cache_file))
    {
      g_autofree gchar *relative = g_file_get_relative_path (cache_file, parent);

      if (relative != NULL)
        {
          g_autoptr(GFile) child = g_file_get_child (workdir, relative);
          return g_file_get_path (child);
        }

      return g_file_get_path (workdir);
    }

  if (g_file_has_prefix (file, workdir) || g_file_equal (file, workdir))
    {
      g_autofree gchar *relative = g_file_get_relative_path (workdir, parent);

      if (relative != NULL)
        {
          g_autoptr(GFile) child = g_file_get_child (workdir, relative);
          return g_file_get_path (child);
        }

      return g_file_get_path (workdir);
    }

  /* Else, we are relative to the parent of the tags file. */

  return g_file_get_path (parent);
}

static void
ide_ctags_service_build_index_cb (DzlTaskCache  *cache,
                                  gconstpointer  key,
                                  GTask         *task,
                                  gpointer       user_data)
{
  IdeCtagsService *self = user_data;
  g_autoptr(IdeCtagsIndex) index = NULL;
  GFile *file = (GFile *)key;
  g_autofree gchar *path_root = NULL;

  IDE_ENTRY;

  g_assert (IDE_IS_CTAGS_SERVICE (self));
  g_assert (key != NULL);
  g_assert (G_IS_FILE (key));
  g_assert (G_IS_TASK (task));

  path_root = resolve_path_root (self, file);
  index = ide_ctags_index_new (file, path_root, get_file_mtime (file));

#ifdef IDE_ENABLE_TRACE
  {
    g_autofree gchar *uri = g_file_get_uri (file);
    IDE_TRACE_MSG ("Building ctags in memory index for %s", uri);
  }
#endif

  g_async_initable_init_async (G_ASYNC_INITABLE (index),
                               G_PRIORITY_DEFAULT,
                               g_task_get_cancellable (task),
                               ide_ctags_service_build_index_init_cb,
                               g_object_ref (task));

  IDE_EXIT;
}

static void
ide_ctags_service_tags_loaded_cb (GObject      *object,
                                  GAsyncResult *result,
                                  gpointer      user_data)
{
  DzlTaskCache *cache = (DzlTaskCache *)object;
  g_autoptr(IdeCtagsService) self = user_data;
  g_autoptr(IdeCtagsIndex) index = NULL;
  GError *error = NULL;

  IDE_ENTRY;

  g_assert (DZL_IS_TASK_CACHE (cache));
  g_assert (IDE_IS_CTAGS_SERVICE (self));

  if (!(index = dzl_task_cache_get_finish (cache, result, &error)))
    {
      /* don't log if it was an empty file */
      if (!g_error_matches (error, G_IO_ERROR, G_IO_ERROR_NONE))
        g_debug ("%s", error->message);

      g_clear_error (&error);

      IDE_EXIT;
    }

  g_assert (IDE_IS_CTAGS_INDEX (index));

  for (guint i = 0; i < self->highlighters->len; i++)
    {
      IdeCtagsHighlighter *highlighter = g_ptr_array_index (self->highlighters, i);
      ide_ctags_highlighter_add_index (highlighter, index);
    }

  for (guint i = 0; i < self->completions->len; i++)
    {
      IdeCtagsCompletionProvider *provider = g_ptr_array_index (self->completions, i);
      ide_ctags_completion_provider_add_index (provider, index);
    }

  IDE_EXIT;
}

static gboolean
file_is_newer (IdeCtagsIndex *index,
               GFile         *file)
{
  guint64 file_mtime;
  guint64 ctags_mtime;

  g_assert (IDE_IS_CTAGS_INDEX (index));
  g_assert (G_IS_FILE (file));

  file_mtime = get_file_mtime (file);
  ctags_mtime = ide_ctags_index_get_mtime (index);

  return file_mtime > ctags_mtime;
}

static gboolean
do_load (gpointer data)
{
  IdeCtagsIndex *prev;
  struct {
    IdeCtagsService *self;
    GFile *file;
  } *pair = data;

  if ((prev = dzl_task_cache_peek (pair->self->indexes, pair->file)))
    {
      if (!file_is_newer (prev, pair->file))
        goto cleanup;
    }

  dzl_task_cache_get_async (pair->self->indexes,
                            pair->file,
                            TRUE,
                            pair->self->cancellable,
                            ide_ctags_service_tags_loaded_cb,
                            g_object_ref (pair->self));

cleanup:
  g_object_unref (pair->self);
  g_object_unref (pair->file);
  g_slice_free1 (sizeof *pair, pair);

  return G_SOURCE_REMOVE;
}

static void
ide_ctags_service_load_tags (IdeCtagsService *self,
                             GFile           *file)
{
  struct {
    IdeCtagsService *self;
    GFile *file;
  } *pair;

  g_assert (IDE_IS_CTAGS_SERVICE (self));
  g_assert (G_IS_FILE (file));

  pair = g_slice_alloc0 (sizeof *pair);
  pair->self = g_object_ref (self);
  pair->file = g_object_ref (file);
  g_idle_add_full (G_PRIORITY_LOW + 100, do_load, pair, NULL);
}

static void
ide_ctags_service_mine_directory (IdeCtagsService *self,
                                  IdeVcs          *vcs,
                                  GFile           *directory,
                                  gboolean         recurse,
                                  GCancellable    *cancellable)
{
  GFileEnumerator *enumerator = NULL;
  gpointer infoptr;
  GFile *child;

  g_assert (IDE_IS_CTAGS_SERVICE (self));
  g_assert (G_IS_FILE (directory));
  g_assert (!cancellable || G_IS_CANCELLABLE (cancellable));

  if (g_cancellable_is_cancelled (cancellable))
    return;

  if (ide_vcs_is_ignored (vcs, directory, NULL))
    return;

  child = g_file_get_child (directory, "tags");
  if (g_file_query_file_type (child, 0, cancellable) == G_FILE_TYPE_REGULAR)
    ide_ctags_service_load_tags (self, child);
  g_clear_object (&child);

  child = g_file_get_child (directory, ".tags");
  if (g_file_query_file_type (child, 0, cancellable) == G_FILE_TYPE_REGULAR)
    ide_ctags_service_load_tags (self, child);
  g_clear_object (&child);

  if (!recurse)
    return;

  enumerator = g_file_enumerate_children (directory,
                                          G_FILE_ATTRIBUTE_STANDARD_IS_SYMLINK","
                                          G_FILE_ATTRIBUTE_STANDARD_NAME","
                                          G_FILE_ATTRIBUTE_STANDARD_TYPE,
                                          G_FILE_QUERY_INFO_NONE,
                                          cancellable,
                                          NULL);

  if (!enumerator)
    return;

  while ((infoptr = g_file_enumerator_next_file (enumerator, cancellable, NULL)))
    {
      g_autoptr(GFileInfo) file_info = infoptr;
      GFileType type = g_file_info_get_file_type (file_info);

      if (g_file_info_get_is_symlink (file_info))
        continue;

      if (type == G_FILE_TYPE_DIRECTORY)
        {
          const gchar *name = g_file_info_get_name (file_info);

          child = g_file_get_child (directory, name);
          ide_ctags_service_mine_directory (self, vcs, child, recurse, cancellable);
          g_clear_object (&child);
        }
    }

  g_file_enumerator_close (enumerator, cancellable, NULL);

  g_object_unref (enumerator);
}

static void
ide_ctags_service_miner (GTask        *task,
                         gpointer      source_object,
                         gpointer      task_data,
                         GCancellable *cancellable)
{
  IdeCtagsService *self = source_object;
  IdeContext *context;
  IdeVcs *vcs;
  GArray *mine_info = task_data;

  IDE_ENTRY;

  g_assert (G_IS_TASK (task));
  g_assert (IDE_IS_CTAGS_SERVICE (self));
  g_assert (mine_info != NULL);

  context = ide_object_get_context (IDE_OBJECT (self));
  vcs = ide_context_get_vcs (context);

  for (guint i = 0; i < mine_info->len; i++)
    {
      const MineInfo *info = &g_array_index (mine_info, MineInfo, i);
      g_autoptr(GFile) file = g_file_new_for_path (info->path);

      ide_ctags_service_mine_directory (self, vcs, file, info->recursive, cancellable);
    }

  self->miner_active = FALSE;

  IDE_EXIT;
}

static void
clear_mine_info (gpointer data)
{
  MineInfo *info = data;

  g_free (info->path);
}

static gboolean
ide_ctags_service_do_mine (gpointer data)
{
  IdeCtagsService *self = data;
  g_autoptr(GTask) task = NULL;
  g_autoptr(GArray) mine_info = NULL;
  g_autofree gchar *path = NULL;
  IdeContext *context;
  MineInfo info;
  GFile *workdir;

  IDE_ENTRY;

  g_assert (IDE_IS_CTAGS_SERVICE (self));

  self->queued_miner_handler = 0;
  self->miner_active = TRUE;

  context = ide_object_get_context (IDE_OBJECT (self));
  workdir = ide_vcs_get_working_directory (ide_context_get_vcs (context));

  mine_info = g_array_new (FALSE, FALSE, sizeof (MineInfo));
  g_array_set_clear_func (mine_info, clear_mine_info);

  /* mine: ~/.cache/gnome-builder/projects/$project_id/ctags/ */
  info.path = ide_context_cache_filename (context, "ctags", NULL);
  info.recursive = TRUE;
  g_array_append_val (mine_info, info);

#if 0
  /* mine: ~/.tags */
  info.path = g_strdup (g_get_home_dir ());
  info.recursive = FALSE;
  g_array_append_val (mine_info, info);
#endif

  /* mine the project tree */
  info.path = g_file_get_path (workdir);
  info.recursive = TRUE;
  g_array_append_val (mine_info, info);

  task = g_task_new (self, NULL, NULL, NULL);
  g_task_set_source_tag (task, ide_ctags_service_do_mine);
  g_task_set_priority (task, G_PRIORITY_LOW + 1000);
  g_task_set_task_data (task, g_steal_pointer (&mine_info), (GDestroyNotify)g_array_unref);
  ide_thread_pool_push_task (IDE_THREAD_POOL_INDEXER, task, ide_ctags_service_miner);

  IDE_RETURN (G_SOURCE_REMOVE);
}

static void
ide_ctags_service_queue_mine (IdeCtagsService *self)
{
  g_assert (IDE_IS_CTAGS_SERVICE (self));

  if (self->queued_miner_handler == 0 && self->miner_active == FALSE)
    {
      self->queued_miner_handler =
        g_timeout_add_seconds_full (1,
                                    G_PRIORITY_DEFAULT,
                                    ide_ctags_service_do_mine,
                                    g_object_ref (self),
                                    g_object_unref);
    }
}

static void
build_system_tags_cb (GObject      *object,
                      GAsyncResult *result,
                      gpointer      user_data)
{
  g_autoptr(IdeCtagsService) self = user_data;

  g_assert (IDE_IS_TAGS_BUILDER (object));
  g_assert (G_IS_ASYNC_RESULT (result));
  g_assert (IDE_IS_CTAGS_SERVICE (self));

  ide_ctags_service_queue_mine (self);
}

static gboolean
restart_miner (gpointer user_data)
{
  QueuedRequest *qr = user_data;
  g_autoptr(IdeTagsBuilder) tags_builder = NULL;
  IdeBuildSystem *build_system;
  IdeContext *context;

  IDE_ENTRY;

  g_assert (qr != NULL);
  g_assert (IDE_IS_CTAGS_SERVICE (qr->self));
  g_assert (G_IS_FILE (qr->directory));

  g_hash_table_remove (qr->self->build_timeout_by_dir, qr->directory);

  context = ide_object_get_context (IDE_OBJECT (qr->self));
  build_system = ide_context_get_build_system (context);

  if (IDE_IS_TAGS_BUILDER (build_system))
    tags_builder = g_object_ref (IDE_TAGS_BUILDER (build_system));
  else
    tags_builder = ide_ctags_builder_new (context);

  ide_tags_builder_build_async (tags_builder,
                                qr->directory,
                                qr->recursive,
                                NULL,
                                build_system_tags_cb,
                                g_object_ref (qr->self));

  IDE_RETURN (G_SOURCE_REMOVE);
}

static void
ide_ctags_service_queue_build_for_directory (IdeCtagsService *self,
                                             GFile           *directory,
                                             gboolean         recursive)
{
  g_assert (IDE_IS_CTAGS_SERVICE (self));
  g_assert (G_IS_FILE (directory));

  if (ide_object_is_unloading (IDE_OBJECT (self)))
    return;

  if (!g_hash_table_lookup (self->build_timeout_by_dir, directory))
    {
      QueuedRequest *qr;
      guint source_id;

      qr = g_slice_new (QueuedRequest);
      qr->self = g_object_ref (self);
      qr->directory = g_object_ref (directory);
      qr->recursive = !!recursive;

      source_id = g_timeout_add_seconds_full (G_PRIORITY_LOW,
                                              QUEUED_BUILD_TIMEOUT_SECS,
                                              restart_miner,
                                              g_steal_pointer (&qr),
                                              queued_request_free);

      g_hash_table_insert (self->build_timeout_by_dir,
                           g_object_ref (directory),
                           GUINT_TO_POINTER (source_id));
    }
}

static void
ide_ctags_service_buffer_saved (IdeCtagsService  *self,
                                IdeBuffer        *buffer,
                                IdeBufferManager *buffer_manager)
{
  g_autoptr(GFile) parent = NULL;
  IdeContext *context;
  IdeVcs *vcs;
  GFile *file;
  GFile *workdir;

  IDE_ENTRY;

  g_assert (IDE_IS_CTAGS_SERVICE (self));
  g_assert (IDE_IS_BUFFER (buffer));
  g_assert (IDE_IS_BUFFER_MANAGER (buffer_manager));

  context = ide_object_get_context (IDE_OBJECT (self));
  vcs = ide_context_get_vcs (context);
  workdir = ide_vcs_get_working_directory (vcs);

  file = ide_file_get_file (ide_buffer_get_file (buffer));
  parent = g_file_get_parent (file);

  if (g_file_has_prefix (file, workdir))
    ide_ctags_service_queue_build_for_directory (self, parent, FALSE);

  IDE_EXIT;
}

static void
ide_ctags_service_context_loaded (IdeService *service)
{
  IdeBufferManager *buffer_manager;
  IdeCtagsService *self = (IdeCtagsService *)service;
  IdeContext *context;
  GFile *workdir;

  IDE_ENTRY;

  g_assert (IDE_IS_CTAGS_SERVICE (self));

  context = ide_object_get_context (IDE_OBJECT (self));
  buffer_manager = ide_context_get_buffer_manager (context);
  workdir = ide_vcs_get_working_directory (ide_context_get_vcs (context));

  g_signal_connect_object (buffer_manager,
                           "buffer-saved",
                           G_CALLBACK (ide_ctags_service_buffer_saved),
                           self,
                           G_CONNECT_SWAPPED);

  /*
   * Rebuild all ctags for the project at startup of the service.
   * Then we do incrementals from there on out.
   */
  ide_ctags_service_queue_build_for_directory (self, workdir, TRUE);

  IDE_EXIT;
}

static void
ide_ctags_service_start (IdeService *service)
{
}

static void
ide_ctags_service_stop (IdeService *service)
{
  IdeCtagsService *self = (IdeCtagsService *)service;

  g_return_if_fail (IDE_IS_CTAGS_SERVICE (self));

  if (self->cancellable && !g_cancellable_is_cancelled (self->cancellable))
    g_cancellable_cancel (self->cancellable);

  g_clear_object (&self->cancellable);
}

static void
ide_ctags_service_finalize (GObject *object)
{
  IdeCtagsService *self = (IdeCtagsService *)object;

  IDE_ENTRY;

  g_clear_object (&self->indexes);
  g_clear_object (&self->cancellable);
  g_clear_pointer (&self->highlighters, g_ptr_array_unref);
  g_clear_pointer (&self->completions, g_ptr_array_unref);
  g_clear_pointer (&self->build_timeout_by_dir, g_hash_table_unref);

  G_OBJECT_CLASS (ide_ctags_service_parent_class)->finalize (object);

  IDE_EXIT;
}

static void
ide_ctags_service_class_init (IdeCtagsServiceClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = ide_ctags_service_finalize;
}

static void
service_iface_init (IdeServiceInterface *iface)
{
  iface->context_loaded = ide_ctags_service_context_loaded;
  iface->start = ide_ctags_service_start;
  iface->stop = ide_ctags_service_stop;
}

static void
ide_ctags_service_class_finalize (IdeCtagsServiceClass *klass)
{
}

static void
ide_ctags_service_init (IdeCtagsService *self)
{
  self->highlighters = g_ptr_array_new ();
  self->completions = g_ptr_array_new ();

  self->build_timeout_by_dir = g_hash_table_new_full ((GHashFunc)g_file_hash,
                                                      (GEqualFunc)g_file_equal,
                                                      g_object_unref, NULL);

  self->indexes = dzl_task_cache_new ((GHashFunc)g_file_hash,
                                      (GEqualFunc)g_file_equal,
                                      g_object_ref,
                                      g_object_unref,
                                      g_object_ref,
                                      g_object_unref,
                                      0,
                                      ide_ctags_service_build_index_cb,
                                      self,
                                      NULL);

  dzl_task_cache_set_name (self->indexes, "ctags index cache");
}

void
_ide_ctags_service_register_type (GTypeModule *module)
{
  ide_ctags_service_register_type (module);
}

/**
 * ide_ctags_service_get_indexes:
 *
 * Gets a new #GPtrArray containing elements of #IdeCtagsIndex.
 *
 * Note: this does not sort the indexes by importance.
 *
 * Returns: (transfer container) (element-type Ide.CtagsIndex): An array of indexes.
 */
GPtrArray *
ide_ctags_service_get_indexes (IdeCtagsService *self)
{
  g_return_val_if_fail (IDE_IS_CTAGS_SERVICE (self), NULL);

  return dzl_task_cache_get_values (self->indexes);
}

void
ide_ctags_service_register_highlighter (IdeCtagsService     *self,
                                        IdeCtagsHighlighter *highlighter)
{
  g_autoptr(GPtrArray) values = NULL;
  gsize i;

  g_return_if_fail (IDE_IS_CTAGS_SERVICE (self));
  g_return_if_fail (IDE_IS_CTAGS_HIGHLIGHTER (highlighter));

  values = dzl_task_cache_get_values (self->indexes);

  for (i = 0; i < values->len; i++)
    {
      IdeCtagsIndex *index = g_ptr_array_index (values, i);
      ide_ctags_highlighter_add_index (highlighter, index);
    }

  g_ptr_array_add (self->highlighters, highlighter);
}

void
ide_ctags_service_unregister_highlighter (IdeCtagsService     *self,
                                          IdeCtagsHighlighter *highlighter)
{
  g_return_if_fail (IDE_IS_CTAGS_SERVICE (self));
  g_return_if_fail (IDE_IS_CTAGS_HIGHLIGHTER (highlighter));

  g_ptr_array_remove (self->highlighters, highlighter);
}

void
ide_ctags_service_register_completion (IdeCtagsService            *self,
                                       IdeCtagsCompletionProvider *completion)
{
  g_autoptr(GPtrArray) values = NULL;
  gsize i;

  g_return_if_fail (IDE_IS_CTAGS_SERVICE (self));
  g_return_if_fail (IDE_IS_CTAGS_COMPLETION_PROVIDER (completion));

  values = dzl_task_cache_get_values (self->indexes);

  for (i = 0; i < values->len; i++)
    {
      IdeCtagsIndex *index = g_ptr_array_index (values, i);
      ide_ctags_completion_provider_add_index (completion, index);
    }

  g_ptr_array_add (self->completions, completion);
}

void
ide_ctags_service_unregister_completion (IdeCtagsService            *self,
                                         IdeCtagsCompletionProvider *completion)
{
  g_return_if_fail (IDE_IS_CTAGS_SERVICE (self));
  g_return_if_fail (IDE_IS_CTAGS_COMPLETION_PROVIDER (completion));

  g_ptr_array_remove (self->completions, completion);
}
