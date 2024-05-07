/* ide-clang-client.c
 *
 * Copyright 2018-2019 Christian Hergert <chergert@redhat.com>
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

#define G_LOG_DOMAIN "ide-clang-client"

#include "config.h"

#include <glib/gi18n.h>

#include <gio/gunixinputstream.h>
#include <gio/gunixoutputstream.h>
#include <glib-unix.h>
#include <libide-code.h>
#include <libide-foundry.h>
#include <libide-vcs.h>
#include <jsonrpc-glib.h>

#include "ide-clang-client.h"
#include "ide-clang-symbol-tree.h"

struct _IdeClangClient
{
  IdeObject                 parent;
  GQueue                    get_client;
  IdeSubprocessSupervisor  *supervisor;
  JsonrpcClient            *rpc_client;
  GFile                    *root_uri;
  GHashTable               *seq_by_file;
  gint                      state;
};

enum {
  STATE_INITIAL,
  STATE_SPAWNING,
  STATE_RUNNING,
  STATE_SHUTDOWN,
};

typedef struct
{
  IdeClangClient *self;
  GCancellable   *cancellable;
  gchar          *method;
  GVariant       *params;
  GVariant       *id;
  gulong          cancel_id;
} Call;

G_DEFINE_FINAL_TYPE (IdeClangClient, ide_clang_client, IDE_TYPE_OBJECT)

static void
call_free (gpointer data)
{
  Call *c = data;

  if (c->cancel_id != 0)
    g_cancellable_disconnect (c->cancellable, c->cancel_id);

  c->cancel_id = 0;

  g_clear_pointer (&c->method, g_free);
  g_clear_pointer (&c->params, g_variant_unref);
  g_clear_pointer (&c->id, g_variant_unref);
  g_clear_object (&c->cancellable);
  g_clear_object (&c->self);
  g_slice_free (Call, c);
}

static void
ide_clang_client_sync_buffers (IdeClangClient *self)
{
  g_autoptr(GPtrArray) ar = NULL;
  IdeContext *context;
  IdeUnsavedFiles *ufs;

  g_assert (IDE_IS_CLANG_CLIENT (self));

  /* Bail if we're in destruction */
  if (self->state == STATE_SHUTDOWN ||
      !(context = ide_object_get_context (IDE_OBJECT (self))))
    return;

  /*
   * We need to sync buffers to the subprocess, but only those that are of any
   * consequence to us. So that means C, C++, or Obj-C files and headers.
   *
   * Further more, to avoid the chatter, we only want to send updated buffers
   * for unsaved files which we have not already sent or we'll be way too
   * chatty and cancel any cached translation units the subprocess has.
   *
   * Since the subprocess processes commands in order, we can simply call the
   * function to set the buffer on the peer and ignore the result (and it will
   * be used on subsequence commands).
   */

  ufs = ide_unsaved_files_from_context (context);
  ar = ide_unsaved_files_to_array (ufs);
  IDE_PTR_ARRAY_SET_FREE_FUNC (ar, ide_unsaved_file_unref);

  if (self->seq_by_file == NULL)
    self->seq_by_file = g_hash_table_new_full (g_file_hash,
                                               (GEqualFunc)g_file_equal,
                                               g_object_unref,
                                               NULL);

  for (guint i = 0; i < ar->len; i++)
    {
      IdeUnsavedFile *uf = g_ptr_array_index (ar, i);
      GFile *file = ide_unsaved_file_get_file (uf);
      gsize seq = (gsize)ide_unsaved_file_get_sequence (uf);
      gsize prev = GPOINTER_TO_SIZE (g_hash_table_lookup (self->seq_by_file, file));
      g_autofree gchar *name = g_file_get_basename (file);
      const gchar *dot = strrchr (name, '.');

      if (seq <= prev)
        continue;

      if (dot == NULL || !(g_str_equal (dot, ".c") ||
                           g_str_equal (dot, ".h") ||
                           g_str_equal (dot, ".cc") ||
                           g_str_equal (dot, ".hh") ||
                           g_str_equal (dot, ".cpp") ||
                           g_str_equal (dot, ".hpp") ||
                           g_str_equal (dot, ".cxx") ||
                           g_str_equal (dot, ".hxx") ||
                           g_str_equal (dot, ".m")))
        continue;

      g_hash_table_insert (self->seq_by_file, g_object_ref (file), GSIZE_TO_POINTER (seq));

      ide_clang_client_set_buffer_async (self,
                                         file,
                                         ide_unsaved_file_get_content (uf),
                                         NULL, NULL, NULL);
    }
}

static void
ide_clang_client_subprocess_exited (IdeClangClient          *self,
                                    IdeSubprocess           *subprocess,
                                    IdeSubprocessSupervisor *supervisor)
{
  IDE_ENTRY;

  g_assert (IDE_IS_CLANG_CLIENT (self));
  g_assert (IDE_IS_SUBPROCESS (subprocess));
  g_assert (IDE_IS_SUBPROCESS_SUPERVISOR (supervisor));

  ide_object_message (self, _("Clang integration server has exited"));

  if (self->state == STATE_RUNNING)
    self->state = STATE_SPAWNING;

  g_clear_object (&self->rpc_client);
  g_clear_pointer (&self->seq_by_file, g_hash_table_unref);

  IDE_EXIT;
}

static void
ide_clang_client_subprocess_spawned (IdeClangClient          *self,
                                     IdeSubprocess           *subprocess,
                                     IdeSubprocessSupervisor *supervisor)
{
  g_autoptr(GIOStream) stream = NULL;
  g_autoptr(GVariant) params = NULL;
  g_autofree gchar *path = NULL;
  g_autofree gchar *uri = NULL;
  GOutputStream *output;
  GInputStream *input;
  GList *queued;
  gint fd;

  IDE_ENTRY;

  g_assert (IDE_IS_CLANG_CLIENT (self));
  g_assert (IDE_IS_SUBPROCESS (subprocess));
  g_assert (IDE_IS_SUBPROCESS_SUPERVISOR (supervisor));
  g_assert (self->rpc_client == NULL);

  ide_object_message (self,
                      _("Clang integration server has started as process %s"),
                      ide_subprocess_get_identifier (subprocess));

  if (self->state == STATE_SPAWNING)
    self->state = STATE_RUNNING;

  input = ide_subprocess_get_stdout_pipe (subprocess);
  output = ide_subprocess_get_stdin_pipe (subprocess);
  stream = g_simple_io_stream_new (input, output);

  g_assert (G_IS_UNIX_INPUT_STREAM (input));
  g_assert (G_IS_UNIX_OUTPUT_STREAM (output));

  fd = g_unix_input_stream_get_fd (G_UNIX_INPUT_STREAM (input));
  g_unix_set_fd_nonblocking (fd, TRUE, NULL);

  fd = g_unix_output_stream_get_fd (G_UNIX_OUTPUT_STREAM (output));
  g_unix_set_fd_nonblocking (fd, TRUE, NULL);

  self->rpc_client = jsonrpc_client_new (stream);
  jsonrpc_client_set_use_gvariant (self->rpc_client, TRUE);

  queued = g_steal_pointer (&self->get_client.head);

  self->get_client.head = NULL;
  self->get_client.tail = NULL;
  self->get_client.length = 0;

  for (const GList *iter = queued; iter != NULL; iter = iter->next)
    {
      IdeTask *task = iter->data;

      ide_task_return_object (task, g_object_ref (self->rpc_client));
    }

  g_list_free_full (queued, g_object_unref);

  uri = g_file_get_uri (self->root_uri);
  path = g_file_get_path (self->root_uri);
  params = JSONRPC_MESSAGE_NEW (
    "rootUri", JSONRPC_MESSAGE_PUT_STRING (uri),
    "rootPath", JSONRPC_MESSAGE_PUT_STRING (path),
    "processId", JSONRPC_MESSAGE_PUT_INT64 (getpid ()),
    "capabilities", "{", "}"
  );

  jsonrpc_client_call_async (self->rpc_client,
                             "initialize",
                             params,
                             NULL, NULL, NULL);

  IDE_EXIT;
}

static void
ide_clang_client_get_client_async (IdeClangClient      *self,
                                   GCancellable        *cancellable,
                                   GAsyncReadyCallback  callback,
                                   gpointer             user_data)
{
  g_autoptr(IdeTask) task = NULL;

  g_assert (IDE_IS_CLANG_CLIENT (self));
  g_assert (!cancellable || G_IS_CANCELLABLE (cancellable));

  task = ide_task_new (self, cancellable, callback, user_data);
  ide_task_set_source_tag (task, ide_clang_client_get_client_async);

  switch (self->state)
    {
    case STATE_INITIAL:
      self->state = STATE_SPAWNING;
      g_queue_push_tail (&self->get_client, g_steal_pointer (&task));
      ide_subprocess_supervisor_start (self->supervisor);
      break;

    case STATE_SPAWNING:
      g_queue_push_tail (&self->get_client, g_steal_pointer (&task));
      break;

    case STATE_RUNNING:
      ide_task_return_object (task, g_object_ref (self->rpc_client));
      break;

    case STATE_SHUTDOWN:
      ide_task_return_new_error (task,
                                 G_IO_ERROR,
                                 G_IO_ERROR_CLOSED,
                                 "The client has been closed");
      break;

    default:
      g_assert_not_reached ();
      break;
    }
}

static JsonrpcClient *
ide_clang_client_get_client_finish (IdeClangClient  *self,
                                    GAsyncResult    *result,
                                    GError         **error)
{
  g_assert (IDE_IS_CLANG_CLIENT (self));
  g_assert (IDE_IS_TASK (result));

  return ide_task_propagate_object (IDE_TASK (result), error);
}

static void
ide_clang_client_buffer_saved (IdeClangClient   *self,
                               IdeBuffer        *buffer,
                               IdeBufferManager *bufmgr)
{
  GFile *file;

  g_assert (IDE_IS_CLANG_CLIENT (self));
  g_assert (IDE_BUFFER (buffer));
  g_assert (IDE_BUFFER_MANAGER (bufmgr));

  /*
   * We need to clear the cached buffer on the peer (and potentially
   * pop the translation unit cache) now that the buffer has been
   * saved to disk and we no longer need the draft.
   */

  file = ide_buffer_get_file (buffer);
  if (self->seq_by_file != NULL)
    g_hash_table_remove (self->seq_by_file, file);

  /* skip if thereis no peer */
  if (self->rpc_client == NULL)
    return;

  if (file != NULL)
    ide_clang_client_set_buffer_async (self, file, NULL, NULL, NULL, NULL);
}

static void
ide_clang_client_parent_set (IdeObject *object,
                             IdeObject *parent)
{
  IdeClangClient *self = (IdeClangClient *)object;
  g_autoptr(IdeSubprocessLauncher) launcher = NULL;
  g_autofree gchar *cwd = NULL;
  IdeBufferManager *bufmgr;
  IdeContext *context;
  IdeVcs *vcs;
  GFile *workdir;

  g_assert (IDE_IS_CLANG_CLIENT (self));
  g_assert (!parent || IDE_IS_OBJECT (parent));

  if (parent == NULL)
    return;

  context = ide_object_get_context (IDE_OBJECT (self));
  bufmgr = ide_buffer_manager_from_context (context);
  vcs = ide_vcs_from_context (context);
  workdir = ide_vcs_get_workdir (vcs);

  self->root_uri = g_object_ref (workdir);

  if (g_file_is_native (workdir))
    cwd = g_file_get_path (workdir);

  launcher = ide_subprocess_launcher_new (G_SUBPROCESS_FLAGS_STDOUT_PIPE | G_SUBPROCESS_FLAGS_STDIN_PIPE);
  if (cwd != NULL)
    ide_subprocess_launcher_set_cwd (launcher, cwd);
  ide_subprocess_launcher_set_clear_env (launcher, FALSE);
  ide_subprocess_launcher_setenv (launcher, "DZL_COUNTER_DISABLE_SHM", "1", TRUE);
  ide_subprocess_launcher_setenv (launcher, "GIGACAGE_ENABLED", "0", TRUE);
#if 0
  ide_subprocess_launcher_push_argv (launcher, "gdbserver");
  ide_subprocess_launcher_push_argv (launcher, "localhost:8888");
#endif
  ide_subprocess_launcher_push_argv (launcher, PACKAGE_LIBEXECDIR"/gnome-builder-clang");

  self->supervisor = ide_subprocess_supervisor_new ();
  ide_subprocess_supervisor_set_launcher (self->supervisor, launcher);

  g_signal_connect_object (self->supervisor,
                           "spawned",
                           G_CALLBACK (ide_clang_client_subprocess_spawned),
                           self,
                           G_CONNECT_SWAPPED);

  g_signal_connect_object (self->supervisor,
                           "exited",
                           G_CALLBACK (ide_clang_client_subprocess_exited),
                           self,
                           G_CONNECT_SWAPPED);

  g_signal_connect_object (bufmgr,
                           "buffer-saved",
                           G_CALLBACK (ide_clang_client_buffer_saved),
                           self,
                           G_CONNECT_SWAPPED);
}

static void
ide_clang_client_destroy (IdeObject *object)
{
  IdeClangClient *self = (IdeClangClient *)object;
  GList *queued;

  self->state = STATE_SHUTDOWN;

  g_clear_pointer (&self->seq_by_file, g_hash_table_unref);

  if (self->supervisor != NULL)
    {
      g_autoptr(IdeSubprocessSupervisor) supervisor = g_steal_pointer (&self->supervisor);

      ide_subprocess_supervisor_stop (supervisor);
    }

  g_clear_object (&self->rpc_client);
  g_clear_object (&self->root_uri);

  queued = g_steal_pointer (&self->get_client.head);

  self->get_client.head = NULL;
  self->get_client.tail = NULL;
  self->get_client.length = 0;

  for (const GList *iter = queued; iter != NULL; iter = iter->next)
    {
      IdeTask *task = iter->data;

      ide_task_return_new_error (task,
                                 G_IO_ERROR,
                                 G_IO_ERROR_CANCELLED,
                                 "Client is disposing");
    }

  g_list_free_full (queued, g_object_unref);

  IDE_OBJECT_CLASS (ide_clang_client_parent_class)->destroy (object);
}

static void
ide_clang_client_finalize (GObject *object)
{
  IdeClangClient *self = (IdeClangClient *)object;

  g_clear_pointer (&self->seq_by_file, g_hash_table_unref);
  g_clear_object (&self->rpc_client);
  g_clear_object (&self->root_uri);
  g_clear_object (&self->supervisor);

  g_assert (self->get_client.head == NULL);
  g_assert (self->get_client.tail == NULL);
  g_assert (self->get_client.length == 0);

  G_OBJECT_CLASS (ide_clang_client_parent_class)->finalize (object);
}

static void
ide_clang_client_class_init (IdeClangClientClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  IdeObjectClass *i_object_class = IDE_OBJECT_CLASS (klass);

  object_class->finalize = ide_clang_client_finalize;

  i_object_class->parent_set = ide_clang_client_parent_set;
  i_object_class->destroy = ide_clang_client_destroy;;
}

static void
ide_clang_client_init (IdeClangClient *self)
{
}

static void
ide_clang_client_call_cb (GObject      *object,
                          GAsyncResult *result,
                          gpointer      user_data)
{
  JsonrpcClient *rpc_client = (JsonrpcClient *)object;
  g_autoptr(IdeTask) task = user_data;
  g_autoptr(GVariant) reply = NULL;
  g_autoptr(GError) error = NULL;

  g_assert (JSONRPC_IS_CLIENT (rpc_client));
  g_assert (G_IS_ASYNC_RESULT (result));
  g_assert (IDE_IS_TASK (task));

  if (!jsonrpc_client_call_finish (rpc_client, result, &reply, &error))
    ide_task_return_error (task, g_steal_pointer (&error));
  else
    ide_task_return_pointer (task, g_steal_pointer (&reply), g_variant_unref);
}

static void
ide_clang_client_call_get_client_cb (GObject      *object,
                                     GAsyncResult *result,
                                     gpointer      user_data)
{
  IdeClangClient *self = (IdeClangClient *)object;
  g_autoptr(JsonrpcClient) client = NULL;
  g_autoptr(IdeTask) task = user_data;
  g_autoptr(GError) error = NULL;
  Call *call;

  g_assert (IDE_IS_CLANG_CLIENT (self));
  g_assert (G_IS_ASYNC_RESULT (result));
  g_assert (IDE_IS_TASK (task));

  if (!(client = ide_clang_client_get_client_finish (self, result, &error)))
    {
      ide_task_return_error (task, g_steal_pointer (&error));
      return;
    }

  if (ide_task_return_error_if_cancelled (task))
    return;

  call = ide_task_get_task_data (task);

  g_assert (call != NULL);
  g_assert (call->method != NULL);

  jsonrpc_client_call_with_id_async (client,
                                     call->method,
                                     call->params,
                                     &call->id,
                                     ide_task_get_cancellable (task),
                                     ide_clang_client_call_cb,
                                     g_object_ref (task));
}

static void
ide_clang_client_call_cancelled (GCancellable *cancellable,
                                 Call         *call)
{
  GVariantDict dict;

  g_assert (G_IS_CANCELLABLE (cancellable));
  g_assert (call != NULL);
  g_assert (call->cancellable == cancellable);
  g_assert (IDE_IS_CLANG_CLIENT (call->self));

  /* Will be zero if cancelled immediately */
  if (call->cancel_id == 0)
    return;

  if (call->self->rpc_client == NULL)
    return;

  /* Will be NULL if cancelled between getting build flags
   * and submitting request. Task will also be cancelled to
   * handle the cleanup on that side.
   */
  if (call->id == NULL)
    return;

  g_variant_dict_init (&dict, NULL);
  g_variant_dict_insert_value (&dict, "id", call->id);

  ide_clang_client_call_async (call->self,
                               "$/cancelRequest",
                               g_variant_dict_end (&dict),
                               NULL, NULL, NULL);
}

void
ide_clang_client_call_async (IdeClangClient      *self,
                             const gchar         *method,
                             GVariant            *params,
                             GCancellable        *cancellable,
                             GAsyncReadyCallback  callback,
                             gpointer             user_data)
{
  g_autoptr(IdeTask) task = NULL;
  Call *call;

  g_return_if_fail (IDE_IS_CLANG_CLIENT (self));
  g_return_if_fail (method != NULL);
  g_return_if_fail (!cancellable || G_IS_CANCELLABLE (cancellable));

  call = g_slice_new0 (Call);
  call->self = g_object_ref (self);
  call->method = g_strdup (method);
  call->params = params ? g_variant_ref_sink (params) : NULL;

  task = ide_task_new (self, cancellable, callback, user_data);
  ide_task_set_source_tag (task, ide_clang_client_call_async);
  ide_task_set_task_data (task, call, call_free);

  if (cancellable != NULL)
    {
      call->cancellable = g_object_ref (cancellable);
      call->cancel_id = g_cancellable_connect (cancellable,
                                               G_CALLBACK (ide_clang_client_call_cancelled),
                                               call,
                                               NULL);
      if (ide_task_return_error_if_cancelled (task))
        return;
    }

  ide_clang_client_get_client_async (self,
                                     cancellable,
                                     ide_clang_client_call_get_client_cb,
                                     g_steal_pointer (&task));
}

gboolean
ide_clang_client_call_finish (IdeClangClient  *self,
                              GAsyncResult    *result,
                              GVariant       **reply,
                              GError         **error)
{
  g_autoptr(GVariant) v = NULL;
  gboolean ret;

  g_return_val_if_fail (IDE_IS_CLANG_CLIENT (self), FALSE);
  g_return_val_if_fail (IDE_IS_TASK (result), FALSE);

  ret = !!(v = ide_task_propagate_pointer (IDE_TASK (result), error));

  if (reply != NULL)
    *reply = g_steal_pointer (&v);

  return ret;
}

static void
ide_clang_client_index_file_cb (GObject      *object,
                                GAsyncResult *result,
                                gpointer      user_data)
{
  IdeClangClient *self = (IdeClangClient *)object;
  g_autoptr(IdeTask) task = user_data;
  g_autoptr(GVariant) reply = NULL;
  g_autoptr(GError) error = NULL;

  g_assert (IDE_IS_CLANG_CLIENT (self));
  g_assert (G_IS_ASYNC_RESULT (result));
  g_assert (IDE_IS_TASK (task));

  if (!ide_clang_client_call_finish (self, result, &reply, &error))
    ide_task_return_error (task, g_steal_pointer (&error));
  else
    ide_task_return_pointer (task,
                             g_steal_pointer (&reply),
                             g_variant_unref);
}

void
ide_clang_client_index_file_async (IdeClangClient      *self,
                                   GFile               *file,
                                   const gchar * const *flags,
                                   GCancellable        *cancellable,
                                   GAsyncReadyCallback  callback,
                                   gpointer             user_data)
{
  static const char * const empty[] = { NULL };
  g_autoptr(IdeTask) task = NULL;
  g_autoptr(GVariant) params = NULL;
  g_autofree gchar *path = NULL;

  g_return_if_fail (IDE_IS_CLANG_CLIENT (self));
  g_return_if_fail (G_IS_FILE (file));
  g_return_if_fail (!cancellable || G_IS_CANCELLABLE (cancellable));

  task = ide_task_new (self, cancellable, callback, user_data);
  ide_task_set_source_tag (task, ide_clang_client_index_file_async);

  if (!g_file_is_native (file))
    {
      ide_task_return_new_error (task,
                                 G_IO_ERROR,
                                 G_IO_ERROR_NOT_SUPPORTED,
                                 "Only local files can be indexed");
      return;
    }

  if (flags == NULL)
    flags = empty;

  path = g_file_get_path (file);

  params = JSONRPC_MESSAGE_NEW (
    "path", JSONRPC_MESSAGE_PUT_STRING (path),
    "flags", JSONRPC_MESSAGE_PUT_STRV (flags)
  );

  ide_clang_client_call_async (self,
                               "clang/indexFile",
                               params,
                               cancellable,
                               ide_clang_client_index_file_cb,
                               g_steal_pointer (&task));
}

/**
 * ide_clang_client_index_file_finish:
 *
 * Completes the async request to get index entries found in the file.
 *
 * This returns the raw GVariant so that an #IdeCodeIndexEntries subclass
 * can create the actual code index entries on a thread once the indexer
 * is ready for them.
 *
 * Returns: (transfer full): a #GVariant containing the indexed data
 *   or %NULL in case of failure.
 */
GVariant *
ide_clang_client_index_file_finish (IdeClangClient  *self,
                                    GAsyncResult    *result,
                                    GError         **error)
{
  g_return_val_if_fail (IDE_IS_CLANG_CLIENT (self), NULL);
  g_return_val_if_fail (IDE_IS_TASK (result), NULL);

  return ide_task_propagate_pointer (IDE_TASK (result), error);
}

static void
ide_clang_client_get_index_key_cb (GObject      *object,
                                   GAsyncResult *result,
                                   gpointer      user_data)
{
  IdeClangClient *self = (IdeClangClient *)object;
  g_autoptr(IdeTask) task = user_data;
  g_autoptr(GVariant) reply = NULL;
  g_autoptr(GError) error = NULL;

  g_assert (IDE_IS_CLANG_CLIENT (self));
  g_assert (G_IS_ASYNC_RESULT (result));
  g_assert (IDE_IS_TASK (task));

  if (!ide_clang_client_call_finish (self, result, &reply, &error))
    ide_task_return_error (task, g_steal_pointer (&error));
  else if (!g_variant_is_of_type (reply, G_VARIANT_TYPE_STRING))
    ide_task_return_new_error (task,
                               G_IO_ERROR,
                               G_IO_ERROR_INVALID_DATA,
                               "Got a result back that was not a string");
  else
    ide_task_return_pointer (task,
                             g_variant_dup_string (reply, NULL),
                             g_free);
}

void
ide_clang_client_get_index_key_async (IdeClangClient      *self,
                                      GFile               *file,
                                      const gchar * const *flags,
                                      guint                line,
                                      guint                column,
                                      GCancellable        *cancellable,
                                      GAsyncReadyCallback  callback,
                                      gpointer             user_data)
{
  g_autoptr(IdeTask) task = NULL;
  g_autoptr(GVariant) params = NULL;
  g_autofree gchar *path = NULL;

  g_return_if_fail (IDE_IS_CLANG_CLIENT (self));
  g_return_if_fail (G_IS_FILE (file));
  g_return_if_fail (!cancellable || G_IS_CANCELLABLE (cancellable));
  g_return_if_fail (line > 0);
  g_return_if_fail (column > 0);

  task = ide_task_new (self, cancellable, callback, user_data);
  ide_task_set_source_tag (task, ide_clang_client_get_index_key_async);
  ide_task_set_kind (task, IDE_TASK_KIND_INDEXER);

  if (!g_file_is_native (file))
    {
      ide_task_return_new_error (task,
                                 G_IO_ERROR,
                                 G_IO_ERROR_INVALID_FILENAME,
                                 "Only native files are supported");
      return;
    }

  ide_clang_client_sync_buffers (self);

  path = g_file_get_path (file);

  params = JSONRPC_MESSAGE_NEW (
    "path", JSONRPC_MESSAGE_PUT_STRING (path),
    "flags", JSONRPC_MESSAGE_PUT_STRV (flags),
    "line", JSONRPC_MESSAGE_PUT_INT64 (line),
    "column", JSONRPC_MESSAGE_PUT_INT64 (column)
  );

  ide_clang_client_call_async (self,
                               "clang/getIndexKey",
                               params,
                               cancellable,
                               ide_clang_client_get_index_key_cb,
                               g_steal_pointer (&task));
}

gchar *
ide_clang_client_get_index_key_finish (IdeClangClient  *self,
                                       GAsyncResult    *result,
                                       GError         **error)
{
  g_return_val_if_fail (IDE_IS_CLANG_CLIENT (self), NULL);
  g_return_val_if_fail (IDE_IS_TASK (result), NULL);

  return ide_task_propagate_pointer (IDE_TASK (result), error);
}

static void
ide_clang_client_find_nearest_scope_cb (GObject      *object,
                                        GAsyncResult *result,
                                        gpointer      user_data)
{
  IdeClangClient *self = (IdeClangClient *)object;
  g_autoptr(IdeSymbol) ret = NULL;
  g_autoptr(GVariant) reply = NULL;
  g_autoptr(IdeTask) task = user_data;
  g_autoptr(GError) error = NULL;

  g_assert (IDE_IS_CLANG_CLIENT (self));
  g_assert (G_IS_ASYNC_RESULT (result));
  g_assert (IDE_IS_TASK (task));

  if (!ide_clang_client_call_finish (self, result, &reply, &error))
    {
      ide_task_return_error (task, g_steal_pointer (&error));
      return;
    }

  ret = ide_symbol_new_from_variant (reply);

  if (ret == NULL)
    ide_task_return_new_error (task,
                               G_IO_ERROR,
                               G_IO_ERROR_INVALID_DATA,
                               "Failed to decode symbol from IPC peer");
  else
    ide_task_return_pointer (task,
                             g_steal_pointer (&ret),
                             g_object_unref);
}

void
ide_clang_client_find_nearest_scope_async (IdeClangClient      *self,
                                           GFile               *file,
                                           const gchar * const *flags,
                                           guint                line,
                                           guint                column,
                                           GCancellable        *cancellable,
                                           GAsyncReadyCallback  callback,
                                           gpointer             user_data)
{
  g_autoptr(IdeTask) task = NULL;
  g_autoptr(GVariant) params = NULL;
  g_autofree gchar *path = NULL;

  g_return_if_fail (IDE_IS_CLANG_CLIENT (self));
  g_return_if_fail (G_IS_FILE (file));
  g_return_if_fail (!cancellable || G_IS_CANCELLABLE (cancellable));

  task = ide_task_new (self, cancellable, callback, user_data);
  ide_task_set_source_tag (task, ide_clang_client_find_nearest_scope_async);
  ide_task_set_kind (task, IDE_TASK_KIND_COMPILER);

  if (!g_file_is_native (file))
    {
      ide_task_return_new_error (task,
                                 G_IO_ERROR,
                                 G_IO_ERROR_NOT_SUPPORTED,
                                 "File must be a local file");
      return;
    }

  ide_clang_client_sync_buffers (self);

  path = g_file_get_path (file);

  params = JSONRPC_MESSAGE_NEW (
    "path", JSONRPC_MESSAGE_PUT_STRING (path),
    "flags", JSONRPC_MESSAGE_PUT_STRV (flags),
    "line", JSONRPC_MESSAGE_PUT_INT64 (line),
    "column", JSONRPC_MESSAGE_PUT_INT64 (column)
  );

  ide_clang_client_call_async (self,
                               "clang/findNearestScope",
                               params,
                               cancellable,
                               ide_clang_client_find_nearest_scope_cb,
                               g_steal_pointer (&task));
}

IdeSymbol *
ide_clang_client_find_nearest_scope_finish (IdeClangClient  *self,
                                            GAsyncResult    *result,
                                            GError         **error)
{
  g_return_val_if_fail (IDE_IS_CLANG_CLIENT (self), NULL);
  g_return_val_if_fail (IDE_IS_TASK (result), NULL);

  return ide_task_propagate_pointer (IDE_TASK (result), error);
}

static void
ide_clang_client_locate_symbol_cb (GObject      *object,
                                   GAsyncResult *result,
                                   gpointer      user_data)
{
  IdeClangClient *self = (IdeClangClient *)object;
  g_autoptr(IdeSymbol) ret = NULL;
  g_autoptr(GVariant) reply = NULL;
  g_autoptr(IdeTask) task = user_data;
  g_autoptr(GError) error = NULL;

  g_assert (IDE_IS_CLANG_CLIENT (self));
  g_assert (G_IS_ASYNC_RESULT (result));
  g_assert (IDE_IS_TASK (task));

  if (!ide_clang_client_call_finish (self, result, &reply, &error))
    {
      ide_task_return_error (task, g_steal_pointer (&error));
      return;
    }

  ret = ide_symbol_new_from_variant (reply);

  if (ret == NULL)
    ide_task_return_new_error (task,
                               G_IO_ERROR,
                               G_IO_ERROR_INVALID_DATA,
                               "Failed to decode symbol from IPC peer");
  else
    ide_task_return_pointer (task,
                             g_steal_pointer (&ret),
                             g_object_unref);
}

void
ide_clang_client_locate_symbol_async (IdeClangClient      *self,
                                      GFile               *file,
                                      const gchar * const *flags,
                                      guint                line,
                                      guint                column,
                                      GCancellable        *cancellable,
                                      GAsyncReadyCallback  callback,
                                      gpointer             user_data)
{
  g_autoptr(IdeTask) task = NULL;
  g_autoptr(GVariant) params = NULL;
  g_autofree gchar *path = NULL;

  g_return_if_fail (IDE_IS_CLANG_CLIENT (self));
  g_return_if_fail (G_IS_FILE (file));
  g_return_if_fail (!cancellable || G_IS_CANCELLABLE (cancellable));

  task = ide_task_new (self, cancellable, callback, user_data);
  ide_task_set_source_tag (task, ide_clang_client_locate_symbol_async);
  ide_task_set_kind (task, IDE_TASK_KIND_COMPILER);

  if (!g_file_is_native (file))
    {
      ide_task_return_new_error (task,
                                 G_IO_ERROR,
                                 G_IO_ERROR_NOT_SUPPORTED,
                                 "File must be a local file");
      return;
    }

  ide_clang_client_sync_buffers (self);

  path = g_file_get_path (file);

  params = JSONRPC_MESSAGE_NEW (
    "path", JSONRPC_MESSAGE_PUT_STRING (path),
    "flags", JSONRPC_MESSAGE_PUT_STRV (flags),
    "line", JSONRPC_MESSAGE_PUT_INT64 (line),
    "column", JSONRPC_MESSAGE_PUT_INT64 (column)
  );

  ide_clang_client_call_async (self,
                               "clang/locateSymbol",
                               params,
                               cancellable,
                               ide_clang_client_locate_symbol_cb,
                               g_steal_pointer (&task));
}

IdeSymbol *
ide_clang_client_locate_symbol_finish (IdeClangClient  *self,
                                       GAsyncResult    *result,
                                       GError         **error)
{
  g_return_val_if_fail (IDE_IS_CLANG_CLIENT (self), NULL);
  g_return_val_if_fail (IDE_IS_TASK (result), NULL);

  return ide_task_propagate_pointer (IDE_TASK (result), error);
}

static void
ide_clang_client_get_symbol_tree_cb (GObject      *object,
                                     GAsyncResult *result,
                                     gpointer      user_data)
{
  IdeClangClient *self = (IdeClangClient *)object;
  g_autoptr(GVariant) reply = NULL;
  g_autoptr(IdeTask) task = user_data;
  g_autoptr(GError) error = NULL;
  GFile *file;

  g_assert (IDE_IS_CLANG_CLIENT (self));
  g_assert (G_IS_ASYNC_RESULT (result));
  g_assert (IDE_IS_TASK (task));

  file = ide_task_get_task_data (task);

  if (!ide_clang_client_call_finish (self, result, &reply, &error))
    ide_task_return_error (task, g_steal_pointer (&error));
  else
    ide_task_return_object (task, ide_clang_symbol_tree_new (file, reply));
}

void
ide_clang_client_get_symbol_tree_async (IdeClangClient      *self,
                                        GFile               *file,
                                        const gchar * const *flags,
                                        GCancellable        *cancellable,
                                        GAsyncReadyCallback  callback,
                                        gpointer             user_data)
{
  g_autoptr(IdeTask) task = NULL;
  g_autoptr(GVariant) params = NULL;
  g_autofree gchar *path = NULL;

  g_return_if_fail (IDE_IS_CLANG_CLIENT (self));
  g_return_if_fail (G_IS_FILE (file));
  g_return_if_fail (!cancellable || G_IS_CANCELLABLE (cancellable));

  task = ide_task_new (self, cancellable, callback, user_data);
  ide_task_set_source_tag (task, ide_clang_client_get_symbol_tree_async);
  ide_task_set_task_data (task, g_file_dup (file), g_object_unref);
  ide_task_set_kind (task, IDE_TASK_KIND_COMPILER);

  if (!g_file_is_native (file))
    {
      ide_task_return_new_error (task,
                                 G_IO_ERROR,
                                 G_IO_ERROR_NOT_SUPPORTED,
                                 "File must be a local file");
      return;
    }

  ide_clang_client_sync_buffers (self);

  path = g_file_get_path (file);

  params = JSONRPC_MESSAGE_NEW (
    "path", JSONRPC_MESSAGE_PUT_STRING (path),
    "flags", JSONRPC_MESSAGE_PUT_STRV (flags)
  );

  ide_clang_client_call_async (self,
                               "clang/getSymbolTree",
                               params,
                               cancellable,
                               ide_clang_client_get_symbol_tree_cb,
                               g_steal_pointer (&task));
}

IdeSymbolTree *
ide_clang_client_get_symbol_tree_finish (IdeClangClient  *self,
                                         GAsyncResult    *result,
                                         GError         **error)
{
  g_return_val_if_fail (IDE_IS_CLANG_CLIENT (self), NULL);
  g_return_val_if_fail (IDE_IS_TASK (result), NULL);

  return ide_task_propagate_object (IDE_TASK (result), error);
}

static void
ide_clang_client_diagnose_cb (GObject      *object,
                              GAsyncResult *result,
                              gpointer      user_data)
{
  IdeClangClient *self = (IdeClangClient *)object;
  g_autoptr(IdeDiagnostics) ret = NULL;
  g_autoptr(GVariant) reply = NULL;
  g_autoptr(IdeTask) task = user_data;
  g_autoptr(GError) error = NULL;
  GVariantIter iter;
  GVariant *v;

  g_assert (IDE_IS_CLANG_CLIENT (self));
  g_assert (G_IS_ASYNC_RESULT (result));
  g_assert (IDE_IS_TASK (task));

  if (!ide_clang_client_call_finish (self, result, &reply, &error))
    {
      ide_task_return_error (task, g_steal_pointer (&error));
      return;
    }

  ret = ide_diagnostics_new ();

  g_variant_iter_init (&iter, reply);

  while ((v = g_variant_iter_next_value (&iter)))
    {
      IdeDiagnostic *diag = ide_diagnostic_new_from_variant (v);

      if (diag != NULL)
        ide_diagnostics_take (ret, g_steal_pointer (&diag));

      g_variant_unref (v);
    }

  ide_task_return_pointer (task,
                           g_steal_pointer (&ret),
                           g_object_unref);
}

void
ide_clang_client_diagnose_async (IdeClangClient      *self,
                                 GFile               *file,
                                 const gchar * const *flags,
                                 GCancellable        *cancellable,
                                 GAsyncReadyCallback  callback,
                                 gpointer             user_data)
{
  g_autoptr(IdeTask) task = NULL;
  g_autoptr(GVariant) params = NULL;
  g_autofree gchar *path = NULL;

  g_return_if_fail (IDE_IS_CLANG_CLIENT (self));
  g_return_if_fail (G_IS_FILE (file));
  g_return_if_fail (!cancellable || G_IS_CANCELLABLE (cancellable));

  task = ide_task_new (self, cancellable, callback, user_data);
  ide_task_set_source_tag (task, ide_clang_client_diagnose_async);
  ide_task_set_kind (task, IDE_TASK_KIND_COMPILER);

  if (!g_file_is_native (file))
    {
      ide_task_return_new_error (task,
                                 G_IO_ERROR,
                                 G_IO_ERROR_NOT_SUPPORTED,
                                 "File must be a local file");
      return;
    }

  ide_clang_client_sync_buffers (self);

  path = g_file_get_path (file);

  params = JSONRPC_MESSAGE_NEW (
    "path", JSONRPC_MESSAGE_PUT_STRING (path),
    "flags", JSONRPC_MESSAGE_PUT_STRV (flags)
  );

  ide_clang_client_call_async (self,
                               "clang/diagnose",
                               params,
                               cancellable,
                               ide_clang_client_diagnose_cb,
                               g_steal_pointer (&task));
}

IdeDiagnostics *
ide_clang_client_diagnose_finish (IdeClangClient  *self,
                                  GAsyncResult    *result,
                                  GError         **error)
{
  g_return_val_if_fail (IDE_IS_CLANG_CLIENT (self), NULL);
  g_return_val_if_fail (IDE_IS_TASK (result), NULL);

  return ide_task_propagate_pointer (IDE_TASK (result), error);
}

static void
ide_clang_client_get_highlight_index_cb (GObject      *object,
                                         GAsyncResult *result,
                                         gpointer      user_data)
{
  IdeClangClient *self = (IdeClangClient *)object;
  g_autoptr(GVariant) reply = NULL;
  g_autoptr(IdeTask) task = user_data;
  g_autoptr(GError) error = NULL;

  g_assert (IDE_IS_CLANG_CLIENT (self));
  g_assert (G_IS_ASYNC_RESULT (result));
  g_assert (IDE_IS_TASK (task));

  if (!ide_clang_client_call_finish (self, result, &reply, &error))
    ide_task_return_error (task, g_steal_pointer (&error));
  else
    ide_task_return_pointer (task,
                             ide_highlight_index_new_from_variant (reply),
                             ide_highlight_index_unref);
}

void
ide_clang_client_get_highlight_index_async (IdeClangClient      *self,
                                            GFile               *file,
                                            const gchar * const *flags,
                                            GCancellable        *cancellable,
                                            GAsyncReadyCallback  callback,
                                            gpointer             user_data)
{
  g_autoptr(IdeTask) task = NULL;
  g_autoptr(GVariant) params = NULL;
  g_autofree gchar *path = NULL;

  g_return_if_fail (IDE_IS_CLANG_CLIENT (self));
  g_return_if_fail (G_IS_FILE (file));
  g_return_if_fail (!cancellable || G_IS_CANCELLABLE (cancellable));

  task = ide_task_new (self, cancellable, callback, user_data);
  ide_task_set_source_tag (task, ide_clang_client_get_highlight_index_async);
  ide_task_set_kind (task, IDE_TASK_KIND_COMPILER);

  if (!g_file_is_native (file))
    {
      ide_task_return_new_error (task,
                                 G_IO_ERROR,
                                 G_IO_ERROR_NOT_SUPPORTED,
                                 "File must be a local file");
      return;
    }

  ide_clang_client_sync_buffers (self);

  path = g_file_get_path (file);

  params = JSONRPC_MESSAGE_NEW (
    "path", JSONRPC_MESSAGE_PUT_STRING (path),
    "flags", JSONRPC_MESSAGE_PUT_STRV (flags)
  );

  ide_clang_client_call_async (self,
                               "clang/getHighlightIndex",
                               params,
                               cancellable,
                               ide_clang_client_get_highlight_index_cb,
                               g_steal_pointer (&task));
}

IdeHighlightIndex *
ide_clang_client_get_highlight_index_finish (IdeClangClient  *self,
                                             GAsyncResult    *result,
                                             GError         **error)
{
  g_return_val_if_fail (IDE_IS_CLANG_CLIENT (self), NULL);
  g_return_val_if_fail (IDE_IS_TASK (result), NULL);

  return ide_task_propagate_pointer (IDE_TASK (result), error);
}

static void
ide_clang_client_complete_cb (GObject      *object,
                              GAsyncResult *result,
                              gpointer      user_data)
{
  IdeClangClient *self = (IdeClangClient *)object;
  g_autoptr(GVariant) reply = NULL;
  g_autoptr(IdeTask) task = user_data;
  g_autoptr(GError) error = NULL;

  g_assert (IDE_IS_CLANG_CLIENT (self));
  g_assert (G_IS_ASYNC_RESULT (result));
  g_assert (IDE_IS_TASK (task));

  if (!ide_clang_client_call_finish (self, result, &reply, &error))
    ide_task_return_error (task, g_steal_pointer (&error));
  else
    ide_task_return_pointer (task, g_steal_pointer (&reply), g_variant_unref);
}

void
ide_clang_client_complete_async (IdeClangClient      *self,
                                 GFile               *file,
                                 const gchar * const *flags,
                                 guint                line,
                                 guint                column,
                                 GCancellable        *cancellable,
                                 GAsyncReadyCallback  callback,
                                 gpointer             user_data)
{
  g_autoptr(IdeTask) task = NULL;
  g_autoptr(GVariant) params = NULL;
  g_autofree gchar *path = NULL;

  g_return_if_fail (IDE_IS_CLANG_CLIENT (self));
  g_return_if_fail (G_IS_FILE (file));
  g_return_if_fail (!cancellable || G_IS_CANCELLABLE (cancellable));

  task = ide_task_new (self, cancellable, callback, user_data);
  ide_task_set_source_tag (task, ide_clang_client_complete_async);
  ide_task_set_kind (task, IDE_TASK_KIND_COMPILER);

  if (!g_file_is_native (file))
    {
      ide_task_return_new_error (task,
                                 G_IO_ERROR,
                                 G_IO_ERROR_NOT_SUPPORTED,
                                 "File must be a local file");
      return;
    }

  ide_clang_client_sync_buffers (self);

  path = g_file_get_path (file);

  params = JSONRPC_MESSAGE_NEW (
    "path", JSONRPC_MESSAGE_PUT_STRING (path),
    "flags", JSONRPC_MESSAGE_PUT_STRV (flags),
    "line", JSONRPC_MESSAGE_PUT_INT64 (line),
    "column", JSONRPC_MESSAGE_PUT_INT64 (column)
  );

  ide_clang_client_call_async (self,
                               "clang/complete",
                               params,
                               cancellable,
                               ide_clang_client_complete_cb,
                               g_steal_pointer (&task));
}

GVariant *
ide_clang_client_complete_finish (IdeClangClient  *self,
                                  GAsyncResult    *result,
                                  GError         **error)
{
  g_return_val_if_fail (IDE_IS_CLANG_CLIENT (self), NULL);
  g_return_val_if_fail (IDE_IS_TASK (result), NULL);

  return ide_task_propagate_pointer (IDE_TASK (result), error);
}

static void
ide_clang_client_set_buffer_cb (GObject      *object,
                                GAsyncResult *result,
                                gpointer      user_data)
{
  IdeClangClient *self = (IdeClangClient *)object;
  g_autoptr(GVariant) reply = NULL;
  g_autoptr(IdeTask) task = user_data;
  g_autoptr(GError) error = NULL;

  g_assert (IDE_IS_CLANG_CLIENT (self));
  g_assert (G_IS_ASYNC_RESULT (result));
  g_assert (IDE_IS_TASK (task));

  if (!ide_clang_client_call_finish (self, result, &reply, &error))
    ide_task_return_error (task, g_steal_pointer (&error));
  else
    ide_task_return_boolean (task, TRUE);
}

void
ide_clang_client_set_buffer_async (IdeClangClient      *self,
                                   GFile               *file,
                                   GBytes              *bytes,
                                   GCancellable        *cancellable,
                                   GAsyncReadyCallback  callback,
                                   gpointer             user_data)
{
  g_autoptr(IdeTask) task = NULL;
  g_autofree gchar *path = NULL;
  const guint8 *data = NULL;
  GVariantDict dict;
  gsize len;

  g_return_if_fail (IDE_IS_CLANG_CLIENT (self));
  g_return_if_fail (G_IS_FILE (file));
  g_return_if_fail (!cancellable || G_IS_CANCELLABLE (cancellable));

  task = ide_task_new (self, cancellable, callback, user_data);
  ide_task_set_source_tag (task, ide_clang_client_set_buffer_async);
  ide_task_set_kind (task, IDE_TASK_KIND_IO);

  if (!g_file_is_native (file))
    {
      ide_task_return_new_error (task,
                                 G_IO_ERROR,
                                 G_IO_ERROR_NOT_SUPPORTED,
                                 "File must be a local file");
      return;
    }

  path = g_file_get_path (file);

  /* data doesn't need to be utf-8, but it does have to be
   * a valid byte string (no embedded \0 bytes).
   */
  if (bytes != NULL)
    data = g_bytes_get_data (bytes, &len);

  g_variant_dict_init (&dict, NULL);
  g_variant_dict_insert (&dict, "path", "s", path);
  if (data != NULL)
    g_variant_dict_insert (&dict, "contents", "^ay", data);

  ide_clang_client_call_async (self,
                               "clang/setBuffer",
                               g_variant_dict_end (&dict),
                               cancellable,
                               ide_clang_client_set_buffer_cb,
                               g_steal_pointer (&task));
}

gboolean
ide_clang_client_set_buffer_finish (IdeClangClient  *self,
                                    GAsyncResult    *result,
                                    GError         **error)
{
  g_return_val_if_fail (IDE_IS_CLANG_CLIENT (self), FALSE);
  g_return_val_if_fail (IDE_IS_TASK (result), FALSE);

  return ide_task_propagate_boolean (IDE_TASK (result), error);
}
