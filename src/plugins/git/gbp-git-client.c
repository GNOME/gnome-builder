/* gbp-git-client.c
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

#define G_LOG_DOMAIN "gbp-git-client"

#include "config.h"

#include <glib/gi18n.h>

#include <gio/gunixinputstream.h>
#include <gio/gunixoutputstream.h>
#include <glib-unix.h>
#include <libide-vcs.h>
#include <jsonrpc-glib.h>

#include "gbp-git-client.h"

struct _GbpGitClient
{
  IdeObject                 parent;
  GQueue                    get_client;
  IdeSubprocessSupervisor  *supervisor;
  JsonrpcClient            *rpc_client;
  GFile                    *root_uri;
  gint                      state;
  GHashTable               *notif_by_token;
};

enum {
  STATE_INITIAL,
  STATE_SPAWNING,
  STATE_RUNNING,
  STATE_SHUTDOWN,
};

typedef struct
{
  GbpGitClient *self;
  GCancellable   *cancellable;
  gchar          *method;
  GVariant       *params;
  GVariant       *id;
  gulong          cancel_id;
} Call;

G_DEFINE_TYPE (GbpGitClient, gbp_git_client, IDE_TYPE_OBJECT)

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
gbp_git_client_notification_cb (GbpGitClient  *self,
                                const gchar   *command,
                                GVariant      *reply,
                                JsonrpcClient *client)
{
  const gchar *token = NULL;
  IdeNotification *notif;

  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (GBP_IS_GIT_CLIENT (client));
  g_assert (command != NULL);
  g_assert (JSONRPC_IS_CLIENT (client));

  if (reply == NULL)
    return;

  if (g_str_equal (command, "$/progress") &&
      JSONRPC_MESSAGE_PARSE (reply, "token", JSONRPC_MESSAGE_GET_STRING (&token)) &&
      (notif = g_hash_table_lookup (self->notif_by_token, token)))
    {
      gdouble progress;
      const gchar *message;

      if (!JSONRPC_MESSAGE_PARSE (reply, "progress", JSONRPC_MESSAGE_GET_DOUBLE (&progress)))
        progress = 0.0;

      if (!JSONRPC_MESSAGE_PARSE (reply, "message", JSONRPC_MESSAGE_GET_STRING (&message)))
        message = NULL;

      if (message != NULL)
        ide_notification_set_body (notif, message);

      if (progress > 0.0)
        ide_notification_set_progress (notif, CLAMP (progress, 0.0, 1.0));
    }
}

static gchar *
gbp_git_client_track_progress (GbpGitClient    *self,
                               IdeNotification *notif)
{
  g_autofree gchar *token = NULL;

  g_assert (GBP_IS_GIT_CLIENT (self));
  g_assert (IDE_IS_NOTIFICATION (notif));

  if (self->notif_by_token == NULL)
    self->notif_by_token = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, g_object_unref);

  token = g_uuid_string_random ();
  g_hash_table_insert (self->notif_by_token, g_strdup (token), g_object_ref (notif));

  return g_steal_pointer (&token);
}

static void
gbp_git_client_subprocess_exited (GbpGitClient            *self,
                                  IdeSubprocess           *subprocess,
                                  IdeSubprocessSupervisor *supervisor)
{
  IDE_ENTRY;

  g_assert (GBP_IS_GIT_CLIENT (self));
  g_assert (IDE_IS_SUBPROCESS (subprocess));
  g_assert (IDE_IS_SUBPROCESS_SUPERVISOR (supervisor));

  if (self->state == STATE_RUNNING)
    self->state = STATE_SPAWNING;

  g_clear_object (&self->rpc_client);

  IDE_EXIT;
}

static void
gbp_git_client_subprocess_spawned (GbpGitClient            *self,
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

  g_assert (GBP_IS_GIT_CLIENT (self));
  g_assert (IDE_IS_SUBPROCESS (subprocess));
  g_assert (IDE_IS_SUBPROCESS_SUPERVISOR (supervisor));
  g_assert (self->rpc_client == NULL);

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

  g_signal_connect_object (self->rpc_client,
                           "notification",
                           G_CALLBACK (gbp_git_client_notification_cb),
                           self,
                           G_CONNECT_SWAPPED);

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

  jsonrpc_client_call_async (self->rpc_client, "initialize", params, NULL, NULL, NULL);

  IDE_EXIT;
}

static void
gbp_git_client_get_client_async (GbpGitClient        *self,
                                 GCancellable        *cancellable,
                                 GAsyncReadyCallback  callback,
                                 gpointer             user_data)
{
  g_autoptr(IdeTask) task = NULL;

  g_assert (GBP_IS_GIT_CLIENT (self));
  g_assert (!cancellable || G_IS_CANCELLABLE (cancellable));

  task = ide_task_new (self, cancellable, callback, user_data);
  ide_task_set_source_tag (task, gbp_git_client_get_client_async);

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
gbp_git_client_get_client_finish (GbpGitClient  *self,
                                  GAsyncResult  *result,
                                  GError       **error)
{
  g_assert (GBP_IS_GIT_CLIENT (self));
  g_assert (IDE_IS_TASK (result));

  return ide_task_propagate_object (IDE_TASK (result), error);
}

static void
gbp_git_client_parent_set (IdeObject *object,
                           IdeObject *parent)
{
  GbpGitClient *self = (GbpGitClient *)object;
  g_autoptr(IdeSubprocessLauncher) launcher = NULL;
  g_autoptr(GFile) workdir = NULL;
  g_autofree gchar *cwd = NULL;
  IdeContext *context;

  g_assert (GBP_IS_GIT_CLIENT (self));
  g_assert (!parent || IDE_IS_OBJECT (parent));

  if (parent == NULL)
    return;

  context = ide_object_get_context (IDE_OBJECT (self));
  workdir = ide_context_ref_workdir (context);

  self->root_uri = g_object_ref (workdir);

  if (g_file_is_native (workdir))
    cwd = g_file_get_path (workdir);

  launcher = ide_subprocess_launcher_new (G_SUBPROCESS_FLAGS_STDOUT_PIPE | G_SUBPROCESS_FLAGS_STDIN_PIPE);
  if (cwd != NULL)
    ide_subprocess_launcher_set_cwd (launcher, cwd);
  ide_subprocess_launcher_set_clear_env (launcher, FALSE);
  ide_subprocess_launcher_setenv (launcher, "DZL_COUNTER_DISABLE_SHM", "1", TRUE);
#if 0
  ide_subprocess_launcher_push_argv (launcher, "gdbserver");
  ide_subprocess_launcher_push_argv (launcher, "localhost:8889");
#endif
  ide_subprocess_launcher_push_argv (launcher, PACKAGE_LIBEXECDIR"/gnome-builder-git");

  self->supervisor = ide_subprocess_supervisor_new ();
  ide_subprocess_supervisor_set_launcher (self->supervisor, launcher);

  g_signal_connect_object (self->supervisor,
                           "spawned",
                           G_CALLBACK (gbp_git_client_subprocess_spawned),
                           self,
                           G_CONNECT_SWAPPED);

  g_signal_connect_object (self->supervisor,
                           "exited",
                           G_CALLBACK (gbp_git_client_subprocess_exited),
                           self,
                           G_CONNECT_SWAPPED);
}

static void
gbp_git_client_destroy (IdeObject *object)
{
  GbpGitClient *self = (GbpGitClient *)object;
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

  IDE_OBJECT_CLASS (gbp_git_client_parent_class)->destroy (object);
}

static void
gbp_git_client_finalize (GObject *object)
{
  GbpGitClient *self = (GbpGitClient *)object;

  g_clear_object (&self->rpc_client);
  g_clear_object (&self->root_uri);
  g_clear_object (&self->supervisor);
  g_clear_pointer (&self->notif_by_token, g_hash_table_unref);

  g_assert (self->get_client.head == NULL);
  g_assert (self->get_client.tail == NULL);
  g_assert (self->get_client.length == 0);

  G_OBJECT_CLASS (gbp_git_client_parent_class)->finalize (object);
}

static void
gbp_git_client_class_init (GbpGitClientClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  IdeObjectClass *i_object_class = IDE_OBJECT_CLASS (klass);

  object_class->finalize = gbp_git_client_finalize;

  i_object_class->parent_set = gbp_git_client_parent_set;
  i_object_class->destroy = gbp_git_client_destroy;;
}

static void
gbp_git_client_init (GbpGitClient *self)
{
}

GbpGitClient *
gbp_git_client_from_context (IdeContext *context)
{
  GbpGitClient *ret;

  g_return_val_if_fail (IDE_IS_CONTEXT (context), NULL);
  g_return_val_if_fail (!ide_object_in_destruction (IDE_OBJECT (context)), NULL);

  if (!(ret = ide_context_peek_child_typed (context, GBP_TYPE_GIT_CLIENT)))
    {
      g_autoptr(GbpGitClient) client = NULL;

      client = ide_object_ensure_child_typed (IDE_OBJECT (context), GBP_TYPE_GIT_CLIENT);
      ret = ide_context_peek_child_typed (context, GBP_TYPE_GIT_CLIENT);
    }

  return ret;
}

static void
gbp_git_client_call_cb (GObject      *object,
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
gbp_git_client_call_get_client_cb (GObject      *object,
                                   GAsyncResult *result,
                                   gpointer      user_data)
{
  GbpGitClient *self = (GbpGitClient *)object;
  g_autoptr(JsonrpcClient) client = NULL;
  g_autoptr(IdeTask) task = user_data;
  g_autoptr(GError) error = NULL;
  Call *call;

  g_assert (GBP_IS_GIT_CLIENT (self));
  g_assert (G_IS_ASYNC_RESULT (result));
  g_assert (IDE_IS_TASK (task));

  if (!(client = gbp_git_client_get_client_finish (self, result, &error)))
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
                                     gbp_git_client_call_cb,
                                     g_object_ref (task));
}

static void
gbp_git_client_call_cancelled (GCancellable *cancellable,
                               Call         *call)
{
  GVariantDict dict;

  g_assert (G_IS_CANCELLABLE (cancellable));
  g_assert (call != NULL);
  g_assert (call->cancellable == cancellable);
  g_assert (GBP_IS_GIT_CLIENT (call->self));

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

  gbp_git_client_call_async (call->self,
                             "$/cancelRequest",
                             g_variant_dict_end (&dict),
                             NULL, NULL, NULL);
}

static gboolean
gbp_git_client_call (GbpGitClient  *self,
                     const gchar   *method,
                     GVariant      *params,
                     GCancellable  *cancellable,
                     GVariant     **reply,
                     GError       **error)
{
  g_return_val_if_fail (GBP_IS_GIT_CLIENT (self), FALSE);
  g_return_val_if_fail (method != NULL, FALSE);
  g_return_val_if_fail (!cancellable || G_IS_CANCELLABLE (cancellable), FALSE);

  if (self->rpc_client == NULL)
    {
      g_set_error (error,
                   G_IO_ERROR,
                   G_IO_ERROR_NOT_CONNECTED,
                   "Cannot call synchronously without connection");
      return FALSE;
    }

  return jsonrpc_client_call (self->rpc_client, method, params, cancellable, reply, error);
}

void
gbp_git_client_call_async (GbpGitClient        *self,
                           const gchar         *method,
                           GVariant            *params,
                           GCancellable        *cancellable,
                           GAsyncReadyCallback  callback,
                           gpointer             user_data)
{
  g_autoptr(IdeTask) task = NULL;
  Call *call;

  g_return_if_fail (GBP_IS_GIT_CLIENT (self));
  g_return_if_fail (method != NULL);
  g_return_if_fail (!cancellable || G_IS_CANCELLABLE (cancellable));

  call = g_slice_new0 (Call);
  call->self = g_object_ref (self);
  call->method = g_strdup (method);
  call->params = params ? g_variant_ref_sink (params) : NULL;

  task = ide_task_new (self, cancellable, callback, user_data);
  ide_task_set_source_tag (task, gbp_git_client_call_async);
  ide_task_set_task_data (task, call, call_free);

  if (cancellable != NULL)
    {
      call->cancellable = g_object_ref (cancellable);
      call->cancel_id = g_cancellable_connect (cancellable,
                                               G_CALLBACK (gbp_git_client_call_cancelled),
                                               call,
                                               NULL);
      if (ide_task_return_error_if_cancelled (task))
        return;
    }

  gbp_git_client_get_client_async (self,
                                   cancellable,
                                   gbp_git_client_call_get_client_cb,
                                   g_steal_pointer (&task));
}

gboolean
gbp_git_client_call_finish (GbpGitClient  *self,
                            GAsyncResult    *result,
                            GVariant       **reply,
                            GError         **error)
{
  g_autoptr(GVariant) v = NULL;
  gboolean ret;

  g_return_val_if_fail (GBP_IS_GIT_CLIENT (self), FALSE);
  g_return_val_if_fail (IDE_IS_TASK (result), FALSE);

  ret = !!(v = ide_task_propagate_pointer (IDE_TASK (result), error));

  if (reply != NULL)
    *reply = g_steal_pointer (&v);

  return ret;
}

static void
gbp_git_client_is_ignored_cb (GObject      *object,
                              GAsyncResult *result,
                              gpointer      user_data)
{
  GbpGitClient *self = (GbpGitClient *)object;
  g_autoptr(IdeTask) task = user_data;
  g_autoptr(GVariant) reply = NULL;
  g_autoptr(GError) error = NULL;

  g_assert (GBP_IS_GIT_CLIENT (self));
  g_assert (G_IS_ASYNC_RESULT (result));
  g_assert (IDE_IS_TASK (task));

  if (!gbp_git_client_call_finish (self, result, &reply, &error))
    ide_task_return_error (task, g_steal_pointer (&error));
  else
    ide_task_return_pointer (task, g_steal_pointer (&reply), g_variant_unref);
}

void
gbp_git_client_is_ignored_async (GbpGitClient        *self,
                                 const gchar         *path,
                                 GCancellable        *cancellable,
                                 GAsyncReadyCallback  callback,
                                 gpointer             user_data)
{
  g_autoptr(IdeTask) task = NULL;
  g_autoptr(GVariant) params = NULL;

  g_return_if_fail (GBP_IS_GIT_CLIENT (self));
  g_return_if_fail (path != NULL);
  g_return_if_fail (!cancellable || G_IS_CANCELLABLE (cancellable));

  task = ide_task_new (self, cancellable, callback, user_data);
  ide_task_set_source_tag (task, gbp_git_client_is_ignored_async);

  params = JSONRPC_MESSAGE_NEW (
    "path", JSONRPC_MESSAGE_PUT_STRING (path)
  );

  gbp_git_client_call_async (self,
                             "git/isIgnored",
                             params,
                             cancellable,
                             gbp_git_client_is_ignored_cb,
                             g_steal_pointer (&task));
}

gboolean
gbp_git_client_is_ignored_finish (GbpGitClient  *self,
                                  GAsyncResult  *result,
                                  GError       **error)
{
  g_autoptr(GVariant) ret = NULL;

  g_return_val_if_fail (GBP_IS_GIT_CLIENT (self), FALSE);
  g_return_val_if_fail (IDE_IS_TASK (result), FALSE);

  if (!(ret = ide_task_propagate_pointer (IDE_TASK (result), error)))
    return FALSE;

  if (g_variant_is_of_type (ret, G_VARIANT_TYPE_BOOLEAN))
    return g_variant_get_boolean (ret);

  g_set_error (error,
               G_IO_ERROR,
               G_IO_ERROR_INVALID_DATA,
               "Expected boolean reply");

  return FALSE;
}

static void
gbp_git_client_clone_url_cb (GObject      *object,
                             GAsyncResult *result,
                             gpointer      user_data)
{
  GbpGitClient *self = (GbpGitClient *)object;
  g_autoptr(IdeTask) task = user_data;
  g_autoptr(GVariant) reply = NULL;
  g_autoptr(GError) error = NULL;
  const gchar *token = NULL;

  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (GBP_IS_GIT_CLIENT (self));
  g_assert (G_IS_ASYNC_RESULT (result));
  g_assert (IDE_IS_TASK (task));

  if (!gbp_git_client_call_finish (self, result, &reply, &error))
    ide_task_return_error (task, g_steal_pointer (&error));
  else
    ide_task_return_boolean (task, TRUE);

  if ((token = ide_task_get_task_data (task)) && self->notif_by_token)
    g_hash_table_remove (self->notif_by_token, token);
}

void
gbp_git_client_clone_url_async (GbpGitClient        *self,
                                const gchar         *url,
                                GFile               *destination,
                                const gchar         *branch,
                                IdeNotification     *notif,
                                GCancellable        *cancellable,
                                GAsyncReadyCallback  callback,
                                gpointer             user_data)
{
  g_autoptr(IdeTask) task = NULL;
  g_autoptr(GVariant) command = NULL;
  g_autofree gchar *token = NULL;
  g_autofree gchar *dest_uri = NULL;

  g_return_if_fail (GBP_IS_GIT_CLIENT (self));
  g_return_if_fail (url != NULL);
  g_return_if_fail (G_IS_FILE (destination));
  g_return_if_fail (!notif || IDE_IS_NOTIFICATION (notif));
  g_return_if_fail (!cancellable || G_IS_CANCELLABLE (cancellable));

  task = ide_task_new (self, cancellable, callback, user_data);
  ide_task_set_source_tag (task, gbp_git_client_clone_url_async);

  if (notif != NULL)
    {
      token = gbp_git_client_track_progress (self, notif);
      ide_notification_set_title (notif, _("Cloning Repository…"));
      ide_notification_set_icon_name (notif, "builder-vcs-git-symbolic");
      ide_notification_set_progress (notif, 0.0);
    }

  if (branch == NULL)
    branch = "master";

  dest_uri = g_file_get_uri (destination);

  command = JSONRPC_MESSAGE_NEW (
    "token", JSONRPC_MESSAGE_PUT_STRING (token),
    "url", JSONRPC_MESSAGE_PUT_STRING (url),
    "destination", JSONRPC_MESSAGE_PUT_STRING (dest_uri),
    "branch", JSONRPC_MESSAGE_PUT_STRING (branch)
  );

  if (token != NULL)
    ide_task_set_task_data (task, g_strdup (token), g_free);

  gbp_git_client_call_async (self,
                             "git/cloneUrl",
                             command,
                             cancellable,
                             gbp_git_client_clone_url_cb,
                             g_steal_pointer (&task));
}

gboolean
gbp_git_client_clone_url_finish (GbpGitClient  *self,
                                 GAsyncResult  *result,
                                 GError       **error)
{
  g_return_val_if_fail (GBP_IS_GIT_CLIENT (self), FALSE);
  g_return_val_if_fail (IDE_IS_TASK (result), FALSE);

  return ide_task_propagate_boolean (IDE_TASK (result), error);
}

static void
gbp_git_client_update_submodules_cb (GObject      *object,
                                     GAsyncResult *result,
                                     gpointer      user_data)
{
  GbpGitClient *self = (GbpGitClient *)object;
  g_autoptr(IdeTask) task = user_data;
  g_autoptr(GVariant) reply = NULL;
  g_autoptr(GError) error = NULL;
  const gchar *token = NULL;

  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (GBP_IS_GIT_CLIENT (self));
  g_assert (G_IS_ASYNC_RESULT (result));
  g_assert (IDE_IS_TASK (task));

  if (!gbp_git_client_call_finish (self, result, &reply, &error))
    ide_task_return_error (task, g_steal_pointer (&error));
  else
    ide_task_return_boolean (task, TRUE);

  if ((token = ide_task_get_task_data (task)) && self->notif_by_token)
    g_hash_table_remove (self->notif_by_token, token);
}

void
gbp_git_client_update_submodules_async (GbpGitClient        *self,
                                        IdeNotification     *notif,
                                        GCancellable        *cancellable,
                                        GAsyncReadyCallback  callback,
                                        gpointer             user_data)
{
  g_autoptr(IdeTask) task = NULL;
  g_autoptr(GVariant) command = NULL;
  g_autofree gchar *token = NULL;

  g_return_if_fail (GBP_IS_GIT_CLIENT (self));
  g_return_if_fail (!notif || IDE_IS_NOTIFICATION (notif));
  g_return_if_fail (!cancellable || G_IS_CANCELLABLE (cancellable));

  task = ide_task_new (self, cancellable, callback, user_data);
  ide_task_set_source_tag (task, gbp_git_client_update_submodules_async);

  if (notif != NULL)
    {
      token = gbp_git_client_track_progress (self, notif);
      ide_notification_set_title (notif, _("Updating Git Submodules…"));
      ide_notification_set_icon_name (notif, "builder-vcs-git-symbolic");
      ide_notification_set_progress (notif, 0.0);
      ide_task_set_task_data (task, g_strdup (token), g_free);
    }

  command = JSONRPC_MESSAGE_NEW (
    "token", JSONRPC_MESSAGE_PUT_STRING (token)
  );

  gbp_git_client_call_async (self,
                             "git/updateSubmodules",
                             command,
                             cancellable,
                             gbp_git_client_update_submodules_cb,
                             g_steal_pointer (&task));
}

gboolean
gbp_git_client_update_submodules_finish (GbpGitClient  *self,
                                         GAsyncResult  *result,
                                         GError       **error)
{
  g_return_val_if_fail (GBP_IS_GIT_CLIENT (self), FALSE);
  g_return_val_if_fail (IDE_IS_TASK (result), FALSE);

  return ide_task_propagate_boolean (IDE_TASK (result), error);
}

static void
gbp_git_client_update_config_cb (GObject      *object,
                                 GAsyncResult *result,
                                 gpointer      user_data)
{
  GbpGitClient *self = (GbpGitClient *)object;
  g_autoptr(IdeTask) task = user_data;
  g_autoptr(GVariant) reply = NULL;
  g_autoptr(GError) error = NULL;

  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (GBP_IS_GIT_CLIENT (self));
  g_assert (G_IS_ASYNC_RESULT (result));
  g_assert (IDE_IS_TASK (task));

  if (!gbp_git_client_call_finish (self, result, &reply, &error))
    ide_task_return_error (task, g_steal_pointer (&error));
  else
    ide_task_return_boolean (task, TRUE);
}

gboolean
gbp_git_client_update_config (GbpGitClient  *self,
                              gboolean       global,
                              const gchar   *key,
                              GVariant      *value,
                              GCancellable  *cancellable,
                              GError       **error)
{
  g_autoptr(GVariant) reply = NULL;
  GVariantDict dict;

  g_return_val_if_fail (GBP_IS_GIT_CLIENT (self), FALSE);
  g_return_val_if_fail (key != NULL, FALSE);
  g_return_val_if_fail (!cancellable || G_IS_CANCELLABLE (cancellable), FALSE);

  g_variant_dict_init (&dict, NULL);
  g_variant_dict_insert (&dict, "global", "b", global);
  g_variant_dict_insert (&dict, "key", "s", key);
  g_variant_dict_insert_value (&dict, "value", value);

  return gbp_git_client_call (self,
                              "git/updateConfig",
                              g_variant_dict_end (&dict),
                              cancellable,
                              &reply,
                              error);
}

void
gbp_git_client_update_config_async (GbpGitClient        *self,
                                    gboolean             global,
                                    const gchar         *key,
                                    GVariant            *value,
                                    GCancellable        *cancellable,
                                    GAsyncReadyCallback  callback,
                                    gpointer             user_data)
{
  g_autoptr(IdeTask) task = NULL;
  GVariantDict dict;

  g_return_if_fail (GBP_IS_GIT_CLIENT (self));
  g_return_if_fail (key != NULL);
  g_return_if_fail (!cancellable || G_IS_CANCELLABLE (cancellable));

  task = ide_task_new (self, cancellable, callback, user_data);
  ide_task_set_source_tag (task, gbp_git_client_update_config_async);

  g_variant_dict_init (&dict, NULL);
  g_variant_dict_insert (&dict, "global", "b", global);
  g_variant_dict_insert (&dict, "key", "s", key);
  g_variant_dict_insert_value (&dict, "value", value);

  gbp_git_client_call_async (self,
                             "git/updateConfig",
                             g_variant_dict_end (&dict),
                             cancellable,
                             gbp_git_client_update_config_cb,
                             g_steal_pointer (&task));
}

gboolean
gbp_git_client_update_config_finish (GbpGitClient  *self,
                                     GAsyncResult  *result,
                                     GError       **error)
{
  g_return_val_if_fail (GBP_IS_GIT_CLIENT (self), FALSE);
  g_return_val_if_fail (IDE_IS_TASK (result), FALSE);

  return ide_task_propagate_boolean (IDE_TASK (result), error);
}

GVariant *
gbp_git_client_read_config (GbpGitClient  *self,
                            const gchar   *key,
                            GCancellable  *cancellable,
                            GError       **error)
{
  g_autoptr(GVariant) reply = NULL;

  g_return_val_if_fail (GBP_IS_GIT_CLIENT (self), FALSE);
  g_return_val_if_fail (key != NULL, FALSE);
  g_return_val_if_fail (!cancellable || G_IS_CANCELLABLE (cancellable), FALSE);

  if (!gbp_git_client_call (self,
                            "git/readConfig",
                            g_variant_new_string (key),
                            cancellable,
                            &reply,
                            error))
    return NULL;

  return g_steal_pointer (&reply);
}

static void
gbp_git_client_create_repo_cb (GObject      *object,
                               GAsyncResult *result,
                               gpointer      user_data)
{
  GbpGitClient *self = (GbpGitClient *)object;
  g_autoptr(IdeTask) task = user_data;
  g_autoptr(GVariant) reply = NULL;
  g_autoptr(GError) error = NULL;

  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (GBP_IS_GIT_CLIENT (self));
  g_assert (G_IS_ASYNC_RESULT (result));
  g_assert (IDE_IS_TASK (task));

  if (!gbp_git_client_call_finish (self, result, &reply, &error))
    ide_task_return_error (task, g_steal_pointer (&error));
  else
    ide_task_return_boolean (task, TRUE);
}

void
gbp_git_client_create_repo_async (GbpGitClient        *self,
                                  GFile               *in_directory,
                                  gboolean             bare,
                                  GCancellable        *cancellable,
                                  GAsyncReadyCallback  callback,
                                  gpointer             user_data)
{
  g_autoptr(IdeTask) task = NULL;
  g_autoptr(GVariant) command = NULL;
  g_autofree gchar *uri = NULL;

  g_return_if_fail (GBP_IS_GIT_CLIENT (self));
  g_return_if_fail (G_IS_FILE (in_directory));
  g_return_if_fail (!cancellable || G_IS_CANCELLABLE (cancellable));

  task = ide_task_new (self, cancellable, callback, user_data);
  ide_task_set_source_tag (task, gbp_git_client_create_repo_async);

  uri = g_file_get_uri (in_directory);

  command = JSONRPC_MESSAGE_NEW (
    "location", JSONRPC_MESSAGE_PUT_STRING (uri),
    "bare", JSONRPC_MESSAGE_PUT_BOOLEAN (bare)
  );

  gbp_git_client_call_async (self,
                             "git/createRepo",
                             command,
                             cancellable,
                             gbp_git_client_create_repo_cb,
                             g_steal_pointer (&task));
}

gboolean
gbp_git_client_create_repo_finish (GbpGitClient  *self,
                                   GAsyncResult  *result,
                                   GError       **error)
{
  g_return_val_if_fail (GBP_IS_GIT_CLIENT (self), FALSE);
  g_return_val_if_fail (IDE_IS_TASK (result), FALSE);

  return ide_task_propagate_boolean (IDE_TASK (result), error);
}

typedef struct
{
  GFile *workdir;
  GFile *dot_git;
  gchar *branch;
  guint  is_worktree : 1;
} Discover;

static void
discover_free (Discover *state)
{
  g_clear_object (&state->workdir);
  g_clear_object (&state->dot_git);
  g_clear_pointer (&state->branch, g_free);
  g_slice_free (Discover, state);
}

static void
gbp_git_client_discover_cb (GObject      *object,
                            GAsyncResult *result,
                            gpointer      user_data)
{
  GbpGitClient *self = (GbpGitClient *)object;
  g_autoptr(IdeTask) task = user_data;
  g_autoptr(GVariant) reply = NULL;
  g_autoptr(GError) error = NULL;
  const gchar *gitdir = NULL;
  const gchar *workdir = NULL;
  const gchar *branch = NULL;
  Discover *state;
  gboolean is_worktree = FALSE;
  gboolean r;

  g_assert (GBP_IS_GIT_CLIENT (self));
  g_assert (G_IS_ASYNC_RESULT (result));
  g_assert (IDE_IS_TASK (task));

  if (!gbp_git_client_call_finish (self, result, &reply, &error))
    {
      ide_task_return_error (task, g_steal_pointer (&error));
      return;
    }

  state = ide_task_get_task_data (task);

  r = JSONRPC_MESSAGE_PARSE (reply,
    "workdir", JSONRPC_MESSAGE_GET_STRING (&workdir),
    "gitdir", JSONRPC_MESSAGE_GET_STRING (&gitdir),
    "branch", JSONRPC_MESSAGE_GET_STRING (&branch),
    "is-worktree", JSONRPC_MESSAGE_GET_BOOLEAN (&is_worktree)
  );

  if (!r)
    {
      ide_task_return_new_error (task,
                                 G_IO_ERROR,
                                 G_IO_ERROR_INVALID_DATA,
                                 "Invalid reply from peer");
      return;
    }

  state->dot_git = g_file_new_for_uri (gitdir);
  state->workdir = g_file_new_for_uri (workdir);
  state->branch = g_strdup (branch);
  state->is_worktree = !!is_worktree;

  ide_task_return_boolean (task, TRUE);
}

void
gbp_git_client_discover_async (GbpGitClient        *self,
                               GFile               *directory,
                               GCancellable        *cancellable,
                               GAsyncReadyCallback  callback,
                               gpointer             user_data)
{
  g_autoptr(IdeTask) task = NULL;
  g_autoptr(GVariant) command = NULL;
  g_autofree gchar *uri = NULL;
  Discover *state;

  g_return_if_fail (GBP_IS_GIT_CLIENT (self));
  g_return_if_fail (G_IS_FILE (directory));
  g_return_if_fail (!cancellable || G_IS_CANCELLABLE (cancellable));

  state = g_slice_new0 (Discover);

  task = ide_task_new (self, cancellable, callback, user_data);
  ide_task_set_source_tag (task, gbp_git_client_discover_async);
  ide_task_set_task_data (task, state, discover_free);

  uri = g_file_get_uri (directory);

  command = JSONRPC_MESSAGE_NEW (
    "location", JSONRPC_MESSAGE_PUT_STRING (uri)
  );

  gbp_git_client_call_async (self,
                             "git/discover",
                             command,
                             cancellable,
                             gbp_git_client_discover_cb,
                             g_steal_pointer (&task));
}

gboolean
gbp_git_client_discover_finish (GbpGitClient  *self,
                                GAsyncResult  *result,
                                GFile        **workdir,
                                GFile        **dot_git,
                                gchar        **branch,
                                gboolean      *is_worktree,
                                GError       **error)
{
  g_return_val_if_fail (GBP_IS_GIT_CLIENT (self), FALSE);
  g_return_val_if_fail (IDE_IS_TASK (result), FALSE);

  ide_clear_param (workdir, NULL);
  ide_clear_param (dot_git, NULL);
  ide_clear_param (branch, NULL);
  ide_clear_param (is_worktree, FALSE);

  if (ide_task_propagate_boolean (IDE_TASK (result), error))
    {
      Discover *state = ide_task_get_task_data (IDE_TASK (result));

      g_assert (state != NULL);

      if (workdir != NULL)
        *workdir = g_steal_pointer (&state->workdir);

      if (dot_git != NULL)
        *dot_git = g_steal_pointer (&state->dot_git);

      if (branch != NULL)
        *branch = g_steal_pointer (&state->branch);

      if (is_worktree != NULL)
        *is_worktree = state->is_worktree;

      return TRUE;
    }

  return FALSE;
}

static void
gbp_git_client_get_changes_cb (GObject      *object,
                               GAsyncResult *result,
                               gpointer      user_data)
{
  GbpGitClient *self = (GbpGitClient *)object;
  g_autoptr(IdeTask) task = user_data;
  g_autoptr(GVariant) reply = NULL;
  g_autoptr(GError) error = NULL;
  LineCache *cache;

  g_assert (GBP_IS_GIT_CLIENT (self));
  g_assert (G_IS_ASYNC_RESULT (result));
  g_assert (IDE_IS_TASK (task));

  if (!gbp_git_client_call_finish (self, result, &reply, &error))
    {
      ide_task_return_error (task, g_steal_pointer (&error));
      return;
    }

  if (!(cache = line_cache_new_from_variant (reply)))
    {
      ide_task_return_new_error (task,
                                 G_IO_ERROR,
                                 G_IO_ERROR_INVALID_DATA,
                                 "Invalid line-cache data from peer");
      return;
    }

  ide_task_return_pointer (task, g_steal_pointer (&cache), line_cache_free);
}

void
gbp_git_client_get_changes_async (GbpGitClient        *self,
                                  const gchar         *path,
                                  const gchar         *contents,
                                  GCancellable        *cancellable,
                                  GAsyncReadyCallback  callback,
                                  gpointer             user_data)
{
  g_autoptr(IdeTask) task = NULL;
  g_autoptr(GVariant) command = NULL;

  g_assert (GBP_IS_GIT_CLIENT (self));
  g_assert (path != NULL);
  g_assert (contents != NULL);
  g_assert (!cancellable || G_IS_CANCELLABLE (cancellable));

  task = ide_task_new (self, cancellable, callback, user_data);
  ide_task_set_source_tag (task, gbp_git_client_get_changes_async);

  command = JSONRPC_MESSAGE_NEW (
    "path", JSONRPC_MESSAGE_PUT_STRING (path),
    "contents", JSONRPC_MESSAGE_PUT_STRING (contents)
  );

  gbp_git_client_call_async (self,
                             "git/getChanges",
                             command,
                             cancellable,
                             gbp_git_client_get_changes_cb,
                             g_steal_pointer (&task));
}

LineCache *
gbp_git_client_get_changes_finish (GbpGitClient  *self,
                                   GAsyncResult  *result,
                                   GError       **error)
{
  g_return_val_if_fail (GBP_IS_GIT_CLIENT (self), NULL);
  g_return_val_if_fail (IDE_IS_TASK (result), NULL);

  return ide_task_propagate_pointer (IDE_TASK (result), error);
}
