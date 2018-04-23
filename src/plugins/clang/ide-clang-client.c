/* ide-clang-client.c
 *
 * Copyright 2018 Christian Hergert <chergert@redhat.com>
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

#define G_LOG_DOMAIN "ide-clang-client"

#include "config.h"

#include <jsonrpc-glib.h>

#include "ide-clang-client.h"

struct _IdeClangClient
{
  IdeObject                 parent;
  GQueue                    get_client;
  IdeSubprocessSupervisor  *supervisor;
  JsonrpcClient            *rpc_client;
  GFile                    *root_uri;
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
  gchar    *method;
  GVariant *params;
} Call;

G_DEFINE_TYPE_EXTENDED (IdeClangClient, ide_clang_client, IDE_TYPE_OBJECT, 0,
                        G_IMPLEMENT_INTERFACE (IDE_TYPE_SERVICE, NULL))

static void
call_free (gpointer data)
{
  Call *c = data;

  g_clear_pointer (&c->method, g_free);
  g_clear_pointer (&c->params, g_variant_unref);
  g_slice_free (Call, c);
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

  g_assert (IDE_IS_CLANG_CLIENT (self));
  g_assert (IDE_IS_SUBPROCESS (subprocess));
  g_assert (IDE_IS_SUBPROCESS_SUPERVISOR (supervisor));

  input = ide_subprocess_get_stdout_pipe (subprocess);
  output = ide_subprocess_get_stdin_pipe (subprocess);
  stream = g_simple_io_stream_new (input, output);

  g_clear_object (&self->rpc_client);
  self->rpc_client = jsonrpc_client_new (stream);

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
ide_clang_client_constructed (GObject *object)
{
  IdeClangClient *self = (IdeClangClient *)object;
  g_autoptr(IdeSubprocessLauncher) launcher = NULL;
  g_autofree gchar *cwd = NULL;
  IdeContext *context;
  IdeVcs *vcs;
  GFile *workdir;

  context = ide_object_get_context (IDE_OBJECT (self));
  vcs = ide_context_get_vcs (context);
  workdir = ide_vcs_get_working_directory (vcs);

  self->root_uri = g_object_ref (workdir);

  if (g_file_is_native (workdir))
    cwd = g_file_get_path (workdir);

  launcher = ide_subprocess_launcher_new (G_SUBPROCESS_FLAGS_STDOUT_PIPE | G_SUBPROCESS_FLAGS_STDIN_PIPE);
  if (cwd != NULL)
    ide_subprocess_launcher_set_cwd (launcher, cwd);
  ide_subprocess_launcher_push_argv (launcher, PACKAGE_LIBEXECDIR"/gnome-builder-clang");

  self->supervisor = ide_subprocess_supervisor_new ();
  ide_subprocess_supervisor_set_launcher (self->supervisor, launcher);

  g_signal_connect_object (self->supervisor,
                           "spawned",
                           G_CALLBACK (ide_clang_client_subprocess_spawned),
                           self,
                           G_CONNECT_SWAPPED);

  G_OBJECT_CLASS (ide_clang_client_parent_class)->constructed (object);
}

static void
ide_clang_client_dispose (GObject *object)
{
  IdeClangClient *self = (IdeClangClient *)object;
  GList *queued;

  self->state = STATE_SHUTDOWN;

  if (self->supervisor != NULL)
    {
      g_autoptr(IdeSubprocessSupervisor) supervisor = g_steal_pointer (&self->supervisor);

      ide_subprocess_supervisor_stop (supervisor);
    }

  g_clear_object (&self->rpc_client);

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

  G_OBJECT_CLASS (ide_clang_client_parent_class)->dispose (object);
}

static void
ide_clang_client_finalize (GObject *object)
{
  IdeClangClient *self = (IdeClangClient *)object;

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

  object_class->constructed = ide_clang_client_constructed;
  object_class->dispose = ide_clang_client_dispose;
  object_class->finalize = ide_clang_client_finalize;
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
    ide_task_return_pointer (task, g_steal_pointer (&reply), (GDestroyNotify)g_variant_unref);
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

  call = ide_task_get_task_data (task);

  g_assert (call != NULL);
  g_assert (call->method != NULL);

  jsonrpc_client_call_async (client,
                             call->method,
                             call->params,
                             ide_task_get_cancellable (task),
                             ide_clang_client_call_cb,
                             g_object_ref (task));
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
  call->method = g_strdup (method);
  call->params = g_variant_ref_sink (params);

  task = ide_task_new (self, cancellable, callback, user_data);
  ide_task_set_source_tag (task, ide_clang_client_call_async);
  ide_task_set_task_data (task, call, call_free);

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
  g_autoptr(GPtrArray) ret = NULL;
  GVariantIter iter;

  g_assert (IDE_IS_CLANG_CLIENT (self));
  g_assert (G_IS_ASYNC_RESULT (result));
  g_assert (IDE_IS_TASK (task));

  if (!ide_clang_client_call_finish (self, result, &reply, &error))
    {
      ide_task_return_error (task, g_steal_pointer (&error));
      return;
    }

  ret = g_ptr_array_new_with_free_func ((GDestroyNotify)ide_code_index_entry_free);

  if (g_variant_iter_init (&iter, reply))
    {
      g_autoptr(IdeCodeIndexEntryBuilder) builder = ide_code_index_entry_builder_new ();
      GVariant *kv;

      while ((kv = g_variant_iter_next_value (&iter)))
        {
          const gchar *name = NULL;
          const gchar *key= NULL;
          IdeSymbolKind kind = 0;
          IdeSymbolFlags flags = 0;
          GVariantDict dict;
          struct {
            guint32 line;
            guint32 column;
          } begin, end;

          g_variant_dict_init (&dict, kv);

          g_variant_dict_lookup (&dict, "name", "&s", &name);
          g_variant_dict_lookup (&dict, "key", "&s", &key);
          g_variant_dict_lookup (&dict, "kind", "i", &kind);
          g_variant_dict_lookup (&dict, "flags", "i", &flags);
          g_variant_dict_lookup (&dict, "range", "(uuuu)",
                                 &begin.line, &begin.column,
                                 &end.line, &end.column);

          ide_code_index_entry_builder_set_name (builder, name);
          ide_code_index_entry_builder_set_key (builder, key);
          ide_code_index_entry_builder_set_flags (builder, flags);
          ide_code_index_entry_builder_set_kind (builder, kind);
          ide_code_index_entry_builder_set_range (builder,
                                                  begin.line, begin.column,
                                                  end.line, end.column);

          g_ptr_array_add (ret, ide_code_index_entry_builder_build (builder));

          g_variant_dict_clear (&dict);
          g_variant_unref (kv);
        }
    }

  ide_task_return_pointer (task,
                           g_steal_pointer (&ret),
                           (GDestroyNotify)g_ptr_array_unref);
}

void
ide_clang_client_index_file_async (IdeClangClient      *self,
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
  ide_task_set_source_tag (task, ide_clang_client_index_file_async);

  if (!g_file_is_native (file))
    {
      ide_task_return_new_error (task,
                                 G_IO_ERROR,
                                 G_IO_ERROR_NOT_SUPPORTED,
                                 "Only local files can be indexed");
      return;
    }

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
 * Returns: (transfer full) (element-type Ide.CodeIndexEntry):
 *   an array of code index entries found in the file
 *
 * Since: 3.30
 */
GPtrArray *
ide_clang_client_index_file_finish (IdeClangClient  *self,
                                    GAsyncResult    *result,
                                    GError         **error)
{
  GPtrArray *ret;

  g_return_val_if_fail (IDE_IS_CLANG_CLIENT (self), NULL);
  g_return_val_if_fail (IDE_IS_TASK (result), NULL);

  ret = ide_task_propagate_pointer (IDE_TASK (result), error);

  IDE_PTR_ARRAY_CLEAR_FREE_FUNC (ret);

  return ret;
}
