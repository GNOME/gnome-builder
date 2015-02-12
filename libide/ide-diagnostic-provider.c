/* ide-diagnostic-provider.c
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

#include "ide-diagnostic-provider.h"
#include "ide-file.h"

G_DEFINE_ABSTRACT_TYPE (IdeDiagnosticProvider,
                        ide_diagnostic_provider,
                        IDE_TYPE_OBJECT)

static void
ide_diagnostic_provider_class_init (IdeDiagnosticProviderClass *klass)
{
}

static void
ide_diagnostic_provider_init (IdeDiagnosticProvider *self)
{
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

  if (IDE_DIAGNOSTIC_PROVIDER_GET_CLASS (self)->diagnose_async)
    IDE_DIAGNOSTIC_PROVIDER_GET_CLASS (self)->diagnose_async (self, file, cancellable, callback, user_data);
}

IdeDiagnostics *
ide_diagnostic_provider_diagnose_finish (IdeDiagnosticProvider  *self,
                                         GAsyncResult           *result,
                                         GError                **error)
{
  g_return_val_if_fail (IDE_IS_DIAGNOSTIC_PROVIDER (self), NULL);
  g_return_val_if_fail (G_IS_ASYNC_RESULT (result), NULL);

  if (IDE_DIAGNOSTIC_PROVIDER_GET_CLASS (self)->diagnose_finish)
    return IDE_DIAGNOSTIC_PROVIDER_GET_CLASS (self)->diagnose_finish (self, result, error);

  return NULL;
}
