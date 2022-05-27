/* ide-diagnostic-provider.h
 *
 * Copyright 2015-2019 Christian Hergert <chergert@redhat.com>
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

#pragma once

#if !defined (IDE_CODE_INSIDE) && !defined (IDE_CODE_COMPILATION)
# error "Only <libide-code.h> can be included directly."
#endif

#include <libide-core.h>

#include "ide-code-types.h"

G_BEGIN_DECLS

#define IDE_TYPE_DIAGNOSTIC_PROVIDER (ide_diagnostic_provider_get_type())

IDE_AVAILABLE_IN_ALL
G_DECLARE_INTERFACE (IdeDiagnosticProvider, ide_diagnostic_provider, IDE, DIAGNOSTIC_PROVIDER, IdeObject)

struct _IdeDiagnosticProviderInterface
{
  GTypeInterface parent_iface;

  void            (*load)            (IdeDiagnosticProvider  *self);
  void            (*unload)          (IdeDiagnosticProvider  *self);
  void            (*diagnose_async)  (IdeDiagnosticProvider  *self,
                                      GFile                  *file,
                                      GBytes                 *contents,
                                      const gchar            *lang_id,
                                      GCancellable           *cancellable,
                                      GAsyncReadyCallback     callback,
                                      gpointer                user_data);
  IdeDiagnostics *(*diagnose_finish) (IdeDiagnosticProvider  *self,
                                      GAsyncResult           *result,
                                      GError                **error);
};

IDE_AVAILABLE_IN_ALL
void            ide_diagnostic_provider_load             (IdeDiagnosticProvider  *self);
IDE_AVAILABLE_IN_ALL
void            ide_diagnostic_provider_unload           (IdeDiagnosticProvider  *self);
IDE_AVAILABLE_IN_ALL
void            ide_diagnostic_provider_emit_invalidated (IdeDiagnosticProvider  *self);
IDE_AVAILABLE_IN_ALL
void            ide_diagnostic_provider_diagnose_async   (IdeDiagnosticProvider  *self,
                                                          GFile                  *file,
                                                          GBytes                 *contents,
                                                          const gchar            *lang_id,
                                                          GCancellable           *cancellable,
                                                          GAsyncReadyCallback     callback,
                                                          gpointer                user_data);
IDE_AVAILABLE_IN_ALL
IdeDiagnostics *ide_diagnostic_provider_diagnose_finish  (IdeDiagnosticProvider  *self,
                                                          GAsyncResult           *result,
                                                          GError                **error);


G_END_DECLS
