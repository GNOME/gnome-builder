/* GIO - GLib Input, Output and Streaming Library
 *
 * Copyright 2012, 2013, 2016 Red Hat, Inc.
 * Copyright 2012, 2013 Canonical Limited
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published
 * by the Free Software Foundation; either version 2 of the licence or (at
 * your option) any later version.
 *
 * See the included COPYING file for more information.
 *
 * Authors: Colin Walters <walters@verbum.org>
 *          Ryan Lortie <desrt@desrt.ca>
 *          Alexander Larsson <alexl@redhat.com>
 *          Christian Hergert <chergert@redhat.com>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#define G_LOG_DOMAIN "ide-flatpak-subprocess"

#include "config.h"

#include <errno.h>
#include <fcntl.h>
#include <gio/gunixinputstream.h>
#include <gio/gunixoutputstream.h>
#include <gio/gunixfdlist.h>
#include <glib-unix.h>
#include <libide-core.h>
#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#include "ide-flatpak-subprocess-private.h"
#include "ide-task.h"

#define FLATPAK_HOST_COMMAND_FLAGS_CLEAR_ENV (1 << 0)
#define FLATPAK_HOST_COMMAND_FLAGS_WATCH_BUS (1 << 1)

/*
 * One very non-ideal thing about this implementation is that we use a new
 * GDBusConnection for every instance. This is due to some difficulty in
 * dealing with our connection being closed out from underneath us. If we
 * can determine what was/is causing that, we should be able to move back
 * to a shared connection (although we might want a dedicated connection
 * for all subprocesses so that we can have exit-on-close => false).
 */

struct _IdeFlatpakSubprocess
{
  GObject parent_instance;

  GDBusConnection *connection;
  gulong connection_closed_handler;

  GPid client_pid;
  int status;

  GSubprocessFlags flags;

  /* No reference */
  GThread *spawn_thread;

  char **argv;
  char **env;
  char *cwd;

  char *identifier;

  GOutputStream *stdin_pipe;
  GInputStream *stdout_pipe;
  GInputStream *stderr_pipe;

  IdeUnixFDMap *unix_fd_map;

  GMainContext *main_context;

  guint exited_subscription;

  /* GList of IdeTasks for wait_async() */
  GList *waiting;

  /* Mutex/Cond pair guards client_has_exited */
  GMutex waiter_mutex;
  GCond waiter_cond;

  guint client_has_exited : 1;
  guint clear_env : 1;
};

/* ide_subprocess_communicate implementation below:
 *
 * This is a tough problem.  We have to watch 5 things at the same time:
 *
 *  - writing to stdin made progress
 *  - reading from stdout made progress
 *  - reading from stderr made progress
 *  - process terminated
 *  - cancellable being cancelled by caller
 *
 * We use a GMainContext for all of these (either as async function
 * calls or as a GSource (in the case of the cancellable).  That way at
 * least we don't have to worry about threading.
 *
 * For the sync case we use the usual trick of creating a private main
 * context and iterating it until completion.
 *
 * It's very possible that the process will dump a lot of data to stdout
 * just before it quits, so we can easily have data to read from stdout
 * and see the process has terminated at the same time.  We want to make
 * sure that we read all of the data from the pipes first, though, so we
 * do IO operations at a higher priority than the wait operation (which
 * is at G_IO_PRIORITY_DEFAULT).  Even in the case that we have to do
 * multiple reads to get this data, the pipe() will always be polling
 * as ready and with the async result for the read at a higher priority,
 * the main context will not dispatch the completion for the wait().
 *
 * We keep our own private GCancellable.  In the event that any of the
 * above suffers from an error condition (including the user cancelling
 * their cancellable) we immediately dispatch the IdeTask with the error
 * result and fire our cancellable to cleanup any pending operations.
 * In the case that the error is that the user's cancellable was fired,
 * it's vaguely wasteful to report an error because IdeTask will handle
 * this automatically, so we just return FALSE.
 *
 * We let each pending sub-operation take a ref on the IdeTask of the
 * communicate operation.  We have to be careful that we don't report
 * the task completion more than once, though, so we keep a flag for
 * that.
 */
typedef struct
{
  const char          *stdin_data;
  gsize                stdin_length;
  gsize                stdin_offset;

  gboolean             add_nul;

  GInputStream        *stdin_buf;
  GMemoryOutputStream *stdout_buf;
  GMemoryOutputStream *stderr_buf;

  GCancellable        *cancellable;
  GSource             *cancellable_source;

  guint                outstanding_ops;
  gboolean             reported_error;
} CommunicateState;

enum {
  PROP_0,
  PROP_ARGV,
  PROP_CLEAR_ENV,
  PROP_CWD,
  PROP_ENV,
  PROP_FLAGS,
  PROP_UNIX_FD_MAP,
  N_PROPS
};

static void              ide_flatpak_subprocess_sync_complete        (IdeFlatpakSubprocess   *self,
                                                                      GAsyncResult         **result);
static void              ide_flatpak_subprocess_sync_done            (GObject               *object,
                                                                      GAsyncResult          *result,
                                                                      gpointer               user_data);
static void              ide_flatpak_subprocess_sync_setup           (IdeFlatpakSubprocess  *self);
static CommunicateState *ide_flatpak_subprocess_communicate_internal (IdeFlatpakSubprocess  *subprocess,
                                                                      gboolean               add_nul,
                                                                      GBytes                *stdin_buf,
                                                                      GCancellable          *cancellable,
                                                                      GAsyncReadyCallback    callback,
                                                                      gpointer               user_data);

static GParamSpec *properties [N_PROPS];

static const char *
ide_flatpak_subprocess_get_identifier (IdeSubprocess *subprocess)
{
  IdeFlatpakSubprocess *self = (IdeFlatpakSubprocess *)subprocess;

  g_assert (IDE_IS_FLATPAK_SUBPROCESS (self));

  return self->identifier;
}

static GInputStream *
ide_flatpak_subprocess_get_stdout_pipe (IdeSubprocess *subprocess)
{
  IdeFlatpakSubprocess *self = (IdeFlatpakSubprocess *)subprocess;

  g_assert (IDE_IS_FLATPAK_SUBPROCESS (self));

  return self->stdout_pipe;
}

static GInputStream *
ide_flatpak_subprocess_get_stderr_pipe (IdeSubprocess *subprocess)
{
  IdeFlatpakSubprocess *self = (IdeFlatpakSubprocess *)subprocess;

  g_assert (IDE_IS_FLATPAK_SUBPROCESS (self));

  return self->stderr_pipe;
}

static GOutputStream *
ide_flatpak_subprocess_get_stdin_pipe (IdeSubprocess *subprocess)
{
  IdeFlatpakSubprocess *self = (IdeFlatpakSubprocess *)subprocess;

  g_assert (IDE_IS_FLATPAK_SUBPROCESS (self));

  return self->stdin_pipe;
}

static void
ide_flatpak_subprocess_wait_cb (GObject      *object,
                                 GAsyncResult *result,
                                 gpointer      user_data)
{
  IdeFlatpakSubprocess *self = (IdeFlatpakSubprocess *)object;
  gboolean *completed = user_data;

  g_assert (IDE_IS_FLATPAK_SUBPROCESS (self));
  g_assert (completed != NULL);

  ide_subprocess_wait_finish (IDE_SUBPROCESS (self), result, NULL);

  *completed = TRUE;

  if (self->main_context != NULL)
    g_main_context_wakeup (self->main_context);
}

static gboolean
ide_flatpak_subprocess_wait (IdeSubprocess  *subprocess,
                              GCancellable   *cancellable,
                              GError        **error)
{
  IdeFlatpakSubprocess *self = (IdeFlatpakSubprocess *)subprocess;

  g_assert (IDE_IS_FLATPAK_SUBPROCESS (self));

  g_object_ref (self);

  g_mutex_lock (&self->waiter_mutex);

  if (!self->client_has_exited)
    {
      g_autoptr(GMainContext) free_me = NULL;
      GMainContext *main_context;
      gboolean completed = FALSE;

      if (NULL == (main_context = g_main_context_get_thread_default ()))
        {
          if (IDE_IS_MAIN_THREAD ())
            main_context = g_main_context_default ();
          else
            main_context = free_me = g_main_context_new ();
        }

      self->main_context = g_main_context_ref (main_context);
      g_mutex_unlock (&self->waiter_mutex);

      ide_subprocess_wait_async (IDE_SUBPROCESS (self),
                                 cancellable,
                                 ide_flatpak_subprocess_wait_cb,
                                 &completed);

      while (!completed)
        g_main_context_iteration (main_context, TRUE);

      goto cleanup;
    }

  g_mutex_unlock (&self->waiter_mutex);

cleanup:
  g_object_unref (self);

  return self->client_has_exited;
}

static void
ide_flatpak_subprocess_wait_async (IdeSubprocess       *subprocess,
                                    GCancellable        *cancellable,
                                    GAsyncReadyCallback  callback,
                                    gpointer             user_data)
{
  IdeFlatpakSubprocess *self = (IdeFlatpakSubprocess *)subprocess;
  g_autoptr(IdeTask) task = NULL;
  g_autoptr(GMutexLocker) locker = NULL;

  g_assert (IDE_IS_FLATPAK_SUBPROCESS (self));
  g_assert (!cancellable || G_IS_CANCELLABLE (cancellable));

  task = ide_task_new (self, cancellable, callback, user_data);
  ide_task_set_source_tag (task, ide_flatpak_subprocess_wait_async);
  ide_task_set_priority (task, G_PRIORITY_DEFAULT_IDLE);

  locker = g_mutex_locker_new (&self->waiter_mutex);

  if (self->client_has_exited)
    {
      ide_task_return_boolean (task, TRUE);
      return;
    }

  self->waiting = g_list_append (self->waiting, g_steal_pointer (&task));
}

static gboolean
ide_flatpak_subprocess_wait_finish (IdeSubprocess  *subprocess,
                                     GAsyncResult   *result,
                                     GError        **error)
{
  g_assert (IDE_IS_FLATPAK_SUBPROCESS (subprocess));
  g_assert (IDE_IS_TASK (result));

  return ide_task_propagate_boolean (IDE_TASK (result), error);
}

static void
ide_flatpak_subprocess_communicate_utf8_async (IdeSubprocess       *subprocess,
                                                const char          *stdin_buf,
                                                GCancellable        *cancellable,
                                                GAsyncReadyCallback  callback,
                                                gpointer             user_data)
{
  IdeFlatpakSubprocess *self = (IdeFlatpakSubprocess *)subprocess;
  g_autoptr(GBytes) stdin_bytes = NULL;
  size_t stdin_buf_len = 0;

  g_return_if_fail (IDE_IS_FLATPAK_SUBPROCESS (subprocess));
  g_return_if_fail (stdin_buf == NULL || (self->flags & G_SUBPROCESS_FLAGS_STDIN_PIPE));
  g_return_if_fail (cancellable == NULL || G_IS_CANCELLABLE (cancellable));

  if (stdin_buf != NULL)
    stdin_buf_len = strlen (stdin_buf);
  stdin_bytes = g_bytes_new (stdin_buf, stdin_buf_len);

  ide_flatpak_subprocess_communicate_internal (self, TRUE, stdin_bytes, cancellable, callback, user_data);
}

static gboolean
communicate_result_validate_utf8 (const char            *stream_name,
                                  char                 **return_location,
                                  GMemoryOutputStream   *buffer,
                                  GError               **error)
{
  IDE_ENTRY;

  if (return_location == NULL)
    IDE_RETURN (TRUE);

  if (buffer)
    {
      const char *end;
      GError *local_error = NULL;

      if (!g_output_stream_is_closed (G_OUTPUT_STREAM (buffer)))
        g_output_stream_close (G_OUTPUT_STREAM (buffer), NULL, &local_error);

      if (local_error != NULL)
        {
          g_propagate_error (error, local_error);
          IDE_RETURN (FALSE);
        }

      *return_location = g_memory_output_stream_steal_data (buffer);
      if (!g_utf8_validate (*return_location, -1, &end))
        {
          g_free (*return_location);
          g_set_error (error, G_IO_ERROR, G_IO_ERROR_FAILED,
                       "Invalid UTF-8 in child %s at offset %lu",
                       stream_name,
                       (unsigned long) (end - *return_location));
          IDE_RETURN (FALSE);
        }
    }
  else
    *return_location = NULL;

  IDE_RETURN (TRUE);
}

static gboolean
ide_flatpak_subprocess_communicate_utf8_finish (IdeSubprocess  *subprocess,
                                                 GAsyncResult   *result,
                                                 char          **stdout_buf,
                                                 char          **stderr_buf,
                                                 GError        **error)
{
  gboolean ret = FALSE;
  CommunicateState *state;

  IDE_ENTRY;

  g_return_val_if_fail (IDE_IS_FLATPAK_SUBPROCESS (subprocess), FALSE);
  g_return_val_if_fail (ide_task_is_valid (result, subprocess), FALSE);
  g_return_val_if_fail (error == NULL || *error == NULL, FALSE);

  g_object_ref (result);

  state = ide_task_get_task_data ((IdeTask*)result);
  if (!ide_task_propagate_boolean ((IdeTask*)result, error))
    IDE_GOTO (out);

  if (!communicate_result_validate_utf8 ("stdout", stdout_buf, state->stdout_buf, error))
    IDE_GOTO (out);

  if (!communicate_result_validate_utf8 ("stderr", stderr_buf, state->stderr_buf, error))
    IDE_GOTO (out);

  ret = TRUE;

 out:
  g_object_unref (result);

  IDE_RETURN (ret);
}

static gboolean
ide_flatpak_subprocess_communicate_utf8 (IdeSubprocess  *subprocess,
                                         const char     *stdin_buf,
                                         GCancellable   *cancellable,
                                         char          **stdout_buf,
                                         char          **stderr_buf,
                                         GError        **error)
{
  IdeFlatpakSubprocess *self = (IdeFlatpakSubprocess *)subprocess;
  g_autoptr(GAsyncResult) result = NULL;
  g_autoptr(GBytes) stdin_bytes = NULL;
  size_t stdin_buf_len = 0;
  gboolean success;

  IDE_ENTRY;

  g_return_val_if_fail (IDE_IS_FLATPAK_SUBPROCESS (subprocess), FALSE);
  g_return_val_if_fail (stdin_buf == NULL || (self->flags & G_SUBPROCESS_FLAGS_STDIN_PIPE), FALSE);
  g_return_val_if_fail (cancellable == NULL || G_IS_CANCELLABLE (cancellable), FALSE);
  g_return_val_if_fail (error == NULL || *error == NULL, FALSE);

  if (stdin_buf != NULL)
    stdin_buf_len = strlen (stdin_buf);
  stdin_bytes = g_bytes_new (stdin_buf, stdin_buf_len);

  ide_flatpak_subprocess_sync_setup (self);
  ide_flatpak_subprocess_communicate_internal (self,
                                               TRUE,
                                               stdin_bytes,
                                               cancellable,
                                               ide_flatpak_subprocess_sync_done,
                                               &result);
  ide_flatpak_subprocess_sync_complete (self, &result);
  success = ide_subprocess_communicate_utf8_finish (subprocess, result, stdout_buf, stderr_buf, error);

  IDE_RETURN (success);
}

static gboolean
ide_flatpak_subprocess_get_successful (IdeSubprocess *subprocess)
{
  IdeFlatpakSubprocess *self = (IdeFlatpakSubprocess *)subprocess;

  g_assert (IDE_IS_FLATPAK_SUBPROCESS (self));

  return WIFEXITED (self->status) && WEXITSTATUS (self->status) == 0;
}

static gboolean
ide_flatpak_subprocess_get_if_exited (IdeSubprocess *subprocess)
{
  IdeFlatpakSubprocess *self = (IdeFlatpakSubprocess *)subprocess;

  g_assert (IDE_IS_FLATPAK_SUBPROCESS (self));

  return WIFEXITED (self->status);
}

static int
ide_flatpak_subprocess_get_exit_status (IdeSubprocess *subprocess)
{
  IdeFlatpakSubprocess *self = (IdeFlatpakSubprocess *)subprocess;

  g_assert (IDE_IS_FLATPAK_SUBPROCESS (self));
  g_assert (self->client_has_exited);

  if (!WIFEXITED (self->status))
    return 1;

  return WEXITSTATUS (self->status);
}

static gboolean
ide_flatpak_subprocess_get_if_signaled (IdeSubprocess *subprocess)
{
  IdeFlatpakSubprocess *self = (IdeFlatpakSubprocess *)subprocess;

  g_assert (IDE_IS_FLATPAK_SUBPROCESS (self));
  g_assert (self->client_has_exited == TRUE);

  return WIFSIGNALED (self->status);
}

static int
ide_flatpak_subprocess_get_term_sig (IdeSubprocess *subprocess)
{
  IdeFlatpakSubprocess *self = (IdeFlatpakSubprocess *)subprocess;

  g_assert (IDE_IS_FLATPAK_SUBPROCESS (self));
  g_assert (self->client_has_exited == TRUE);

  return WTERMSIG (self->status);
}

static int
ide_flatpak_subprocess_get_status (IdeSubprocess *subprocess)
{
  IdeFlatpakSubprocess *self = (IdeFlatpakSubprocess *)subprocess;

  g_assert (IDE_IS_FLATPAK_SUBPROCESS (self));
  g_assert (self->client_has_exited == TRUE);

  return self->status;
}

static void
ide_flatpak_subprocess_send_signal (IdeSubprocess *subprocess,
                                     int           signal_num)
{
  IdeFlatpakSubprocess *self = (IdeFlatpakSubprocess *)subprocess;

  IDE_ENTRY;

  g_assert (IDE_IS_FLATPAK_SUBPROCESS (self));

  /* Signal delivery is not guaranteed, so we can drop this on the floor. */
  if (self->client_has_exited || self->connection == NULL)
    IDE_EXIT;

  IDE_TRACE_MSG ("Sending signal %d to pid %u", signal_num, (guint)self->client_pid);

  g_dbus_connection_call_sync (self->connection,
                               "org.freedesktop.Flatpak",
                               "/org/freedesktop/Flatpak/Development",
                               "org.freedesktop.Flatpak.Development",
                               "HostCommandSignal",
                               g_variant_new ("(uub)", self->client_pid, signal_num, TRUE),
                               NULL,
                               G_DBUS_CALL_FLAGS_NONE, -1,
                               NULL, NULL);

  IDE_EXIT;
}

static void
ide_flatpak_subprocess_force_exit (IdeSubprocess *subprocess)
{
  g_assert (IDE_IS_FLATPAK_SUBPROCESS (subprocess));

  ide_flatpak_subprocess_send_signal (subprocess, SIGKILL);
}

static void
ide_flatpak_subprocess_sync_setup (IdeFlatpakSubprocess *self)
{
  g_autoptr(GMainContext) free_me = NULL;
  GMainContext *main_context = NULL;

  IDE_ENTRY;

  g_assert (IDE_IS_FLATPAK_SUBPROCESS (self));

  g_mutex_lock (&self->waiter_mutex);

  g_assert (self->main_context == NULL);

  if (NULL == (main_context = g_main_context_get_thread_default ()))
    {
      if (IDE_IS_MAIN_THREAD ())
        main_context = g_main_context_default ();
      else
        main_context = free_me = g_main_context_new ();
    }

  self->main_context = g_main_context_ref (main_context);

  g_mutex_unlock (&self->waiter_mutex);

  IDE_EXIT;
}

static void
ide_flatpak_subprocess_sync_complete (IdeFlatpakSubprocess  *self,
                                       GAsyncResult          **result)
{
  IDE_ENTRY;

  g_assert (IDE_IS_FLATPAK_SUBPROCESS (self));
  g_assert (result != NULL);
  g_assert (*result == NULL || G_IS_ASYNC_RESULT (*result));
  g_assert (self->main_context != NULL);

  while (*result == NULL)
    g_main_context_iteration (self->main_context, TRUE);

  IDE_EXIT;
}

static void
ide_flatpak_subprocess_sync_done (GObject      *object,
                                   GAsyncResult *result,
                                   gpointer      user_data)
{
  IdeFlatpakSubprocess *self = (IdeFlatpakSubprocess *)object;
  GAsyncResult **ret = user_data;

  IDE_ENTRY;

  g_assert (IDE_IS_FLATPAK_SUBPROCESS (self));
  g_assert (ret != NULL);
  g_assert (*ret == NULL);
  g_assert (G_IS_ASYNC_RESULT (result));

  *ret = g_object_ref (result);

  g_mutex_lock (&self->waiter_mutex);
  if (self->main_context != NULL)
    g_main_context_wakeup (self->main_context);
  g_mutex_unlock (&self->waiter_mutex);

  IDE_EXIT;
}

static void
ide_subprocess_communicate_state_free (gpointer data)
{
  CommunicateState *state = data;

  g_clear_object (&state->cancellable);
  g_clear_object (&state->stdin_buf);
  g_clear_object (&state->stdout_buf);
  g_clear_object (&state->stderr_buf);

  if (state->cancellable_source != NULL)
    {
      GSource *source = g_steal_pointer (&state->cancellable_source);

      if (!g_source_is_destroyed (source))
        g_source_destroy (source);
      g_source_unref (source);
    }

  g_slice_free (CommunicateState, state);
}

static gboolean
ide_subprocess_communicate_cancelled (gpointer user_data)
{
  CommunicateState *state = user_data;

  IDE_ENTRY;

  g_assert (state != NULL);
  g_assert (!state->cancellable || G_IS_CANCELLABLE (state->cancellable));

  if (state->cancellable != NULL)
    g_cancellable_cancel (state->cancellable);

  IDE_RETURN (G_SOURCE_REMOVE);
}

static void
ide_subprocess_communicate_made_progress (GObject      *source_object,
                                          GAsyncResult *result,
                                          gpointer      user_data)
{
  CommunicateState *state;
  IdeFlatpakSubprocess *subprocess;
  g_autoptr(GError) error = NULL;
  g_autoptr(IdeTask) task = user_data;
  gpointer source;

  IDE_ENTRY;

  g_assert (source_object != NULL);

  subprocess = ide_task_get_source_object (task);
  state = ide_task_get_task_data (task);
  source = source_object;

  state->outstanding_ops--;

  if (source == subprocess->stdin_pipe ||
      source == state->stdout_buf ||
      source == state->stderr_buf)
    {
      if (g_output_stream_splice_finish (source, result, &error) == -1)
        IDE_GOTO (out);

      if (source == state->stdout_buf || source == state->stderr_buf)
        {
          /* This is a memory stream, so it can't be cancelled or return
           * an error really.
           */
          if (state->add_nul)
            {
              gsize bytes_written = 0;
              if (!g_output_stream_write_all (source, "\0", 1, &bytes_written, NULL, &error))
                IDE_GOTO (out);
            }
          if (!g_output_stream_close (source, NULL, &error))
            IDE_GOTO (out);
        }
    }
  else if (source == subprocess)
    {
      ide_subprocess_wait_finish (IDE_SUBPROCESS (subprocess), result, &error);
    }
  else
    g_assert_not_reached ();

 out:
  if (error != NULL)
    {
      /* Only report the first error we see.
       *
       * We might be seeing an error as a result of the cancellation
       * done when the process quits.
       */
      if (!state->reported_error)
        {
          state->reported_error = TRUE;
          g_cancellable_cancel (state->cancellable);
          ide_task_return_error (task, g_steal_pointer (&error));
        }
    }
  else if (state->outstanding_ops == 0)
    {
      ide_task_return_boolean (task, TRUE);
    }

  IDE_EXIT;
}

static CommunicateState *
ide_flatpak_subprocess_communicate_internal (IdeFlatpakSubprocess *subprocess,
                                             gboolean              add_nul,
                                             GBytes               *stdin_buf,
                                             GCancellable         *cancellable,
                                             GAsyncReadyCallback   callback,
                                             gpointer              user_data)
{
  CommunicateState *state;
  g_autoptr(IdeTask) task = NULL;

  IDE_ENTRY;

  g_assert (IDE_IS_FLATPAK_SUBPROCESS (subprocess));
  g_assert (!cancellable || G_IS_CANCELLABLE (cancellable));

  task = ide_task_new (subprocess, cancellable, callback, user_data);
  ide_task_set_source_tag (task, ide_flatpak_subprocess_communicate_internal);
  ide_task_set_priority (task, G_PRIORITY_DEFAULT_IDLE);
  ide_task_set_release_on_propagate (task, FALSE);

  state = g_slice_new0 (CommunicateState);
  ide_task_set_task_data (task, state, ide_subprocess_communicate_state_free);

  state->cancellable = g_cancellable_new ();
  state->add_nul = add_nul;
  state->outstanding_ops = 1;

  if (cancellable)
    {
      state->cancellable_source = g_cancellable_source_new (cancellable);
      /* No ref held here, but we unref the source from state's free function */
      g_source_set_callback (state->cancellable_source, ide_subprocess_communicate_cancelled, state, NULL);
      g_source_attach (state->cancellable_source, g_main_context_get_thread_default ());
    }

  /* Increment the outstanding ops count, to protect from reentrancy */
  if (subprocess->stdin_pipe)
    state->outstanding_ops++;
  if (subprocess->stdout_pipe)
    state->outstanding_ops++;
  if (subprocess->stderr_pipe)
    state->outstanding_ops++;

  if (subprocess->stdin_pipe)
    {
      g_assert (stdin_buf != NULL);
      state->stdin_buf = g_memory_input_stream_new_from_bytes (stdin_buf);
      g_output_stream_splice_async (subprocess->stdin_pipe, (GInputStream*)state->stdin_buf,
                                    G_OUTPUT_STREAM_SPLICE_CLOSE_SOURCE | G_OUTPUT_STREAM_SPLICE_CLOSE_TARGET,
                                    G_PRIORITY_DEFAULT, state->cancellable,
                                    ide_subprocess_communicate_made_progress, g_object_ref (task));
    }

  if (subprocess->stdout_pipe)
    {
      state->stdout_buf = (GMemoryOutputStream*)g_memory_output_stream_new_resizable ();
      g_output_stream_splice_async ((GOutputStream*)state->stdout_buf, subprocess->stdout_pipe,
                                    G_OUTPUT_STREAM_SPLICE_CLOSE_SOURCE,
                                    G_PRIORITY_DEFAULT, state->cancellable,
                                    ide_subprocess_communicate_made_progress, g_object_ref (task));
    }

  if (subprocess->stderr_pipe)
    {
      state->stderr_buf = (GMemoryOutputStream*)g_memory_output_stream_new_resizable ();
      g_output_stream_splice_async ((GOutputStream*)state->stderr_buf, subprocess->stderr_pipe,
                                    G_OUTPUT_STREAM_SPLICE_CLOSE_SOURCE,
                                    G_PRIORITY_DEFAULT, state->cancellable,
                                    ide_subprocess_communicate_made_progress, g_object_ref (task));
    }

  ide_subprocess_wait_async (IDE_SUBPROCESS (subprocess), state->cancellable,
                             ide_subprocess_communicate_made_progress, g_object_ref (task));

  IDE_RETURN (state);
}

static void
ide_flatpak_subprocess_communicate_async (IdeSubprocess       *subprocess,
                                           GBytes              *stdin_buf,
                                           GCancellable        *cancellable,
                                           GAsyncReadyCallback  callback,
                                           gpointer             user_data)
{
  IdeFlatpakSubprocess *self = (IdeFlatpakSubprocess *)subprocess;

  g_assert (IDE_IS_FLATPAK_SUBPROCESS (self));
  g_assert (!cancellable || G_IS_CANCELLABLE (cancellable));

  ide_flatpak_subprocess_communicate_internal (self, FALSE, stdin_buf, cancellable, callback, user_data);
}

static gboolean
ide_flatpak_subprocess_communicate_finish (IdeSubprocess  *subprocess,
                                            GAsyncResult   *result,
                                            GBytes        **stdout_buf,
                                            GBytes        **stderr_buf,
                                            GError        **error)
{
  CommunicateState *state;
  IdeTask *task = (IdeTask *)result;
  gboolean success;

  IDE_ENTRY;

  g_assert (IDE_IS_FLATPAK_SUBPROCESS (subprocess));
  g_assert (IDE_IS_TASK (task));

  g_object_ref (task);

  state = ide_task_get_task_data (task);

  g_assert (state != NULL);

  success = ide_task_propagate_boolean (task, error);

  if (success)
    {
      if (stdout_buf)
        *stdout_buf = state->stdout_buf ?
                      g_memory_output_stream_steal_as_bytes (state->stdout_buf) :
                      g_bytes_new (NULL, 0);

      if (stderr_buf)
        *stderr_buf = state->stderr_buf ?
                      g_memory_output_stream_steal_as_bytes (state->stderr_buf) :
                      g_bytes_new (NULL, 0);
    }

  g_object_unref (task);

  IDE_RETURN (success);
}

static gboolean
ide_flatpak_subprocess_communicate (IdeSubprocess  *subprocess,
                                    GBytes         *stdin_buf,
                                    GCancellable   *cancellable,
                                    GBytes        **stdout_buf,
                                    GBytes        **stderr_buf,
                                    GError        **error)
{
  IdeFlatpakSubprocess *self = (IdeFlatpakSubprocess *)subprocess;
  g_autoptr(GAsyncResult) result = NULL;
  gboolean ret;

  IDE_ENTRY;

  g_assert (IDE_IS_FLATPAK_SUBPROCESS (self));
  g_assert (!cancellable || G_IS_CANCELLABLE (cancellable));

  ide_flatpak_subprocess_sync_setup (self);
  ide_flatpak_subprocess_communicate_internal (self,
                                               FALSE,
                                               stdin_buf,
                                               cancellable,
                                               ide_flatpak_subprocess_sync_done,
                                               &result);
  ide_flatpak_subprocess_sync_complete (self, &result);

  ret = ide_flatpak_subprocess_communicate_finish (subprocess, result, stdout_buf, stderr_buf, error);

  IDE_RETURN (ret);
}

static void
subprocess_iface_init (IdeSubprocessInterface *iface)
{
  iface->get_identifier = ide_flatpak_subprocess_get_identifier;
  iface->get_stdout_pipe = ide_flatpak_subprocess_get_stdout_pipe;
  iface->get_stderr_pipe = ide_flatpak_subprocess_get_stderr_pipe;
  iface->get_stdin_pipe = ide_flatpak_subprocess_get_stdin_pipe;
  iface->wait = ide_flatpak_subprocess_wait;
  iface->wait_async = ide_flatpak_subprocess_wait_async;
  iface->wait_finish = ide_flatpak_subprocess_wait_finish;
  iface->get_successful = ide_flatpak_subprocess_get_successful;
  iface->get_if_exited = ide_flatpak_subprocess_get_if_exited;
  iface->get_exit_status = ide_flatpak_subprocess_get_exit_status;
  iface->get_if_signaled = ide_flatpak_subprocess_get_if_signaled;
  iface->get_term_sig = ide_flatpak_subprocess_get_term_sig;
  iface->get_status = ide_flatpak_subprocess_get_status;
  iface->send_signal = ide_flatpak_subprocess_send_signal;
  iface->force_exit = ide_flatpak_subprocess_force_exit;
  iface->communicate = ide_flatpak_subprocess_communicate;
  iface->communicate_utf8 = ide_flatpak_subprocess_communicate_utf8;
  iface->communicate_async = ide_flatpak_subprocess_communicate_async;
  iface->communicate_finish = ide_flatpak_subprocess_communicate_finish;
  iface->communicate_utf8_async = ide_flatpak_subprocess_communicate_utf8_async;
  iface->communicate_utf8_finish = ide_flatpak_subprocess_communicate_utf8_finish;
}

static void
maybe_create_input_stream (GInputStream **ret,
                           int          *fdptr,
                           gboolean       needs_stream)
{
  g_assert (ret != NULL);
  g_assert (*ret == NULL);
  g_assert (fdptr != NULL);

  /*
   * Only create a stream if we aren't merging to stdio and the flags request
   * that we need a stream.  We are also stealing the file-descriptor while
   * doing so.
   */
  if (needs_stream)
    {
      if (*fdptr > 2)
        *ret = g_unix_input_stream_new (*fdptr, TRUE);
    }
  else if (*fdptr != -1)
    {
      close (*fdptr);
    }

  *fdptr = -1;
}

static void
maybe_create_output_stream (GOutputStream **ret,
                            int           *fdptr,
                            gboolean        needs_stream)
{
  g_assert (ret != NULL);
  g_assert (*ret == NULL);
  g_assert (fdptr != NULL);

  /*
   * Only create a stream if we aren't merging to stdio and the flags request
   * that we need a stream.  We are also stealing the file-descriptor while
   * doing so.
   */
  if (needs_stream)
    {
      if (*fdptr > 2)
        *ret = g_unix_output_stream_new (*fdptr, TRUE);
    }
  else if (*fdptr != -1)
    {
      close (*fdptr);
    }

  *fdptr = -1;
}

static void
ide_flatpak_subprocess_complete_command_locked (IdeFlatpakSubprocess *self,
                                                int                  exit_status)
{
  GList *waiting;

  IDE_ENTRY;

  g_assert (IDE_IS_FLATPAK_SUBPROCESS (self));
  g_assert (G_IS_DBUS_CONNECTION (self->connection));

  self->client_has_exited = TRUE;
  self->status = exit_status;

  /*
   * Clear process identifiers to prevent accidental use by API consumers
   * after the process has exited.
   */
  self->client_pid = 0;
  g_clear_pointer (&self->identifier, g_free);

  /* Complete async workers */
  waiting = g_steal_pointer (&self->waiting);

  for (const GList *iter = waiting; iter != NULL; iter = iter->next)
    {
      g_autoptr(IdeTask) task = iter->data;

      ide_task_return_boolean (task, TRUE);
    }

  g_list_free (waiting);

  /* Notify synchronous waiters */
  g_cond_broadcast (&self->waiter_cond);

  g_signal_handler_disconnect (self->connection, self->connection_closed_handler);
  self->connection_closed_handler = 0;

  g_clear_object (&self->connection);

  if (self->main_context != NULL)
    g_main_context_wakeup (self->main_context);

  IDE_EXIT;
}

static void
host_command_exited_cb (GDBusConnection *connection,
                        const char      *sender_name,
                        const char      *object_path,
                        const char      *interface_name,
                        const char      *signal_name,
                        GVariant        *parameters,
                        gpointer         user_data)
{
  g_autoptr(IdeFlatpakSubprocess) finalize_protect = NULL;
  IdeFlatpakSubprocess *self = user_data;
  g_autoptr(GMutexLocker) locker = NULL;
  guint32 client_pid = 0;
  guint32 exit_status = 0;

  IDE_ENTRY;

  g_assert (G_IS_DBUS_CONNECTION (connection));
  g_assert (IDE_IS_FLATPAK_SUBPROCESS (self));

  finalize_protect = g_object_ref (self);

  if (!g_variant_is_of_type (parameters, G_VARIANT_TYPE ("(uu)")))
    IDE_EXIT;

  g_variant_get (parameters, "(uu)", &client_pid, &exit_status);
  if (client_pid != (guint32)self->client_pid)
    IDE_EXIT;

  locker = g_mutex_locker_new (&self->waiter_mutex);

  IDE_TRACE_MSG ("Host process %u exited with %u",
                 (guint)self->client_pid,
                 (guint)exit_status);

  /* We can release our D-Bus signal handler now */
  if (self->exited_subscription != 0)
    {
      IDE_TRACE_MSG ("Unsubscribing from D-Bus subscription %d", self->exited_subscription);
      g_dbus_connection_signal_unsubscribe (self->connection, self->exited_subscription);
      self->exited_subscription = 0;
    }

  ide_flatpak_subprocess_complete_command_locked (self, exit_status);

  IDE_EXIT;
}

static void
ide_flatpak_subprocess_cancelled (IdeFlatpakSubprocess *self,
                                  GCancellable         *cancellable)
{
  IDE_ENTRY;

  g_assert (G_IS_CANCELLABLE (cancellable));
  g_assert (IDE_IS_FLATPAK_SUBPROCESS (self));

  ide_subprocess_force_exit (IDE_SUBPROCESS (self));

  IDE_EXIT;
}

static inline void
maybe_close (int *fd)
{
  g_assert (fd != NULL);
  g_assert (*fd >= -1);

  if (*fd > 2)
    close (*fd);

  *fd = -1;
}

static void
ide_flatpak_subprocess_connection_closed (IdeFlatpakSubprocess *self,
                                          gboolean              remote_peer_vanished,
                                          const GError         *error,
                                          GDBusConnection      *connection)
{
  g_autoptr(GMutexLocker) locker = NULL;

  IDE_ENTRY;

  g_assert (IDE_IS_FLATPAK_SUBPROCESS (self));
  g_assert (G_IS_DBUS_CONNECTION (connection));

  locker = g_mutex_locker_new (&self->waiter_mutex);

  IDE_TRACE_MSG ("Synthesizing failure for client pid %u", (guint)self->client_pid);

  self->exited_subscription = 0;
  ide_flatpak_subprocess_complete_command_locked (self, -1);

  IDE_EXIT;
}

static gboolean
ide_flatpak_subprocess_initable_init (GInitable     *initable,
                                      GCancellable  *cancellable,
                                      GError       **error)
{
  IdeFlatpakSubprocess *self = (IdeFlatpakSubprocess *)initable;
  g_autoptr(GVariantBuilder) fd_builder = g_variant_builder_new (G_VARIANT_TYPE ("a{uh}"));
  g_autoptr(GVariantBuilder) env_builder = g_variant_builder_new (G_VARIANT_TYPE ("a{ss}"));
  g_autoptr(GUnixFDList) fd_list = g_unix_fd_list_new ();
  g_autoptr(GVariant) reply = NULL;
  g_autoptr(GVariant) params = NULL;
  guint32 client_pid = 0;
  int stdout_pair[2] = { -1, -1 };
  int stderr_pair[2] = { -1, -1 };
  int stdin_pair[2] = { -1, -1 };
  int stdin_handle = -1;
  int stdout_handle = -1;
  int stderr_handle = -1;
  gboolean ret = FALSE;
  guint flags = FLATPAK_HOST_COMMAND_FLAGS_WATCH_BUS;
  guint length;

  IDE_ENTRY;

  g_assert (IDE_IS_FLATPAK_SUBPROCESS (self));
  g_assert (IDE_IS_UNIX_FD_MAP (self->unix_fd_map));
  g_assert (!cancellable || G_IS_CANCELLABLE (cancellable));

  if (!(self->connection = g_bus_get_sync (G_BUS_TYPE_SESSION, cancellable, error)))
    IDE_RETURN (FALSE);

  if (self->clear_env)
    flags |= FLATPAK_HOST_COMMAND_FLAGS_CLEAR_ENV;

  /* Handle STDIN for the process.
   *
   * Make sure we handle inherit STDIN, a new pipe (so that the application can
   * get the stdin stream), or simply redirect to /dev/null.
   */
  if (-1 != (stdin_pair[0] = ide_unix_fd_map_steal_stdin (self->unix_fd_map)))
    {
      self->flags &= ~G_SUBPROCESS_FLAGS_STDIN_PIPE;
    }
  else if (self->flags & G_SUBPROCESS_FLAGS_STDIN_INHERIT)
    {
      self->flags &= ~G_SUBPROCESS_FLAGS_STDIN_PIPE;
      stdin_pair[0] = STDIN_FILENO;
    }
  else if (self->flags & G_SUBPROCESS_FLAGS_STDIN_PIPE)
    {
      if (!g_unix_open_pipe (stdin_pair, FD_CLOEXEC, error))
        IDE_GOTO (cleanup_fds);
    }
  else
    {
      self->flags &= ~G_SUBPROCESS_FLAGS_STDIN_PIPE;
      stdin_pair[0] = open ("/dev/null", O_CLOEXEC | O_RDWR, 0);
      if (stdin_pair[0] == -1)
        IDE_GOTO (cleanup_fds);
    }

  g_assert (stdin_pair[0] != -1);

  stdin_handle = g_unix_fd_list_append (fd_list, stdin_pair[0], error);
  if (stdin_handle == -1)
    IDE_GOTO (cleanup_fds);
  else
    maybe_close (&stdin_pair[0]);


  /* Setup STDOUT for the process.
   *
   * Make sure we redirect STDOUT to our stdout, unless a pipe was requested
   * for the application to read. However, if silence was requested, redirect
   * to /dev/null.
   */
  if (-1 != (stdout_pair[1] = ide_unix_fd_map_steal_stdout (self->unix_fd_map)))
    {
      self->flags &= ~G_SUBPROCESS_FLAGS_STDOUT_PIPE;
    }
  else if (self->flags & G_SUBPROCESS_FLAGS_STDOUT_SILENCE)
    {
      self->flags &= ~G_SUBPROCESS_FLAGS_STDOUT_PIPE;
      stdout_pair[1] = open ("/dev/null", O_CLOEXEC | O_RDWR, 0);
      if (stdout_pair[1] == -1)
        IDE_GOTO (cleanup_fds);
    }
  else if (self->flags & G_SUBPROCESS_FLAGS_STDOUT_PIPE)
    {
      if (!g_unix_open_pipe (stdout_pair, FD_CLOEXEC, error))
        IDE_GOTO (cleanup_fds);
    }
  else
    {
      self->flags &= ~G_SUBPROCESS_FLAGS_STDOUT_PIPE;
      stdout_pair[1] = STDOUT_FILENO;
    }

  g_assert (stdout_pair[1] != -1);

  stdout_handle = g_unix_fd_list_append (fd_list, stdout_pair[1], error);
  if (stdout_handle == -1)
    IDE_GOTO (cleanup_fds);
  else
    maybe_close (&stdout_pair[1]);


  /* Handle STDERR for the process.
   *
   * If silence is requested, we simply redirect to /dev/null. If the
   * application requested to read from the subprocesses stderr, then we need
   * to create a pipe. Otherwose, merge stderr into our own stderr.
   */
  if (-1 != (stderr_pair[1] = ide_unix_fd_map_steal_stderr (self->unix_fd_map)))
    {
      self->flags &= ~G_SUBPROCESS_FLAGS_STDERR_PIPE;
    }
  else if (self->flags & G_SUBPROCESS_FLAGS_STDERR_SILENCE)
    {
      self->flags &= ~G_SUBPROCESS_FLAGS_STDERR_PIPE;
      stderr_pair[1] = open ("/dev/null", O_CLOEXEC | O_RDWR, 0);
      if (stderr_pair[1] == -1)
        IDE_GOTO (cleanup_fds);
    }
  else if (self->flags & G_SUBPROCESS_FLAGS_STDERR_PIPE)
    {
      if (!g_unix_open_pipe (stderr_pair, FD_CLOEXEC, error))
        IDE_GOTO (cleanup_fds);
    }
  else
    {
      self->flags &= ~G_SUBPROCESS_FLAGS_STDERR_PIPE;
      stderr_pair[1] = STDERR_FILENO;
    }

  g_assert (stderr_pair[1] != -1);

  stderr_handle = g_unix_fd_list_append (fd_list, stderr_pair[1], error);
  if (stderr_handle == -1)
    IDE_GOTO (cleanup_fds);
  else
    maybe_close (&stderr_pair[1]);


  /* Build our FDs for the message. */
  g_variant_builder_add (fd_builder, "{uh}", 0, stdin_handle);
  g_variant_builder_add (fd_builder, "{uh}", 1, stdout_handle);
  g_variant_builder_add (fd_builder, "{uh}", 2, stderr_handle);


  /* Now add the rest of our FDs that we might need to map in for which
   * the subprocess launcher tried to map.
   */
  length = ide_unix_fd_map_get_length (self->unix_fd_map);
  for (guint i = 0; i < length; i++)
    {
      int source_fd;
      int dest_fd;

      if (-1 != (source_fd = ide_unix_fd_map_peek (self->unix_fd_map, i, &dest_fd)))
        {
          int dest_handle = g_unix_fd_list_append (fd_list, source_fd, NULL);

          if (dest_handle != -1)
            g_variant_builder_add (fd_builder, "{uh}", dest_fd, dest_handle);
        }
    }

  /* We don't want to allow these FDs to be used again. */
  g_clear_object (&self->unix_fd_map);


  /*
   * Build streams for our application to use.
   */
  maybe_create_output_stream (&self->stdin_pipe, &stdin_pair[1], !!(self->flags & G_SUBPROCESS_FLAGS_STDIN_PIPE));
  maybe_create_input_stream (&self->stdout_pipe, &stdout_pair[0], !!(self->flags & G_SUBPROCESS_FLAGS_STDOUT_PIPE));
  maybe_create_input_stream (&self->stderr_pipe, &stderr_pair[0], !!(self->flags & G_SUBPROCESS_FLAGS_STDERR_PIPE));


  /*
   * Build our environment variables message.
   */
  if (self->env != NULL)
    {
      for (guint i = 0; self->env[i]; i++)
        {
          const char *pair = self->env[i];
          const char *eq = strchr (pair, '=');
          const char *val = eq ? eq + 1 : "";
          g_autofree char *key = eq ? g_strndup (pair, eq - pair) : g_strdup (pair);

          g_variant_builder_add (env_builder, "{ss}", key, val);
        }
    }


  /*
   * Make sure we've closed or stolen all of the FDs that are in play
   * before calling the D-Bus service.
   */
  g_assert_cmpint (-1, ==, stdin_pair[0]);
  g_assert_cmpint (-1, ==, stdin_pair[1]);
  g_assert_cmpint (-1, ==, stdout_pair[0]);
  g_assert_cmpint (-1, ==, stdout_pair[1]);
  g_assert_cmpint (-1, ==, stderr_pair[0]);
  g_assert_cmpint (-1, ==, stderr_pair[1]);


  /*
   * Connect to the HostCommandExited signal so that we can make progress
   * on all tasks waiting on ide_subprocess_wait() and its async variants.
   * We need to do this before spawning the process to avoid the race.
   */
  self->exited_subscription = g_dbus_connection_signal_subscribe (self->connection,
                                                                  NULL,
                                                                  "org.freedesktop.Flatpak.Development",
                                                                  "HostCommandExited",
                                                                  "/org/freedesktop/Flatpak/Development",
                                                                  NULL,
                                                                  G_DBUS_SIGNAL_FLAGS_NONE,
                                                                  host_command_exited_cb,
                                                                  self,
                                                                  NULL);


  /*
   * We wait to connect to closed until here so that we don't lose our
   * connection potentially during setup.
   */
  self->connection_closed_handler =
    g_signal_connect_object (self->connection,
                             "closed",
                             G_CALLBACK (ide_flatpak_subprocess_connection_closed),
                             self,
                             G_CONNECT_SWAPPED);


  /*
   * Now call the HostCommand service to execute the process within the host
   * system. We need to ensure our fd_list is sent across for redirecting
   * various standard streams.
   */
  g_assert_cmpint (g_unix_fd_list_get_length (fd_list), >=, 3);
  params = g_variant_new ("(^ay^aay@a{uh}@a{ss}u)",
                          self->cwd ?: g_get_home_dir (),
                          self->argv,
                          g_variant_builder_end (g_steal_pointer (&fd_builder)),
                          g_variant_builder_end (g_steal_pointer (&env_builder)),
                          flags);
  g_variant_take_ref (params);

#ifdef IDE_ENABLE_TRACE
  {
    g_autofree char *str = g_variant_print (params, TRUE);
    IDE_TRACE_MSG ("Calling HostCommand with %s", str);
  }
#endif

  reply = g_dbus_connection_call_with_unix_fd_list_sync (self->connection,
                                                         "org.freedesktop.Flatpak",
                                                         "/org/freedesktop/Flatpak/Development",
                                                         "org.freedesktop.Flatpak.Development",
                                                         "HostCommand",
                                                         params,
                                                         G_VARIANT_TYPE ("(u)"),
                                                         G_DBUS_CALL_FLAGS_NONE,
                                                         -1,
                                                         fd_list,
                                                         NULL,
                                                         cancellable,
                                                         error);
  if (reply == NULL)
    IDE_GOTO (cleanup_fds);

  g_variant_get (reply, "(u)", &client_pid);

  self->client_pid = (GPid)client_pid;
  self->identifier = g_strdup_printf ("%u", client_pid);

  IDE_TRACE_MSG ("HostCommand() spawned client_pid %u", (guint)client_pid);

  if (cancellable != NULL && !g_cancellable_is_cancelled (cancellable))
    {
      g_signal_connect_object (cancellable,
                               "cancelled",
                               G_CALLBACK (ide_flatpak_subprocess_cancelled),
                               self,
                               G_CONNECT_SWAPPED);
      if (g_cancellable_is_cancelled (cancellable) && !self->client_has_exited)
        ide_flatpak_subprocess_force_exit (IDE_SUBPROCESS (self));
    }

  ret = TRUE;

cleanup_fds:

  /* Close lingering stdin fds */
  maybe_close (&stdin_pair[0]);
  maybe_close (&stdin_pair[1]);

  /* Close lingering stdout fds */
  maybe_close (&stdout_pair[0]);
  maybe_close (&stdout_pair[1]);

  /* Close lingering stderr fds */
  maybe_close (&stderr_pair[0]);
  maybe_close (&stderr_pair[1]);

  IDE_RETURN (ret);
}

static void
initiable_iface_init (GInitableIface *iface)
{
  iface->init = ide_flatpak_subprocess_initable_init;
}

G_DEFINE_FINAL_TYPE_WITH_CODE (IdeFlatpakSubprocess, ide_flatpak_subprocess, G_TYPE_OBJECT,
                               G_IMPLEMENT_INTERFACE (G_TYPE_INITABLE, initiable_iface_init)
                               G_IMPLEMENT_INTERFACE (IDE_TYPE_SUBPROCESS, subprocess_iface_init))

static void
ide_flatpak_subprocess_dispose (GObject *object)
{
  IdeFlatpakSubprocess *self = (IdeFlatpakSubprocess *)object;

  g_assert (IDE_IS_FLATPAK_SUBPROCESS (self));

  if (self->exited_subscription != 0)
    {
      if (self->connection != NULL && !g_dbus_connection_is_closed (self->connection))
        {
          IDE_TRACE_MSG ("Unsubscribing from D-Bus subscription %d", self->exited_subscription);
          g_dbus_connection_signal_unsubscribe (self->connection, self->exited_subscription);
        }

      self->exited_subscription = 0;
    }

  if (self->waiting != NULL)
    g_warning ("improper disposal while async operations are active!");

  G_OBJECT_CLASS (ide_flatpak_subprocess_parent_class)->dispose (object);
}

static void
ide_flatpak_subprocess_finalize (GObject *object)
{
  IdeFlatpakSubprocess *self = (IdeFlatpakSubprocess *)object;

  IDE_ENTRY;

  g_assert (self->waiting == NULL);
  g_assert_cmpint (self->exited_subscription, ==, 0);

  g_clear_pointer (&self->identifier, g_free);
  g_clear_pointer (&self->cwd, g_free);
  g_clear_pointer (&self->argv, g_strfreev);
  g_clear_pointer (&self->env, g_strfreev);
  g_clear_pointer (&self->main_context, g_main_context_unref);

  g_clear_object (&self->stdin_pipe);
  g_clear_object (&self->stdout_pipe);
  g_clear_object (&self->stderr_pipe);
  g_clear_object (&self->connection);
  g_clear_object (&self->unix_fd_map);

  g_mutex_clear (&self->waiter_mutex);
  g_cond_clear (&self->waiter_cond);

  G_OBJECT_CLASS (ide_flatpak_subprocess_parent_class)->finalize (object);

  IDE_EXIT;
}

static void
ide_flatpak_subprocess_get_property (GObject    *object,
                                     guint       prop_id,
                                     GValue     *value,
                                     GParamSpec *pspec)
{
  IdeFlatpakSubprocess *self = IDE_FLATPAK_SUBPROCESS (object);

  switch (prop_id)
    {
    case PROP_CLEAR_ENV:
      g_value_set_boolean (value, self->clear_env);
      break;

    case PROP_CWD:
      g_value_set_string (value, self->cwd);
      break;

    case PROP_ARGV:
      g_value_set_boxed (value, self->argv);
      break;

    case PROP_ENV:
      g_value_set_boxed (value, self->env);
      break;

    case PROP_FLAGS:
      g_value_set_flags (value, self->flags);
      break;

    case PROP_UNIX_FD_MAP:
      g_value_set_object (value, self->unix_fd_map);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
ide_flatpak_subprocess_set_property (GObject      *object,
                                     guint         prop_id,
                                     const GValue *value,
                                     GParamSpec   *pspec)
{
  IdeFlatpakSubprocess *self = IDE_FLATPAK_SUBPROCESS (object);

  switch (prop_id)
    {
    case PROP_ARGV:
      self->argv = g_value_dup_boxed (value);
      break;

    case PROP_CLEAR_ENV:
      self->clear_env = g_value_get_boolean (value);
      break;

    case PROP_CWD:
      self->cwd = g_value_dup_string (value);
      break;

    case PROP_ENV:
      self->env = g_value_dup_boxed (value);
      break;

    case PROP_FLAGS:
      self->flags = g_value_get_flags (value);
      break;

    case PROP_UNIX_FD_MAP:
      self->unix_fd_map = g_value_dup_object (value);
      if (self->unix_fd_map == NULL)
        self->unix_fd_map = ide_unix_fd_map_new ();
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
ide_flatpak_subprocess_class_init (IdeFlatpakSubprocessClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->dispose = ide_flatpak_subprocess_dispose;
  object_class->finalize = ide_flatpak_subprocess_finalize;
  object_class->get_property = ide_flatpak_subprocess_get_property;
  object_class->set_property = ide_flatpak_subprocess_set_property;

  properties [PROP_CLEAR_ENV] =
    g_param_spec_boolean ("clear-env", NULL, NULL,
                          FALSE,
                          (G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS));

  properties [PROP_CWD] =
    g_param_spec_string ("cwd",
                         "Current Working Directory",
                         "The working directory for spawning the process",
                         NULL,
                         (G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS));

  properties [PROP_ARGV] =
    g_param_spec_boxed ("argv",
                        "Argv",
                        "The arguments for the process, including argv0",
                        G_TYPE_STRV,
                        (G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS));

  properties [PROP_ENV] =
    g_param_spec_boxed ("env",
                        "Environment",
                        "The environment variables for the process",
                        G_TYPE_STRV,
                        (G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS));

  properties [PROP_FLAGS] =
    g_param_spec_flags ("flags",
                        "Flags",
                        "The subprocess flags to use when spawning",
                        G_TYPE_SUBPROCESS_FLAGS,
                        G_SUBPROCESS_FLAGS_NONE,
                        (G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS));

  properties [PROP_UNIX_FD_MAP] =
    g_param_spec_object ("unix-fd-map", NULL, NULL,
                         IDE_TYPE_UNIX_FD_MAP,
                         (G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, N_PROPS, properties);
}

static void
ide_flatpak_subprocess_init (IdeFlatpakSubprocess *self)
{
  IDE_ENTRY;

  g_mutex_init (&self->waiter_mutex);
  g_cond_init (&self->waiter_cond);

  IDE_EXIT;
}

IdeSubprocess *
_ide_flatpak_subprocess_new (const char          *cwd,
                             const char * const  *argv,
                             const char * const  *env,
                             GSubprocessFlags     flags,
                             gboolean             clear_env,
                             IdeUnixFDMap        *unix_fd_map,
                             GCancellable        *cancellable,
                             GError             **error)
{
  g_autoptr(IdeFlatpakSubprocess) ret = NULL;

  g_return_val_if_fail (argv != NULL, NULL);
  g_return_val_if_fail (argv[0] != NULL, NULL);
  g_return_val_if_fail (!unix_fd_map || IDE_IS_UNIX_FD_MAP (unix_fd_map), NULL);
  g_return_val_if_fail (!cancellable || G_IS_CANCELLABLE (cancellable), NULL);

  ret = g_object_new (IDE_TYPE_FLATPAK_SUBPROCESS,
                      "cwd", cwd,
                      "argv", argv,
                      "clear-env", clear_env,
                      "env", env,
                      "flags", flags,
                      "unix-fd-map", unix_fd_map,
                      NULL);

  if (!g_initable_init (G_INITABLE (ret), cancellable, error))
    return NULL;

  return IDE_SUBPROCESS (g_steal_pointer (&ret));
}
