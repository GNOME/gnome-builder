/* gbp-buildconfig-run-command-provider.c
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

#define G_LOG_DOMAIN "gbp-buildconfig-run-command-provider"

#include "config.h"

#include <glib/gi18n.h>

#include <libide-foundry.h>
#include <libide-threading.h>

#include "gbp-buildconfig-run-command-provider.h"
#include "ide-buildconfig-config.h"

struct _GbpBuildconfigRunCommandProvider
{
  IdeObject parent_instance;
};

static void
gbp_buildconfig_run_command_provider_list_commands_async (IdeRunCommandProvider *provider,
                                                          GCancellable          *cancellable,
                                                          GAsyncReadyCallback    callback,
                                                          gpointer               user_data)
{
  GbpBuildconfigRunCommandProvider *self = (GbpBuildconfigRunCommandProvider *)provider;
  g_autoptr(GListStore) store = NULL;
  g_autoptr(IdeTask) task = NULL;
  g_autoptr(IdeRunCommand) command = NULL;
  g_auto(GStrv) env = NULL;
  IdeConfigManager *config_manager;
  const char * const *argv;
  IdeEnvironment *envobj;
  IdeContext *context;
  IdeConfig *config;

  IDE_ENTRY;

  g_assert (GBP_IS_BUILDCONFIG_RUN_COMMAND_PROVIDER (self));
  g_assert (!cancellable || G_IS_CANCELLABLE (cancellable));

  task = ide_task_new (self, cancellable, callback, user_data);
  ide_task_set_source_tag (task, gbp_buildconfig_run_command_provider_list_commands_async);

  context = ide_object_get_context (IDE_OBJECT (self));
  config_manager = ide_config_manager_from_context (context);
  config = ide_config_manager_get_current (config_manager);

  if (!IDE_IS_BUILDCONFIG_CONFIG (config))
    {
      ide_task_return_new_error (task,
                                 G_IO_ERROR,
                                 G_IO_ERROR_NOT_SUPPORTED,
                                 "Project is not configured with buildconfig, cannot list commands");
      IDE_EXIT;
    }

  if (!(argv = ide_buildconfig_config_get_run_command (IDE_BUILDCONFIG_CONFIG (config))) || argv[0] == NULL)
    {
      ide_task_return_new_error (task,
                                 G_IO_ERROR,
                                 G_IO_ERROR_NOT_SUPPORTED,
                                 "No configured run commands for buildconfig");
      IDE_EXIT;
    }

  if ((envobj = ide_config_get_runtime_environment (config)))
    {
      env = ide_environment_get_environ (envobj);

      if (env != NULL && env[0] == NULL)
        g_clear_pointer (&env, g_strfreev);
    }

  command = ide_run_command_new ();
  ide_run_command_set_id (command, "buildconfig:");
  ide_run_command_set_priority (command, -500);
  ide_run_command_set_display_name (command, argv[0]);
  ide_run_command_set_argv (command, (const char * const *)argv);
  ide_run_command_set_environ (command, (const char * const *)env);
  ide_run_command_set_can_default (command, TRUE);

  store = g_list_store_new (IDE_TYPE_RUN_COMMAND);
  g_list_store_append (store, command);

  ide_task_return_pointer (task, g_steal_pointer (&store), g_object_unref);

  IDE_EXIT;
}

static GListModel *
gbp_buildconfig_run_command_provider_list_commands_finish (IdeRunCommandProvider  *provider,
                                                           GAsyncResult           *result,
                                                           GError                **error)
{
  GListModel *ret;

  IDE_ENTRY;

  g_assert (GBP_IS_BUILDCONFIG_RUN_COMMAND_PROVIDER (provider));
  g_assert (IDE_IS_TASK (result));
  g_assert (ide_task_is_valid (IDE_TASK (result), provider));

  ret = ide_task_propagate_pointer (IDE_TASK (result), error);

  IDE_RETURN (ret);
}

static void
run_command_provider_iface_init (IdeRunCommandProviderInterface *iface)
{
  iface->list_commands_async = gbp_buildconfig_run_command_provider_list_commands_async;
  iface->list_commands_finish = gbp_buildconfig_run_command_provider_list_commands_finish;
}

G_DEFINE_FINAL_TYPE_WITH_CODE (GbpBuildconfigRunCommandProvider, gbp_buildconfig_run_command_provider, IDE_TYPE_OBJECT,
                               G_IMPLEMENT_INTERFACE (IDE_TYPE_RUN_COMMAND_PROVIDER, run_command_provider_iface_init))

static void
gbp_buildconfig_run_command_provider_class_init (GbpBuildconfigRunCommandProviderClass *klass)
{
}

static void
gbp_buildconfig_run_command_provider_init (GbpBuildconfigRunCommandProvider *self)
{
}
