/* ide-subprocess-supervisor.c
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

#define G_LOG_DOMAIN "ide-subproces-supervisor"

#include "config.h"

#include <libide-core.h>

#include "ide-marshal.h"

#include "ide-subprocess.h"
#include "ide-subprocess-supervisor.h"

/*
 * We will rate limit supervision to once per RATE_LIMIT_THRESHOLD_SECONDS
 * so that we don't allow ourself to flap the worker process in case it is
 * buggy and crashing/exiting too frequently.
 */
#define RATE_LIMIT_THRESHOLD_SECONDS G_GINT64_CONSTANT(5)

typedef struct
{
  IdeSubprocessLauncher *launcher;
  IdeSubprocess *subprocess;
  gchar *identifier;
  gint64 last_spawn_time;
  guint restart_timeout;
  guint supervising : 1;
} IdeSubprocessSupervisorPrivate;

G_DEFINE_TYPE_WITH_PRIVATE (IdeSubprocessSupervisor, ide_subprocess_supervisor, G_TYPE_OBJECT)

enum {
  SPAWNED,
  SUPERVISE,
  UNSUPERVISE,
  EXITED,
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
  g_clear_pointer (&priv->identifier, g_free);

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
                  NULL, NULL,
                  ide_marshal_VOID__OBJECT,
                  G_TYPE_NONE,
                  1,
                  IDE_TYPE_SUBPROCESS | G_SIGNAL_TYPE_STATIC_SCOPE);
  g_signal_set_va_marshaller (signals [SPAWNED],
                              G_TYPE_FROM_CLASS (klass),
                              ide_marshal_VOID__OBJECTv);

  signals [SUPERVISE] =
    g_signal_new_class_handler ("supervise",
                                G_TYPE_FROM_CLASS (klass),
                                G_SIGNAL_RUN_LAST,
                                G_CALLBACK (ide_subprocess_supervisor_real_supervise),
                                g_signal_accumulator_true_handled, NULL,
                                ide_marshal_BOOLEAN__OBJECT,
                                G_TYPE_BOOLEAN,
                                1,
                                IDE_TYPE_SUBPROCESS_LAUNCHER | G_SIGNAL_TYPE_STATIC_SCOPE);
  g_signal_set_va_marshaller (signals [SUPERVISE],
                              G_TYPE_FROM_CLASS (klass),
                              ide_marshal_BOOLEAN__OBJECTv);

  signals [UNSUPERVISE] =
    g_signal_new_class_handler ("unsupervise",
                                G_TYPE_FROM_CLASS (klass),
                                G_SIGNAL_RUN_LAST,
                                G_CALLBACK (ide_subprocess_supervisor_real_unsupervise),
                                g_signal_accumulator_true_handled, NULL,
                                ide_marshal_BOOLEAN__OBJECT,
                                G_TYPE_BOOLEAN,
                                1,
                                IDE_TYPE_SUBPROCESS_LAUNCHER | G_SIGNAL_TYPE_STATIC_SCOPE);
  g_signal_set_va_marshaller (signals [UNSUPERVISE],
                              G_TYPE_FROM_CLASS (klass),
                              ide_marshal_BOOLEAN__OBJECTv);

  signals [EXITED] =
    g_signal_new_class_handler ("exited",
                                G_TYPE_FROM_CLASS (klass),
                                G_SIGNAL_RUN_LAST,
                                NULL, NULL, NULL,
                                ide_marshal_VOID__OBJECT,
                                G_TYPE_NONE,
                                1,
                                IDE_TYPE_SUBPROCESS | G_SIGNAL_TYPE_STATIC_SCOPE);
  g_signal_set_va_marshaller (signals [EXITED],
                              G_TYPE_FROM_CLASS (klass),
                              ide_marshal_VOID__OBJECTv);
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

  if (priv->supervising)
    IDE_EXIT;

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
  IdeSubprocessSupervisor *self = data;
  IdeSubprocessSupervisorPrivate *priv = ide_subprocess_supervisor_get_instance_private (self);

  IDE_ENTRY;

  g_assert (IDE_IS_SUBPROCESS_SUPERVISOR (self));
  g_assert (priv->supervising == TRUE);

  priv->restart_timeout = 0;
  priv->supervising = FALSE;
  ide_subprocess_supervisor_start (self);

  IDE_RETURN (G_SOURCE_REMOVE);
}

static void
ide_subprocess_supervisor_start_in_usec (IdeSubprocessSupervisor *self,
                                         gint64                   usec)
{
  IdeSubprocessSupervisorPrivate *priv = ide_subprocess_supervisor_get_instance_private (self);

  IDE_ENTRY;

  g_assert (IDE_IS_SUBPROCESS_SUPERVISOR (self));

  /* Wait to re-start the supervisor until our RATE_LIMIT_THRESHOLD_SECONDS
   * have elapsed since our last spawn time. The amount of time required
   * will be given to us in the @usec parameter.
   */
  g_clear_handle_id (&priv->restart_timeout, g_source_remove);
  priv->restart_timeout =
    g_timeout_add_full (G_PRIORITY_DEFAULT,
                        MAX (250, usec / 1000L),
                        ide_subprocess_supervisor_start_in_usec_cb,
                        g_object_ref (self),
                        g_object_unref);

  IDE_EXIT;
}

void
ide_subprocess_supervisor_stop (IdeSubprocessSupervisor *self)
{
  IdeSubprocessSupervisorPrivate *priv = ide_subprocess_supervisor_get_instance_private (self);
  gboolean ret;

  IDE_ENTRY;

  g_return_if_fail (IDE_IS_SUBPROCESS_SUPERVISOR (self));

  g_clear_handle_id (&priv->restart_timeout, g_source_remove);

  if (!priv->supervising)
    return;

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
  GTimeSpan span;

  g_assert (IDE_IS_SUBPROCESS_SUPERVISOR (self));
  g_assert (required_sleep != NULL);

  span = g_get_monotonic_time () - priv->last_spawn_time;

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

  IDE_ENTRY;

  g_return_if_fail (IDE_IS_SUBPROCESS_SUPERVISOR (self));
  g_return_if_fail (IDE_IS_SUBPROCESS (subprocess));

  if (!ide_subprocess_wait_finish (subprocess, result, &error))
    g_warning ("%s", error->message);

  g_signal_emit (self, signals [EXITED], 0, subprocess);

  if (ide_subprocess_get_if_exited (subprocess))
    g_debug ("process %s exited with code: %u",
             priv->identifier,
             ide_subprocess_get_exit_status (subprocess));
  else
    g_debug ("process %s terminated due to signal: %u",
             priv->identifier,
             ide_subprocess_get_term_sig (subprocess));

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
            {
              ide_subprocess_supervisor_start_in_usec (self, sleep_usec);
              IDE_EXIT;
            }
          else
            {
              priv->supervising = FALSE;
              ide_subprocess_supervisor_start (self);
              IDE_EXIT;
            }
        }
    }

  IDE_EXIT;
}

void
ide_subprocess_supervisor_set_subprocess (IdeSubprocessSupervisor *self,
                                          IdeSubprocess           *subprocess)
{
  IdeSubprocessSupervisorPrivate *priv = ide_subprocess_supervisor_get_instance_private (self);

  IDE_ENTRY;

  g_return_if_fail (IDE_IS_SUBPROCESS_SUPERVISOR (self));
  g_return_if_fail (!subprocess || IDE_IS_SUBPROCESS (subprocess));

  if (g_set_object (&priv->subprocess, subprocess))
    {
      g_clear_pointer (&priv->identifier, g_free);

      if (subprocess != NULL)
        {
          priv->last_spawn_time = g_get_monotonic_time ();
          priv->identifier = g_strdup (ide_subprocess_get_identifier (subprocess));

          g_debug ("Setting subprocess to %s", priv->identifier);

          ide_subprocess_wait_async (priv->subprocess,
                                     NULL,
                                     ide_subprocess_supervisor_wait_cb,
                                     g_object_ref (self));
          g_signal_emit (self, signals [SPAWNED], 0, subprocess);
        }
    }

  IDE_EXIT;
}
