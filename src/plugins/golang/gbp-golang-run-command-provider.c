/* gbp-golang-run-command-provider.c
 *
 * Copyright 2016-2022 Christian Hergert <chergert@redhat.com>
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

#define G_LOG_DOMAIN "gbp-golang-run-command-provider"

#include "config.h"

#include <glib/gi18n.h>

#include <libide-foundry.h>
#include <libide-threading.h>

#include "gbp-golang-build-system.h"
#include "gbp-golang-run-command-provider.h"

struct _GbpGolangRunCommandProvider
{
  IdeObject parent_instance;
};

static void
gbp_golang_run_command_provider_list_commands_async (IdeRunCommandProvider *provider,
                                                     GCancellable          *cancellable,
                                                     GAsyncReadyCallback    callback,
                                                     gpointer               user_data)
{
  GbpGolangRunCommandProvider *self = (GbpGolangRunCommandProvider *)provider;
  g_autoptr(IdeRunCommand) run_command = NULL;
  g_autoptr(GListStore) store = NULL;
  g_autoptr(IdeTask) task = NULL;
  g_autofree char *go = NULL;
  g_autofree char *project_dir = NULL;
  IdeBuildSystem *build_system;
  IdeContext *context;

  IDE_ENTRY;

  g_assert (GBP_IS_GOLANG_RUN_COMMAND_PROVIDER (self));
  g_assert (!cancellable || G_IS_CANCELLABLE (cancellable));

  task = ide_task_new (self, cancellable, callback, user_data);
  ide_task_set_source_tag (task, gbp_golang_run_command_provider_list_commands_async);

  context = ide_object_get_context (IDE_OBJECT (self));
  build_system = ide_build_system_from_context (context);

  if (!GBP_IS_GOLANG_BUILD_SYSTEM (build_system))
    {
      ide_task_return_new_error (task,
                                 G_IO_ERROR,
                                 G_IO_ERROR_NOT_SUPPORTED,
                                 "Not a golang build system");
      IDE_EXIT;
    }

  go = gbp_golang_build_system_locate_go (GBP_GOLANG_BUILD_SYSTEM (build_system), NULL, NULL);
  project_dir = gbp_golang_build_system_get_project_dir (GBP_GOLANG_BUILD_SYSTEM (build_system));

  run_command = ide_run_command_new ();
  ide_run_command_set_id (run_command, "golang:run");
  ide_run_command_set_priority (run_command, -500);
  ide_run_command_set_display_name (run_command, _("go run"));
  ide_run_command_set_argv (run_command, IDE_STRV_INIT (go, "run", "main.go"));
  ide_run_command_set_cwd (run_command, project_dir);
  ide_run_command_set_can_default (run_command, TRUE);

  store = g_list_store_new (IDE_TYPE_RUN_COMMAND);
  g_list_store_append (store, run_command);
  ide_task_return_pointer (task, g_steal_pointer (&store), g_object_unref);

  IDE_EXIT;
}

static GListModel *
gbp_golang_run_command_provider_list_commands_finish (IdeRunCommandProvider  *provider,
                                                      GAsyncResult           *result,
                                                      GError                **error)
{
  GListModel *ret;

  IDE_ENTRY;

  g_assert (GBP_IS_GOLANG_RUN_COMMAND_PROVIDER (provider));
  g_assert (IDE_IS_TASK (result));

  ret = ide_task_propagate_pointer (IDE_TASK (result), error);

  IDE_RETURN (ret);
}

static void
run_command_provider_iface_init (IdeRunCommandProviderInterface *iface)
{
  iface->list_commands_async = gbp_golang_run_command_provider_list_commands_async;
  iface->list_commands_finish = gbp_golang_run_command_provider_list_commands_finish;
}

G_DEFINE_FINAL_TYPE_WITH_CODE (GbpGolangRunCommandProvider, gbp_golang_run_command_provider, IDE_TYPE_OBJECT,
                               G_IMPLEMENT_INTERFACE (IDE_TYPE_RUN_COMMAND_PROVIDER, run_command_provider_iface_init))

static void
gbp_golang_run_command_provider_class_init (GbpGolangRunCommandProviderClass *klass)
{
}

static void
gbp_golang_run_command_provider_init (GbpGolangRunCommandProvider *self)
{
}
