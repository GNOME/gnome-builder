/* ide-code-action-provider.h
 *
 * Copyright 2021 Georg Vienna <georg.vienna@himbarsoft.com>
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

#include <gtk/gtk.h>
#include <libide-core.h>

#include "ide-code-types.h"
#include "ide-diagnostics.h"

G_BEGIN_DECLS

#define IDE_TYPE_CODE_ACTION_PROVIDER (ide_code_action_provider_get_type())

IDE_AVAILABLE_IN_ALL
G_DECLARE_INTERFACE (IdeCodeActionProvider, ide_code_action_provider, IDE, CODE_ACTION_PROVIDER, IdeObject)

struct _IdeCodeActionProviderInterface
{
  GTypeInterface parent;

  void       (*load)               (IdeCodeActionProvider  *self);
  void       (*query_async)        (IdeCodeActionProvider  *self,
                                    IdeBuffer              *buffer,
                                    GCancellable           *cancellable,
                                    GAsyncReadyCallback     callback,
                                    gpointer                user_data);
  GPtrArray* (*query_finish)       (IdeCodeActionProvider  *self,
                                    GAsyncResult           *result,
                                    GError                **error);
  void       (*set_diagnostics)    (IdeCodeActionProvider  *self,
                                    IdeDiagnostics         *diags);
};

IDE_AVAILABLE_IN_ALL
void       ide_code_action_provider_load            (IdeCodeActionProvider  *self);
IDE_AVAILABLE_IN_ALL
void       ide_code_action_provider_query_async     (IdeCodeActionProvider  *self,
                                                     IdeBuffer              *buffer,
                                                     GCancellable           *cancellable,
                                                     GAsyncReadyCallback     callback,
                                                     gpointer                user_data);
IDE_AVAILABLE_IN_ALL
GPtrArray *ide_code_action_provider_query_finish    (IdeCodeActionProvider  *self,
                                                     GAsyncResult           *result,
                                                     GError                **error);
IDE_AVAILABLE_IN_ALL
void       ide_code_action_provider_set_diagnostics (IdeCodeActionProvider  *self,
                                                     IdeDiagnostics         *diags);

G_END_DECLS
