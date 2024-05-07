/* ide-clang-diagnostic-provider.c
 *
 * Copyright 2015-2019 Christian Hergert <christian@hergert.me>
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

#define G_LOG_DOMAIN "ide-clang-diagnostic-provider"

#include "config.h"

#include <glib/gi18n.h>

#include <libide-foundry.h>

#include "ide-clang-client.h"
#include "ide-clang-diagnostic-provider.h"

struct _IdeClangDiagnosticProvider
{
  IdeObject       parent_instance;
  IdeBuildSystem *build_system;
  IdeClangClient *client;
};

static gboolean
ide_clang_diagnostic_provider_check_status (IdeClangDiagnosticProvider *self,
                                            IdeTask                    *task)
{
  IDE_ENTRY;

  g_assert (IDE_IS_CLANG_DIAGNOSTIC_PROVIDER (self));
  g_assert (IDE_IS_TASK (task));

  if (self->client == NULL || self->build_system == NULL)
    {
      ide_task_return_new_error (task,
                                 G_IO_ERROR,
                                 G_IO_ERROR_CANCELLED,
                                 "Operation cancelled");
      IDE_RETURN (FALSE);
    }

  IDE_RETURN (TRUE);
}

static void
ide_clang_diagnostic_provider_diagnose_cb (GObject      *object,
                                           GAsyncResult *result,
                                           gpointer      user_data)
{
  IdeClangClient *client = (IdeClangClient *)object;
  g_autoptr(IdeTask) task = user_data;
  g_autoptr(IdeDiagnostics) diagnostics = NULL;
  g_autoptr(GError) error = NULL;

  IDE_ENTRY;

  g_assert (IDE_IS_CLANG_CLIENT (client));
  g_assert (G_IS_ASYNC_RESULT (result));
  g_assert (IDE_IS_TASK (task));

  if (!(diagnostics = ide_clang_client_diagnose_finish (client, result, &error)))
    ide_task_return_error (task, g_steal_pointer (&error));
  else
    ide_task_return_pointer (task,
                             IDE_OBJECT (g_steal_pointer (&diagnostics)),
                             ide_object_unref_and_destroy);

  IDE_EXIT;
}

static void
diagnose_get_build_flags_cb (GObject      *object,
                             GAsyncResult *result,
                             gpointer      user_data)
{
  IdeBuildSystem *build_system = (IdeBuildSystem *)object;
  g_autoptr(IdeTask) task = user_data;
  g_autoptr(GError) error = NULL;
  g_auto(GStrv) flags = NULL;
  IdeClangDiagnosticProvider *self;
  GCancellable *cancellable;
  GFile *file;

  IDE_ENTRY;

  g_assert (IDE_IS_BUILD_SYSTEM (build_system));
  g_assert (G_IS_ASYNC_RESULT (result));
  g_assert (IDE_IS_TASK (task));

  self = ide_task_get_source_object (task);
  file = ide_task_get_task_data (task);
  cancellable = ide_task_get_cancellable (task);

  g_assert (IDE_IS_CLANG_DIAGNOSTIC_PROVIDER (self));
  g_assert (!cancellable || G_IS_CANCELLABLE (cancellable));
  g_assert (!file || G_IS_FILE (file));

  if (!(flags = ide_build_system_get_build_flags_finish (build_system, result, &error)))
    {
      if (!ide_error_ignore (error))
        {
          g_debug ("Failed to get build flags: %s", error->message);
          ide_task_return_error (task, g_steal_pointer (&error));
          IDE_EXIT;
        }
    }

  if (!ide_clang_diagnostic_provider_check_status (self, task))
    IDE_EXIT;

  ide_clang_client_diagnose_async (self->client,
                                   file,
                                   (const gchar * const *)flags,
                                   cancellable,
                                   ide_clang_diagnostic_provider_diagnose_cb,
                                   g_steal_pointer (&task));

  IDE_EXIT;
}

static void
ide_clang_diagnostic_provider_diagnose_async (IdeDiagnosticProvider *provider,
                                              GFile                 *file,
                                              GBytes                *contents,
                                              const gchar           *lang_id,
                                              GCancellable          *cancellable,
                                              GAsyncReadyCallback    callback,
                                              gpointer               user_data)
{
  IdeClangDiagnosticProvider *self = (IdeClangDiagnosticProvider *)provider;
  g_autoptr(IdeTask) task = NULL;

  IDE_ENTRY;

  g_assert (IDE_IS_CLANG_DIAGNOSTIC_PROVIDER (self));
  g_assert (IDE_IS_CLANG_DIAGNOSTIC_PROVIDER (self));

  task = ide_task_new (self, cancellable, callback, user_data);
  ide_task_set_task_data (task, g_file_dup (file), g_object_unref);
  ide_task_set_kind (task, IDE_TASK_KIND_COMPILER);

  if (!ide_clang_diagnostic_provider_check_status (self, task))
    IDE_EXIT;

  ide_build_system_get_build_flags_async (self->build_system,
                                          file,
                                          cancellable,
                                          diagnose_get_build_flags_cb,
                                          g_steal_pointer (&task));

  IDE_EXIT;
}

static IdeDiagnostics *
ide_clang_diagnostic_provider_diagnose_finish (IdeDiagnosticProvider  *provider,
                                               GAsyncResult           *result,
                                               GError                **error)
{
  IdeDiagnostics *ret;

  IDE_ENTRY;

  g_return_val_if_fail (IDE_IS_CLANG_DIAGNOSTIC_PROVIDER (provider), NULL);
  g_return_val_if_fail (IDE_IS_TASK (result), NULL);

  ret = ide_task_propagate_pointer (IDE_TASK (result), error);

  IDE_RETURN (ret);
}

static void
ide_clang_diagnostic_provider_load (IdeDiagnosticProvider *provider)
{
  IdeClangDiagnosticProvider *self = (IdeClangDiagnosticProvider *)provider;
  g_autoptr(IdeClangClient) client = NULL;
  IdeBuildSystem *build_system;
  IdeContext *context;

  IDE_ENTRY;

  g_assert (IDE_IS_CLANG_DIAGNOSTIC_PROVIDER (self));

  context = ide_object_get_context (IDE_OBJECT (self));
  client = ide_object_ensure_child_typed (IDE_OBJECT (context), IDE_TYPE_CLANG_CLIENT);
  build_system = ide_build_system_from_context (context);

  g_set_object (&self->client, client);
  g_set_object (&self->build_system, build_system);

  IDE_EXIT;
}

static void
ide_clang_diagnostic_provider_unload (IdeDiagnosticProvider *provider)
{
  IdeClangDiagnosticProvider *self = (IdeClangDiagnosticProvider *)provider;

  IDE_ENTRY;

  g_assert (IDE_IS_CLANG_DIAGNOSTIC_PROVIDER (self));

  g_clear_object (&self->client);
  g_clear_object (&self->build_system);

  IDE_EXIT;
}

static void
diagnostic_provider_iface_init (IdeDiagnosticProviderInterface *iface)
{
  iface->load = ide_clang_diagnostic_provider_load;
  iface->unload = ide_clang_diagnostic_provider_unload;
  iface->diagnose_async = ide_clang_diagnostic_provider_diagnose_async;
  iface->diagnose_finish = ide_clang_diagnostic_provider_diagnose_finish;
}

G_DEFINE_FINAL_TYPE_WITH_CODE (IdeClangDiagnosticProvider,
                               ide_clang_diagnostic_provider,
                               IDE_TYPE_OBJECT,
                               G_IMPLEMENT_INTERFACE (IDE_TYPE_DIAGNOSTIC_PROVIDER,
                                                      diagnostic_provider_iface_init))

static void
ide_clang_diagnostic_provider_class_init (IdeClangDiagnosticProviderClass *klass)
{
}

static void
ide_clang_diagnostic_provider_init (IdeClangDiagnosticProvider *self)
{
}
