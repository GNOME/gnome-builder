/* ide-command-provider.h
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

#pragma once

#if !defined (IDE_GUI_INSIDE) && !defined (IDE_GUI_COMPILATION)
# error "Only <libide-gui.h> can be included directly."
#endif

#include <libide-core.h>

#include "ide-command.h"
#include "ide-workspace.h"

G_BEGIN_DECLS

#define IDE_TYPE_COMMAND_PROVIDER (ide_command_provider_get_type())

IDE_AVAILABLE_IN_3_32
G_DECLARE_INTERFACE (IdeCommandProvider, ide_command_provider, IDE, COMMAND_PROVIDER, GObject)

struct _IdeCommandProviderInterface
{
  GTypeInterface parent_iface;

  void        (*query_async)       (IdeCommandProvider   *self,
                                    IdeWorkspace         *workspace,
                                    const gchar          *typed_text,
                                    GCancellable         *cancellable,
                                    GAsyncReadyCallback   callback,
                                    gpointer              user_data);
  GPtrArray  *(*query_finish)      (IdeCommandProvider   *self,
                                    GAsyncResult         *result,
                                    GError              **error);
  IdeCommand *(*get_command_by_id) (IdeCommandProvider   *self,
                                    IdeWorkspace         *workspace,
                                    const gchar          *command_id);
  void        (*load_shortcuts)    (IdeCommandProvider   *self,
                                    IdeWorkspace         *workspace);
  void        (*unload_shortcuts)  (IdeCommandProvider   *self,
                                    IdeWorkspace         *workspace);
};

IDE_AVAILABLE_IN_3_34
void        ide_command_provider_load_shortcuts    (IdeCommandProvider   *self,
                                                    IdeWorkspace         *workspace);
IDE_AVAILABLE_IN_3_34
void        ide_command_provider_unload_shortcuts  (IdeCommandProvider   *self,
                                                    IdeWorkspace         *workspace);
IDE_AVAILABLE_IN_3_32
void        ide_command_provider_query_async       (IdeCommandProvider   *self,
                                                    IdeWorkspace         *workspace,
                                                    const gchar          *typed_text,
                                                    GCancellable         *cancellable,
                                                    GAsyncReadyCallback   callback,
                                                    gpointer              user_data);
IDE_AVAILABLE_IN_3_32
GPtrArray  *ide_command_provider_query_finish      (IdeCommandProvider   *self,
                                                    GAsyncResult         *result,
                                                    GError              **error);
IDE_AVAILABLE_IN_3_34
IdeCommand *ide_command_provider_get_command_by_id (IdeCommandProvider   *self,
                                                    IdeWorkspace         *workspace,
                                                    const gchar          *command_id);

G_END_DECLS
