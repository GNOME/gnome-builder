/* gbp-vim-command-provider.c
 *
 * Copyright 2018-2019 Christian Hergert <chergert@redhat.com>
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

#define G_LOG_DOMAIN "gbp-vim-command-provider"

#include "config.h"

#include <glib/gi18n.h>
#include <libide-editor.h>
#include <libide-gui.h>
#include <libide-threading.h>

#include "gb-vim.h"
#include "gbp-vim-command.h"
#include "gbp-vim-command-provider.h"

struct _GbpVimCommandProvider
{
  GObject parent_instance;
};

static void
gbp_vim_command_provider_query_async (IdeCommandProvider  *provider,
                                      IdeWorkspace        *workspace,
                                      const gchar         *typed_text,
                                      GCancellable        *cancellable,
                                      GAsyncReadyCallback  callback,
                                      gpointer             user_data)
{
  GbpVimCommandProvider *self = (GbpVimCommandProvider *)provider;
  g_autoptr(IdeTask) task = NULL;
  g_autoptr(GPtrArray) results = NULL;
  g_autofree const gchar **commands = NULL;
  g_autofree const gchar **descriptions = NULL;
  IdePage *page;

  g_assert (GBP_IS_VIM_COMMAND_PROVIDER (self));
  g_assert (IDE_IS_WORKSPACE (workspace));
  g_assert (typed_text != NULL);
  g_assert (!cancellable || G_IS_CANCELLABLE (cancellable));

  task = ide_task_new (self, cancellable, callback, user_data);
  ide_task_set_source_tag (task, gbp_vim_command_provider_query_async);

  results = g_ptr_array_new_with_free_func (g_object_unref);
  page = ide_workspace_get_most_recent_page (workspace);

  if (!IDE_IS_EDITOR_PAGE (page))
    goto no_active_widget;

  commands = gb_vim_commands (typed_text, &descriptions);

  for (guint i = 0; commands[i]; i++)
    {
      g_autoptr(GbpVimCommand) command = NULL;

      command = gbp_vim_command_new (GTK_WIDGET (page),
                                     typed_text,
                                     g_dgettext (GETTEXT_PACKAGE, commands[i]),
                                     g_dgettext (GETTEXT_PACKAGE, descriptions[i]));
      g_ptr_array_add (results, g_steal_pointer (&command));
    }

no_active_widget:
  ide_task_return_pointer (task,
                           g_steal_pointer (&results),
                           g_ptr_array_unref);
}

static GPtrArray *
gbp_vim_command_provider_query_finish (IdeCommandProvider  *provider,
                                       GAsyncResult        *result,
                                       GError             **error)
{
  GPtrArray *ret;

  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (GBP_IS_VIM_COMMAND_PROVIDER (provider));
  g_assert (G_IS_ASYNC_RESULT (result));

  ret = ide_task_propagate_pointer (IDE_TASK (result), error);

  return IDE_PTR_ARRAY_STEAL_FULL (&ret);
}

static void
command_provider_iface_init (IdeCommandProviderInterface *iface)
{
  iface->query_async = gbp_vim_command_provider_query_async;
  iface->query_finish = gbp_vim_command_provider_query_finish;
}

G_DEFINE_TYPE_WITH_CODE (GbpVimCommandProvider, gbp_vim_command_provider, G_TYPE_OBJECT,
                         G_IMPLEMENT_INTERFACE (IDE_TYPE_COMMAND_PROVIDER, command_provider_iface_init))

static void
gbp_vim_command_provider_class_init (GbpVimCommandProviderClass *klass)
{
}

static void
gbp_vim_command_provider_init (GbpVimCommandProvider *self)
{
}
