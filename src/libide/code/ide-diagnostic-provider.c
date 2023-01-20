/* ide-diagnostic-provider.c
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

#define G_LOG_DOMAIN "ide-diagnostic-provider"

#include "config.h"

#include "ide-marshal.h"

#include "ide-buffer.h"
#include "ide-diagnostic-provider.h"

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
                  0,
                  NULL, NULL,
                  ide_marshal_VOID__VOID,
                  G_TYPE_NONE, 0);
  g_signal_set_va_marshaller (signals [INVALIDATED],
                              G_TYPE_FROM_INTERFACE (iface),
                              ide_marshal_VOID__VOIDv);
}

/**
 * ide_diagnostic_provider_load:
 * @self: a #IdeDiagnosticProvider
 *
 * Loads the provider, discovering any necessary resources.
 */
void
ide_diagnostic_provider_load (IdeDiagnosticProvider *self)
{
  g_return_if_fail (IDE_IS_DIAGNOSTIC_PROVIDER (self));

  if (IDE_DIAGNOSTIC_PROVIDER_GET_IFACE (self)->load)
    IDE_DIAGNOSTIC_PROVIDER_GET_IFACE (self)->load (self);
}

/**
 * ide_diagnostic_provider_unload:
 * @self: a #IdeDiagnosticProvider
 *
 * Unloads the provider and any allocated resources.
 */
void
ide_diagnostic_provider_unload (IdeDiagnosticProvider *self)
{
  g_return_if_fail (IDE_IS_DIAGNOSTIC_PROVIDER (self));

  if (IDE_DIAGNOSTIC_PROVIDER_GET_IFACE (self)->unload)
    IDE_DIAGNOSTIC_PROVIDER_GET_IFACE (self)->unload (self);
}

/**
 * ide_diagnostic_provider_diagnose_async:
 * @self: a #IdeDiagnosticProvider
 * @file: a #GFile
 * @contents: (nullable): the content for the buffer
 * @lang_id: (nullable): the language id for the buffer
 * @cancellable: (nullable): a #GCancellable or %NULL
 * @callback: a callback to execute upon completion
 * @user_data: closure data for @callback
 *
 * Requests the provider diagnose @file using @contents as the contents of
 * the file.
 *
 * @callback is executed upon completion, and the caller should call
 * ide_diagnostic_provider_diagnose_finish() to get the result.
 */
void
ide_diagnostic_provider_diagnose_async (IdeDiagnosticProvider *self,
                                        GFile                 *file,
                                        GBytes                *contents,
                                        const gchar           *lang_id,
                                        GCancellable          *cancellable,
                                        GAsyncReadyCallback    callback,
                                        gpointer               user_data)
{
  g_return_if_fail (IDE_IS_DIAGNOSTIC_PROVIDER (self));
  g_return_if_fail (G_IS_FILE (file));
  g_return_if_fail (!cancellable || G_IS_CANCELLABLE (cancellable));

  IDE_DIAGNOSTIC_PROVIDER_GET_IFACE (self)->diagnose_async (self,
                                                            file,
                                                            contents,
                                                            lang_id,
                                                            cancellable,
                                                            callback,
                                                            user_data);
}

/**
 * ide_diagnostic_provider_diagnose_finish:
 * @self: a #IdeDiagnosticProvider
 *
 * Completes an asynchronous request to diagnose a file.
 *
 * Returns: (transfer full): an #IdeDiagnostics or %NULL and @error is set.
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

void
ide_diagnostic_provider_emit_invalidated (IdeDiagnosticProvider *self)
{
  g_return_if_fail (IDE_IS_DIAGNOSTIC_PROVIDER (self));

  g_signal_emit (self, signals [INVALIDATED], 0);
}
