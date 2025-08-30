/* gbp-flatpak-client.c
 *
 * Copyright 2019-2021 Christian Hergert <chergert@redhat.com>
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

#define G_LOG_DOMAIN "gbp-flatpak-client"

#include "config.h"

#include <gio/gunixinputstream.h>
#include <gio/gunixoutputstream.h>
#include <glib/gi18n.h>
#include <glib-unix.h>
#include <libide-threading.h>

#include "gbp-flatpak-client.h"

struct _GbpFlatpakClient
{
  GObject                  parent_instance;
  IdeSubprocessSupervisor *supervisor;
  GDBusConnection         *connection;
  IpcFlatpakService       *service;
  GMutex                   mutex;
  GQueue                   get_service;
  int                      state;
  guint                    disposed : 1;
};

enum {
  STATE_INITIAL,
  STATE_SPAWNING,
  STATE_RUNNING,
  STATE_SHUTDOWN,
};

G_DEFINE_FINAL_TYPE (GbpFlatpakClient, gbp_flatpak_client, G_TYPE_OBJECT)

static void
gbp_flatpak_client_reset (GbpFlatpakClient *self)
{
  g_autoptr(GInputStream) input_stream = NULL;
  g_autoptr(GOutputStream) output_stream = NULL;
  g_autoptr(GIOStream) io_stream = NULL;
  g_autoptr(GDBusConnection) service_connection = NULL;
  g_autoptr(IdeSubprocessLauncher) launcher = NULL;
  g_autoptr(GError) error = NULL;
  g_autofree char *data_dir = NULL;
  struct {
    int read;
    int write;
  } pipe_fds[2] = {{-1, -1}, {-1, -1}};

  IDE_ENTRY;

  g_assert (GBP_IS_FLATPAK_CLIENT (self));

  if (pipe ((int *)&pipe_fds[0]) != 0)
    IDE_GOTO (handle_error);

  if (pipe ((int *)&pipe_fds[1]) != 0)
    IDE_GOTO (handle_error);

  g_assert (pipe_fds[0].read >= 0);
  g_assert (pipe_fds[0].write >= 0);
  g_assert (pipe_fds[1].read >= 0);
  g_assert (pipe_fds[1].write >= 0);

  if (!g_unix_set_fd_nonblocking (pipe_fds[0].read, TRUE, &error) ||
      !g_unix_set_fd_nonblocking (pipe_fds[0].write, TRUE, &error) ||
      !g_unix_set_fd_nonblocking (pipe_fds[1].read, TRUE, &error) ||
      !g_unix_set_fd_nonblocking (pipe_fds[1].write, TRUE, &error))
    IDE_GOTO (handle_error);

  input_stream = g_unix_input_stream_new (pipe_fds[0].read, TRUE);
  pipe_fds[0].read = -1;

  output_stream = g_unix_output_stream_new (pipe_fds[1].write, TRUE);
  pipe_fds[1].write = -1;

  io_stream = g_simple_io_stream_new (input_stream, output_stream);
  service_connection = g_dbus_connection_new_sync (io_stream, NULL,
                                                   G_DBUS_CONNECTION_FLAGS_DELAY_MESSAGE_PROCESSING,
                                                   NULL, NULL, NULL);
  g_dbus_connection_set_exit_on_close (service_connection, FALSE);

  launcher = ide_subprocess_launcher_new (0);
  ide_subprocess_launcher_set_cwd (launcher, g_get_home_dir ());
  ide_subprocess_launcher_set_clear_env (launcher, FALSE);

  if (g_getenv ("BUILDER_FLATPAK_DEBUG") != NULL)
    {
      ide_subprocess_launcher_setenv (launcher, "G_DEBUG", "fatal-criticals", TRUE);
      ide_subprocess_launcher_push_argv (launcher, "gdbserver");
      ide_subprocess_launcher_push_argv (launcher, "localhost:8888");
    }

  ide_subprocess_launcher_take_fd (launcher, pipe_fds[1].read, 3);
  pipe_fds[1].read = -1;

  ide_subprocess_launcher_take_fd (launcher, pipe_fds[0].write, 4);
  pipe_fds[0].write = -1;

  ide_subprocess_launcher_push_argv (launcher, PACKAGE_LIBEXECDIR"/gnome-builder-flatpak");
  ide_subprocess_launcher_push_argv (launcher, "--read-fd=3");
  ide_subprocess_launcher_push_argv (launcher, "--write-fd=4");

  /* Setup our data-dir to be the build artifact directory */
  data_dir = ide_dup_default_cache_dir ();
  ide_subprocess_launcher_push_argv (launcher, "--data-dir");
  ide_subprocess_launcher_push_argv (launcher, data_dir);

  if (ide_log_get_verbosity () > 0)
    ide_subprocess_launcher_push_argv (launcher, "--verbose");

  g_set_object (&self->connection, service_connection);

  g_mutex_lock (&self->mutex);
  if (self->supervisor != NULL)
    ide_subprocess_supervisor_set_launcher (self->supervisor, launcher);
  g_mutex_unlock (&self->mutex);

handle_error:
  if (error != NULL)
    g_warning ("Error resetting daemon: %s", error->message);

  if (pipe_fds[0].read != -1)  close (pipe_fds[0].read);
  if (pipe_fds[0].write != -1) close (pipe_fds[0].write);
  if (pipe_fds[1].read != -1)  close (pipe_fds[1].read);
  if (pipe_fds[1].write != -1) close (pipe_fds[1].write);

  IDE_EXIT;
}

static void
gbp_flatpak_client_proxy_created_cb (GObject      *object,
                                     GAsyncResult *result,
                                     gpointer      user_data)
{
  g_autoptr(GbpFlatpakClient) self = user_data;
  g_autoptr(IpcFlatpakService) service = NULL;
  g_autoptr(GError) error = NULL;
  GList *queued;

  IDE_ENTRY;

  g_assert (GBP_IS_FLATPAK_CLIENT (self));
  g_assert (G_IS_ASYNC_RESULT (result));

  g_mutex_lock (&self->mutex);

  if ((service = ipc_flatpak_service_proxy_new_finish (result, &error)))
    {
      g_autofree char *home_install = NULL;

      /* We can have long running operations, so set no timeout */
      g_dbus_proxy_set_default_timeout (G_DBUS_PROXY (service), G_MAXINT);

      /* Add the --user installation as our first call before queued
       * events can submit their operations.
       */
      home_install = g_build_filename (g_get_home_dir (),
                                       ".local",
                                       "share",
                                       "flatpak",
                                       NULL);
      if (g_file_test (home_install, G_FILE_TEST_IS_DIR))
        ipc_flatpak_service_call_add_installation (service, home_install, TRUE, NULL, NULL, NULL);
    }

  if (self->state == STATE_SPAWNING && service != NULL)
    self->state = STATE_RUNNING;

  g_assert (error != NULL || service != NULL);

  g_set_object (&self->service, service);

  queued = g_steal_pointer (&self->get_service.head);

  self->get_service.head = NULL;
  self->get_service.tail = NULL;
  self->get_service.length = 0;

  for (const GList *iter = queued; iter != NULL; iter = iter->next)
    {
      IdeTask *task = iter->data;

      if (error)
        ide_task_return_error (task, g_error_copy (error));
      else
        ide_task_return_object (task, g_object_ref (self->service));
    }

  g_list_free_full (queued, g_object_unref);

  g_mutex_unlock (&self->mutex);

  IDE_EXIT;
}

static void
gbp_flatpak_client_subprocess_spawned (GbpFlatpakClient        *self,
                                       IdeSubprocess           *subprocess,
                                       IdeSubprocessSupervisor *supervisor)
{
  IDE_ENTRY;

  g_assert (GBP_IS_FLATPAK_CLIENT (self));
  g_assert (IDE_IS_SUBPROCESS (subprocess));
  g_assert (IDE_IS_SUBPROCESS_SUPERVISOR (supervisor));

  g_mutex_lock (&self->mutex);

  g_assert (self->service == NULL);
  g_assert (self->connection != NULL);

  ide_subprocess_supervisor_set_launcher (self->supervisor, NULL);

  ipc_flatpak_service_proxy_new (self->connection,
                                 G_DBUS_PROXY_FLAGS_NONE,
                                 NULL,
                                 "/org/gnome/Builder/Flatpak",
                                 NULL,
                                 gbp_flatpak_client_proxy_created_cb,
                                 g_object_ref (self));

  g_dbus_connection_start_message_processing (self->connection);

  g_mutex_unlock (&self->mutex);

  IDE_EXIT;
}

static void
gbp_flatpak_client_subprocess_exited (GbpFlatpakClient        *self,
                                      IdeSubprocess           *subprocess,
                                      IdeSubprocessSupervisor *supervisor)
{
  IDE_ENTRY;

  g_assert (GBP_IS_FLATPAK_CLIENT (self));
  g_assert (IDE_IS_SUBPROCESS (subprocess));
  g_assert (IDE_IS_SUBPROCESS_SUPERVISOR (supervisor));

  g_mutex_lock (&self->mutex);
  if (self->state == STATE_RUNNING)
    self->state = STATE_SPAWNING;
  g_clear_object (&self->connection);
  g_clear_object (&self->service);
  g_mutex_unlock (&self->mutex);

  if (!self->disposed)
    gbp_flatpak_client_reset (self);

  IDE_EXIT;
}

static void
gbp_flatpak_client_dispose (GObject *object)
{
  GbpFlatpakClient *self = (GbpFlatpakClient *)object;
  g_autoptr(IdeSubprocessSupervisor) supervisor = g_steal_pointer (&self->supervisor);

  self->disposed = TRUE;

  if (supervisor != NULL)
    ide_subprocess_supervisor_stop (supervisor);

  g_mutex_lock (&self->mutex);
  g_clear_object (&self->connection);
  g_clear_object (&self->service);
  g_mutex_unlock (&self->mutex);

  G_OBJECT_CLASS (gbp_flatpak_client_parent_class)->dispose (object);
}

static void
gbp_flatpak_client_finalize (GObject *object)
{
  GbpFlatpakClient *self = (GbpFlatpakClient *)object;

  g_mutex_clear (&self->mutex);

  G_OBJECT_CLASS (gbp_flatpak_client_parent_class)->finalize (object);
}

static void
gbp_flatpak_client_constructed (GObject *object)
{
  GbpFlatpakClient *self = (GbpFlatpakClient *)object;

  IDE_ENTRY;

  g_assert (GBP_IS_FLATPAK_CLIENT (self));

  G_OBJECT_CLASS (gbp_flatpak_client_parent_class)->constructed (object);

  self->supervisor = ide_subprocess_supervisor_new ();
  g_signal_connect_object (self->supervisor,
                           "spawned",
                           G_CALLBACK (gbp_flatpak_client_subprocess_spawned),
                           self,
                           G_CONNECT_SWAPPED);
  g_signal_connect_object (self->supervisor,
                           "exited",
                           G_CALLBACK (gbp_flatpak_client_subprocess_exited),
                           self,
                           G_CONNECT_SWAPPED);

  gbp_flatpak_client_reset (self);

  IDE_EXIT;
}

static void
gbp_flatpak_client_class_init (GbpFlatpakClientClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->constructed = gbp_flatpak_client_constructed;
  object_class->dispose = gbp_flatpak_client_dispose;
  object_class->finalize = gbp_flatpak_client_finalize;
}

static void
gbp_flatpak_client_init (GbpFlatpakClient *self)
{
  g_mutex_init (&self->mutex);
}

GbpFlatpakClient *
gbp_flatpak_client_get_default (void)
{
  static GbpFlatpakClient *instance;

  if (g_once_init_enter (&instance))
    {
      GbpFlatpakClient *client;

      client = g_object_new (GBP_TYPE_FLATPAK_CLIENT, NULL);
      gbp_flatpak_client_get_service_async (client, NULL, NULL, NULL);
      g_once_init_leave (&instance, client);
    }

  return instance;
}

static void
gbp_flatpak_client_get_service_cb (GObject      *object,
                                   GAsyncResult *result,
                                   gpointer      user_data)
{
  GbpFlatpakClient *self = (GbpFlatpakClient *)object;
  g_autoptr(IpcFlatpakService) service = NULL;
  g_autoptr(IdeTask) task = user_data;
  g_autoptr(GError) error = NULL;

  g_assert (GBP_IS_FLATPAK_CLIENT (self));
  g_assert (G_IS_ASYNC_RESULT (result));
  g_assert (IDE_IS_TASK (task));

  if (!(service = gbp_flatpak_client_get_service_finish (self, result, &error)))
    ide_task_return_error (task, g_steal_pointer (&error));
  else
    ide_task_return_object (task, g_steal_pointer (&service));
}

IpcFlatpakService *
gbp_flatpak_client_get_service (GbpFlatpakClient  *self,
                                GCancellable      *cancellable,
                                GError           **error)
{
  g_autoptr(IdeTask) task = NULL;
  g_autoptr(GMainContext) gcontext = NULL;
  IpcFlatpakService *ret = NULL;

  g_assert (GBP_IS_FLATPAK_CLIENT (self));
  g_assert (!cancellable || G_IS_CANCELLABLE (cancellable));

  g_mutex_lock (&self->mutex);
  if (self->service != NULL)
    ret = g_object_ref (self->service);
  g_mutex_unlock (&self->mutex);

  if (ret != NULL)
    return g_steal_pointer (&ret);

  task = ide_task_new (self, cancellable, NULL, NULL);
  ide_task_set_source_tag (task, gbp_flatpak_client_get_service);

  gcontext = g_main_context_ref_thread_default ();
  gbp_flatpak_client_get_service_async (self,
                                        cancellable,
                                        gbp_flatpak_client_get_service_cb,
                                        g_object_ref (task));
  while (!ide_task_get_completed (task))
    g_main_context_iteration (gcontext, TRUE);

  return ide_task_propagate_object (task, error);
}

void
gbp_flatpak_client_get_service_async (GbpFlatpakClient    *self,
                                      GCancellable        *cancellable,
                                      GAsyncReadyCallback  callback,
                                      gpointer             user_data)
{
  g_autoptr(IdeTask) task = NULL;

  g_assert (GBP_IS_FLATPAK_CLIENT (self));
  g_assert (!cancellable || G_IS_CANCELLABLE (cancellable));

  g_mutex_lock (&self->mutex);

  task = ide_task_new (self, cancellable, callback, user_data);
  ide_task_set_source_tag (task, gbp_flatpak_client_get_service_async);

  switch (self->state)
    {
    case STATE_INITIAL:
      {
        g_autoptr(IdeSubprocessSupervisor) supervisor = g_object_ref (self->supervisor);

        self->state = STATE_SPAWNING;
        g_queue_push_tail (&self->get_service, g_steal_pointer (&task));
        g_mutex_unlock (&self->mutex);
        ide_subprocess_supervisor_start (supervisor);
        return;
      }

    case STATE_SPAWNING:
      g_queue_push_tail (&self->get_service, g_steal_pointer (&task));
      break;

    case STATE_RUNNING:
      ide_task_return_object (task, g_object_ref (self->service));
      break;

    case STATE_SHUTDOWN:
      ide_task_return_new_error (task,
                                 G_IO_ERROR,
                                 G_IO_ERROR_CLOSED,
                                 _("The client has been closed"));
      break;

    default:
      g_assert_not_reached ();
    }

  g_mutex_unlock (&self->mutex);
}

IpcFlatpakService *
gbp_flatpak_client_get_service_finish (GbpFlatpakClient  *self,
                                       GAsyncResult      *result,
                                       GError           **error)
{
  g_assert (GBP_IS_FLATPAK_CLIENT (self));
  g_assert (IDE_IS_TASK (result));

  return ide_task_propagate_object (IDE_TASK (result), error);
}

void
gbp_flatpak_client_force_exit (GbpFlatpakClient *self)
{
  IdeSubprocess *subprocess;

  g_return_if_fail (GBP_IS_FLATPAK_CLIENT (self));

  if ((subprocess = ide_subprocess_supervisor_get_subprocess (self->supervisor)))
    ide_subprocess_force_exit (subprocess);
}
