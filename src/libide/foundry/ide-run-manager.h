/* ide-run-manager.h
 *
 * Copyright 2016-2019 Christian Hergert <chergert@redhat.com>
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

#include <libpeas.h>

#include <libide-core.h>

#include "ide-foundry-types.h"

G_BEGIN_DECLS

#define IDE_TYPE_RUN_MANAGER (ide_run_manager_get_type())

IDE_AVAILABLE_IN_ALL
G_DECLARE_FINAL_TYPE (IdeRunManager, ide_run_manager, IDE, RUN_MANAGER, IdeObject)

IDE_AVAILABLE_IN_ALL
IdeRunManager  *ide_run_manager_from_context                   (IdeContext           *context);
IDE_AVAILABLE_IN_ALL
void            ide_run_manager_cancel                         (IdeRunManager        *self);
IDE_AVAILABLE_IN_ALL
const char     *ide_run_manager_get_icon_name                  (IdeRunManager        *self);
IDE_AVAILABLE_IN_ALL
gboolean        ide_run_manager_get_busy                       (IdeRunManager        *self);
IDE_AVAILABLE_IN_ALL
void            ide_run_manager_set_run_tool_from_plugin_info  (IdeRunManager        *self,
                                                                PeasPluginInfo       *plugin_info);
IDE_AVAILABLE_IN_ALL
void            ide_run_manager_run_async                      (IdeRunManager        *self,
                                                                GCancellable         *cancellable,
                                                                GAsyncReadyCallback   callback,
                                                                gpointer              user_data);
IDE_AVAILABLE_IN_ALL
gboolean        ide_run_manager_run_finish                     (IdeRunManager        *self,
                                                                GAsyncResult         *result,
                                                                GError              **error);
IDE_AVAILABLE_IN_ALL
void            ide_run_manager_list_commands_async            (IdeRunManager        *self,
                                                                GCancellable         *cancellable,
                                                                GAsyncReadyCallback   callback,
                                                                gpointer              user_data);
IDE_AVAILABLE_IN_ALL
GListModel     *ide_run_manager_list_commands_finish           (IdeRunManager        *self,
                                                                GAsyncResult         *result,
                                                                GError              **error);
IDE_AVAILABLE_IN_ALL
void            ide_run_manager_discover_run_command_async     (IdeRunManager        *self,
                                                                GCancellable         *cancellable,
                                                                GAsyncReadyCallback   callback,
                                                                gpointer              user_data);
IDE_AVAILABLE_IN_ALL
IdeRunCommand  *ide_run_manager_discover_run_command_finish    (IdeRunManager        *self,
                                                                GAsyncResult         *result,
                                                                GError              **error);


G_END_DECLS
