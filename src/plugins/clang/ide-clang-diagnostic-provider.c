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

#include <glib/gi18n.h>
#include <libide-foundry.h>

#include "ide-clang-client.h"
#include "ide-clang-diagnostic-provider.h"

struct _IdeClangDiagnosticProvider
{
  IdeObject parent_instance;
};

static void diagnostic_provider_iface_init (IdeDiagnosticProviderInterface *iface);

static void
ide_clang_diagnostic_provider_diagnose_cb (GObject      *object,
                                           GAsyncResult *result,
                                           gpointer      user_data)
{
  IdeClangClient *client = (IdeClangClient *)object;
  g_autoptr(IdeTask) task = user_data;
  g_autoptr(IdeDiagnostics) diagnostics = NULL;
  g_autoptr(GError) error = NULL;

  g_assert (IDE_IS_CLANG_CLIENT (client));
  g_assert (G_IS_ASYNC_RESULT (result));
  g_assert (IDE_IS_TASK (task));

  if (!(diagnostics = ide_clang_client_diagnose_finish (client, result, &error)))
    ide_task_return_error (task, g_steal_pointer (&error));
  else
    ide_task_return_pointer (task,
                             IDE_OBJECT (g_steal_pointer (&diagnostics)),
                             ide_object_unref_and_destroy);
}

static void
diagnose_get_build_flags_cb (GObject      *object,
                             GAsyncResult *result,
                             gpointer      user_data)
{
  IdeBuildSystem *build_system = (IdeBuildSystem *)object;
  g_autoptr(IdeTask) task = user_data;
  g_autoptr(IdeClangClient) client = NULL;
  g_auto(GStrv) flags = NULL;
  GCancellable *cancellable;
  IdeContext *context;
  GFile *file;

  g_assert (IDE_IS_BUILD_SYSTEM (build_system));
  g_assert (G_IS_ASYNC_RESULT (result));
  g_assert (IDE_IS_TASK (task));

  flags = ide_build_system_get_build_flags_finish (build_system, result, NULL);
  context = ide_object_get_context (IDE_OBJECT (build_system));
  client = ide_object_ensure_child_typed (IDE_OBJECT (context), IDE_TYPE_CLANG_CLIENT);
  file = ide_task_get_task_data (task);
  cancellable = ide_task_get_cancellable (task);

  ide_clang_client_diagnose_async (client,
                                   file,
                                   (const gchar * const *)flags,
                                   cancellable,
                                   ide_clang_diagnostic_provider_diagnose_cb,
                                   g_steal_pointer (&task));
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
  IdeBuildSystem *build_system;
  IdeContext *context;

  g_assert (IDE_IS_CLANG_DIAGNOSTIC_PROVIDER (self));
  g_assert (IDE_IS_CLANG_DIAGNOSTIC_PROVIDER (self));

  task = ide_task_new (self, cancellable, callback, user_data);
  ide_task_set_task_data (task, g_object_ref (file), g_object_unref);
  ide_task_set_kind (task, IDE_TASK_KIND_COMPILER);

  context = ide_object_get_context (IDE_OBJECT (self));
  build_system = ide_build_system_from_context (context);

  ide_build_system_get_build_flags_async (build_system,
                                          file,
                                          cancellable,
                                          diagnose_get_build_flags_cb,
                                          g_steal_pointer (&task));
}

static IdeDiagnostics *
ide_clang_diagnostic_provider_diagnose_finish (IdeDiagnosticProvider  *provider,
                                               GAsyncResult           *result,
                                               GError                **error)
{
  g_return_val_if_fail (IDE_IS_CLANG_DIAGNOSTIC_PROVIDER (provider), NULL);
  g_return_val_if_fail (IDE_IS_TASK (result), NULL);

  return ide_task_propagate_pointer (IDE_TASK (result), error);
}

static void
diagnostic_provider_iface_init (IdeDiagnosticProviderInterface *iface)
{
  iface->diagnose_async = ide_clang_diagnostic_provider_diagnose_async;
  iface->diagnose_finish = ide_clang_diagnostic_provider_diagnose_finish;
}

G_DEFINE_TYPE_WITH_CODE (IdeClangDiagnosticProvider,
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
