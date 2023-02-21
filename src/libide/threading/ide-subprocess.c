/* ide-subprocess.c
 *
 * Copyright 2016-2019 Christian Hergert <chergert@redhat.com>
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

#define G_LOG_DOMAIN "ide-subprocess"

#include "config.h"

#include <string.h>

#include <glib/gi18n.h>

#include <libide-core.h>

#include "ide-subprocess.h"

G_DEFINE_INTERFACE (IdeSubprocess, ide_subprocess, G_TYPE_OBJECT)

static void
ide_subprocess_default_init (IdeSubprocessInterface *iface)
{
}

#define WRAP_INTERFACE_METHOD(self, name, default_return, ...) \
  ((IDE_SUBPROCESS_GET_IFACE(self)->name != NULL) ? \
    IDE_SUBPROCESS_GET_IFACE(self)->name (self, ##__VA_ARGS__) : \
    default_return)

const gchar *
ide_subprocess_get_identifier (IdeSubprocess *self)
{
  g_return_val_if_fail (IDE_IS_SUBPROCESS (self), NULL);

  return WRAP_INTERFACE_METHOD (self, get_identifier, NULL);
}

/**
 * ide_subprocess_get_stdout_pipe:
 *
 * Returns: (transfer none): a #GInputStream or %NULL.
 */
GInputStream *
ide_subprocess_get_stdout_pipe (IdeSubprocess *self)
{
  g_return_val_if_fail (IDE_IS_SUBPROCESS (self), NULL);

  return WRAP_INTERFACE_METHOD (self, get_stdout_pipe, NULL);
}

/**
 * ide_subprocess_get_stderr_pipe:
 *
 * Returns: (transfer none): a #GInputStream or %NULL.
 */
GInputStream *
ide_subprocess_get_stderr_pipe (IdeSubprocess *self)
{
  g_return_val_if_fail (IDE_IS_SUBPROCESS (self), NULL);

  return WRAP_INTERFACE_METHOD (self, get_stderr_pipe, NULL);
}

/**
 * ide_subprocess_get_stdin_pipe:
 *
 * Returns: (transfer none): a #GOutputStream or %NULL.
 */
GOutputStream *
ide_subprocess_get_stdin_pipe (IdeSubprocess *self)
{
  g_return_val_if_fail (IDE_IS_SUBPROCESS (self), NULL);

  return WRAP_INTERFACE_METHOD (self, get_stdin_pipe, NULL);
}

gboolean
ide_subprocess_wait (IdeSubprocess  *self,
                     GCancellable   *cancellable,
                     GError        **error)
{
  g_return_val_if_fail (IDE_IS_SUBPROCESS (self), FALSE);
  g_return_val_if_fail (!cancellable || G_IS_CANCELLABLE (cancellable), FALSE);

  return WRAP_INTERFACE_METHOD (self, wait, FALSE, cancellable, error);
}

gboolean
ide_subprocess_wait_check (IdeSubprocess  *self,
                           GCancellable   *cancellable,
                           GError        **error)
{
  g_return_val_if_fail (IDE_IS_SUBPROCESS (self), FALSE);
  g_return_val_if_fail (!cancellable || G_IS_CANCELLABLE (cancellable), FALSE);

  return ide_subprocess_wait (self, cancellable, error) &&
         ide_subprocess_check_exit_status (self, error);
}

void
ide_subprocess_wait_async (IdeSubprocess       *self,
                           GCancellable        *cancellable,
                           GAsyncReadyCallback  callback,
                           gpointer             user_data)
{
  g_return_if_fail (IDE_IS_SUBPROCESS (self));
  g_return_if_fail (!cancellable || G_IS_CANCELLABLE (cancellable));

  WRAP_INTERFACE_METHOD (self, wait_async, NULL, cancellable, callback, user_data);
}

gboolean
ide_subprocess_wait_finish (IdeSubprocess  *self,
                            GAsyncResult   *result,
                            GError        **error)
{
  g_return_val_if_fail (IDE_IS_SUBPROCESS (self), FALSE);

  return WRAP_INTERFACE_METHOD (self, wait_finish, FALSE, result, error);
}

static void
ide_subprocess_wait_check_cb (GObject      *object,
                              GAsyncResult *result,
                              gpointer      user_data)
{
  IdeSubprocess *self = (IdeSubprocess *)object;
  g_autoptr(GTask) task = user_data;
  g_autoptr(GError) error = NULL;

  IDE_ENTRY;

  g_assert (IDE_IS_SUBPROCESS (self));
  g_assert (G_IS_TASK (task));

  if (!ide_subprocess_wait_finish (self, result, &error))
    {
      g_task_return_error (task, g_steal_pointer (&error));
      IDE_EXIT;
    }

  if (ide_subprocess_get_if_signaled (self))
    {
      gint term_sig = ide_subprocess_get_term_sig (self);

      g_task_return_new_error (task,
                               G_SPAWN_ERROR,
                               G_SPAWN_ERROR_FAILED,
                               _("Process terminated by signal %d"),
                               term_sig);
      IDE_EXIT;
    }

  if (!ide_subprocess_check_exit_status (self, &error))
    {
      g_task_return_error (task, g_steal_pointer (&error));
      IDE_EXIT;
    }

  g_task_return_boolean (task, TRUE);

  IDE_EXIT;
}

void
ide_subprocess_wait_check_async (IdeSubprocess       *self,
                                 GCancellable        *cancellable,
                                 GAsyncReadyCallback  callback,
                                 gpointer             user_data)
{
  g_autoptr(GTask) task = NULL;

  IDE_ENTRY;

  g_return_if_fail (IDE_IS_SUBPROCESS (self));
  g_return_if_fail (!cancellable || G_IS_CANCELLABLE (cancellable));

  task = g_task_new (self, cancellable, callback, user_data);
  g_task_set_source_tag (task, ide_subprocess_wait_check_async);

  ide_subprocess_wait_async (self,
                             cancellable,
                             ide_subprocess_wait_check_cb,
                             g_steal_pointer (&task));

  IDE_EXIT;
}

gboolean
ide_subprocess_wait_check_finish (IdeSubprocess  *self,
                                  GAsyncResult   *result,
                                  GError        **error)
{
  g_return_val_if_fail (IDE_IS_SUBPROCESS (self), FALSE);
  g_return_val_if_fail (G_IS_TASK (result), FALSE);
  g_return_val_if_fail (g_task_is_valid (G_TASK (result), self), FALSE);

  return g_task_propagate_boolean (G_TASK (result), error);
}

gboolean
ide_subprocess_get_successful (IdeSubprocess *self)
{
  g_return_val_if_fail (IDE_IS_SUBPROCESS (self), FALSE);

  return WRAP_INTERFACE_METHOD (self, get_successful, FALSE);
}

gboolean
ide_subprocess_get_if_exited (IdeSubprocess *self)
{
  g_return_val_if_fail (IDE_IS_SUBPROCESS (self), FALSE);

  return WRAP_INTERFACE_METHOD (self, get_if_exited, FALSE);
}

gint
ide_subprocess_get_exit_status (IdeSubprocess *self)
{
  g_return_val_if_fail (IDE_IS_SUBPROCESS (self), 0);

  return WRAP_INTERFACE_METHOD (self, get_exit_status, 0);
}

gboolean
ide_subprocess_get_if_signaled (IdeSubprocess *self)
{
  g_return_val_if_fail (IDE_IS_SUBPROCESS (self), FALSE);

  return WRAP_INTERFACE_METHOD (self, get_if_signaled, FALSE);
}

gint
ide_subprocess_get_term_sig (IdeSubprocess *self)
{
  g_return_val_if_fail (IDE_IS_SUBPROCESS (self), 0);

  return WRAP_INTERFACE_METHOD (self, get_term_sig, 0);
}

gint
ide_subprocess_get_status (IdeSubprocess *self)
{
  g_return_val_if_fail (IDE_IS_SUBPROCESS (self), 0);

  return WRAP_INTERFACE_METHOD (self, get_status, 0);
}

void
ide_subprocess_send_signal (IdeSubprocess *self,
                            gint           signal_num)
{
  g_return_if_fail (IDE_IS_SUBPROCESS (self));

  g_debug ("Send signal %d on subprocess %s",
           signal_num,
           ide_subprocess_get_identifier (self));

  WRAP_INTERFACE_METHOD (self, send_signal, NULL, signal_num);
}

void
ide_subprocess_force_exit (IdeSubprocess *self)
{
  g_return_if_fail (IDE_IS_SUBPROCESS (self));

  g_debug ("Force exit on subprocess %s",
           ide_subprocess_get_identifier (self));

  WRAP_INTERFACE_METHOD (self, force_exit, NULL);
}

gboolean
ide_subprocess_communicate (IdeSubprocess  *self,
                            GBytes         *stdin_buf,
                            GCancellable   *cancellable,
                            GBytes        **stdout_buf,
                            GBytes        **stderr_buf,
                            GError        **error)
{
  g_return_val_if_fail (IDE_IS_SUBPROCESS (self), FALSE);
  g_return_val_if_fail (!cancellable || G_IS_CANCELLABLE (cancellable), FALSE);

  return WRAP_INTERFACE_METHOD (self, communicate, FALSE, stdin_buf, cancellable, stdout_buf, stderr_buf, error);
}

/**
 * ide_subprocess_communicate_utf8:
 * @self: an #IdeSubprocess
 * @stdin_buf: (nullable): input to deliver to the subprocesses stdin stream
 * @cancellable: (nullable): an optional #GCancellable
 * @stdout_buf: (out) (nullable): an optional location for the stdout contents
 * @stderr_buf: (out) (nullable): an optional location for the stderr contents
 *
 * This process acts identical to g_subprocess_communicate_utf8().
 *
 * Returns: %TRUE if successful; otherwise %FALSE and @error is set.
 */
gboolean
ide_subprocess_communicate_utf8 (IdeSubprocess  *self,
                                 const gchar    *stdin_buf,
                                 GCancellable   *cancellable,
                                 gchar         **stdout_buf,
                                 gchar         **stderr_buf,
                                 GError        **error)
{
  g_return_val_if_fail (IDE_IS_SUBPROCESS (self), FALSE);
  g_return_val_if_fail (!cancellable || G_IS_CANCELLABLE (cancellable), FALSE);

  return WRAP_INTERFACE_METHOD (self, communicate_utf8, FALSE, stdin_buf, cancellable, stdout_buf, stderr_buf, error);
}

/**
 * ide_subprocess_communicate_async:
 * @self: An #IdeSubprocess
 * @stdin_buf: (nullable): a #GBytes to send to stdin or %NULL
 * @cancellable: (nullable): a #GCancellable or %NULL
 * @callback: A callback to complete the request
 * @user_data: user data for @callback
 *
 * Asynchronously communicates with the the child process.
 *
 * There is no need to call ide_subprocess_wait() on the process if using
 * this asynchronous operation as it will internally wait for the child
 * to exit or be signaled.
 *
 * Ensure you've set the proper flags to ensure that you can write to stdin
 * or read from stderr/stdout as necessary.
 */
void
ide_subprocess_communicate_async (IdeSubprocess       *self,
                                  GBytes              *stdin_buf,
                                  GCancellable        *cancellable,
                                  GAsyncReadyCallback  callback,
                                  gpointer             user_data)
{
  g_return_if_fail (IDE_IS_SUBPROCESS (self));
  g_return_if_fail (!cancellable || G_IS_CANCELLABLE (cancellable));

  WRAP_INTERFACE_METHOD (self, communicate_async, NULL, stdin_buf, cancellable, callback, user_data);
}

/**
 * ide_subprocess_communicate_finish:
 * @self: An #IdeSubprocess
 * @result: a #GAsyncResult
 * @stdout_buf: (out) (optional): A location for a #Bytes.
 * @stderr_buf: (out) (optional): A location for a #Bytes.
 * @error: a location for a #GError
 *
 * Finishes a request to ide_subprocess_communicate_async().
 *
 * Returns: %TRUE if successful; otherwise %FALSE and @error is set.
 */
gboolean
ide_subprocess_communicate_finish (IdeSubprocess  *self,
                                   GAsyncResult   *result,
                                   GBytes        **stdout_buf,
                                   GBytes        **stderr_buf,
                                   GError        **error)
{
  g_return_val_if_fail (IDE_IS_SUBPROCESS (self), FALSE);
  g_return_val_if_fail (G_IS_ASYNC_RESULT (result), FALSE);

  return WRAP_INTERFACE_METHOD (self, communicate_finish, FALSE, result, stdout_buf, stderr_buf, error);
}

/**
 * ide_subprocess_communicate_utf8_async:
 * @stdin_buf: (nullable): The data to send to stdin or %NULL
 *
 *
 */
void
ide_subprocess_communicate_utf8_async (IdeSubprocess       *self,
                                       const gchar         *stdin_buf,
                                       GCancellable        *cancellable,
                                       GAsyncReadyCallback  callback,
                                       gpointer             user_data)
{
  g_return_if_fail (IDE_IS_SUBPROCESS (self));
  g_return_if_fail (!cancellable || G_IS_CANCELLABLE (cancellable));

  WRAP_INTERFACE_METHOD (self, communicate_utf8_async, NULL, stdin_buf, cancellable, callback, user_data);
}

/**
 * ide_subprocess_communicate_utf8_finish:
 * @self: An #IdeSubprocess
 * @result: a #GAsyncResult
 * @stdout_buf: (out) (optional): A location for the UTF-8 formatted output string or %NULL
 * @stderr_buf: (out) (optional): A location for the UTF-8 formatted output string or %NULL
 * @error: A location for a #GError, or %NULL
 *
 * Returns: %TRUE if successful; otherwise %FALSE and @error is set.
 */
gboolean
ide_subprocess_communicate_utf8_finish (IdeSubprocess  *self,
                                        GAsyncResult   *result,
                                        gchar         **stdout_buf,
                                        gchar         **stderr_buf,
                                        GError        **error)
{
  g_return_val_if_fail (IDE_IS_SUBPROCESS (self), FALSE);
  g_return_val_if_fail (G_IS_ASYNC_RESULT (result), FALSE);

  return WRAP_INTERFACE_METHOD (self, communicate_utf8_finish, FALSE, result, stdout_buf, stderr_buf, error);
}

gboolean
ide_subprocess_check_exit_status (IdeSubprocess  *self,
                                  GError        **error)
{
  gint exit_status;

  g_return_val_if_fail (IDE_IS_SUBPROCESS (self), FALSE);

  exit_status = ide_subprocess_get_exit_status (self);

  return g_spawn_check_wait_status (exit_status, error);
}

typedef struct
{
  GWeakRef subprocess_wr;
  GWeakRef cancellable_wr;
  gpointer handler_id;
  int signum;
} CancelSignalData;

static void
signal_upon_cancel_activate (GCancellable *cancellable,
                             gpointer      user_data)
{
  CancelSignalData *data = user_data;
  g_autoptr(IdeSubprocess) subprocess = NULL;

  g_assert (G_IS_CANCELLABLE (cancellable));
  g_assert (data != NULL);

  if ((subprocess = g_weak_ref_get (&data->subprocess_wr)))
    ide_subprocess_send_signal (subprocess, data->signum);
}

static void
cancel_signal_data_finalize (CancelSignalData *data)
{
  g_weak_ref_clear (&data->cancellable_wr);
  g_weak_ref_clear (&data->subprocess_wr);
}

static void
cancel_signal_data_invalidate_by_subprocess (CancelSignalData *data)
{
  g_autoptr(GCancellable) cancellable = NULL;
  gulong handler_id;

  g_assert (data != NULL);

  g_weak_ref_set (&data->subprocess_wr, NULL);

  handler_id = GPOINTER_TO_SIZE (g_atomic_pointer_exchange (&data->handler_id, NULL));
  if (handler_id != 0 && (cancellable = g_weak_ref_get (&data->cancellable_wr)))
    g_cancellable_disconnect (cancellable, handler_id);

  g_atomic_rc_box_release_full (data, (GDestroyNotify)cancel_signal_data_finalize);
}

static void
cancel_signal_data_invalidate_by_cancellable (CancelSignalData *data)
{
  g_autoptr(IdeSubprocess) subprocess = NULL;

  g_assert (data != NULL);

  g_weak_ref_set (&data->cancellable_wr, NULL);

  if ((subprocess = g_weak_ref_get (&data->subprocess_wr)))
    g_object_set_data (G_OBJECT (subprocess), "SIGNAL_UPON_CANCEL", NULL);

  g_atomic_rc_box_release_full (data, (GDestroyNotify)cancel_signal_data_finalize);
}

void
ide_subprocess_send_signal_upon_cancel (IdeSubprocess *self,
                                        GCancellable  *cancellable,
                                        int            signal_num)
{
  CancelSignalData *data;

  g_return_if_fail (IDE_IS_SUBPROCESS (self));
  g_return_if_fail (!cancellable || G_IS_CANCELLABLE (cancellable));

  if (cancellable == NULL)
    return;

  data = g_atomic_rc_box_new0 (CancelSignalData);
  g_weak_ref_init (&data->subprocess_wr, self);
  g_weak_ref_init (&data->cancellable_wr, cancellable);
  data->signum = signal_num;

  g_object_set_data_full (G_OBJECT (self),
                          "SIGNAL_UPON_CANCEL",
                          g_atomic_rc_box_acquire (data),
                          (GDestroyNotify)cancel_signal_data_invalidate_by_subprocess);

  data->handler_id = GSIZE_TO_POINTER (g_cancellable_connect (cancellable,
                                                              G_CALLBACK (signal_upon_cancel_activate),
                                                              g_atomic_rc_box_acquire (data),
                                                              (GDestroyNotify)cancel_signal_data_invalidate_by_cancellable));

  g_atomic_rc_box_release_full (data, (GDestroyNotify)cancel_signal_data_finalize);
}
