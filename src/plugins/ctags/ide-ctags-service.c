/* ide-ctags-service.c
 *
 * Copyright 2015-2019 Christian Hergert <christian@hergert.me>
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

#define G_LOG_DOMAIN "ide-ctags-service"

#include <glib/gi18n.h>
#include <gtksourceview/gtksource.h>

#include <libide-code.h>
#include <libide-vcs.h>

#include "ide-ctags-builder.h"
#include "ide-ctags-completion-provider.h"
#include "ide-ctags-highlighter.h"
#include "ide-ctags-index.h"
#include "ide-ctags-service.h"
#include "ide-tags-builder.h"

#define QUEUED_BUILD_TIMEOUT_SECS 5

struct _IdeCtagsService
{
  IdeObject         parent_instance;

  IdeTaskCache     *indexes;
  GCancellable     *cancellable;
  GPtrArray        *highlighters;
  GPtrArray        *completions;
  GHashTable       *build_timeout_by_dir;

  IdeNotification  *notif;
  gint              n_active;

  guint             did_full_build : 1;
  guint             queued_miner_handler;
  guint             miner_active : 1;
  guint             paused : 1;
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

G_DEFINE_FINAL_TYPE (IdeCtagsService, ide_ctags_service, IDE_TYPE_OBJECT)

enum {
  PROP_0,
  PROP_PAUSED,
  N_PROPS
};

static GParamSpec *properties [N_PROPS];

static void
queued_request_free (gpointer data)
{
  QueuedRequest *qr = data;

  g_clear_object (&qr->self);
  g_clear_object (&qr->directory);
  g_slice_free (QueuedRequest, qr);
}

static gboolean
is_supported_language (const gchar *lang_id)
{
  /* Some languages we expect ctags to actually parse when saved.
   * Keep in sync with our .plugin file.
   */
  static const gchar *supported[] = {
    "c", "cpp", "chdr", "cpphdr", "python", "python3",
    "js", "css", "html", "ruby",
    NULL
  };

  if (lang_id == NULL)
    return FALSE;

  return g_strv_contains (supported, lang_id);
}

static void
show_notification (IdeCtagsService *self)
{
  g_autoptr(IdeContext) context = NULL;
  g_autoptr(IdeNotification) notif = NULL;
  g_autoptr(GIcon) icon = NULL;

  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (IDE_IS_CTAGS_SERVICE (self));
  g_assert (self->n_active >= 0);

  self->n_active++;

  if (self->n_active > 1)
    return;

  g_assert (self->notif == NULL);

  if (!(context = ide_object_ref_context (IDE_OBJECT (self))))
    return;

  notif = ide_notification_new ();
  icon = g_icon_new_for_string ("media-playback-pause-symbolic", NULL);
  ide_notification_set_title (notif, _("Indexing Source Code"));
  ide_notification_set_body (notif, _("Search, autocompletion, and symbol information may be limited until Ctags indexing is complete."));
  ide_notification_set_has_progress (notif, TRUE);
  ide_notification_set_progress_is_imprecise (notif, TRUE);
  ide_notification_add_button (notif, NULL, icon, "context.workbench.ctags.paused");
  ide_notification_attach (notif, IDE_OBJECT (context));

  self->notif = g_steal_pointer (&notif);
}

static void
hide_notification (IdeCtagsService *self)
{
  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (IDE_IS_CTAGS_SERVICE (self));
  g_assert (self->n_active > 0);

  self->n_active--;

  if (self->n_active == 0)
    {
      if (self->notif != NULL)
        {
          ide_notification_withdraw_in_seconds (self->notif, 3);
          g_clear_object (&self->notif);
        }
    }
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
  g_autoptr(GFile) workdir = NULL;
  IdeContext *context;

  context = ide_object_get_context (IDE_OBJECT (self));
  workdir = ide_context_ref_workdir (context);
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
ide_ctags_service_build_index_cb (IdeTaskCache  *cache,
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
  IdeTaskCache *cache = (IdeTaskCache *)object;
  g_autoptr(IdeCtagsService) self = user_data;
  g_autoptr(IdeCtagsIndex) index = NULL;
  g_autoptr(GError) error = NULL;

  IDE_ENTRY;

  g_assert (IDE_IS_TASK_CACHE (cache));
  g_assert (IDE_IS_CTAGS_SERVICE (self));

  if (!(index = ide_task_cache_get_finish (cache, result, &error)))
    {
      /* don't log if it was an empty file */
      if (!g_error_matches (error, G_IO_ERROR, G_IO_ERROR_NONE))
        g_debug ("%s", error->message);

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

  if ((prev = ide_task_cache_peek (pair->self->indexes, pair->file)))
    {
      if (!file_is_newer (prev, pair->file))
        goto cleanup;
    }

  ide_task_cache_get_async (pair->self->indexes,
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
  g_autoptr(IdeContext) context = NULL;
  g_autoptr(IdeVcs) vcs = NULL;
  GArray *mine_info = task_data;

  IDE_ENTRY;

  g_assert (G_IS_TASK (task));
  g_assert (IDE_IS_CTAGS_SERVICE (self));
  g_assert (mine_info != NULL);

  if ((context = ide_object_ref_context (IDE_OBJECT (self))))
    vcs = ide_object_get_child_typed (IDE_OBJECT (context), IDE_TYPE_VCS);

  for (guint i = 0; i < mine_info->len; i++)
    {
      const MineInfo *info = &g_array_index (mine_info, MineInfo, i);
      g_autoptr(GFile) file = g_file_new_for_path (info->path);

      ide_ctags_service_mine_directory (self, vcs, file, info->recursive, cancellable);
    }

  self->miner_active = FALSE;

  g_task_return_boolean (task, TRUE);

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
  g_autoptr(IdeContext) context = NULL;
  g_autoptr(GTask) task = NULL;
  g_autoptr(GArray) mine_info = NULL;
  g_autoptr(GFile) workdir = NULL;
  MineInfo info;

  IDE_ENTRY;

  g_assert (IDE_IS_CTAGS_SERVICE (self));

  self->queued_miner_handler = 0;
  self->miner_active = TRUE;

  if (!ide_object_in_destruction (IDE_OBJECT (self)) &&
      (context = ide_object_ref_context (IDE_OBJECT (self))))
    {
      workdir = ide_context_ref_workdir (context);

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
    }

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

  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (IDE_IS_TAGS_BUILDER (object));
  g_assert (G_IS_ASYNC_RESULT (result));
  g_assert (IDE_IS_CTAGS_SERVICE (self));

  ide_object_destroy (IDE_OBJECT (object));
  ide_ctags_service_queue_mine (self);
  hide_notification (self);
}

static gboolean
restart_miner (gpointer user_data)
{
  g_autoptr(IdeTagsBuilder) tags_builder = NULL;
  QueuedRequest *qr = user_data;

  IDE_ENTRY;

  g_assert (qr != NULL);
  g_assert (IDE_IS_CTAGS_SERVICE (qr->self));
  g_assert (G_IS_FILE (qr->directory));

  /* Just skip for now if we got here somehow and paused */
  if (qr->self->paused)
    IDE_RETURN (G_SOURCE_CONTINUE);

  g_hash_table_remove (qr->self->build_timeout_by_dir, qr->directory);

  tags_builder = ide_ctags_builder_new ();
  ide_object_append (IDE_OBJECT (qr->self), IDE_OBJECT (tags_builder));

  show_notification (qr->self);

  ide_tags_builder_build_async (tags_builder,
                                qr->directory,
                                qr->recursive,
                                qr->self->cancellable,
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

  if (ide_object_in_destruction (IDE_OBJECT (self)))
    return;

  if (!g_hash_table_lookup (self->build_timeout_by_dir, directory))
    {
      QueuedRequest *qr;
      GSource *source;
      guint source_id = 0;

      qr = g_slice_new (QueuedRequest);
      qr->self = g_object_ref (self);
      qr->directory = g_object_ref (directory);
      qr->recursive = !!recursive;

      source_id = g_timeout_add_seconds_full (G_PRIORITY_LOW,
                                              QUEUED_BUILD_TIMEOUT_SECS,
                                              restart_miner,
                                              g_steal_pointer (&qr),
                                              queued_request_free);

      if (self->paused &&
          (source = g_main_context_find_source_by_id (NULL, source_id)))
        g_source_set_ready_time (source, -1);

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
  g_autoptr(GFile) workdir = NULL;
  IdeContext *context;
  GFile *file;

  IDE_ENTRY;

  g_assert (IDE_IS_CTAGS_SERVICE (self));
  g_assert (IDE_IS_BUFFER (buffer));
  g_assert (IDE_IS_BUFFER_MANAGER (buffer_manager));

  context = ide_object_get_context (IDE_OBJECT (self));
  workdir = ide_context_ref_workdir (context);

  if (!self->did_full_build)
    {
      self->did_full_build = TRUE;
      ide_ctags_service_queue_build_for_directory (self, workdir, TRUE);
      return;
    }

  file = ide_buffer_get_file (buffer);
  parent = g_file_get_parent (file);

  if (g_file_has_prefix (file, workdir) &&
      is_supported_language (ide_buffer_get_language_id (buffer)))
    ide_ctags_service_queue_build_for_directory (self, parent, FALSE);

  IDE_EXIT;
}

static void
ide_ctags_service_parent_set (IdeObject *object,
                              IdeObject *parent)
{
  IdeCtagsService *self = (IdeCtagsService *)object;
  g_autoptr(GFile) workdir = NULL;
  IdeBufferManager *bufmgr;

  IDE_ENTRY;

  g_assert (IDE_IS_CTAGS_SERVICE (self));
  g_assert (!parent || IDE_IS_CONTEXT (parent));

  if (parent == NULL)
    IDE_EXIT;

  bufmgr = ide_buffer_manager_from_context (IDE_CONTEXT (parent));
  workdir = ide_context_ref_workdir (IDE_CONTEXT (parent));

  g_signal_connect_object (bufmgr,
                           "buffer-saved",
                           G_CALLBACK (ide_ctags_service_buffer_saved),
                           self,
                           G_CONNECT_SWAPPED);

  /* We don't do any rebuilds up-front because users find them annoying.
   * Instead, we wait until a file has been saved and then re-index content
   * for the whole project (if necessary).
   */

  IDE_EXIT;
}

static void
ide_ctags_service_destroy (IdeObject *object)
{
  IdeCtagsService *self = (IdeCtagsService *)object;

  g_assert (IDE_IS_CTAGS_SERVICE (self));

  g_clear_handle_id (&self->queued_miner_handler, g_source_remove);

  g_cancellable_cancel (self->cancellable);
  g_clear_object (&self->cancellable);

  IDE_OBJECT_CLASS (ide_ctags_service_parent_class)->destroy (object);
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
ide_ctags_service_get_property (GObject    *object,
                                guint       prop_id,
                                GValue     *value,
                                GParamSpec *pspec)
{
  IdeCtagsService *self = IDE_CTAGS_SERVICE (object);

  switch (prop_id)
    {
    case PROP_PAUSED:
      g_value_set_boolean (value, self->paused);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
ide_ctags_service_set_property (GObject      *object,
                                guint         prop_id,
                                const GValue *value,
                                GParamSpec   *pspec)
{
  IdeCtagsService *self = IDE_CTAGS_SERVICE (object);

  switch (prop_id)
    {
    case PROP_PAUSED:
      if (g_value_get_boolean (value) != self->paused)
        {
          if (self->paused)
            ide_ctags_service_unpause (self);
          else
            ide_ctags_service_pause (self);
        }
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
ide_ctags_service_class_init (IdeCtagsServiceClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  IdeObjectClass *i_object_class = IDE_OBJECT_CLASS (klass);

  object_class->get_property = ide_ctags_service_get_property;
  object_class->set_property = ide_ctags_service_set_property;

  i_object_class->parent_set = ide_ctags_service_parent_set;
  i_object_class->destroy = ide_ctags_service_destroy;

  object_class->finalize = ide_ctags_service_finalize;

  properties [PROP_PAUSED] =
    g_param_spec_boolean ("paused",
                          "Paused",
                          "If the service is paused",
                          FALSE,
                          (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, N_PROPS, properties);
}

static void
ide_ctags_service_init (IdeCtagsService *self)
{
  self->highlighters = g_ptr_array_new ();
  self->completions = g_ptr_array_new ();

  self->build_timeout_by_dir = g_hash_table_new_full ((GHashFunc)g_file_hash,
                                                      (GEqualFunc)g_file_equal,
                                                      g_object_unref, NULL);

  self->indexes = ide_task_cache_new ((GHashFunc)g_file_hash,
                                      (GEqualFunc)g_file_equal,
                                      g_object_ref,
                                      g_object_unref,
                                      g_object_ref,
                                      g_object_unref,
                                      0,
                                      ide_ctags_service_build_index_cb,
                                      self,
                                      NULL);

  ide_task_cache_set_name (self->indexes, "ctags index cache");
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

  return ide_task_cache_get_values (self->indexes);
}

static void
ide_ctags_service_highlighter_destroyed_cb (IdeCtagsService     *self,
                                            IdeCtagsHighlighter *highlighter)
{
  IDE_ENTRY;

  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (IDE_IS_CTAGS_SERVICE (self));
  g_assert (IDE_IS_CTAGS_HIGHLIGHTER (highlighter));

  if (self->highlighters != NULL)
    g_ptr_array_remove (self->highlighters, highlighter);

  IDE_EXIT;
}

void
ide_ctags_service_register_highlighter (IdeCtagsService     *self,
                                        IdeCtagsHighlighter *highlighter)
{
  g_autoptr(GPtrArray) values = NULL;

  IDE_ENTRY;

  g_return_if_fail (IDE_IS_MAIN_THREAD ());
  g_return_if_fail (IDE_IS_CTAGS_SERVICE (self));
  g_return_if_fail (IDE_IS_CTAGS_HIGHLIGHTER (highlighter));

  g_ptr_array_add (self->highlighters, highlighter);
  g_signal_connect_object (highlighter,
                           "destroy",
                           G_CALLBACK (ide_ctags_service_highlighter_destroyed_cb),
                           self,
                           G_CONNECT_SWAPPED);

  values = ide_task_cache_get_values (self->indexes);

  for (guint i = 0; i < values->len; i++)
    {
      IdeCtagsIndex *index = g_ptr_array_index (values, i);
      ide_ctags_highlighter_add_index (highlighter, index);
    }

  IDE_EXIT;
}

static void
ide_ctags_service_completion_destroyed_cb (IdeCtagsService            *self,
                                           IdeCtagsCompletionProvider *completion)
{
  IDE_ENTRY;

  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (IDE_IS_CTAGS_SERVICE (self));
  g_assert (IDE_IS_CTAGS_COMPLETION_PROVIDER (completion));

  if (self->completions != NULL)
    g_ptr_array_remove (self->completions, completion);

  IDE_EXIT;
}

void
ide_ctags_service_register_completion (IdeCtagsService            *self,
                                       IdeCtagsCompletionProvider *completion)
{
  g_autoptr(GPtrArray) values = NULL;

  IDE_ENTRY;

  g_return_if_fail (IDE_IS_CTAGS_SERVICE (self));
  g_return_if_fail (IDE_IS_CTAGS_COMPLETION_PROVIDER (completion));

  g_ptr_array_add (self->completions, completion);
  g_signal_connect_object (completion,
                           "destroy",
                           G_CALLBACK (ide_ctags_service_completion_destroyed_cb),
                           self,
                           G_CONNECT_SWAPPED);

  values = ide_task_cache_get_values (self->indexes);

  for (guint i = 0; i < values->len; i++)
    {
      IdeCtagsIndex *index = g_ptr_array_index (values, i);
      ide_ctags_completion_provider_add_index (completion, index);
    }

  IDE_EXIT;
}

void
ide_ctags_service_pause (IdeCtagsService *self)
{
  GHashTableIter iter;
  gpointer key;
  gpointer value;

  g_return_if_fail (IDE_IS_CTAGS_SERVICE (self));

  if (ide_object_in_destruction (IDE_OBJECT (self)))
    return;

  if (self->paused)
    return;

  g_cancellable_cancel (self->cancellable);
  g_clear_object (&self->cancellable);
  self->cancellable = g_cancellable_new ();

  self->paused = TRUE;

  /* Make sure we show the pause state so the user can unpause */
  show_notification (self);
  ide_notification_set_title (self->notif, _("Indexing Source Code (Paused)"));

  g_hash_table_iter_init (&iter, self->build_timeout_by_dir);

  while (g_hash_table_iter_next (&iter, &key, &value))
    {
      GSource *source;

      /* Make the source innactive until we are unpaused */
      if ((source = g_main_context_find_source_by_id (NULL, GPOINTER_TO_UINT (value))))
        g_source_set_ready_time (source, -1);
    }
}

void
ide_ctags_service_unpause (IdeCtagsService *self)
{
  GHashTableIter iter;
  gpointer key;
  gpointer value;

  g_return_if_fail (IDE_IS_CTAGS_SERVICE (self));

  if (ide_object_in_destruction (IDE_OBJECT (self)))
    return;

  if (!self->paused)
    return;

  self->paused = FALSE;

  g_hash_table_iter_init (&iter, self->build_timeout_by_dir);

  while (g_hash_table_iter_next (&iter, &key, &value))
    {
      GSource *source;

      /* Make the source innactive until we are unpaused */
      if ((source = g_main_context_find_source_by_id (NULL, GPOINTER_TO_UINT (value))))
        g_source_set_ready_time (source, 0);
    }

  /* Now we can drop our paused state */
  ide_notification_set_title (self->notif, _("Indexing Source Code"));
  hide_notification (self);
}
