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

#include "gbp-shellcmd-run-command.h"
#include "gbp-shellcmd-run-command-provider.h"

struct _GbpShellcmdRunCommandProvider
{
  IdeObject parent_instance;
};

static void
gbp_shellcmd_run_command_provider_populate (GListStore *store,
                                            const char *settings_path)
{
  g_autoptr(GSettings) settings = NULL;
  g_auto(GStrv) run_commands = NULL;

  IDE_ENTRY;

  g_assert (G_IS_LIST_STORE (store));
  g_assert (settings_path != NULL);

  IDE_TRACE_MSG ("Adding commands to GListStore %p from %s", store, settings_path);

  settings = g_settings_new_with_path ("org.gnome.builder.shellcmd", settings_path);
  run_commands = g_settings_get_strv (settings, "run-commands");

  for (guint i = 0; run_commands[i]; i++)
    {
      g_autofree char *run_settings_path = g_strconcat (settings_path, run_commands[i], "/", NULL);
      g_autoptr(GbpShellcmdRunCommand) run_command = gbp_shellcmd_run_command_new (run_settings_path);

      g_list_store_append (store, run_command);
    }

  IDE_EXIT;
}

static void
gbp_shellcmd_run_command_provider_list_commands_async (IdeRunCommandProvider *provider,
                                                       GCancellable          *cancellable,
                                                       GAsyncReadyCallback    callback,
                                                       gpointer               user_data)
{
  g_autoptr(GListStore) store = NULL;
  g_autoptr(IdeTask) task = NULL;
  g_autofree char *project_id = NULL;
  g_autofree char *project_settings_path = NULL;
  IdeContext *context;

  IDE_ENTRY;

  g_assert (GBP_IS_SHELLCMD_RUN_COMMAND_PROVIDER (provider));
  g_assert (!cancellable || G_IS_CANCELLABLE (cancellable));

  task = ide_task_new (provider, cancellable, callback, user_data);
  ide_task_set_source_tag (task, gbp_shellcmd_run_command_provider_list_commands_async);

  store = g_list_store_new (IDE_TYPE_RUN_COMMAND);

  /* Add project shell commands so they resolve first */
  context = ide_object_get_context (IDE_OBJECT (provider));
  project_id = ide_context_dup_project_id (context);
  project_settings_path = g_strconcat (SHELLCMD_SETTINGS_BASE, "projects/", project_id, "/", NULL);
  gbp_shellcmd_run_command_provider_populate (store, project_settings_path);

  /* Then application-wide commands for lower priority */
  gbp_shellcmd_run_command_provider_populate (store, SHELLCMD_SETTINGS_BASE);

  ide_task_return_pointer (task, g_steal_pointer (&store), g_object_unref);

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
