/* ide-run-command-provider.h
 *
 * Copyright 2022 Christian Hergert <chergert@redhat.com>
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

#if !defined (IDE_FOUNDRY_INSIDE) && !defined (IDE_FOUNDRY_COMPILATION)
# error "Only <libide-foundry.h> can be included directly."
#endif

#include <libide-core.h>

#include "ide-pipeline-phase.h"

G_BEGIN_DECLS

#define IDE_TYPE_RUN_COMMAND_PROVIDER (ide_run_command_provider_get_type())

IDE_AVAILABLE_IN_ALL
G_DECLARE_INTERFACE (IdeRunCommandProvider, ide_run_command_provider, IDE, RUN_COMMAND_PROVIDER, IdeObject)

struct _IdeRunCommandProviderInterface
{
  GTypeInterface parent_iface;

  void        (*invalidated)          (IdeRunCommandProvider  *self);
  void        (*list_commands_async)  (IdeRunCommandProvider  *self,
                                       GCancellable           *cancellable,
                                       GAsyncReadyCallback     callback,
                                       gpointer                user_data);
  GListModel *(*list_commands_finish) (IdeRunCommandProvider  *self,
                                       GAsyncResult           *result,
                                       GError                **error);
};

IDE_AVAILABLE_IN_ALL
void        ide_run_command_provider_invalidate           (IdeRunCommandProvider  *self);
IDE_AVAILABLE_IN_ALL
void        ide_run_command_provider_invalidates_at_phase (IdeRunCommandProvider  *self,
                                                           IdePipelinePhase        phase);
IDE_AVAILABLE_IN_ALL
void        ide_run_command_provider_list_commands_async  (IdeRunCommandProvider  *self,
                                                           GCancellable           *cancellable,
                                                           GAsyncReadyCallback     callback,
                                                           gpointer                user_data);
IDE_AVAILABLE_IN_ALL
GListModel *ide_run_command_provider_list_commands_finish (IdeRunCommandProvider  *self,
                                                           GAsyncResult           *result,
                                                           GError                **error);

G_END_DECLS
