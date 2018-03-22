/* ide-diagnostic-provider.c
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

#define G_LOG_DOMAIN "ide-diagnostic-provider"

#include "config.h"

#include "buffers/ide-buffer.h"
#include "ide-context.h"
#include "ide-debug.h"

#include "diagnostics/ide-diagnostic-provider.h"
#include "diagnostics/ide-diagnostics.h"
#include "files/ide-file.h"

G_DEFINE_INTERFACE (IdeDiagnosticProvider, ide_diagnostic_provider, IDE_TYPE_OBJECT)

enum {
  INVALIDATED,
  N_SIGNALS
};

static guint signals [N_SIGNALS];

static void
ide_diagnostic_provider_default_init (IdeDiagnosticProviderInterface *iface)
{
  /**
   * IdeDiagnosticProvider::invlaidated:
   *
   * This signal should be emitted by diagnostic providers when they know their
   * diagnostics have been invalidated out-of-band.
   */
  signals [INVALIDATED] =
    g_signal_new ("invalidated",
                  G_TYPE_FROM_INTERFACE (iface),
                  G_SIGNAL_RUN_LAST,
                  0, NULL, NULL, NULL, G_TYPE_NONE, 0);

}

/* If the file does not match a loaded buffer, buffer is %NULL */
void
ide_diagnostic_provider_diagnose_async (IdeDiagnosticProvider *self,
                                        IdeFile               *file,
                                        IdeBuffer             *buffer,
                                        GCancellable          *cancellable,
                                        GAsyncReadyCallback    callback,
                                        gpointer               user_data)
{
  IDE_ENTRY;

  g_return_if_fail (IDE_IS_DIAGNOSTIC_PROVIDER (self));
  g_return_if_fail (IDE_IS_FILE (file));
  g_return_if_fail (IDE_IS_BUFFER (buffer) || buffer == NULL);
  g_return_if_fail (!cancellable || G_IS_CANCELLABLE (cancellable));

  IDE_DIAGNOSTIC_PROVIDER_GET_IFACE (self)->diagnose_async (self,
                                                            file,
                                                            buffer,
                                                            cancellable,
                                                            callback,
                                                            user_data);

  IDE_EXIT;
}

/**
 * ide_diagnostic_provider_diagnose_finish:
 *
 * Completes an asynchronous call to ide_diagnostic_provider_diagnose_async().
 *
 * Returns: (transfer full) (nullable): #IdeDiagnostics or %NULL and @error is set.
 */
IdeDiagnostics *
ide_diagnostic_provider_diagnose_finish (IdeDiagnosticProvider  *self,
                                         GAsyncResult           *result,
                                         GError                **error)
{
  IdeDiagnostics *ret;

  IDE_ENTRY;

  g_return_val_if_fail (IDE_IS_DIAGNOSTIC_PROVIDER (self), NULL);
  g_return_val_if_fail (G_IS_ASYNC_RESULT (result), NULL);

  ret = IDE_DIAGNOSTIC_PROVIDER_GET_IFACE (self)->diagnose_finish (self, result, error);

  IDE_TRACE_MSG ("%s diagnosis completed (%p) with %"G_GSIZE_FORMAT" diagnostics",
                 G_OBJECT_TYPE_NAME (self),
                 ret,
                 ret ? ide_diagnostics_get_size (ret) : 0);

  IDE_RETURN (ret);
}

void
ide_diagnostic_provider_emit_invalidated (IdeDiagnosticProvider *self)
{
  g_return_if_fail (IDE_IS_DIAGNOSTIC_PROVIDER (self));

  g_signal_emit (self, signals [INVALIDATED], 0);
}

void
ide_diagnostic_provider_load (IdeDiagnosticProvider *self)
{
  g_return_if_fail (IDE_IS_DIAGNOSTIC_PROVIDER (self));

  if (IDE_DIAGNOSTIC_PROVIDER_GET_IFACE (self)->load)
    IDE_DIAGNOSTIC_PROVIDER_GET_IFACE (self)->load (self);
}
