/* gbp-flatpak-runner.c
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

#define G_LOG_DOMAIN "gbp-flatpak-runner"

#include <stdlib.h>
#include <glib/gi18n.h>

#include "gbp-flatpak-runner.h"

struct _GbpFlatpakRunner
{
  IdeRunner parent_instance;
};

G_DEFINE_TYPE (GbpFlatpakRunner, gbp_flatpak_runner, IDE_TYPE_RUNNER)

static void
gbp_flatpak_runner_run_wait_cb (GObject      *object,
                                GAsyncResult *result,
                                gpointer      user_data)
{
  IdeSubprocess *subprocess = (IdeSubprocess *)object;
  g_autoptr(GTask) task = user_data;
  GError *error = NULL;
  GbpFlatpakRunner *self;

  IDE_ENTRY;

  g_assert (IDE_IS_SUBPROCESS (subprocess));
  g_assert (G_IS_ASYNC_RESULT (result));
  g_assert (G_IS_TASK (task));

  self = g_task_get_source_object (task);

  g_assert (GBP_IS_FLATPAK_RUNNER (self));

  g_signal_emit_by_name (&self->parent_instance, "exited");

  if (!ide_subprocess_wait_finish (subprocess, result, &error))
    {
      g_task_return_error (task, error);
      IDE_EXIT;
    }

  if (ide_subprocess_get_if_exited (subprocess))
    {
      gint exit_code;

      exit_code = ide_subprocess_get_exit_status (subprocess);

      if (exit_code == EXIT_SUCCESS)
        {
          g_task_return_boolean (task, TRUE);
          IDE_EXIT;
        }
    }

  g_task_return_new_error (task,
                           G_IO_ERROR,
                           G_IO_ERROR_FAILED,
                           "%s",
                           _("Process quit unexpectedly"));

  IDE_EXIT;
}

static void
gbp_flatpak_runner_run_async (IdeRunner           *runner,
                              GCancellable        *cancellable,
                              GAsyncReadyCallback  callback,
                              gpointer             user_data)
{
  GbpFlatpakRunner *self = (GbpFlatpakRunner *)runner;
  g_autoptr(GTask) task = NULL;
  g_autoptr(IdeSubprocessLauncher) launcher = NULL;
  g_autoptr(IdeSubprocess) subprocess = NULL;
  g_auto(GStrv) argv = NULL;
  const gchar *identifier;
  GError *error = NULL;
  guint argpos = 0;

  IDE_ENTRY;

  g_assert (GBP_IS_FLATPAK_RUNNER (self));
  g_assert (!cancellable || G_IS_CANCELLABLE (cancellable));

  task = g_task_new (self, cancellable, callback, user_data);
  g_task_set_source_tag (task, gbp_flatpak_runner_run_async);

  launcher = ide_subprocess_launcher_new (0);

  ide_subprocess_launcher_set_flags (launcher, ide_runner_get_flags (&self->parent_instance));

  /*
   * We want the runners to run on the host so that we aren't captive to
   * our containing system (flatpak, jhbuild, etc).
   */
  ide_subprocess_launcher_set_run_on_host (launcher, ide_runner_get_run_on_host (&self->parent_instance));

  /*
   * We don't want the environment cleared because we need access to
   * things like DISPLAY, WAYLAND_DISPLAY, and DBUS_SESSION_BUS_ADDRESS.
   */
  ide_subprocess_launcher_set_clear_env (launcher, ide_runner_get_clear_env (&self->parent_instance));

  /*
   * Overlay the environment provided.
   */
  ide_subprocess_launcher_overlay_environment (launcher, ide_runner_get_environment (&self->parent_instance));

  /*
   * Push all of our configured arguments in order.
   */
  argv = ide_runner_get_argv (&self->parent_instance);
  for (argpos = 0; argv[argpos] != NULL; argpos++)
    ide_subprocess_launcher_push_argv (launcher, argv[argpos]);

  /*
   * Set the working directory for the process.
   * FIXME: Allow this to be configurable! Add IdeRunner::cwd.
   */
  ide_subprocess_launcher_set_cwd (launcher, g_get_home_dir ());

  subprocess = ide_subprocess_launcher_spawn (launcher, cancellable, &error);

  g_assert (subprocess == NULL || IDE_IS_SUBPROCESS (subprocess));

  if (subprocess == NULL)
    {
      g_task_return_error (task, error);
      IDE_GOTO (failure);
    }

  identifier = ide_subprocess_get_identifier (subprocess);

  g_signal_emit_by_name (&self->parent_instance, "spawned", identifier);

  ide_subprocess_wait_async (subprocess,
                             cancellable,
                             gbp_flatpak_runner_run_wait_cb,
                             g_steal_pointer (&task));

failure:
  IDE_EXIT;
}

static gboolean
gbp_flatpak_runner_run_finish (IdeRunner     *runner,
                               GAsyncResult  *result,
                               GError       **error)
{
  GbpFlatpakRunner *self = (GbpFlatpakRunner *)runner;

  g_assert (GBP_IS_FLATPAK_RUNNER (self));
  g_assert (G_IS_TASK (result));
  g_assert (g_task_is_valid (G_TASK (result), self));
  g_assert (g_task_get_source_tag (G_TASK (result)) == gbp_flatpak_runner_run_async);

  return g_task_propagate_boolean (G_TASK (result), error);
}

GbpFlatpakRunner *
gbp_flatpak_runner_new (IdeContext *context)
{
  g_return_val_if_fail (IDE_IS_CONTEXT (context), NULL);

  return g_object_new (GBP_TYPE_FLATPAK_RUNNER,
                       "context", context,
                       NULL);
}

static void
gbp_flatpak_runner_class_init (GbpFlatpakRunnerClass *klass)
{
  IdeRunnerClass *runner_class = IDE_RUNNER_CLASS (klass);

  runner_class->run_async = gbp_flatpak_runner_run_async;
  runner_class->run_finish = gbp_flatpak_runner_run_finish;
}

static void
gbp_flatpak_runner_init (GbpFlatpakRunner *self)
{
}
