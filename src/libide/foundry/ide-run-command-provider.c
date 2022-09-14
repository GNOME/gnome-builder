/* ide-run-command-provider.c
 *
 * Copyright 2022 Christian Hergert <chergert@redhat.com>
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

#define G_LOG_DOMAIN "ide-run-command-provider"

#include "config.h"

#include "ide-build-manager.h"
#include "ide-pipeline.h"
#include "ide-run-command-provider.h"

G_DEFINE_INTERFACE (IdeRunCommandProvider, ide_run_command_provider, IDE_TYPE_OBJECT)

enum {
  INVALIDATED,
  N_SIGNALS
};

static guint signals[N_SIGNALS];

static void
ide_run_command_provider_default_init (IdeRunCommandProviderInterface *iface)
{
  signals[INVALIDATED] =
    g_signal_new ("invalidated",
                  G_TYPE_FROM_INTERFACE (iface),
                  G_SIGNAL_RUN_LAST,
                  G_STRUCT_OFFSET (IdeRunCommandProviderInterface, invalidated),
                  NULL, NULL,
                  NULL,
                  G_TYPE_NONE, 0);
}

void
ide_run_command_provider_list_commands_async (IdeRunCommandProvider *self,
                                              GCancellable          *cancellable,
                                              GAsyncReadyCallback    callback,
                                              gpointer               user_data)
{
  g_return_if_fail (IDE_IS_RUN_COMMAND_PROVIDER (self));
  g_return_if_fail (!cancellable || G_IS_CANCELLABLE (cancellable));

  IDE_RUN_COMMAND_PROVIDER_GET_IFACE (self)->list_commands_async (self, cancellable, callback, user_data);
}

/**
 * ide_run_command_provider_list_commands_finish:
 * @self: a #IdeRunCommandProvider
 * @result: a #GAsyncResult
 * @error: location for a #GError
 *
 * Completes request to list run commands.
 *
 * Returns: (transfer full): a #GListModel of #IdeRunCommand
 */
GListModel *
ide_run_command_provider_list_commands_finish (IdeRunCommandProvider  *self,
                                               GAsyncResult           *result,
                                               GError                **error)
{
  g_return_val_if_fail (IDE_IS_RUN_COMMAND_PROVIDER (self), NULL);

  return IDE_RUN_COMMAND_PROVIDER_GET_IFACE (self)->list_commands_finish (self, result, error);
}

/**
 * ide_run_command_provider_invalidate:
 * @self: a #IdeRunCommandProvider
 *
 * Emits the #IdeRunCommandProvider::invalidated signal.
 *
 * This often results in #IdeRunCommands requesting a new set of results for
 * the run command provider via ide_run_command_provider_list_commands_async().
 */
void
ide_run_command_provider_invalidate (IdeRunCommandProvider *self)
{
  g_return_if_fail (IDE_IS_RUN_COMMAND_PROVIDER (self));

  g_signal_emit (self, signals[INVALIDATED], 0);
}

static void
ide_run_command_provider_pipeline_notify_phase_cb (IdeRunCommandProvider *self,
                                                   GParamSpec            *pspec,
                                                   IdePipeline           *pipeline)
{
  IdePipelinePhase current_phase;
  IdePipelinePhase invalidate_phase;

  IDE_ENTRY;

  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (IDE_IS_RUN_COMMAND_PROVIDER (self));
  g_assert (IDE_IS_PIPELINE (pipeline));

  invalidate_phase = GPOINTER_TO_UINT (g_object_get_data (G_OBJECT (self), "PIPELINE_PHASE"));
  current_phase = ide_pipeline_get_phase (pipeline);

  /* Only invalidate when the phase exactly matches. We could check to see if
   * the current phase is > than the last notified phase, but generally
   * speaking, the users of this have a pipeline stage attached at exactly that
   * phase to be notified of.
   */
  if (invalidate_phase != 0 && current_phase != 0 && invalidate_phase == current_phase)
    ide_run_command_provider_invalidate (self);

  IDE_EXIT;
}

/**
 * ide_run_command_provider_invalidates_at_phase:
 * @self: an #IdeRunCommandProvider
 * @phase: an #IdePipelinePhase
 *
 * Invalidates the provider when @phase is reached.
 *
 * This is a helper for run command provider implementations to use which
 * will automatically invalidate @self when pipeline @phase is reached.
 *
 * Calling this function will unset any previous call to the function. Setting
 * @phase to 0 will not subscribe to any new phase.
 */
void
ide_run_command_provider_invalidates_at_phase  (IdeRunCommandProvider  *self,
                                                IdePipelinePhase        phase)
{
  GSignalGroup *signal_group;

  IDE_ENTRY;

  g_return_if_fail (IDE_IS_MAIN_THREAD ());
  g_return_if_fail (IDE_IS_RUN_COMMAND_PROVIDER (self));

  g_object_set_data (G_OBJECT (self), "PIPELINE_PHASE", GUINT_TO_POINTER (phase));

  if (phase == 0)
    IDE_EXIT;


  if (!(signal_group = g_object_get_data (G_OBJECT (self), "PIPELINE_SIGNAL_GROUP")))
    {
      IdeContext *context = ide_object_get_context (IDE_OBJECT (self));
      IdeBuildManager *build_manager = ide_build_manager_from_context (context);

      signal_group = g_signal_group_new (IDE_TYPE_PIPELINE);
      g_signal_group_connect_object (signal_group,
                                     "notify::phase",
                                     G_CALLBACK (ide_run_command_provider_pipeline_notify_phase_cb),
                                     self,
                                     G_CONNECT_SWAPPED);
      g_object_set_data_full (G_OBJECT (self),
                              "PIPELINE_SIGNAL_GROUP",
                              signal_group,
                              g_object_unref);
      g_object_bind_property (build_manager, "pipeline", signal_group, "target", G_BINDING_SYNC_CREATE);
    }

  IDE_EXIT;
}
