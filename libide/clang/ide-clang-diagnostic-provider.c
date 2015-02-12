/* ide-clang-diagnostic-provider.c
 *
 * Copyright (C) 2015 Christian Hergert <christian@hergert.me>
 *
 * This file is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as
 * published by the Free Software Foundation; either version 3 of the
 * License, or (at your option) any later version.
 *
 * This file is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <glib/gi18n.h>

#include "ide-clang-diagnostic-provider.h"
#include "ide-clang-service.h"
#include "ide-clang-translation-unit.h"
#include "ide-context.h"
#include "ide-diagnostics.h"

G_DEFINE_TYPE (IdeClangDiagnosticProvider, ide_clang_diagnostic_provider,
               IDE_TYPE_DIAGNOSTIC_PROVIDER)

static void
get_translation_unit_cb (GObject      *object,
                         GAsyncResult *result,
                         gpointer      user_data)
{
  IdeClangService *service = (IdeClangService *)object;
  g_autoptr(IdeClangTranslationUnit) tu = NULL;
  g_autoptr(GError) error = NULL;
  g_autoptr(GTask) task = user_data;
  IdeDiagnostics *diagnostics;

  tu = ide_clang_service_get_translation_unit_finish (service, result, &error);

  if (!tu)
    {
      g_task_return_error (task, error);
      return;
    }

  diagnostics = ide_clang_translation_unit_get_diagnostics (tu);

  g_task_return_pointer (task,
                         ide_diagnostics_ref (diagnostics),
                         (GDestroyNotify)ide_diagnostics_unref);
}

static void
ide_clang_diagnostic_provider_diagnose_async (IdeDiagnosticProvider *provider,
                                              IdeFile               *file,
                                              GCancellable          *cancellable,
                                              GAsyncReadyCallback    callback,
                                              gpointer               user_data)
{
  IdeClangDiagnosticProvider *self = (IdeClangDiagnosticProvider *)provider;
  g_autoptr(GTask) task = NULL;
  IdeClangService *service;
  IdeContext *context;

  g_return_if_fail (IDE_IS_CLANG_DIAGNOSTIC_PROVIDER (self));

  task = g_task_new (self, cancellable, callback, user_data);

  context = ide_object_get_context (IDE_OBJECT (provider));
  service = ide_context_get_service_typed (context, IDE_TYPE_CLANG_SERVICE);

  ide_clang_service_get_translation_unit_async (service,
                                                file,
                                                0,
                                                cancellable,
                                                get_translation_unit_cb,
                                                g_object_ref (task));
}

static IdeDiagnostics *
ide_clang_diagnostic_provider_diagnose_finish (IdeDiagnosticProvider  *provider,
                                               GAsyncResult           *result,
                                               GError                **error)
{
  IdeClangDiagnosticProvider *self = (IdeClangDiagnosticProvider *)provider;
  GTask *task = (GTask *)result;

  g_return_val_if_fail (IDE_IS_CLANG_DIAGNOSTIC_PROVIDER (self), NULL);
  g_return_val_if_fail (G_IS_TASK (task), NULL);

  return g_task_propagate_pointer (task, error);
}

static void
ide_clang_diagnostic_provider_class_init (IdeClangDiagnosticProviderClass *klass)
{
  IdeDiagnosticProviderClass *provider_class;

  provider_class = IDE_DIAGNOSTIC_PROVIDER_CLASS (klass);
  provider_class->diagnose_async = ide_clang_diagnostic_provider_diagnose_async;
  provider_class->diagnose_finish = ide_clang_diagnostic_provider_diagnose_finish;
}

static void
ide_clang_diagnostic_provider_init (IdeClangDiagnosticProvider *self)
{
}
