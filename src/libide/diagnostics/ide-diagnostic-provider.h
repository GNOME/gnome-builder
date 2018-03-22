/* ide-diagnostic-provider.h
 *
 * Copyright 2015 Christian Hergert <christian@hergert.me>
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

#pragma once

#include "ide-version-macros.h"

#include "ide-object.h"

G_BEGIN_DECLS

#define IDE_TYPE_DIAGNOSTIC_PROVIDER (ide_diagnostic_provider_get_type())

IDE_AVAILABLE_IN_ALL
G_DECLARE_INTERFACE (IdeDiagnosticProvider, ide_diagnostic_provider, IDE, DIAGNOSTIC_PROVIDER, IdeObject)

struct _IdeDiagnosticProviderInterface
{
  GTypeInterface parent_interface;

  void            (*load)            (IdeDiagnosticProvider  *self);
  void            (*diagnose_async)  (IdeDiagnosticProvider  *self,
                                      IdeFile                *file,
                                      IdeBuffer              *buffer,
                                      GCancellable           *cancellable,
                                      GAsyncReadyCallback     callback,
                                      gpointer                user_data);
  IdeDiagnostics *(*diagnose_finish) (IdeDiagnosticProvider  *self,
                                      GAsyncResult           *result,
                                      GError                **error);
};

IDE_AVAILABLE_IN_ALL
void            ide_diagnostic_provider_diagnose_async   (IdeDiagnosticProvider  *self,
                                                          IdeFile                *file,
                                                          IdeBuffer              *buffer,
                                                          GCancellable           *cancellable,
                                                          GAsyncReadyCallback     callback,
                                                          gpointer                user_data);
IDE_AVAILABLE_IN_ALL
IdeDiagnostics *ide_diagnostic_provider_diagnose_finish  (IdeDiagnosticProvider  *self,
                                                          GAsyncResult           *result,
                                                          GError                **error);
IDE_AVAILABLE_IN_ALL
void            ide_diagnostic_provider_emit_invalidated (IdeDiagnosticProvider  *self);
IDE_AVAILABLE_IN_ALL
void            ide_diagnostic_provider_load             (IdeDiagnosticProvider  *self);

G_END_DECLS
