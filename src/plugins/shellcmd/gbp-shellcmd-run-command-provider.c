/* gbp-shellcmd-run-command-provider.c
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

#define G_LOG_DOMAIN "gbp-shellcmd-run-command-provider"
#define SHELLCMD_SETTINGS_BASE "/org/gnome/builder/shellcmd/"

#include "config.h"

#include <libide-threading.h>

#include "gbp-shellcmd-command-model.h"
#include "gbp-shellcmd-run-command.h"
#include "gbp-shellcmd-run-command-provider.h"

struct _GbpShellcmdRunCommandProvider
{
  IdeObject parent_instance;
};

static void
gbp_shellcmd_run_command_provider_list_commands_async (IdeRunCommandProvider *provider,
                                                       GCancellable          *cancellable,
                                                       GAsyncReadyCallback    callback,
                                                       gpointer               user_data)
{
  g_autoptr(GbpShellcmdCommandModel) app_commands = NULL;
  g_autoptr(GbpShellcmdCommandModel) project_commands = NULL;
  g_autoptr(GListStore) store = NULL;
  g_autoptr(GSettings) app_settings = NULL;
  g_autoptr(GSettings) project_settings = NULL;
  g_autoptr(IdeTask) task = NULL;
  g_autofree char *project_id = NULL;
  g_autofree char *project_settings_path = NULL;
  IdeContext *context;

  IDE_ENTRY;

  g_assert (GBP_IS_SHELLCMD_RUN_COMMAND_PROVIDER (provider));
  g_assert (!cancellable || G_IS_CANCELLABLE (cancellable));

  task = ide_task_new (provider, cancellable, callback, user_data);
  ide_task_set_source_tag (task, gbp_shellcmd_run_command_provider_list_commands_async);

  context = ide_object_get_context (IDE_OBJECT (provider));
  project_id = ide_context_dup_project_id (context);
  project_settings_path = g_strconcat (SHELLCMD_SETTINGS_BASE, "projects/", project_id, "/", NULL);

  app_settings = g_settings_new_with_path ("org.gnome.builder.shellcmd", SHELLCMD_SETTINGS_BASE);
  project_settings = g_settings_new_with_path ("org.gnome.builder.shellcmd", project_settings_path);

  app_commands = gbp_shellcmd_command_model_new (app_settings, "run-commands");
  project_commands = gbp_shellcmd_command_model_new (project_settings, "run-commands");

  store = g_list_store_new (G_TYPE_LIST_MODEL);
  g_list_store_append (store, project_commands);
  g_list_store_append (store, app_commands);

  ide_task_return_pointer (task,
                           g_object_new (GTK_TYPE_FLATTEN_LIST_MODEL,
                                         "model", store,
                                         NULL),
                           g_object_unref);

  IDE_EXIT;
}

static GListModel *
gbp_shellcmd_run_command_provider_list_commands_finish (IdeRunCommandProvider  *provider,
                                                        GAsyncResult           *result,
                                                        GError                **error)
{
  GListModel *ret;

  IDE_ENTRY;

  g_assert (GBP_IS_SHELLCMD_RUN_COMMAND_PROVIDER (provider));
  g_assert (IDE_IS_TASK (result));

  ret = ide_task_propagate_pointer (IDE_TASK (result), error);

  IDE_RETURN (ret);
}

static void
run_command_provider_iface_init (IdeRunCommandProviderInterface *iface)
{
  iface->list_commands_async = gbp_shellcmd_run_command_provider_list_commands_async;
  iface->list_commands_finish = gbp_shellcmd_run_command_provider_list_commands_finish;
}

G_DEFINE_FINAL_TYPE_WITH_CODE (GbpShellcmdRunCommandProvider, gbp_shellcmd_run_command_provider, IDE_TYPE_OBJECT,
                               G_IMPLEMENT_INTERFACE (IDE_TYPE_RUN_COMMAND_PROVIDER, run_command_provider_iface_init))

static void
gbp_shellcmd_run_command_provider_class_init (GbpShellcmdRunCommandProviderClass *klass)
{
}

static void
gbp_shellcmd_run_command_provider_init (GbpShellcmdRunCommandProvider *self)
{
}

char *
gbp_shellcmd_run_command_provider_create_settings_path (IdeContext *context)
{
  g_autofree char *uuid = NULL;

  g_assert (!context || IDE_IS_CONTEXT (context));

  uuid = g_uuid_string_random ();

  if (ide_context_has_project (context))
    {
      g_autofree char *project_id = ide_context_dup_project_id (context);
      return g_strconcat (SHELLCMD_SETTINGS_BASE, "projects/", project_id, "/", uuid, "/", NULL);
    }

  return g_strconcat (SHELLCMD_SETTINGS_BASE, "/", uuid, "/", NULL);
}
