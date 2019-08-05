/* gbp-shellcmd-command-provider.c
 *
 * Copyright 2019 Christian Hergert <chergert@redhat.com>
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

#define G_LOG_DOMAIN "gbp-shellcmd-command-provider"

#include "config.h"

#include <glib/gi18n.h>
#include <libide-threading.h>

#include "gbp-shellcmd-command.h"
#include "gbp-shellcmd-command-provider.h"

struct _GbpShellcmdCommandProvider
{
  GObject parent_instance;
};

static void
gbp_shellcmd_command_provider_query_async (IdeCommandProvider  *provider,
                                           IdeWorkspace        *workspace,
                                           const gchar         *typed_text,
                                           GCancellable        *cancellable,
                                           GAsyncReadyCallback  callback,
                                           gpointer             user_data)
{
  GbpShellcmdCommandProvider *self = (GbpShellcmdCommandProvider *)provider;
  g_autoptr(IdeTask) task = NULL;
  g_autoptr(GPtrArray) ret = NULL;
  g_autofree gchar *bash_c = NULL;
  g_autofree gchar *quoted = NULL;

  g_assert (GBP_IS_SHELLCMD_COMMAND_PROVIDER (provider));
  g_assert (IDE_IS_WORKSPACE (workspace));
  g_assert (typed_text != NULL);
  g_assert (!cancellable || G_IS_CANCELLABLE (cancellable));

  task = ide_task_new (self, cancellable, callback, user_data);
  ide_task_set_source_tag (task, gbp_shellcmd_command_provider_query_async);

  quoted = g_shell_quote (typed_text);
  bash_c = g_strdup_printf ("/bin/sh -c %s", quoted);

  ret = g_ptr_array_new_with_free_func ((GDestroyNotify)ide_object_unref_and_destroy);

  g_ptr_array_add (ret,
                   g_object_new (GBP_TYPE_SHELLCMD_COMMAND,
                                 "title", _("Run in host environment"),
                                 "subtitle", typed_text,
                                 "command", bash_c,
                                 "locality", GBP_SHELLCMD_COMMAND_LOCALITY_HOST,
                                 NULL));

  g_ptr_array_add (ret,
                   g_object_new (GBP_TYPE_SHELLCMD_COMMAND,
                                 "title", _("Run in build environment"),
                                 "subtitle", typed_text,
                                 "command", bash_c,
                                 "locality", GBP_SHELLCMD_COMMAND_LOCALITY_BUILD,
                                 NULL));

  g_ptr_array_add (ret,
                   g_object_new (GBP_TYPE_SHELLCMD_COMMAND,
                                 "title", _("Run in runtime environment"),
                                 "subtitle", typed_text,
                                 "command", bash_c,
                                 "locality", GBP_SHELLCMD_COMMAND_LOCALITY_RUN,
                                 NULL));

  ide_task_return_pointer (task,
                           g_steal_pointer (&ret),
                           g_ptr_array_unref);
}

static GPtrArray *
gbp_shellcmd_command_provider_query_finish (IdeCommandProvider  *provider,
                                            GAsyncResult        *result,
                                            GError             **error)
{
  g_autoptr(GPtrArray) ret = NULL;

  g_assert (GBP_IS_SHELLCMD_COMMAND_PROVIDER (provider));
  g_assert (IDE_IS_TASK (result));

  ret = ide_task_propagate_pointer (IDE_TASK (result), error);
  return IDE_PTR_ARRAY_STEAL_FULL (&ret);
}

static void
command_provider_iface_init (IdeCommandProviderInterface *iface)
{
  iface->query_async = gbp_shellcmd_command_provider_query_async;
  iface->query_finish = gbp_shellcmd_command_provider_query_finish;
}

G_DEFINE_TYPE_WITH_CODE (GbpShellcmdCommandProvider, gbp_shellcmd_command_provider, G_TYPE_OBJECT,
                         G_IMPLEMENT_INTERFACE (IDE_TYPE_COMMAND_PROVIDER,
                                                command_provider_iface_init))

static void
gbp_shellcmd_command_provider_finalize (GObject *object)
{
  G_OBJECT_CLASS (gbp_shellcmd_command_provider_parent_class)->finalize (object);
}

static void
gbp_shellcmd_command_provider_class_init (GbpShellcmdCommandProviderClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = gbp_shellcmd_command_provider_finalize;
}

static void
gbp_shellcmd_command_provider_init (GbpShellcmdCommandProvider *self)
{
}
