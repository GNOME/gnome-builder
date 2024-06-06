/* gbp-git-client.c
 *
 * Copyright 2019 Christian Hergert <chergert@redhat.com>
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

#include <sys/socket.h>

#include <glib/gi18n.h>
#include <glib/gstdio.h>
#include <glib-unix.h>

#include <gio/gunixinputstream.h>
#include <gio/gunixoutputstream.h>

#include <libide-threading.h>

#include "gbp-git-client.h"

struct _GbpGitClient
{
  IdeObject                parent;
  IdeSubprocessSupervisor *supervisor;
  GDBusConnection         *connection;
  IpcGitService           *service;
  GQueue                   get_service;
  gint                     state;
};

enum {
  STATE_INITIAL,
  STATE_SPAWNING,
  STATE_RUNNING,
  STATE_SHUTDOWN,
};

G_DEFINE_FINAL_TYPE (GbpGitClient, gbp_git_client, IDE_TYPE_OBJECT)

static void
new_connection_cb (GObject      *object,
                   GAsyncResult *result,
                   gpointer      user_data)
{
  g_autoptr(GbpGitClient) self = user_data;
  g_autoptr(GDBusConnection) connection = NULL;
  g_autoptr(GError) error = NULL;
  GList *queued;

  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (GBP_IS_GIT_CLIENT (self));
  g_assert (G_IS_ASYNC_RESULT (result));

  if (!(connection = g_dbus_connection_new_finish (result, &error)))
    {
      g_warning ("Failed to create D-Bus connection to gnome-builder-git: %s",
                 error->message);
      return;
    }

  g_set_object (&self->connection, connection);

  g_dbus_connection_start_message_processing (self->connection);

  self->service = ipc_git_service_proxy_new_sync (self->connection,
                                                  G_DBUS_PROXY_FLAGS_DO_NOT_LOAD_PROPERTIES,
                                                  NULL,
                                                  "/org/gnome/Builder/Git",
                                                  NULL,
                                                  NULL);

  /* We can have long running operations, so set no timeout */
  g_dbus_proxy_set_default_timeout (G_DBUS_PROXY (self->service), G_MAXINT);

  queued = g_steal_pointer (&self->get_service.head);

  self->get_service.head = NULL;
  self->get_service.tail = NULL;
  self->get_service.length = 0;

  for (const GList *iter = queued; iter != NULL; iter = iter->next)
    {
      IdeTask *task = iter->data;

      ide_task_return_object (task, g_object_ref (self->service));
    }

  g_list_free_full (queued, g_object_unref);
}


static gboolean
gbp_git_client_subprocess_supervise (GbpGitClient            *self,
                                     IdeSubprocessLauncher   *launcher,
                                     IdeSubprocessSupervisor *supervisor)
{
  int pair[2] = {-1, -1};
  int res;

  IDE_ENTRY;

  g_assert (GBP_IS_GIT_CLIENT (self));
  g_assert (IDE_IS_SUBPROCESS_LAUNCHER (launcher));
  g_assert (IDE_IS_SUBPROCESS_SUPERVISOR (supervisor));

  g_clear_object (&self->service);
  g_clear_object (&self->connection);

#ifdef __APPLE__
  res = socketpair (AF_UNIX, SOCK_STREAM, 0, pair);

  if (res != -1)
    {
      fcntl (pair[0], F_SETFD, FD_CLOEXEC);
      fcntl (pair[1], F_SETFD, FD_CLOEXEC);

      g_unix_set_fd_nonblocking (pair[0], TRUE, NULL);
      g_unix_set_fd_nonblocking (pair[1], TRUE, NULL);
    }
#else
  res = socketpair (AF_UNIX, SOCK_STREAM|SOCK_NONBLOCK|SOCK_CLOEXEC, 0, pair);
#endif

  if (res == 0)
    {
      g_autoptr(GSocket) socket = g_socket_new_from_fd (pair[0], NULL);

      if (socket != NULL)
        {
          g_autoptr(GSocketConnection) stream = NULL;
          g_autofree char *guid = NULL;

          pair[0] = -1;

          ide_subprocess_launcher_take_fd (launcher, g_steal_fd (&pair[1]), 3);

          stream = g_socket_connection_factory_create_connection (socket);
          guid = g_dbus_generate_guid ();
          g_dbus_connection_new (G_IO_STREAM (stream), guid,
                                 (G_DBUS_CONNECTION_FLAGS_DELAY_MESSAGE_PROCESSING |
                                  G_DBUS_CONNECTION_FLAGS_AUTHENTICATION_SERVER),
                                 NULL,
                                 NULL,
                                 new_connection_cb,
                                 g_object_ref (self));
        }
    }

  g_clear_fd (&pair[0], NULL);
  g_clear_fd (&pair[1], NULL);

  IDE_RETURN (FALSE);
}

static void
gbp_git_client_subprocess_spawned (GbpGitClient            *self,
                                   IdeSubprocess           *subprocess,
                                   IdeSubprocessSupervisor *supervisor)
{
  IDE_ENTRY;

  g_assert (GBP_IS_GIT_CLIENT (self));
  g_assert (IDE_IS_SUBPROCESS (subprocess));
  g_assert (IDE_IS_SUBPROCESS_SUPERVISOR (supervisor));

  ide_object_message (self,
                      _("Git integration has started as process %s"),
                      ide_subprocess_get_identifier (subprocess));

  ide_object_lock (IDE_OBJECT (self));

  if (self->state == STATE_SPAWNING)
    self->state = STATE_RUNNING;

  ide_object_unlock (IDE_OBJECT (self));

  IDE_EXIT;
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

  ide_object_message (self, _("Git integration has exited"));

  ide_object_lock (IDE_OBJECT (self));

  if (self->state == STATE_RUNNING)
    self->state = STATE_SPAWNING;

  g_clear_object (&self->service);
  g_clear_object (&self->connection);

  ide_object_unlock (IDE_OBJECT (self));

  IDE_EXIT;
}

static void
gbp_git_client_destroy (IdeObject *object)
{
  GbpGitClient *self = (GbpGitClient *)object;
  g_autoptr(IdeSubprocessSupervisor) supervisor = g_steal_pointer (&self->supervisor);

  if (supervisor != NULL)
    ide_subprocess_supervisor_stop (supervisor);

  IDE_OBJECT_CLASS (gbp_git_client_parent_class)->destroy (object);
}

static void
gbp_git_client_parent_set (IdeObject *object,
                           IdeObject *parent)
{
  GbpGitClient *self=  (GbpGitClient *)object;
  g_autoptr(IdeSubprocessLauncher) launcher = NULL;

  g_assert (GBP_IS_GIT_CLIENT (self));
  g_assert (!parent || IDE_IS_OBJECT (parent));

  if (parent == NULL)
    return;

  ide_object_lock (IDE_OBJECT (self));

  launcher = ide_subprocess_launcher_new (0);
  ide_subprocess_launcher_set_cwd (launcher, g_get_home_dir ());
  ide_subprocess_launcher_set_clear_env (launcher, FALSE);
#if 0
  ide_subprocess_launcher_push_argv (launcher, "gdbserver");
  ide_subprocess_launcher_push_argv (launcher, "localhost:8888");
#endif
  ide_subprocess_launcher_push_argv (launcher, PACKAGE_LIBEXECDIR"/gnome-builder-git");

  self->supervisor = ide_subprocess_supervisor_new ();
  ide_subprocess_supervisor_set_launcher (self->supervisor, launcher);

  g_signal_connect_object (self->supervisor,
                           "supervise",
                           G_CALLBACK (gbp_git_client_subprocess_supervise),
                           self,
                           G_CONNECT_SWAPPED);

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

  ide_object_unlock (IDE_OBJECT (self));
}

static void
gbp_git_client_class_init (GbpGitClientClass *klass)
{
  IdeObjectClass *i_object_class = IDE_OBJECT_CLASS (klass);

  i_object_class->destroy = gbp_git_client_destroy;
  i_object_class->parent_set = gbp_git_client_parent_set;
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
gbp_git_client_get_service_cb (GObject      *object,
                               GAsyncResult *result,
                               gpointer      user_data)
{
  GbpGitClient *self = (GbpGitClient *)object;
  g_autoptr(IpcGitService) service = NULL;
  g_autoptr(IdeTask) task = user_data;
  g_autoptr(GError) error = NULL;

  g_assert (GBP_IS_GIT_CLIENT (self));
  g_assert (G_IS_ASYNC_RESULT (result));
  g_assert (IDE_IS_TASK (task));

  if (!(service = gbp_git_client_get_service_finish (self, result, &error)))
    ide_task_return_error (task, g_steal_pointer (&error));
  else
    ide_task_return_object (task, g_steal_pointer (&service));
}

IpcGitService *
gbp_git_client_get_service (GbpGitClient  *self,
                            GCancellable  *cancellable,
                            GError       **error)
{
  g_autoptr(IdeTask) task = NULL;
  g_autoptr(GMainContext) gcontext = NULL;
  IpcGitService *ret = NULL;

  g_assert (GBP_IS_GIT_CLIENT (self));
  g_assert (!cancellable || G_IS_CANCELLABLE (cancellable));

  ide_object_lock (IDE_OBJECT (self));
  if (self->service != NULL)
    ret = g_object_ref (self->service);
  ide_object_unlock (IDE_OBJECT (self));

  if (ret != NULL)
    return g_steal_pointer (&ret);

  task = ide_task_new (self, cancellable, NULL, NULL);
  ide_task_set_source_tag (task, gbp_git_client_get_service);

  gcontext = g_main_context_ref_thread_default ();

  gbp_git_client_get_service_async (self,
                                    cancellable,
                                    gbp_git_client_get_service_cb,
                                    g_object_ref (task));

  while (!ide_task_get_completed (task))
    g_main_context_iteration (gcontext, TRUE);

  return ide_task_propagate_object (task, error);
}

void
gbp_git_client_get_service_async (GbpGitClient        *self,
                                  GCancellable        *cancellable,
                                  GAsyncReadyCallback  callback,
                                  gpointer             user_data)
{
  g_autoptr(IdeTask) task = NULL;

  g_assert (GBP_IS_GIT_CLIENT (self));
  g_assert (!cancellable || G_IS_CANCELLABLE (cancellable));

  ide_object_lock (IDE_OBJECT (self));

  task = ide_task_new (self, cancellable, callback, user_data);
  ide_task_set_source_tag (task, gbp_git_client_get_service_async);

  switch (self->state)
    {
    case STATE_INITIAL:
      self->state = STATE_SPAWNING;
      g_queue_push_tail (&self->get_service, g_steal_pointer (&task));
      ide_subprocess_supervisor_start (self->supervisor);
      break;

    case STATE_SPAWNING:
      g_queue_push_tail (&self->get_service, g_steal_pointer (&task));
      break;

    case STATE_RUNNING:
      if (self->service != NULL)
        ide_task_return_object (task, g_object_ref (self->service));
      else
        g_queue_push_tail (&self->get_service, g_steal_pointer (&task));
      break;

    case STATE_SHUTDOWN:
      ide_task_return_new_error (task,
                                 G_IO_ERROR,
                                 G_IO_ERROR_CLOSED,
                                 _("The client has been closed"));
      break;

    default:
      g_assert_not_reached ();
      break;
    }

  ide_object_unlock (IDE_OBJECT (self));
}

IpcGitService *
gbp_git_client_get_service_finish (GbpGitClient  *self,
                                   GAsyncResult  *result,
                                   GError       **error)
{
  g_assert (GBP_IS_GIT_CLIENT (self));
  g_assert (IDE_IS_TASK (result));

  return ide_task_propagate_object (IDE_TASK (result), error);
}
