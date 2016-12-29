/* GIO - GLib Input, Output and Streaming Library
 *
 * Copyright © 2012, 2013, 2016 Red Hat, Inc.
 * Copyright © 2012, 2013 Canonical Limited
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
 */

#define G_LOG_DOMAIN "ide-breakout-subprocess"

#include <egg-counter.h>
#include <errno.h>
#include <fcntl.h>
#include <gio/gunixinputstream.h>
#include <gio/gunixoutputstream.h>
#include <gio/gunixfdlist.h>
#include <glib-unix.h>
#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#include "ide-debug.h"
#include "ide-macros.h"

#include "application/ide-application.h"
#include "subprocess/ide-breakout-subprocess.h"
#include "subprocess/ide-breakout-subprocess-private.h"
#include "util/ide-glib.h"

#ifndef FLATPAK_HOST_COMMAND_FLAGS_CLEAR_ENV
# define FLATPAK_HOST_COMMAND_FLAGS_CLEAR_ENV (1 << 0)
#endif

/*
 * One very non-ideal thing about this implementation is that we use a new
 * GDBusConnection for every instance. This is due to some difficulty in
 * dealing with our connection being closed out from underneath us. If we
 * can determine what was/is causing that, we should be able to move back
 * to a shared connection (although we might want a dedicated connection
 * for all subprocesses so that we can have exit-on-close => false).
 */

EGG_DEFINE_COUNTER (instances, "Subprocess", "HostCommand Instances", "Number of IdeBreakoutSubprocess instances")

struct _IdeBreakoutSubprocess
{
  GObject parent_instance;

  GDBusConnection *connection;
  gulong connection_closed_handler;

  GPid client_pid;
  gint status;

  GSubprocessFlags flags;

  /* No reference */
  GThread *spawn_thread;

  gchar **argv;
  gchar **env;
  gchar *cwd;

  gchar *identifier;

  gint stdin_fd;
  gint stdout_fd;
  gint stderr_fd;

  GOutputStream *stdin_pipe;
  GInputStream *stdout_pipe;
  GInputStream *stderr_pipe;

  IdeBreakoutFdMapping *fd_mapping;
  guint fd_mapping_len;

  GMainContext *main_context;

  guint sigint_id;
  guint sigterm_id;
  guint exited_subscription;

  /* GList of GTasks for wait_async() */
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
 * their cancellable) we immediately dispatch the GTask with the error
 * result and fire our cancellable to cleanup any pending operations.
 * In the case that the error is that the user's cancellable was fired,
 * it's vaguely wasteful to report an error because GTask will handle
 * this automatically, so we just return FALSE.
 *
 * We let each pending sub-operation take a ref on the GTask of the
 * communicate operation.  We have to be careful that we don't report
 * the task completion more than once, though, so we keep a flag for
 * that.
 */
typedef struct
{
  const gchar *stdin_data;
  gsize stdin_length;
  gsize stdin_offset;

  gboolean add_nul;

  GInputStream *stdin_buf;
  GMemoryOutputStream *stdout_buf;
  GMemoryOutputStream *stderr_buf;

  GCancellable *cancellable;
  GSource      *cancellable_source;

  guint         outstanding_ops;
  gboolean      reported_error;
} CommunicateState;

enum {
  PROP_0,
  PROP_ARGV,
  PROP_CWD,
  PROP_ENV,
  PROP_FLAGS,
  N_PROPS
};

static void              ide_breakout_subprocess_sync_complete        (IdeBreakoutSubprocess  *self,
                                                                       GAsyncResult          **result);
static void              ide_breakout_subprocess_sync_done            (GObject                *object,
                                                                       GAsyncResult           *result,
                                                                       gpointer                user_data);
static CommunicateState *ide_breakout_subprocess_communicate_internal (IdeBreakoutSubprocess  *subprocess,
                                                                       gboolean                add_nul,
                                                                       GBytes                 *stdin_buf,
                                                                       GCancellable           *cancellable,
                                                                       GAsyncReadyCallback     callback,
                                                                       gpointer                user_data);

static GParamSpec *properties [N_PROPS];

static const gchar *
ide_breakout_subprocess_get_identifier (IdeSubprocess *subprocess)
{
  IdeBreakoutSubprocess *self = (IdeBreakoutSubprocess *)subprocess;

  g_assert (IDE_IS_BREAKOUT_SUBPROCESS (self));

  return self->identifier;
}

static GInputStream *
ide_breakout_subprocess_get_stdout_pipe (IdeSubprocess *subprocess)
{
  IdeBreakoutSubprocess *self = (IdeBreakoutSubprocess *)subprocess;

  g_assert (IDE_IS_BREAKOUT_SUBPROCESS (self));

  return self->stdout_pipe;
}

static GInputStream *
ide_breakout_subprocess_get_stderr_pipe (IdeSubprocess *subprocess)
{
  IdeBreakoutSubprocess *self = (IdeBreakoutSubprocess *)subprocess;

  g_assert (IDE_IS_BREAKOUT_SUBPROCESS (self));

  return self->stderr_pipe;
}

static GOutputStream *
ide_breakout_subprocess_get_stdin_pipe (IdeSubprocess *subprocess)
{
  IdeBreakoutSubprocess *self = (IdeBreakoutSubprocess *)subprocess;

  g_assert (IDE_IS_BREAKOUT_SUBPROCESS (self));

  return self->stdin_pipe;
}

static void
ide_breakout_subprocess_wait_cb (GObject      *object,
                                 GAsyncResult *result,
                                 gpointer      user_data)
{
  IdeBreakoutSubprocess *self = (IdeBreakoutSubprocess *)object;
  gboolean *completed = user_data;

  g_assert (IDE_IS_BREAKOUT_SUBPROCESS (self));
  g_assert (completed != NULL);

  ide_subprocess_wait_finish (IDE_SUBPROCESS (self), result, NULL);

  *completed = TRUE;

  if (self->main_context != NULL)
    g_main_context_wakeup (self->main_context);
}

static gboolean
ide_breakout_subprocess_wait (IdeSubprocess  *subprocess,
                              GCancellable   *cancellable,
                              GError        **error)
{
  IdeBreakoutSubprocess *self = (IdeBreakoutSubprocess *)subprocess;

  g_assert (IDE_IS_BREAKOUT_SUBPROCESS (self));

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
                                 ide_breakout_subprocess_wait_cb,
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
ide_breakout_subprocess_wait_async (IdeSubprocess       *subprocess,
                                    GCancellable        *cancellable,
                                    GAsyncReadyCallback  callback,
                                    gpointer             user_data)
{
  IdeBreakoutSubprocess *self = (IdeBreakoutSubprocess *)subprocess;
  g_autoptr(GTask) task = NULL;
  g_autoptr(GMutexLocker) locker = NULL;

  g_assert (IDE_IS_BREAKOUT_SUBPROCESS (self));
  g_assert (!cancellable || G_IS_CANCELLABLE (cancellable));

  task = g_task_new (self, cancellable, callback, user_data);
  g_task_set_source_tag (task, ide_breakout_subprocess_wait_async);

  locker = g_mutex_locker_new (&self->waiter_mutex);

  if (self->client_has_exited)
    {
      ide_g_task_return_boolean_from_main (task, TRUE);
      return;
    }

  self->waiting = g_list_append (self->waiting, g_steal_pointer (&task));
}

static gboolean
ide_breakout_subprocess_wait_finish (IdeSubprocess  *subprocess,
                                     GAsyncResult   *result,
                                     GError        **error)
{
  g_assert (IDE_IS_BREAKOUT_SUBPROCESS (subprocess));
  g_assert (G_IS_TASK (result));

  return g_task_propagate_boolean (G_TASK (result), error);
}

void
ide_subprocess_communicate_utf8_async (IdeSubprocess       *subprocess,
                                       const char          *stdin_buf,
                                       GCancellable        *cancellable,
                                       GAsyncReadyCallback  callback,
                                       gpointer             user_data)
{
  IdeBreakoutSubprocess *self = (IdeBreakoutSubprocess *)subprocess;
  g_autoptr(GBytes) stdin_bytes = NULL;
  size_t stdin_buf_len = 0;

  g_return_if_fail (IDE_IS_BREAKOUT_SUBPROCESS (subprocess));
  g_return_if_fail (stdin_buf == NULL || (self->flags & G_SUBPROCESS_FLAGS_STDIN_PIPE));
  g_return_if_fail (cancellable == NULL || G_IS_CANCELLABLE (cancellable));

  if (stdin_buf != NULL)
    stdin_buf_len = strlen (stdin_buf);
  stdin_bytes = g_bytes_new (stdin_buf, stdin_buf_len);

  ide_breakout_subprocess_communicate_internal (self, TRUE, stdin_bytes, cancellable, callback, user_data);
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

gboolean
ide_subprocess_communicate_utf8_finish (IdeSubprocess  *subprocess,
                                        GAsyncResult   *result,
                                        char          **stdout_buf,
                                        char          **stderr_buf,
                                        GError        **error)
{
  gboolean ret = FALSE;
  CommunicateState *state;

  IDE_ENTRY;

  g_return_val_if_fail (IDE_IS_BREAKOUT_SUBPROCESS (subprocess), FALSE);
  g_return_val_if_fail (g_task_is_valid (result, subprocess), FALSE);
  g_return_val_if_fail (error == NULL || *error == NULL, FALSE);

  g_object_ref (result);

  state = g_task_get_task_data ((GTask*)result);
  if (!g_task_propagate_boolean ((GTask*)result, error))
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
ide_breakout_subprocess_communicate_utf8 (IdeSubprocess  *subprocess,
                                          const char     *stdin_buf,
                                          GCancellable   *cancellable,
                                          char          **stdout_buf,
                                          char          **stderr_buf,
                                          GError        **error)
{
  IdeBreakoutSubprocess *self = (IdeBreakoutSubprocess *)subprocess;
  g_autoptr(GAsyncResult) result = NULL;
  g_autoptr(GBytes) stdin_bytes = NULL;
  size_t stdin_buf_len = 0;
  gboolean success;

  IDE_ENTRY;

  g_return_val_if_fail (IDE_IS_BREAKOUT_SUBPROCESS (subprocess), FALSE);
  g_return_val_if_fail (stdin_buf == NULL || (self->flags & G_SUBPROCESS_FLAGS_STDIN_PIPE), FALSE);
  g_return_val_if_fail (cancellable == NULL || G_IS_CANCELLABLE (cancellable), FALSE);
  g_return_val_if_fail (error == NULL || *error == NULL, FALSE);

  if (stdin_buf != NULL)
    stdin_buf_len = strlen (stdin_buf);
  stdin_bytes = g_bytes_new (stdin_buf, stdin_buf_len);

  ide_breakout_subprocess_communicate_internal (self,
                                                TRUE,
                                                stdin_bytes,
                                                cancellable,
                                                ide_breakout_subprocess_sync_done,
                                                &result);
  ide_breakout_subprocess_sync_complete (self, &result);
  success = ide_subprocess_communicate_utf8_finish (subprocess, result, stdout_buf, stderr_buf, error);

  IDE_RETURN (success);
}

static gboolean
ide_breakout_subprocess_get_successful (IdeSubprocess *subprocess)
{
  IdeBreakoutSubprocess *self = (IdeBreakoutSubprocess *)subprocess;

  g_assert (IDE_IS_BREAKOUT_SUBPROCESS (self));

  return WIFEXITED (self->status) && WEXITSTATUS (self->status) == 0;
}

static gboolean
ide_breakout_subprocess_get_if_exited (IdeSubprocess *subprocess)
{
  IdeBreakoutSubprocess *self = (IdeBreakoutSubprocess *)subprocess;

  g_assert (IDE_IS_BREAKOUT_SUBPROCESS (self));

  return WIFEXITED (self->status);
}

static gint
ide_breakout_subprocess_get_exit_status (IdeSubprocess *subprocess)
{
  IdeBreakoutSubprocess *self = (IdeBreakoutSubprocess *)subprocess;

  g_assert (IDE_IS_BREAKOUT_SUBPROCESS (self));
  g_assert (self->client_has_exited);

  if (!WIFEXITED (self->status))
    return 1;

  return WEXITSTATUS (self->status);
}

static gboolean
ide_breakout_subprocess_get_if_signaled (IdeSubprocess *subprocess)
{
  IdeBreakoutSubprocess *self = (IdeBreakoutSubprocess *)subprocess;

  g_assert (IDE_IS_BREAKOUT_SUBPROCESS (self));
  g_assert (self->client_has_exited == TRUE);

  return WIFSIGNALED (self->status);
}

static gint
ide_breakout_subprocess_get_term_sig (IdeSubprocess *subprocess)
{
  IdeBreakoutSubprocess *self = (IdeBreakoutSubprocess *)subprocess;

  g_assert (IDE_IS_BREAKOUT_SUBPROCESS (self));
  g_assert (self->client_has_exited == TRUE);

  return WTERMSIG (self->status);
}

static gint
ide_breakout_subprocess_get_status (IdeSubprocess *subprocess)
{
  IdeBreakoutSubprocess *self = (IdeBreakoutSubprocess *)subprocess;

  g_assert (IDE_IS_BREAKOUT_SUBPROCESS (self));
  g_assert (self->client_has_exited == TRUE);

  return self->status;
}

static void
ide_breakout_subprocess_send_signal (IdeSubprocess *subprocess,
                                     gint           signal_num)
{
  IdeBreakoutSubprocess *self = (IdeBreakoutSubprocess *)subprocess;

  IDE_ENTRY;

  g_assert (IDE_IS_BREAKOUT_SUBPROCESS (self));

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
ide_breakout_subprocess_force_exit (IdeSubprocess *subprocess)
{
  IdeBreakoutSubprocess *self = (IdeBreakoutSubprocess *)subprocess;

  g_assert (IDE_IS_BREAKOUT_SUBPROCESS (self));

  ide_breakout_subprocess_send_signal (subprocess, SIGKILL);
}

static void
ide_breakout_subprocess_sync_complete (IdeBreakoutSubprocess  *self,
                                       GAsyncResult          **result)
{
  g_autoptr(GMainContext) free_me = NULL;
  GMainContext *main_context = NULL;

  IDE_ENTRY;

  g_assert (IDE_IS_BREAKOUT_SUBPROCESS (self));
  g_assert (result != NULL);
  g_assert (*result == NULL || G_IS_ASYNC_RESULT (*result));

  if (NULL == (main_context = g_main_context_get_thread_default ()))
    {
      if (IDE_IS_MAIN_THREAD ())
        main_context = g_main_context_default ();
      else
        main_context = free_me = g_main_context_new ();
    }

  g_mutex_lock (&self->waiter_mutex);
  self->main_context = g_main_context_ref (main_context);
  g_mutex_unlock (&self->waiter_mutex);

  while (*result == NULL)
    g_main_context_iteration (main_context, TRUE);

  IDE_EXIT;
}

static void
ide_breakout_subprocess_sync_done (GObject      *object,
                                   GAsyncResult *result,
                                   gpointer      user_data)
{
  IdeBreakoutSubprocess *self = (IdeBreakoutSubprocess *)object;
  GAsyncResult **ret = user_data;

  IDE_ENTRY;

  g_assert (IDE_IS_BREAKOUT_SUBPROCESS (self));
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

  if (state->cancellable_source)
    {
      if (!g_source_is_destroyed (state->cancellable_source))
        g_source_destroy (state->cancellable_source);
      g_source_unref (state->cancellable_source);
    }

  g_slice_free (CommunicateState, state);
}

static gboolean
ide_subprocess_communicate_cancelled (gpointer user_data)
{
  CommunicateState *state = user_data;

  IDE_ENTRY;

  g_assert (state != NULL);
  g_assert (G_IS_CANCELLABLE (state->cancellable));

  g_cancellable_cancel (state->cancellable);

  IDE_RETURN (G_SOURCE_REMOVE);
}

static void
ide_subprocess_communicate_made_progress (GObject      *source_object,
                                          GAsyncResult *result,
                                          gpointer      user_data)
{
  CommunicateState *state;
  IdeBreakoutSubprocess *subprocess;
  GError *error = NULL;
  gpointer source;
  GTask *task;

  IDE_ENTRY;

  g_assert (source_object != NULL);

  task = user_data;
  subprocess = g_task_get_source_object (task);
  state = g_task_get_task_data (task);
  source = source_object;

  state->outstanding_ops--;

  if (source == subprocess->stdin_pipe ||
      source == state->stdout_buf ||
      source == state->stderr_buf)
    {
      if (g_output_stream_splice_finish ((GOutputStream*) source, result, &error) == -1)
        goto out;

      if (source == state->stdout_buf ||
          source == state->stderr_buf)
        {
          /* This is a memory stream, so it can't be cancelled or return
           * an error really.
           */
          if (state->add_nul)
            {
              gsize bytes_written;
              if (!g_output_stream_write_all (source, "\0", 1, &bytes_written, NULL, &error))
                goto out;
            }
          if (!g_output_stream_close (source, NULL, &error))
            goto out;
        }
    }
  else if (source == subprocess)
    {
      (void) ide_subprocess_wait_finish (IDE_SUBPROCESS (subprocess), result, &error);
    }
  else
    g_assert_not_reached ();

 out:
  if (error)
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
          ide_g_task_return_error_from_main (task, error);
        }
      else
        g_error_free (error);
    }
  else if (state->outstanding_ops == 0)
    {
      ide_g_task_return_boolean_from_main (task, TRUE);
    }

  /* And drop the original ref */
  g_object_unref (task);

  IDE_EXIT;
}

static CommunicateState *
ide_breakout_subprocess_communicate_internal (IdeBreakoutSubprocess *subprocess,
                                              gboolean               add_nul,
                                              GBytes                *stdin_buf,
                                              GCancellable          *cancellable,
                                              GAsyncReadyCallback    callback,
                                              gpointer               user_data)
{
  CommunicateState *state;
  g_autoptr(GTask) task = NULL;

  IDE_ENTRY;

  g_assert (IDE_IS_BREAKOUT_SUBPROCESS (subprocess));
  g_assert (!cancellable || G_IS_CANCELLABLE (cancellable));

  task = g_task_new (subprocess, cancellable, callback, user_data);
  g_task_set_source_tag (task, ide_breakout_subprocess_communicate_internal);

  state = g_slice_new0 (CommunicateState);
  g_task_set_task_data (task, state, ide_subprocess_communicate_state_free);

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

  if (subprocess->stdin_pipe)
    {
      g_assert (stdin_buf != NULL);
      state->stdin_buf = g_memory_input_stream_new_from_bytes (stdin_buf);
      g_output_stream_splice_async (subprocess->stdin_pipe, (GInputStream*)state->stdin_buf,
                                    G_OUTPUT_STREAM_SPLICE_CLOSE_SOURCE | G_OUTPUT_STREAM_SPLICE_CLOSE_TARGET,
                                    G_PRIORITY_DEFAULT, state->cancellable,
                                    ide_subprocess_communicate_made_progress, g_object_ref (task));
      state->outstanding_ops++;
    }

  if (subprocess->stdout_pipe)
    {
      state->stdout_buf = (GMemoryOutputStream*)g_memory_output_stream_new_resizable ();
      g_output_stream_splice_async ((GOutputStream*)state->stdout_buf, subprocess->stdout_pipe,
                                    G_OUTPUT_STREAM_SPLICE_CLOSE_SOURCE,
                                    G_PRIORITY_DEFAULT, state->cancellable,
                                    ide_subprocess_communicate_made_progress, g_object_ref (task));
      state->outstanding_ops++;
    }

  if (subprocess->stderr_pipe)
    {
      state->stderr_buf = (GMemoryOutputStream*)g_memory_output_stream_new_resizable ();
      g_output_stream_splice_async ((GOutputStream*)state->stderr_buf, subprocess->stderr_pipe,
                                    G_OUTPUT_STREAM_SPLICE_CLOSE_SOURCE,
                                    G_PRIORITY_DEFAULT, state->cancellable,
                                    ide_subprocess_communicate_made_progress, g_object_ref (task));
      state->outstanding_ops++;
    }

  ide_subprocess_wait_async (IDE_SUBPROCESS (subprocess), state->cancellable,
                             ide_subprocess_communicate_made_progress, g_object_ref (task));

  IDE_RETURN (state);
}

static void
ide_breakout_subprocess_communicate_async (IdeSubprocess       *subprocess,
                                           GBytes              *stdin_buf,
                                           GCancellable        *cancellable,
                                           GAsyncReadyCallback  callback,
                                           gpointer             user_data)
{
  IdeBreakoutSubprocess *self = (IdeBreakoutSubprocess *)subprocess;

  g_assert (IDE_IS_BREAKOUT_SUBPROCESS (self));
  g_assert (!cancellable || G_IS_CANCELLABLE (cancellable));

  ide_breakout_subprocess_communicate_internal (self, FALSE, stdin_buf, cancellable, callback, user_data);
}

static gboolean
ide_breakout_subprocess_communicate_finish (IdeSubprocess  *subprocess,
                                            GAsyncResult   *result,
                                            GBytes        **stdout_buf,
                                            GBytes        **stderr_buf,
                                            GError        **error)
{
  IdeBreakoutSubprocess *self = (IdeBreakoutSubprocess *)subprocess;
  CommunicateState *state;
  GTask *task = (GTask *)result;
  gboolean success;

  IDE_ENTRY;

  g_assert (IDE_IS_BREAKOUT_SUBPROCESS (self));
  g_assert (G_IS_TASK (task));

  g_object_ref (task);

  state = g_task_get_task_data (task);

  g_assert (state != NULL);

  success = g_task_propagate_boolean (task, error);

  if (success)
    {
      if (stdout_buf)
        *stdout_buf = g_memory_output_stream_steal_as_bytes (state->stdout_buf);
      if (stderr_buf)
        *stderr_buf = g_memory_output_stream_steal_as_bytes (state->stderr_buf);
    }

  g_object_unref (task);

  IDE_RETURN (success);
}

static gboolean
ide_breakout_subprocess_communicate (IdeSubprocess  *subprocess,
                                     GBytes         *stdin_buf,
                                     GCancellable   *cancellable,
                                     GBytes        **stdout_buf,
                                     GBytes        **stderr_buf,
                                     GError        **error)
{
  IdeBreakoutSubprocess *self = (IdeBreakoutSubprocess *)subprocess;
  g_autoptr(GAsyncResult) result = NULL;
  gboolean ret;

  IDE_ENTRY;

  g_assert (IDE_IS_BREAKOUT_SUBPROCESS (self));
  g_assert (!cancellable || G_IS_CANCELLABLE (cancellable));

  ide_breakout_subprocess_communicate_internal (self,
                                                FALSE,
                                                stdin_buf,
                                                cancellable,
                                                ide_breakout_subprocess_sync_done,
                                                &result);
  ide_breakout_subprocess_sync_complete (self, &result);

  ret = ide_breakout_subprocess_communicate_finish (subprocess, result, stdout_buf, stderr_buf, error);

  IDE_RETURN (ret);
}

static void
subprocess_iface_init (IdeSubprocessInterface *iface)
{
  iface->get_identifier = ide_breakout_subprocess_get_identifier;
  iface->get_stdout_pipe = ide_breakout_subprocess_get_stdout_pipe;
  iface->get_stderr_pipe = ide_breakout_subprocess_get_stderr_pipe;
  iface->get_stdin_pipe = ide_breakout_subprocess_get_stdin_pipe;
  iface->wait = ide_breakout_subprocess_wait;
  iface->wait_async = ide_breakout_subprocess_wait_async;
  iface->wait_finish = ide_breakout_subprocess_wait_finish;
  iface->get_successful = ide_breakout_subprocess_get_successful;
  iface->get_if_exited = ide_breakout_subprocess_get_if_exited;
  iface->get_exit_status = ide_breakout_subprocess_get_exit_status;
  iface->get_if_signaled = ide_breakout_subprocess_get_if_signaled;
  iface->get_term_sig = ide_breakout_subprocess_get_term_sig;
  iface->get_status = ide_breakout_subprocess_get_status;
  iface->send_signal = ide_breakout_subprocess_send_signal;
  iface->force_exit = ide_breakout_subprocess_force_exit;
  iface->communicate = ide_breakout_subprocess_communicate;
  iface->communicate_utf8 = ide_breakout_subprocess_communicate_utf8;
  iface->communicate_async = ide_breakout_subprocess_communicate_async;
  iface->communicate_finish = ide_breakout_subprocess_communicate_finish;
}

static gboolean
sigterm_handler (gpointer user_data)
{
  IdeBreakoutSubprocess *self = user_data;
  g_autoptr(GDBusConnection) bus = NULL;

  g_assert (IDE_IS_BREAKOUT_SUBPROCESS (self));

  g_dbus_connection_call_sync (self->connection,
                               "org.freedesktop.Flatpak",
                               "/org/freedesktop/Flatpak/Development",
                               "org.freedesktop.Flatpak.Development",
                               "HostCommandSignal",
                               g_variant_new ("(uub)", self->client_pid, SIGTERM, TRUE),
                               NULL,
                               G_DBUS_CALL_FLAGS_NONE, -1,
                               NULL, NULL);

  kill (getpid (), SIGTERM);

  return G_SOURCE_CONTINUE;
}

static gboolean
sigint_handler (gpointer user_data)
{
  IdeBreakoutSubprocess *self = user_data;
  g_autoptr(GDBusConnection) bus = NULL;

  g_assert (IDE_IS_BREAKOUT_SUBPROCESS (self));

  g_dbus_connection_call_sync (self->connection,
                               "org.freedesktop.Flatpak",
                               "/org/freedesktop/Flatpak/Development",
                               "org.freedesktop.Flatpak.Development",
                               "HostCommandSignal",
                               g_variant_new ("(uub)", self->client_pid, SIGINT, TRUE),
                               NULL,
                               G_DBUS_CALL_FLAGS_NONE, -1,
                               NULL, NULL);

  kill (getpid (), SIGINT);

  return G_SOURCE_CONTINUE;
}

static void
maybe_create_input_stream (GInputStream **ret,
                           gint          *fdptr,
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
                            gint           *fdptr,
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

static inline void
set_error_from_errno (GError **error)
{
  g_set_error (error,
               G_IO_ERROR,
               g_io_error_from_errno (errno),
               "%s",
               g_strerror (errno));
}

static void
ide_breakout_subprocess_complete_command_locked (IdeBreakoutSubprocess *self,
                                                 gint                   exit_status)
{
  GList *waiting;

  IDE_ENTRY;

  g_assert (IDE_IS_BREAKOUT_SUBPROCESS (self));
  g_assert (G_IS_DBUS_CONNECTION (self->connection));

  self->client_has_exited = TRUE;
  self->status = exit_status;

  /*
   * Clear process identifiers to prevent accidental use by API consumers
   * after the process has exited.
   */
  self->client_pid = 0;
  g_clear_pointer (&self->identifier, g_free);

  /* Remove our sources used for signal propagation */
  ide_clear_source (&self->sigint_id);
  ide_clear_source (&self->sigterm_id);

  /* Complete async workers */
  waiting = self->waiting;
  self->waiting = NULL;

  for (GList *iter = waiting; iter != NULL; iter = iter->next)
    {
      g_autoptr(GTask) task = iter->data;

      ide_g_task_return_boolean_from_main (task, TRUE);
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
                        const gchar     *sender_name,
                        const gchar     *object_path,
                        const gchar     *interface_name,
                        const gchar     *signal_name,
                        GVariant        *parameters,
                        gpointer         user_data)
{
  g_autoptr(IdeBreakoutSubprocess) finalize_protect = NULL;
  IdeBreakoutSubprocess *self = user_data;
  g_autoptr(GMutexLocker) locker = NULL;
  guint32 client_pid = 0;
  guint32 exit_status = 0;

  IDE_ENTRY;

  g_assert (G_IS_DBUS_CONNECTION (connection));
  g_assert (IDE_IS_BREAKOUT_SUBPROCESS (self));

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

  /* We can release our dbus signal handler now */
  if (self->exited_subscription != 0)
    {
      IDE_TRACE_MSG ("Unsubscribing from DBus subscription %d", self->exited_subscription);
      g_dbus_connection_signal_unsubscribe (self->connection, self->exited_subscription);
      self->exited_subscription = 0;
    }

  ide_breakout_subprocess_complete_command_locked (self, exit_status);

  IDE_EXIT;
}

static void
ide_breakout_subprocess_cancelled (IdeBreakoutSubprocess *self,
                                   GCancellable          *cancellable)
{
  IDE_ENTRY;

  g_assert (G_IS_CANCELLABLE (cancellable));
  g_assert (IDE_IS_BREAKOUT_SUBPROCESS (self));

  ide_subprocess_force_exit (IDE_SUBPROCESS (self));

  IDE_EXIT;
}

static inline void
maybe_close (gint *fd)
{
  g_assert (fd != NULL);
  g_assert (*fd >= -1);

  if (*fd > 2)
    close (*fd);

  *fd = -1;
}

static void
ide_breakout_subprocess_connection_closed (IdeBreakoutSubprocess *self,
                                           gboolean               remote_peer_vanished,
                                           const GError          *error,
                                           GDBusConnection       *connection)
{
  g_autoptr(GMutexLocker) locker = NULL;

  IDE_ENTRY;

  g_assert (IDE_IS_BREAKOUT_SUBPROCESS (self));
  g_assert (G_IS_DBUS_CONNECTION (connection));

  locker = g_mutex_locker_new (&self->waiter_mutex);

  IDE_TRACE_MSG ("Synthesizing failure for client pid %u", (guint)self->client_pid);

  self->exited_subscription = 0;
  ide_breakout_subprocess_complete_command_locked (self, -1);

  IDE_EXIT;
}

static gboolean
ide_breakout_subprocess_initable_init (GInitable     *initable,
                                       GCancellable  *cancellable,
                                       GError       **error)
{
  IdeBreakoutSubprocess *self = (IdeBreakoutSubprocess *)initable;
  g_autoptr(GVariantBuilder) fd_builder = g_variant_builder_new (G_VARIANT_TYPE ("a{uh}"));
  g_autoptr(GVariantBuilder) env_builder = g_variant_builder_new (G_VARIANT_TYPE ("a{ss}"));
  g_autoptr(GUnixFDList) fd_list = g_unix_fd_list_new ();
  g_autoptr(GVariant) reply = NULL;
  g_autoptr(GVariant) params = NULL;
  guint32 client_pid = 0;
  gint stdout_pair[2] = { -1, -1 };
  gint stderr_pair[2] = { -1, -1 };
  gint stdin_pair[2] = { -1, -1 };
  gint stdin_handle = -1;
  gint stdout_handle = -1;
  gint stderr_handle = -1;
  gboolean ret = FALSE;

  IDE_ENTRY;

  g_assert (IDE_IS_BREAKOUT_SUBPROCESS (self));
  g_assert (!cancellable || G_IS_CANCELLABLE (cancellable));

  /*
   * FIXME:
   *
   * Because we are seeing a rather difficult to track down bug where we lose
   * the connection upon submission of the HostCommand() after a timeout period
   * (the dbus-daemon is closing our connection) we are using a private
   * GDBusConnection for the command.
   *
   * This means we need to ensure we close the connection as soon as we can
   * so that we don't hold things open for too long. Additionally, if we do
   * get disconnected from the daemon, we don't want to crash but instead will
   * synthesize the completion of the command. However, this should be much
   * more unlikely since we haven't seen this failure case.
   *
   * One thing we could look into for recovery is to send a SIGKILL to a new
   * connection (of our client pid) during recovery to ensure that it dies and
   * force our operation to fail. Callers could handle this during wait_async()
   * wait_finish() pairs. Again, not ideal.
   */
  self->connection =
    g_dbus_connection_new_for_address_sync (g_getenv ("DBUS_SESSION_BUS_ADDRESS"),
                                            G_DBUS_CONNECTION_FLAGS_AUTHENTICATION_CLIENT | G_DBUS_CONNECTION_FLAGS_MESSAGE_BUS_CONNECTION,
                                            NULL,
                                            cancellable,
                                            error);

  if (self->connection == NULL)
    IDE_RETURN (FALSE);

  g_dbus_connection_set_exit_on_close (self->connection, FALSE);


  /*
   * Handle STDIN for the process.
   *
   * Make sure we handle inherit STDIN, a new pipe (so that the application can
   * get the stdin stream), or simply redirect to /dev/null.
   */
  if (self->stdin_fd != -1)
    {
      self->flags &= ~G_SUBPROCESS_FLAGS_STDIN_PIPE;
      stdin_pair[0] = self->stdin_fd;
      self->stdin_fd = -1;
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


  /*
   * Setup STDOUT for the process.
   *
   * Make sure we redirect STDOUT to our stdout, unless a pipe was requested
   * for the application to read. However, if silence was requested, redirect
   * to /dev/null.
   */
  if (self->stdout_fd != -1)
    {
      self->flags &= ~G_SUBPROCESS_FLAGS_STDOUT_PIPE;
      stdout_pair[1] = self->stdout_fd;
      self->stdout_fd = -1;
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


  /*
   * Handle STDERR for the process.
   *
   * If silence is requested, we simply redirect to /dev/null. If the
   * application requested to read from the subprocesses stderr, then we need
   * to create a pipe. Otherwose, merge stderr into our own stderr.
   */
  if (self->stderr_fd != -1)
    {
      self->flags &= ~G_SUBPROCESS_FLAGS_STDERR_PIPE;
      stderr_pair[1] = self->stderr_fd;
      self->stderr_fd = -1;
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


  /*
   * Build our FDs for the message.
   */
  g_variant_builder_add (fd_builder, "{uh}", 0, stdin_handle);
  g_variant_builder_add (fd_builder, "{uh}", 1, stdout_handle);
  g_variant_builder_add (fd_builder, "{uh}", 2, stderr_handle);


  /*
   * Now add the rest of our FDs that we might need to map in for which
   * the subprocess launcher tried to map.
   */
  for (guint i = 0; i < self->fd_mapping_len; i++)
    {
      const IdeBreakoutFdMapping *map = &self->fd_mapping[i];
      g_autoptr(GError) fd_error = NULL;
      gint dest_handle;

      dest_handle = g_unix_fd_list_append (fd_list, map->source_fd, &fd_error);

      if (dest_handle != -1)
        g_variant_builder_add (fd_builder, "{uh}", map->dest_fd, dest_handle);
      else
        g_warning ("%s", fd_error->message);

      close (map->source_fd);
    }

  /*
   * We don't want to allow these FDs to be used again.
   */
  self->fd_mapping_len = 0;
  g_clear_pointer (&self->fd_mapping, g_free);


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
          const gchar *pair = self->env[i];
          const gchar *eq = strchr (pair, '=');
          const gchar *val = eq ? eq + 1 : "";
          g_autofree gchar *key = eq ? g_strndup (pair, eq - pair) : g_strdup (pair);

          g_variant_builder_add (env_builder, "{ss}", key, val);
        }
    }


  /*
   * Register signal handlers for SIGTERM/SIGINT so that we can terminate
   * the host process with us (which won't be guaranteed since its outside
   * our cgroup, nor can we use a process group leader).
   */
  self->sigterm_id = g_unix_signal_add (SIGTERM, sigterm_handler, self);
  self->sigint_id = g_unix_signal_add (SIGINT, sigint_handler, self);


  /*
   * Make sure we've closed or stolen all of the FDs that are in play
   * before calling the DBus service.
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
                             G_CALLBACK (ide_breakout_subprocess_connection_closed),
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
                          self->clear_env ? FLATPAK_HOST_COMMAND_FLAGS_CLEAR_ENV : 0);
  g_variant_ref_sink (params);

#ifdef IDE_ENABLE_TRACE
  {
    g_autofree gchar *str = g_variant_print (params, TRUE);
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

  if (cancellable != NULL)
    {
      g_signal_connect_object (cancellable,
                               "cancelled",
                               G_CALLBACK (ide_breakout_subprocess_cancelled),
                               self,
                               G_CONNECT_SWAPPED);
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
  iface->init = ide_breakout_subprocess_initable_init;
}

G_DEFINE_TYPE_EXTENDED (IdeBreakoutSubprocess, ide_breakout_subprocess, G_TYPE_OBJECT, 0,
                        G_IMPLEMENT_INTERFACE (G_TYPE_INITABLE, initiable_iface_init)
                        G_IMPLEMENT_INTERFACE (IDE_TYPE_SUBPROCESS, subprocess_iface_init))

static void
ide_breakout_subprocess_dispose (GObject *object)
{
  IdeBreakoutSubprocess *self = (IdeBreakoutSubprocess *)object;

  g_assert (IDE_IS_BREAKOUT_SUBPROCESS (self));

  if (self->exited_subscription != 0)
    {
      if (self->connection != NULL && !g_dbus_connection_is_closed (self->connection))
        {
          IDE_TRACE_MSG ("Unsubscribing from DBus subscription %d", self->exited_subscription);
          g_dbus_connection_signal_unsubscribe (self->connection, self->exited_subscription);
        }

      self->exited_subscription = 0;
    }

  if (self->waiting != NULL)
    g_warning ("improper disposal while async operations are active!");

  ide_clear_source (&self->sigint_id);
  ide_clear_source (&self->sigterm_id);

  G_OBJECT_CLASS (ide_breakout_subprocess_parent_class)->dispose (object);
}

static void
ide_breakout_subprocess_finalize (GObject *object)
{
  IdeBreakoutSubprocess *self = (IdeBreakoutSubprocess *)object;

  IDE_ENTRY;

  g_assert (self->waiting == NULL);
  g_assert_cmpint (self->sigint_id, ==, 0);
  g_assert_cmpint (self->sigterm_id, ==, 0);
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

  g_mutex_clear (&self->waiter_mutex);
  g_cond_clear (&self->waiter_cond);

  if (self->stdin_fd != -1)
    close (self->stdin_fd);

  if (self->stdout_fd != -1)
    close (self->stdout_fd);

  if (self->stderr_fd != -1)
    close (self->stderr_fd);

  for (guint i = 0; i < self->fd_mapping_len; i++)
    close (self->fd_mapping[i].source_fd);
  g_clear_pointer (&self->fd_mapping, g_free);

  G_OBJECT_CLASS (ide_breakout_subprocess_parent_class)->finalize (object);

  EGG_COUNTER_DEC (instances);

  IDE_EXIT;
}

static void
ide_breakout_subprocess_get_property (GObject    *object,
                                      guint       prop_id,
                                      GValue     *value,
                                      GParamSpec *pspec)
{
  IdeBreakoutSubprocess *self = IDE_BREAKOUT_SUBPROCESS (object);

  switch (prop_id)
    {
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

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
ide_breakout_subprocess_set_property (GObject      *object,
                                      guint         prop_id,
                                      const GValue *value,
                                      GParamSpec   *pspec)
{
  IdeBreakoutSubprocess *self = IDE_BREAKOUT_SUBPROCESS (object);

  switch (prop_id)
    {
    case PROP_CWD:
      self->cwd = g_value_dup_string (value);
      break;

    case PROP_ARGV:
      self->argv = g_value_dup_boxed (value);
      break;

    case PROP_ENV:
      self->env = g_value_dup_boxed (value);
      break;

    case PROP_FLAGS:
      self->flags = g_value_get_flags (value);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
ide_breakout_subprocess_class_init (IdeBreakoutSubprocessClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->dispose = ide_breakout_subprocess_dispose;
  object_class->finalize = ide_breakout_subprocess_finalize;
  object_class->get_property = ide_breakout_subprocess_get_property;
  object_class->set_property = ide_breakout_subprocess_set_property;

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

  g_object_class_install_properties (object_class, N_PROPS, properties);
}

static void
ide_breakout_subprocess_init (IdeBreakoutSubprocess *self)
{
  IDE_ENTRY;

  EGG_COUNTER_INC (instances);

  self->stdin_fd = -1;
  self->stdout_fd = -1;
  self->stderr_fd = -1;

  g_mutex_init (&self->waiter_mutex);
  g_cond_init (&self->waiter_cond);

  IDE_EXIT;
}

IdeSubprocess *
_ide_breakout_subprocess_new (const gchar                 *cwd,
                              const gchar * const         *argv,
                              const gchar * const         *env,
                              GSubprocessFlags             flags,
                              gboolean                     clear_env,
                              gint                         stdin_fd,
                              gint                         stdout_fd,
                              gint                         stderr_fd,
                              const IdeBreakoutFdMapping  *fd_mapping,
                              guint                        fd_mapping_len,
                              GCancellable                *cancellable,
                              GError                     **error)
{
  g_autoptr(IdeBreakoutSubprocess) ret = NULL;

  g_return_val_if_fail (argv != NULL, NULL);
  g_return_val_if_fail (argv[0] != NULL, NULL);

  ret = g_object_new (IDE_TYPE_BREAKOUT_SUBPROCESS,
                      "cwd", cwd,
                      "argv", argv,
                      "env", env,
                      "flags", flags,
                      NULL);

  ret->clear_env = clear_env;
  ret->stdin_fd = stdin_fd;
  ret->stdout_fd = stdout_fd;
  ret->stderr_fd = stderr_fd;

  ret->fd_mapping = g_new0 (IdeBreakoutFdMapping, fd_mapping_len);
  ret->fd_mapping_len = fd_mapping_len;
  memcpy (ret->fd_mapping, fd_mapping, sizeof(gint) * fd_mapping_len);

  if (!g_initable_init (G_INITABLE (ret), cancellable, error))
    return NULL;

  return g_steal_pointer (&ret);
}
