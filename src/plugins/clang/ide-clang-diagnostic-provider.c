/* ide-clang-diagnostic-provider.c
 *
 * Copyright © 2015 Christian Hergert <christian@hergert.me>
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
 */

#define G_LOG_DOMAIN "ide-clang-diagnostic-provider"

#include <glib/gi18n.h>

#include "ide-clang-diagnostic-provider.h"
#include "ide-clang-service.h"
#include "ide-clang-translation-unit.h"

struct _IdeClangDiagnosticProvider
{
  IdeObject parent_instance;
};

struct _IdeClangDiagnosticProviderClass
{
  IdeObjectClass parent_class;
};

static void diagnostic_provider_iface_init (IdeDiagnosticProviderInterface *iface);

G_DEFINE_TYPE_EXTENDED (IdeClangDiagnosticProvider,
                        ide_clang_diagnostic_provider,
                        IDE_TYPE_OBJECT,
                        0,
                        G_IMPLEMENT_INTERFACE (IDE_TYPE_DIAGNOSTIC_PROVIDER,
                                               diagnostic_provider_iface_init))

static void
get_translation_unit_cb (GObject      *object,
                         GAsyncResult *result,
                         gpointer      user_data)
{
  IdeClangService *service = (IdeClangService *)object;
  g_autoptr(IdeClangTranslationUnit) tu = NULL;
  g_autoptr(IdeTask) task = user_data;
  g_autoptr(GError) error = NULL;
  IdeDiagnostics *diagnostics;
  IdeFile *target;
  GFile *gfile;

  tu = ide_clang_service_get_translation_unit_finish (service, result, &error);

  if (!tu)
    {
      ide_task_return_error (task, g_steal_pointer (&error));
      return;
    }

  target = ide_task_get_task_data (task);
  g_assert (IDE_IS_FILE (target));

  gfile = ide_file_get_file (target);
  g_assert (G_IS_FILE (gfile));

  diagnostics = ide_clang_translation_unit_get_diagnostics_for_file (tu, gfile);

  ide_task_return_pointer (task,
                           ide_diagnostics_ref (diagnostics),
                           (GDestroyNotify)ide_diagnostics_unref);
}

static gboolean
is_header (IdeFile *file)
{
  const gchar *path;

  g_assert (IDE_IS_FILE (file));

  path = ide_file_get_path (file);

  return (g_str_has_suffix (path, ".h") ||
          g_str_has_suffix (path, ".hh") ||
          g_str_has_suffix (path, ".hxx") ||
          g_str_has_suffix (path, ".hpp"));
}

static void
ide_clang_diagnostic_provider_diagnose__file_find_other_cb (GObject      *object,
                                                            GAsyncResult *result,
                                                            gpointer      user_data)
{
  IdeFile *file = (IdeFile *)object;
  g_autoptr(IdeFile) other = NULL;
  g_autoptr(IdeTask) task = user_data;
  IdeClangService *service;
  IdeContext *context;

  g_assert (IDE_IS_FILE (file));

  other = ide_file_find_other_finish (file, result, NULL);

  if (other != NULL)
    file = other;

  context = ide_object_get_context (IDE_OBJECT (file));
  service = ide_context_get_service_typed (context, IDE_TYPE_CLANG_SERVICE);

  ide_clang_service_get_translation_unit_async (service,
                                                file,
                                                0,
                                                ide_task_get_cancellable (task),
                                                get_translation_unit_cb,
                                                g_object_ref (task));
}

static void
ide_clang_diagnostic_provider_diagnose_async (IdeDiagnosticProvider *provider,
                                              IdeFile               *file,
                                              IdeBuffer             *buffer,
                                              GCancellable          *cancellable,
                                              GAsyncReadyCallback    callback,
                                              gpointer               user_data)
{
  IdeClangDiagnosticProvider *self = (IdeClangDiagnosticProvider *)provider;
  g_autoptr(IdeTask) task = NULL;

  g_return_if_fail (IDE_IS_CLANG_DIAGNOSTIC_PROVIDER (self));

  task = ide_task_new (self, cancellable, callback, user_data);
  ide_task_set_task_data (task, g_object_ref (file), g_object_unref);

  if (is_header (file))
    {
      ide_file_find_other_async (file,
                                 cancellable,
                                 ide_clang_diagnostic_provider_diagnose__file_find_other_cb,
                                 g_object_ref (task));
    }
  else
    {
      IdeClangService *service;
      IdeContext *context;

      context = ide_object_get_context (IDE_OBJECT (provider));
      service = ide_context_get_service_typed (context, IDE_TYPE_CLANG_SERVICE);

      ide_clang_service_get_translation_unit_async (service,
                                                    file,
                                                    0,
                                                    cancellable,
                                                    get_translation_unit_cb,
                                                    g_object_ref (task));
    }
}

static IdeDiagnostics *
ide_clang_diagnostic_provider_diagnose_finish (IdeDiagnosticProvider  *provider,
                                               GAsyncResult           *result,
                                               GError                **error)
{
  IdeTask *task = (IdeTask *)result;

  g_return_val_if_fail (IDE_IS_CLANG_DIAGNOSTIC_PROVIDER (provider), NULL);
  g_return_val_if_fail (IDE_IS_TASK (task), NULL);

  return ide_task_propagate_pointer (task, error);
}

static void
diagnostic_provider_iface_init (IdeDiagnosticProviderInterface *iface)
{
  iface->diagnose_async = ide_clang_diagnostic_provider_diagnose_async;
  iface->diagnose_finish = ide_clang_diagnostic_provider_diagnose_finish;
}

static void
ide_clang_diagnostic_provider_class_init (IdeClangDiagnosticProviderClass *klass)
{
}

static void
ide_clang_diagnostic_provider_init (IdeClangDiagnosticProvider *self)
{
}
