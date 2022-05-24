/* ide-simple-subprocess.c
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

#define G_LOG_DOMAIN "ide-simple-subprocess"

#include "config.h"

#include <libide-core.h>

#include "ide-simple-subprocess-private.h"

static void subprocess_iface_init (IdeSubprocessInterface *iface);

G_DEFINE_TYPE_EXTENDED (IdeSimpleSubprocess, ide_simple_subprocess, G_TYPE_OBJECT, G_TYPE_FLAG_FINAL,
                        G_IMPLEMENT_INTERFACE (IDE_TYPE_SUBPROCESS, subprocess_iface_init))

static void
ide_simple_subprocess_finalize (GObject *object)
{
  IdeSimpleSubprocess *self = (IdeSimpleSubprocess *)object;

  IDE_ENTRY;

  g_clear_object (&self->subprocess);

  G_OBJECT_CLASS (ide_simple_subprocess_parent_class)->finalize (object);

  IDE_EXIT;
}

static void
ide_simple_subprocess_class_init (IdeSimpleSubprocessClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = ide_simple_subprocess_finalize;
}

static void
ide_simple_subprocess_init (IdeSimpleSubprocess *self)
{
}

#define WRAP_INTERFACE_METHOD(name, ...) \
  g_subprocess_##name(IDE_SIMPLE_SUBPROCESS(subprocess)->subprocess, ## __VA_ARGS__)

static const gchar *
ide_simple_subprocess_get_identifier (IdeSubprocess *subprocess)
{
  return WRAP_INTERFACE_METHOD (get_identifier);
}

static GInputStream *
ide_simple_subprocess_get_stdout_pipe (IdeSubprocess *subprocess)
{
  return WRAP_INTERFACE_METHOD (get_stdout_pipe);
}

static GInputStream *
ide_simple_subprocess_get_stderr_pipe (IdeSubprocess *subprocess)
{
  return WRAP_INTERFACE_METHOD (get_stderr_pipe);
}

static GOutputStream *
ide_simple_subprocess_get_stdin_pipe (IdeSubprocess *subprocess)
{
  return WRAP_INTERFACE_METHOD (get_stdin_pipe);
}

static gboolean
ide_simple_subprocess_wait (IdeSubprocess  *subprocess,
                            GCancellable   *cancellable,
                            GError        **error)
{
  return WRAP_INTERFACE_METHOD (wait, cancellable, error);
}

static void
ide_simple_subprocess_wait_cb (GObject      *object,
                               GAsyncResult *result,
                               gpointer      user_data)
{
  GSubprocess *subprocess = (GSubprocess *)object;
  g_autoptr(GTask) task = user_data;
  g_autoptr(GError) error = NULL;

  IDE_ENTRY;

  g_assert (G_IS_SUBPROCESS (subprocess));
  g_assert (G_IS_TASK (task));

  g_subprocess_wait_finish (subprocess, result, &error);

#ifdef IDE_ENABLE_TRACE
  if (!g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CANCELLED))
    {
      if (g_subprocess_get_if_exited (subprocess))
        IDE_TRACE_MSG ("subprocess exited with exit status: %d",
                       g_subprocess_get_exit_status (subprocess));
      else
        IDE_TRACE_MSG ("subprocess exited due to signal: %d",
                       g_subprocess_get_term_sig (subprocess));
    }
#endif

  if (error != NULL)
    g_task_return_error (task, g_steal_pointer (&error));
  else
    g_task_return_boolean (task, TRUE);

  IDE_EXIT;
}

static void
ide_simple_subprocess_wait_async (IdeSubprocess       *subprocess,
                                  GCancellable        *cancellable,
                                  GAsyncReadyCallback  callback,
                                  gpointer             user_data)
{
  IdeSimpleSubprocess *self = (IdeSimpleSubprocess *)subprocess;
  g_autoptr(GTask) task = NULL;

  IDE_ENTRY;

  g_assert (IDE_IS_SIMPLE_SUBPROCESS (self));
  g_assert (!cancellable || G_IS_CANCELLABLE (cancellable));

  task = g_task_new (self, cancellable, callback, user_data);
  g_task_set_source_tag (task, ide_simple_subprocess_wait_async);

  g_subprocess_wait_async (self->subprocess,
                           cancellable,
                           ide_simple_subprocess_wait_cb,
                           g_steal_pointer (&task));

  IDE_EXIT;
}

static gboolean
ide_simple_subprocess_wait_finish (IdeSubprocess  *subprocess,
                                   GAsyncResult   *result,
                                   GError        **error)
{
  gboolean ret;

  IDE_ENTRY;

  g_assert (IDE_IS_SIMPLE_SUBPROCESS (subprocess));
  g_assert (G_IS_TASK (result));

  ret = g_task_propagate_boolean (G_TASK (result), error);

  IDE_RETURN (ret);
}

static gboolean
ide_simple_subprocess_get_successful (IdeSubprocess *subprocess)
{
  return WRAP_INTERFACE_METHOD (get_successful);
}

static gboolean
ide_simple_subprocess_get_if_exited (IdeSubprocess *subprocess)
{
  return WRAP_INTERFACE_METHOD (get_if_exited);
}

static gint
ide_simple_subprocess_get_exit_status (IdeSubprocess *subprocess)
{
  return WRAP_INTERFACE_METHOD (get_exit_status);
}

static gboolean
ide_simple_subprocess_get_if_signaled (IdeSubprocess *subprocess)
{
  return WRAP_INTERFACE_METHOD (get_if_signaled);
}

static gint
ide_simple_subprocess_get_term_sig (IdeSubprocess *subprocess)
{
  return WRAP_INTERFACE_METHOD (get_term_sig);
}

static gint
ide_simple_subprocess_get_status (IdeSubprocess *subprocess)
{
  return WRAP_INTERFACE_METHOD (get_status);
}

static void
ide_simple_subprocess_send_signal (IdeSubprocess *subprocess,
                                   gint           signal_num)
{
  IDE_ENTRY;
  WRAP_INTERFACE_METHOD (send_signal, signal_num);
  IDE_EXIT;
}

static void
ide_simple_subprocess_force_exit (IdeSubprocess *subprocess)
{
  IDE_ENTRY;
  WRAP_INTERFACE_METHOD (force_exit);
  IDE_EXIT;
}

static gboolean
ide_simple_subprocess_communicate (IdeSubprocess  *subprocess,
                                   GBytes         *stdin_buf,
                                   GCancellable   *cancellable,
                                   GBytes        **stdout_buf,
                                   GBytes        **stderr_buf,
                                   GError        **error)
{
  return WRAP_INTERFACE_METHOD (communicate, stdin_buf, cancellable, stdout_buf, stderr_buf, error);
}

static gboolean
ide_simple_subprocess_communicate_utf8 (IdeSubprocess  *subprocess,
                                        const gchar    *stdin_buf,
                                        GCancellable   *cancellable,
                                        gchar         **stdout_buf,
                                        gchar         **stderr_buf,
                                        GError        **error)
{
  return WRAP_INTERFACE_METHOD (communicate_utf8, stdin_buf, cancellable, stdout_buf, stderr_buf, error);
}

static void
free_object_pair (gpointer data)
{
  gpointer *pair = data;

  g_clear_object (&pair[0]);
  g_clear_object (&pair[1]);
  g_free (pair);
}

static void
ide_simple_subprocess_communicate_cb (GObject      *object,
                                      GAsyncResult *result,
                                      gpointer      user_data)
{
  GSubprocess *subprocess = (GSubprocess *)object;
  g_autoptr(GTask) task = user_data;
  g_autoptr(GError) error = NULL;
  g_autoptr(GBytes) stdout_buf = NULL;
  g_autoptr(GBytes) stderr_buf = NULL;
  gpointer *data;

  if (!g_subprocess_communicate_finish (subprocess, result, &stdout_buf, &stderr_buf, &error))
    {
      g_task_return_error (task, g_steal_pointer (&error));
      return;
    }

  data = g_new0 (gpointer, 2);
  data[0] = g_steal_pointer (&stdout_buf);
  data[1] = g_steal_pointer (&stderr_buf);

  g_task_return_pointer (task, data, free_object_pair);
}

static void
ide_simple_subprocess_communicate_async (IdeSubprocess       *subprocess,
                                         GBytes              *stdin_buf,
                                         GCancellable        *cancellable,
                                         GAsyncReadyCallback  callback,
                                         gpointer             user_data)
{
  IdeSimpleSubprocess *self = (IdeSimpleSubprocess *)subprocess;
  GTask *task = g_task_new (self, cancellable, callback, user_data);
  g_subprocess_communicate_async (self->subprocess, stdin_buf, cancellable, ide_simple_subprocess_communicate_cb, task);
}

static gboolean
ide_simple_subprocess_communicate_finish (IdeSubprocess  *subprocess,
                                          GAsyncResult   *result,
                                          GBytes        **stdout_buf,
                                          GBytes        **stderr_buf,
                                          GError        **error)
{
  gpointer *pair;

  pair = g_task_propagate_pointer (G_TASK (result), error);

  if (pair != NULL)
    {
      if (stdout_buf != NULL)
        *stdout_buf = g_steal_pointer (&pair[0]);

      if (stderr_buf != NULL)
        *stderr_buf = g_steal_pointer (&pair[1]);

      free_object_pair (pair);

      return TRUE;
    }

  return FALSE;
}

static void
ide_simple_subprocess_communicate_utf8_cb (GObject      *object,
                                           GAsyncResult *result,
                                           gpointer      user_data)
{
  GSubprocess *subprocess = (GSubprocess *)object;
  g_autoptr(GTask) task = user_data;
  g_autoptr(GError) error = NULL;
  g_autofree gchar *stdout_buf = NULL;
  g_autofree gchar *stderr_buf = NULL;
  gpointer *data;

  if (!g_subprocess_communicate_utf8_finish (subprocess, result, &stdout_buf, &stderr_buf, &error))
    {
      g_task_return_error (task, g_steal_pointer (&error));
      return;
    }

  data = g_new0 (gpointer, 2);
  data[0] = g_steal_pointer (&stdout_buf);
  data[1] = g_steal_pointer (&stderr_buf);

  g_task_return_pointer (task, data, free_object_pair);
}

static void
ide_simple_subprocess_communicate_utf8_async (IdeSubprocess       *subprocess,
                                              const gchar         *stdin_buf,
                                              GCancellable        *cancellable,
                                              GAsyncReadyCallback  callback,
                                              gpointer             user_data)
{
  IdeSimpleSubprocess *self = (IdeSimpleSubprocess *)subprocess;
  GTask *task = g_task_new (self, cancellable, callback, user_data);
  g_subprocess_communicate_utf8_async (self->subprocess, stdin_buf, cancellable, ide_simple_subprocess_communicate_utf8_cb, task);
}

static gboolean
ide_simple_subprocess_communicate_utf8_finish (IdeSubprocess  *subprocess,
                                               GAsyncResult   *result,
                                               gchar         **stdout_buf,
                                               gchar         **stderr_buf,
                                               GError        **error)
{
  gpointer *pair;

  pair = g_task_propagate_pointer (G_TASK (result), error);

  if (pair != NULL)
    {
      if (stdout_buf != NULL)
        *stdout_buf = g_steal_pointer (&pair[0]);

      if (stderr_buf != NULL)
        *stderr_buf = g_steal_pointer (&pair[1]);

      g_free (pair[0]);
      g_free (pair[1]);
      g_free (pair);

      return TRUE;
    }

  return FALSE;
}

static void
subprocess_iface_init (IdeSubprocessInterface *iface)
{
  iface->get_identifier = ide_simple_subprocess_get_identifier;
  iface->get_stdout_pipe = ide_simple_subprocess_get_stdout_pipe;
  iface->get_stderr_pipe = ide_simple_subprocess_get_stderr_pipe;
  iface->get_stdin_pipe = ide_simple_subprocess_get_stdin_pipe;
  iface->wait = ide_simple_subprocess_wait;
  iface->wait_async = ide_simple_subprocess_wait_async;
  iface->wait_finish = ide_simple_subprocess_wait_finish;
  iface->get_successful = ide_simple_subprocess_get_successful;
  iface->get_if_exited = ide_simple_subprocess_get_if_exited;
  iface->get_exit_status = ide_simple_subprocess_get_exit_status;
  iface->get_if_signaled = ide_simple_subprocess_get_if_signaled;
  iface->get_term_sig = ide_simple_subprocess_get_term_sig;
  iface->get_status = ide_simple_subprocess_get_status;
  iface->send_signal = ide_simple_subprocess_send_signal;
  iface->force_exit = ide_simple_subprocess_force_exit;
  iface->communicate = ide_simple_subprocess_communicate;
  iface->communicate_utf8 = ide_simple_subprocess_communicate_utf8;
  iface->communicate_async = ide_simple_subprocess_communicate_async;
  iface->communicate_finish = ide_simple_subprocess_communicate_finish;
  iface->communicate_utf8_async = ide_simple_subprocess_communicate_utf8_async;
  iface->communicate_utf8_finish = ide_simple_subprocess_communicate_utf8_finish;
}

/**
 * ide_simple_subprocess_new:
 *
 * Creates a new #IdeSimpleSubprocess wrapping the #GSubprocess.
 *
 * Returns: (transfer full): A new #IdeSubprocess
 */
IdeSubprocess *
ide_simple_subprocess_new (GSubprocess *subprocess)
{
  IdeSimpleSubprocess *ret;

  g_return_val_if_fail (G_IS_SUBPROCESS (subprocess), NULL);

  ret = g_object_new (IDE_TYPE_SIMPLE_SUBPROCESS, NULL);
  ret->subprocess = g_object_ref (subprocess);

  return IDE_SUBPROCESS (ret);
}
