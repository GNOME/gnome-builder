/* gbp-maven-run-command-provider.c
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

#define G_LOG_DOMAIN "gbp-maven-run-command-provider"

#include "config.h"

#include <glib/gi18n.h>

#include <libide-foundry.h>
#include <libide-threading.h>

#include "gbp-maven-build-system.h"
#include "gbp-maven-run-command-provider.h"

struct _GbpMavenRunCommandProvider
{
  IdeObject parent_instance;
};

static void
gbp_maven_run_command_provider_list_commands_async (IdeRunCommandProvider *provider,
                                                    GCancellable          *cancellable,
                                                    GAsyncReadyCallback    callback,
                                                    gpointer               user_data)
{
  GbpMavenRunCommandProvider *self = (GbpMavenRunCommandProvider *)provider;
  g_autoptr(IdeRunCommand) run_command = NULL;
  g_autoptr(GStrvBuilder) builder = NULL;
  g_autoptr(GListStore) store = NULL;
  g_autoptr(IdeTask) task = NULL;
  g_autofree char *project_dir = NULL;
  IdeConfigManager *config_manager;
  IdeBuildSystem *build_system;
  g_auto(GStrv) run_argv = NULL;
  g_auto(GStrv) args = NULL;
  const char *run_opts;
  IdeContext *context;
  IdeConfig *config;
  int run_argc;

  IDE_ENTRY;

  g_assert (GBP_IS_MAVEN_RUN_COMMAND_PROVIDER (self));
  g_assert (!cancellable || G_IS_CANCELLABLE (cancellable));

  task = ide_task_new (self, cancellable, callback, user_data);
  ide_task_set_source_tag (task, gbp_maven_run_command_provider_list_commands_async);

  context = ide_object_get_context (IDE_OBJECT (self));
  build_system = ide_build_system_from_context (context);
  config_manager = ide_config_manager_from_context (context);
  config = ide_config_manager_get_current (config_manager);
  run_opts = ide_config_get_run_opts (config);

  if (!GBP_IS_MAVEN_BUILD_SYSTEM (build_system))
    {
      ide_task_return_new_error (task,
                                 G_IO_ERROR,
                                 G_IO_ERROR_NOT_SUPPORTED,
                                 "Not a maven build system");
      IDE_EXIT;
    }

  project_dir = gbp_maven_build_system_get_project_dir (GBP_MAVEN_BUILD_SYSTEM (build_system));

  run_command = ide_run_command_new ();
  ide_run_command_set_id (run_command, "maven:run");
  ide_run_command_set_priority (run_command, -500);
  ide_run_command_set_display_name (run_command, _("Maven Run"));
  ide_run_command_set_cwd (run_command, project_dir);

  /*
   * This requires an argument -Dexec.mainClass="my.package.MainClass"
   * Use run-opts in your config/manifest/etc.
   */
  builder = g_strv_builder_new ();
  g_strv_builder_add_many (builder, "mvn", "exec:java", NULL);
  if (g_shell_parse_argv (run_opts, &run_argc, &run_argv, NULL))
    g_strv_builder_addv (builder, (const char **)run_argv);
  args = g_strv_builder_end (builder);
  ide_run_command_set_argv (run_command, (const char * const *)args);

  store = g_list_store_new (IDE_TYPE_RUN_COMMAND);
  g_list_store_append (store, run_command);
  ide_task_return_pointer (task, g_steal_pointer (&store), g_object_unref);

  IDE_EXIT;
}

static GListModel *
gbp_maven_run_command_provider_list_commands_finish (IdeRunCommandProvider  *provider,
                                                     GAsyncResult           *result,
                                                     GError                **error)
{
  GListModel *ret;

  IDE_ENTRY;

  g_assert (GBP_IS_MAVEN_RUN_COMMAND_PROVIDER (provider));
  g_assert (IDE_IS_TASK (result));

  ret = ide_task_propagate_pointer (IDE_TASK (result), error);

  IDE_RETURN (ret);
}

static void
run_command_provider_iface_init (IdeRunCommandProviderInterface *iface)
{
  iface->list_commands_async = gbp_maven_run_command_provider_list_commands_async;
  iface->list_commands_finish = gbp_maven_run_command_provider_list_commands_finish;
}

G_DEFINE_FINAL_TYPE_WITH_CODE (GbpMavenRunCommandProvider, gbp_maven_run_command_provider, IDE_TYPE_OBJECT,
                               G_IMPLEMENT_INTERFACE (IDE_TYPE_RUN_COMMAND_PROVIDER, run_command_provider_iface_init))

static void
gbp_maven_run_command_provider_class_init (GbpMavenRunCommandProviderClass *klass)
{
}

static void
gbp_maven_run_command_provider_init (GbpMavenRunCommandProvider *self)
{
}
