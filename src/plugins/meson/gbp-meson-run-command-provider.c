/* gbp-meson-run-command-provider.c
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

#define G_LOG_DOMAIN "gbp-meson-run-command-provider"

#include <libide-threading.h>

#include "gbp-meson-build-system.h"
#include "gbp-meson-introspection.h"
#include "gbp-meson-pipeline-addin.h"
#include "gbp-meson-run-command-provider.h"

struct _GbpMesonRunCommandProvider
{
  IdeObject parent_instance;
};

static void
gbp_meson_run_command_provider_list_run_commands_cb (GObject      *object,
                                                     GAsyncResult *result,
                                                     gpointer      user_data)
{
  GbpMesonIntrospection *introspection = (GbpMesonIntrospection *)object;
  g_autoptr(GListModel) run_commands = NULL;
  g_autoptr(IdeTask) task = user_data;
  g_autoptr(GError) error = NULL;

  IDE_ENTRY;

  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (GBP_IS_MESON_INTROSPECTION (introspection));
  g_assert (G_IS_ASYNC_RESULT (result));
  g_assert (IDE_IS_TASK (task));

  if (!(run_commands = gbp_meson_introspection_list_run_commands_finish (introspection, result, &error)))
    ide_task_return_error (task, g_steal_pointer (&error));
  else
    ide_task_return_pointer (task, g_steal_pointer (&run_commands), g_object_unref);

  IDE_EXIT;
}

static void
gbp_meson_run_command_provider_list_commands_async (IdeRunCommandProvider *provider,
                                                    GCancellable          *cancellable,
                                                    GAsyncReadyCallback    callback,
                                                    gpointer               user_data)
{
  GbpMesonRunCommandProvider *self = (GbpMesonRunCommandProvider *)provider;
  GbpMesonIntrospection *introspection;
  g_autoptr(IdeTask) task = NULL;
  IdePipelineAddin *addin;
  IdeBuildManager *build_manager;
  IdeBuildSystem *build_system;
  IdePipeline *pipeline;
  IdeContext *context;

  IDE_ENTRY;

  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (IDE_IS_RUN_COMMAND_PROVIDER (self));
  g_assert (!cancellable || G_IS_CANCELLABLE (cancellable));

  task = ide_task_new (self, cancellable, callback, user_data);
  ide_task_set_source_tag (task, gbp_meson_run_command_provider_list_commands_async);

  context = ide_object_get_context (IDE_OBJECT (self));
  build_system = ide_build_system_from_context (context);
  build_manager = ide_build_manager_from_context (context);
  pipeline = ide_build_manager_get_pipeline (build_manager);

  if (!GBP_IS_MESON_BUILD_SYSTEM (build_system) ||
      pipeline == NULL ||
      !(addin = ide_pipeline_addin_find_by_module_name (pipeline, "meson")) ||
      !(introspection = gbp_meson_pipeline_addin_get_introspection (GBP_MESON_PIPELINE_ADDIN (addin))))
    {
      ide_task_return_new_error (task,
                                 G_IO_ERROR,
                                 G_IO_ERROR_NOT_SUPPORTED,
                                 "Cannot list run commands without a meson-based pipeline");
      IDE_EXIT;
    }

  g_assert (GBP_IS_MESON_INTROSPECTION (introspection));

  gbp_meson_introspection_list_run_commands_async (introspection,
                                                   cancellable,
                                                   gbp_meson_run_command_provider_list_run_commands_cb,
                                                   g_steal_pointer (&task));

  IDE_EXIT;
}

static GListModel *
gbp_meson_run_command_provider_list_commands_finish (IdeRunCommandProvider  *provider,
                                                     GAsyncResult           *result,
                                                     GError                **error)
{
  GListModel *ret;

  IDE_ENTRY;

  g_assert (IDE_IS_RUN_COMMAND_PROVIDER (provider));
  g_assert (IDE_IS_TASK (result));

  ret = ide_task_propagate_pointer (IDE_TASK (result), error);

  IDE_RETURN (ret);
}

static void
run_command_provider_iface (IdeRunCommandProviderInterface *iface)
{
  iface->list_commands_async = gbp_meson_run_command_provider_list_commands_async;
  iface->list_commands_finish = gbp_meson_run_command_provider_list_commands_finish;
}

G_DEFINE_FINAL_TYPE_WITH_CODE (GbpMesonRunCommandProvider, gbp_meson_run_command_provider, IDE_TYPE_OBJECT,
                               G_IMPLEMENT_INTERFACE (IDE_TYPE_RUN_COMMAND_PROVIDER, run_command_provider_iface))

static void
gbp_meson_run_command_provider_parent_set (IdeObject *object,
                                           IdeObject *parent)
{
  GbpMesonRunCommandProvider *self = (GbpMesonRunCommandProvider *)object;

  IDE_ENTRY;

  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (GBP_IS_MESON_RUN_COMMAND_PROVIDER (self));
  g_assert (!parent || IDE_IS_OBJECT (parent));

  if (parent == NULL)
    IDE_EXIT;

  ide_run_command_provider_invalidates_at_phase (IDE_RUN_COMMAND_PROVIDER (self),
                                                 IDE_PIPELINE_PHASE_CONFIGURE);

  IDE_EXIT;
}

static void
gbp_meson_run_command_provider_class_init (GbpMesonRunCommandProviderClass *klass)
{
  IdeObjectClass *ide_object_class = IDE_OBJECT_CLASS (klass);

  ide_object_class->parent_set = gbp_meson_run_command_provider_parent_set;
}

static void
gbp_meson_run_command_provider_init (GbpMesonRunCommandProvider *self)
{
}
