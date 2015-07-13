/* ide-diagnostic-provider.c
 *
 * Copyright (C) 2015 Christian Hergert <christian@hergert.me>
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

#define G_LOG_DOMAIN "ide-diagnostic-provider"

#include "ide-context.h"
#include "ide-diagnostic-provider.h"
#include "ide-file.h"

G_DEFINE_INTERFACE (IdeDiagnosticProvider, ide_diagnostic_provider, IDE_TYPE_OBJECT)

static void
ide_diagnostic_provider_default_init (IdeDiagnosticProviderInterface *iface)
{
  g_object_interface_install_property (iface,
                                       g_param_spec_object ("context",
                                                            "Context",
                                                            "Context",
                                                            IDE_TYPE_CONTEXT,
                                                            (G_PARAM_WRITABLE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS)));
}

void
ide_diagnostic_provider_diagnose_async  (IdeDiagnosticProvider *self,
                                         IdeFile               *file,
                                         GCancellable          *cancellable,
                                         GAsyncReadyCallback    callback,
                                         gpointer               user_data)
{
  g_return_if_fail (IDE_IS_DIAGNOSTIC_PROVIDER (self));
  g_return_if_fail (IDE_IS_FILE (file));
  g_return_if_fail (!cancellable || G_IS_CANCELLABLE (cancellable));

  IDE_DIAGNOSTIC_PROVIDER_GET_IFACE (self)->diagnose_async (self, file, cancellable, callback, user_data);
}

/**
 * ide_diagnostic_provider_diagnose_finish:
 *
 * Completes an asynchronous call to ide_diagnostic_provider_diagnose_async().
 *
 * Returns: (transfer full): #IdeDiagnostics or %NULL and @error is set.
 */
IdeDiagnostics *
ide_diagnostic_provider_diagnose_finish (IdeDiagnosticProvider  *self,
                                         GAsyncResult           *result,
                                         GError                **error)
{
  g_return_val_if_fail (IDE_IS_DIAGNOSTIC_PROVIDER (self), NULL);
  g_return_val_if_fail (G_IS_ASYNC_RESULT (result), NULL);

  return IDE_DIAGNOSTIC_PROVIDER_GET_IFACE (self)->diagnose_finish (self, result, error);
}
