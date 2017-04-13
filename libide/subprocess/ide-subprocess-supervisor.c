/* ide-subprocess-supervisor.c
 *
 * Copyright (C) 2016 Christian Hergert <chergert@redhat.com>
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

#define G_LOG_DOMAIN "ide-subproces-supervisor"

#include "ide-debug.h"

#include "subprocess/ide-subprocess.h"
#include "subprocess/ide-subprocess-supervisor.h"

/*
 * We will rate limit supervision to once per RATE_LIMIT_THRESHOLD_SECONDS
 * so that we don't allow ourself to flap the worker process in case it is
 * buggy and crashing/exiting too frequently.
 */
#define RATE_LIMIT_THRESHOLD_SECONDS 5

typedef struct
{
  IdeSubprocessLauncher *launcher;
  IdeSubprocess *subprocess;
  GTimeVal last_spawn_time;
  guint supervising : 1;
} IdeSubprocessSupervisorPrivate;

G_DEFINE_TYPE_WITH_PRIVATE (IdeSubprocessSupervisor, ide_subprocess_supervisor, G_TYPE_OBJECT)

enum {
  SPAWNED,
  SUPERVISE,
  UNSUPERVISE,
  N_SIGNALS
};

static guint signals [N_SIGNALS];

static void
ide_subprocess_supervisor_reset (IdeSubprocessSupervisor *self)
{
  IdeSubprocessSupervisorPrivate *priv = ide_subprocess_supervisor_get_instance_private (self);

  g_return_if_fail (IDE_IS_SUBPROCESS_SUPERVISOR (self));

  if (priv->subprocess != NULL)
    {
      g_autoptr(IdeSubprocess) subprocess = g_steal_pointer (&priv->subprocess);

      /*
       * We steal the subprocess first before possibly forcing exit from the
       * subprocess so that when ide_subprocess_supervisor_wait_cb() is called
       * it will not be able to match on (priv->subprocess == subprocess).
       */
      ide_subprocess_force_exit (subprocess);
    }
}

static gboolean
ide_subprocess_supervisor_real_supervise (IdeSubprocessSupervisor *self,
                                          IdeSubprocessLauncher   *launcher)
{
  g_autoptr(IdeSubprocess) subprocess = NULL;
  g_autoptr(GError) error = NULL;

  g_assert (IDE_IS_SUBPROCESS_SUPERVISOR (self));
  g_assert (IDE_IS_SUBPROCESS_LAUNCHER (launcher));

  ide_subprocess_supervisor_reset (self);

  subprocess = ide_subprocess_launcher_spawn (launcher, NULL, &error);

  if (subprocess != NULL)
    ide_subprocess_supervisor_set_subprocess (self, subprocess);
  else
    g_warning ("%s", error->message);

  return TRUE;
}

static gboolean
ide_subprocess_supervisor_real_unsupervise (IdeSubprocessSupervisor *self,
                                            IdeSubprocessLauncher   *launcher)
{
  g_assert (IDE_IS_SUBPROCESS_SUPERVISOR (self));
  g_assert (IDE_IS_SUBPROCESS_LAUNCHER (launcher));

  ide_subprocess_supervisor_reset (self);

  return TRUE;
}

static void
ide_subprocess_supervisor_finalize (GObject *object)
{
  IdeSubprocessSupervisor *self = (IdeSubprocessSupervisor *)object;
  IdeSubprocessSupervisorPrivate *priv = ide_subprocess_supervisor_get_instance_private (self);

  /*
   * Subprocess will have completed a wait by this point (or cancelled). It is
   * safe to call force_exit() either way as it will drop the signal delivery
   * on the floor if the process has exited.
   */
  if (priv->subprocess != NULL)
    {
      ide_subprocess_force_exit (priv->subprocess);
      g_clear_object (&priv->subprocess);
    }

  g_clear_object (&priv->launcher);

  G_OBJECT_CLASS (ide_subprocess_supervisor_parent_class)->finalize (object);
}

static void
ide_subprocess_supervisor_class_init (IdeSubprocessSupervisorClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = ide_subprocess_supervisor_finalize;

  signals [SPAWNED] =
    g_signal_new ("spawned",
                  G_TYPE_FROM_CLASS (klass),
                  G_SIGNAL_RUN_LAST,
                  G_STRUCT_OFFSET (IdeSubprocessSupervisorClass, spawned),
                  NULL, NULL, NULL,
                  G_TYPE_NONE, 1, IDE_TYPE_SUBPROCESS);

  signals [SUPERVISE] =
    g_signal_new_class_handler ("supervise",
                                G_TYPE_FROM_CLASS (klass),
                                G_SIGNAL_RUN_LAST,
                                G_CALLBACK (ide_subprocess_supervisor_real_supervise),
                                g_signal_accumulator_true_handled, NULL,
                                NULL,
                                G_TYPE_BOOLEAN, 1, IDE_TYPE_SUBPROCESS_LAUNCHER);

  signals [UNSUPERVISE] =
    g_signal_new_class_handler ("unsupervise",
                                G_TYPE_FROM_CLASS (klass),
                                G_SIGNAL_RUN_LAST,
                                G_CALLBACK (ide_subprocess_supervisor_real_unsupervise),
                                g_signal_accumulator_true_handled, NULL,
                                NULL,
                                G_TYPE_BOOLEAN, 1, IDE_TYPE_SUBPROCESS_LAUNCHER);
}

static void
ide_subprocess_supervisor_init (IdeSubprocessSupervisor *self)
{
}

IdeSubprocessSupervisor *
ide_subprocess_supervisor_new (void)
{
  return g_object_new (IDE_TYPE_SUBPROCESS_SUPERVISOR, NULL);
}

/**
 * ide_subprocess_supervisor_get_launcher:
 *
 * Returns: (nullable) (transfer none): An #IdeSubprocessLauncher or %NULL.
 */
IdeSubprocessLauncher *
ide_subprocess_supervisor_get_launcher (IdeSubprocessSupervisor *self)
{
  IdeSubprocessSupervisorPrivate *priv = ide_subprocess_supervisor_get_instance_private (self);

  g_return_val_if_fail (IDE_IS_SUBPROCESS_SUPERVISOR (self), NULL);

  return priv->launcher;
}

void
ide_subprocess_supervisor_set_launcher (IdeSubprocessSupervisor *self,
                                        IdeSubprocessLauncher   *launcher)
{
  IdeSubprocessSupervisorPrivate *priv = ide_subprocess_supervisor_get_instance_private (self);

  g_return_if_fail (IDE_IS_SUBPROCESS_SUPERVISOR (self));
  g_return_if_fail (!launcher || IDE_IS_SUBPROCESS_LAUNCHER (launcher));

  g_set_object (&priv->launcher, launcher);
}

void
ide_subprocess_supervisor_start (IdeSubprocessSupervisor *self)
{
  IdeSubprocessSupervisorPrivate *priv = ide_subprocess_supervisor_get_instance_private (self);
  gboolean ret;

  IDE_ENTRY;

  g_return_if_fail (IDE_IS_SUBPROCESS_SUPERVISOR (self));

  if (priv->launcher == NULL)
    {
      g_warning ("Cannot supervise process, no launcher has been set");
      IDE_EXIT;
    }

  priv->supervising = TRUE;

  g_signal_emit (self, signals [SUPERVISE], 0, priv->launcher, &ret);

  IDE_EXIT;
}

static gboolean
ide_subprocess_supervisor_start_in_usec_cb (gpointer data)
{
  g_autoptr(IdeSubprocessSupervisor) self = data;

  IDE_ENTRY;

  g_assert (IDE_IS_SUBPROCESS_SUPERVISOR (self));

  ide_subprocess_supervisor_start (self);

  IDE_RETURN (G_SOURCE_REMOVE);
}

static void
ide_subprocess_supervisor_start_in_usec (IdeSubprocessSupervisor *self,
                                         gint64                   usec)
{
  IDE_ENTRY;

  g_assert (IDE_IS_SUBPROCESS_SUPERVISOR (self));

  /* Wait to re-start the supervisor until our RATE_LIMIT_THRESHOLD_SECONDS
   * have elapsed since our last spawn time. The amount of time required
   * will be given to us in the @usec parameter.
   */
  g_timeout_add (MAX (250, usec / 1000L),
                 ide_subprocess_supervisor_start_in_usec_cb,
                 g_object_ref (self));

  IDE_EXIT;
}

void
ide_subprocess_supervisor_stop (IdeSubprocessSupervisor *self)
{
  IdeSubprocessSupervisorPrivate *priv = ide_subprocess_supervisor_get_instance_private (self);
  gboolean ret;

  IDE_ENTRY;

  g_return_if_fail (IDE_IS_SUBPROCESS_SUPERVISOR (self));

  if (priv->launcher == NULL)
    {
      g_warning ("Cannot unsupervise process, no launcher has been set");
      IDE_EXIT;
    }

  priv->supervising = FALSE;

  g_signal_emit (self, signals [UNSUPERVISE], 0, priv->launcher, &ret);

  IDE_EXIT;
}

/**
 * ide_subprocess_supervisor_get_subprocess:
 * @self: An #IdeSubprocessSupervisor
 *
 * Gets the current #IdeSubprocess that is being supervised. This might be
 * %NULL if the ide_subprocess_supervisor_start() has not yet been
 * called or if there was a failure to spawn the process.
 *
 * Returns: (nullable) (transfer none): An #IdeSubprocess or %NULL.
 */
IdeSubprocess *
ide_subprocess_supervisor_get_subprocess (IdeSubprocessSupervisor *self)
{
  IdeSubprocessSupervisorPrivate *priv = ide_subprocess_supervisor_get_instance_private (self);

  g_return_val_if_fail (IDE_IS_SUBPROCESS_SUPERVISOR (self), NULL);

  return priv->subprocess;
}

static gboolean
ide_subprocess_supervisor_needs_rate_limit (IdeSubprocessSupervisor *self,
                                            gint64                  *required_sleep)
{
  IdeSubprocessSupervisorPrivate *priv = ide_subprocess_supervisor_get_instance_private (self);
  GTimeVal now;
  gint64 now_usec;
  gint64 last_usec;
  gint64 span;

  g_assert (IDE_IS_SUBPROCESS_SUPERVISOR (self));
  g_assert (required_sleep != NULL);

  g_get_current_time (&now);

  now_usec = (now.tv_sec * G_USEC_PER_SEC) + now.tv_usec;
  last_usec = (priv->last_spawn_time.tv_sec * G_USEC_PER_SEC) + priv->last_spawn_time.tv_usec;
  span = now_usec - last_usec;

  if (span < (RATE_LIMIT_THRESHOLD_SECONDS * G_USEC_PER_SEC))
    {
      *required_sleep = (RATE_LIMIT_THRESHOLD_SECONDS * G_USEC_PER_SEC) - span;
      return TRUE;
    }

  return FALSE;
}

static void
ide_subprocess_supervisor_wait_cb (GObject      *object,
                                   GAsyncResult *result,
                                   gpointer      user_data)
{
  IdeSubprocess *subprocess = (IdeSubprocess *)object;
  g_autoptr(IdeSubprocessSupervisor) self = user_data;
  IdeSubprocessSupervisorPrivate *priv = ide_subprocess_supervisor_get_instance_private (self);
  g_autoptr(GError) error = NULL;

  g_return_if_fail (IDE_IS_SUBPROCESS_SUPERVISOR (self));
  g_return_if_fail (IDE_IS_SUBPROCESS (subprocess));

  if (!ide_subprocess_wait_finish (subprocess, result, &error))
    g_warning ("%s", error->message);

#ifdef IDE_ENABLE_TRACE
  {
    if (ide_subprocess_get_if_exited (subprocess))
      IDE_TRACE_MSG ("process exited with code: %u",
                     ide_subprocess_get_exit_status (subprocess));
    else
      IDE_TRACE_MSG ("process terminated due to signal: %u",
                     ide_subprocess_get_term_sig (subprocess));
  }
#endif

  /*
   * If we end up here in response to ide_subprocess_supervisor_reset() force
   * exiting the process, we won't successfully match
   * (priv->subprocess==subprocess) and therefore will not restart the process
   * immediately (allowing the caller of ide_subprocess_supervisor_reset() to
   * complete the operation.
   */

  if (priv->subprocess == subprocess)
    {
      g_clear_object (&priv->subprocess);

      if (priv->supervising)
        {
          gint64 sleep_usec;

          if (ide_subprocess_supervisor_needs_rate_limit (self, &sleep_usec))
            ide_subprocess_supervisor_start_in_usec (self, sleep_usec);
          else
            ide_subprocess_supervisor_start (self);
        }
    }
}

void
ide_subprocess_supervisor_set_subprocess (IdeSubprocessSupervisor *self,
                                          IdeSubprocess           *subprocess)
{
  IdeSubprocessSupervisorPrivate *priv = ide_subprocess_supervisor_get_instance_private (self);

  g_return_if_fail (IDE_IS_SUBPROCESS_SUPERVISOR (self));
  g_return_if_fail (!subprocess || IDE_IS_SUBPROCESS (subprocess));

  if (g_set_object (&priv->subprocess, subprocess))
    {
      if (subprocess != NULL)
        {
          g_get_current_time (&priv->last_spawn_time);
          ide_subprocess_wait_async (priv->subprocess,
                                     NULL,
                                     ide_subprocess_supervisor_wait_cb,
                                     g_object_ref (self));
          g_signal_emit (self, signals [SPAWNED], 0, subprocess);
        }
    }
}
