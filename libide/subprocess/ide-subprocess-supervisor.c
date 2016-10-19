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

#include "ide-subprocess.h"
#include "ide-subprocess-supervisor.h"

typedef struct
{
  IdeSubprocessLauncher *launcher;
  IdeSubprocess *subprocess;
  guint supervising : 1;
} IdeSubprocessSupervisorPrivate;

G_DEFINE_TYPE_WITH_PRIVATE (IdeSubprocessSupervisor, ide_subprocess_supervisor, G_TYPE_OBJECT)

enum {
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

      if (ide_subprocess_get_if_exited (subprocess))
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

  subprocess = ide_subprocess_launcher_spawn_sync (launcher, NULL, &error);

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

  if (priv->subprocess != NULL && !ide_subprocess_get_if_exited (priv->subprocess))
    ide_subprocess_force_exit (priv->subprocess);

  g_clear_object (&priv->launcher);
  g_clear_object (&priv->subprocess);

  G_OBJECT_CLASS (ide_subprocess_supervisor_parent_class)->finalize (object);
}

static void
ide_subprocess_supervisor_class_init (IdeSubprocessSupervisorClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = ide_subprocess_supervisor_finalize;

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

  g_return_if_fail (IDE_IS_SUBPROCESS_SUPERVISOR (self));

  if (priv->launcher == NULL)
    {
      g_warning ("Cannot supervise process, no launcher has been set");
      return;
    }

  priv->supervising = TRUE;

  g_signal_emit (self, signals [SUPERVISE], 0, priv->launcher, &ret);
}

void
ide_subprocess_supervisor_stop (IdeSubprocessSupervisor *self)
{
  IdeSubprocessSupervisorPrivate *priv = ide_subprocess_supervisor_get_instance_private (self);
  gboolean ret;

  g_return_if_fail (IDE_IS_SUBPROCESS_SUPERVISOR (self));

  if (priv->launcher == NULL)
    {
      g_warning ("Cannot unsupervise process, no launcher has been set");
      return;
    }

  priv->supervising = FALSE;

  g_signal_emit (self, signals [UNSUPERVISE], 0, priv->launcher, &ret);
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

  if (priv->subprocess == subprocess)
    {
      g_clear_object (&priv->subprocess);
      if (priv->supervising)
        ide_subprocess_supervisor_start (self);
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
      ide_subprocess_wait_async (priv->subprocess,
                                 NULL,
                                 ide_subprocess_supervisor_wait_cb,
                                 g_object_ref (self));
    }
}
