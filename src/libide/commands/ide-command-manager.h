/* ide-command-manager.h
 *
 * Copyright 2019 Christian Hergert <chergert@redhat.com>
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

#if !defined (IDE_COMMANDS_INSIDE) && !defined (IDE_COMMANDS_COMPILATION)
# error "Only <libide-commands.h> can be included directly."
#endif

#include <gtk/gtk.h>

#include <libide-core.h>

#include "ide-command.h"

G_BEGIN_DECLS

#define IDE_TYPE_COMMAND_MANAGER (ide_command_manager_get_type())

IDE_AVAILABLE_IN_ALL
G_DECLARE_FINAL_TYPE (IdeCommandManager, ide_command_manager, IDE, COMMAND_MANAGER, IdeObject)

IDE_AVAILABLE_IN_ALL
IdeCommandManager *ide_command_manager_from_context      (IdeContext           *context);
IDE_AVAILABLE_IN_ALL
IdeCommand        *ide_command_manager_get_command_by_id (IdeCommandManager    *self,
                                                          GtkWidget            *widget,
                                                          const char           *command_id);
IDE_AVAILABLE_IN_ALL
void               ide_command_manager_execute           (IdeCommandManager    *self,
                                                          GtkWidget            *widget,
                                                          const char           *command_id,
                                                          GVariant             *params);
IDE_AVAILABLE_IN_ALL
void               ide_command_manager_load_shortcuts    (IdeCommandManager    *self,
                                                          GtkNative            *native);
IDE_AVAILABLE_IN_ALL
void               ide_command_manager_unload_shortcuts  (IdeCommandManager    *self,
                                                          GtkNative            *native);
IDE_AVAILABLE_IN_ALL
void               ide_command_manager_query_async       (IdeCommandManager    *self,
                                                          GtkWidget            *widget,
                                                          const char           *typed_text,
                                                          GCancellable         *cancellable,
                                                          GAsyncReadyCallback   callback,
                                                          gpointer              user_data);
IDE_AVAILABLE_IN_ALL
GPtrArray         *ide_command_manager_query_finish      (IdeCommandManager    *self,
                                                          GAsyncResult         *result,
                                                          GError              **error);

G_END_DECLS
