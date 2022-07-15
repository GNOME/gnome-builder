/* gbp-shellcmd-search-provider.c
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

#define G_LOG_DOMAIN "gbp-shellcmd-search-provider"

#include "config.h"

#include <libide-gui.h>
#include <libide-search.h>
#include <libide-threading.h>

#include "gbp-shellcmd-command-model.h"
#include "gbp-shellcmd-search-provider.h"
#include "gbp-shellcmd-run-command.h"

struct _GbpShellcmdSearchProvider
{
  IdeObject   parent_instance;
  GListModel *commands;
};

static void
gbp_shellcmd_search_provider_search_async (IdeSearchProvider   *provider,
                                           const char          *query,
                                           guint                max_results,
                                           GCancellable        *cancellable,
                                           GAsyncReadyCallback  callback,
                                           gpointer             user_data)
{
  GbpShellcmdSearchProvider *self = (GbpShellcmdSearchProvider *)provider;
  g_autoptr(IdeTask) task = NULL;

  IDE_ENTRY;

  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (GBP_IS_SHELLCMD_SEARCH_PROVIDER (self));
  g_assert (!cancellable || G_IS_CANCELLABLE (cancellable));

  task = ide_task_new (self, cancellable, callback, user_data);
  ide_task_set_source_tag (task, gbp_shellcmd_search_provider_search_async);

  ide_task_return_unsupported_error (task);

  IDE_EXIT;
}

static GPtrArray *
gbp_shellcmd_search_provider_search_finish (IdeSearchProvider  *provider,
                                            GAsyncResult       *result,
                                            GError            **error)
{
  GPtrArray *ret;

  IDE_ENTRY;

  g_assert (GBP_IS_SHELLCMD_SEARCH_PROVIDER (provider));
  g_assert (IDE_IS_TASK (result));

  ret = ide_task_propagate_pointer (IDE_TASK (result), error);

  IDE_RETURN (ret);
}

static void
gbp_shellcmd_search_provider_load (IdeSearchProvider *provider)
{
  GbpShellcmdSearchProvider *self = (GbpShellcmdSearchProvider *)provider;
  g_autoptr(GbpShellcmdCommandModel) app_commands = NULL;
  g_autoptr(GListStore) store = NULL;
  IdeContext *context;

  IDE_ENTRY;

  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (GBP_IS_SHELLCMD_SEARCH_PROVIDER (self));

  context = ide_object_get_context (IDE_OBJECT (self));
  store = g_list_store_new (G_TYPE_LIST_MODEL);

  app_commands = gbp_shellcmd_command_model_new_for_app ();
  g_list_store_append (store, app_commands);

  if (ide_context_has_project (context))
    {
      g_autoptr(GbpShellcmdCommandModel) project_commands = NULL;

      project_commands = gbp_shellcmd_command_model_new_for_project (context);
      g_list_store_append (store, project_commands);
    }

  self->commands = g_object_new (GTK_TYPE_FLATTEN_LIST_MODEL,
                                 "model", store,
                                 NULL);

  IDE_EXIT;
}

static void
gbp_shellcmd_search_provider_unload (IdeSearchProvider *provider)
{
  GbpShellcmdSearchProvider *self = (GbpShellcmdSearchProvider *)provider;

  IDE_ENTRY;

  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (GBP_IS_SHELLCMD_SEARCH_PROVIDER (self));

  g_clear_object (&self->commands);

  IDE_EXIT;
}

static void
search_provider_iface_init (IdeSearchProviderInterface *iface)
{
  iface->load = gbp_shellcmd_search_provider_load;
  iface->unload = gbp_shellcmd_search_provider_unload;
  iface->search_async = gbp_shellcmd_search_provider_search_async;
  iface->search_finish = gbp_shellcmd_search_provider_search_finish;
}

G_DEFINE_FINAL_TYPE_WITH_CODE (GbpShellcmdSearchProvider, gbp_shellcmd_search_provider, IDE_TYPE_OBJECT,
                               G_IMPLEMENT_INTERFACE (IDE_TYPE_SEARCH_PROVIDER, search_provider_iface_init))

static void
gbp_shellcmd_search_provider_class_init (GbpShellcmdSearchProviderClass *klass)
{
}

static void
gbp_shellcmd_search_provider_init (GbpShellcmdSearchProvider *self)
{
}
