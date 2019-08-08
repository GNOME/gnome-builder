/* ide-application.h
 *
 * Copyright 2014-2019 Christian Hergert <chergert@redhat.com>
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

#include <dazzle.h>
#include <libide-core.h>
#include <libide-projects.h>

#include "ide-workbench.h"

G_BEGIN_DECLS

#define IDE_TYPE_APPLICATION    (ide_application_get_type())
#define IDE_APPLICATION_DEFAULT IDE_APPLICATION(g_application_get_default())

IDE_AVAILABLE_IN_3_32
G_DECLARE_FINAL_TYPE (IdeApplication, ide_application, IDE, APPLICATION, DzlApplication)

IDE_AVAILABLE_IN_3_32
gboolean       ide_application_has_network              (IdeApplication           *self);
IDE_AVAILABLE_IN_3_32
gchar        **ide_application_get_argv                 (IdeApplication           *self,
                                                         GApplicationCommandLine  *cmdline);
IDE_AVAILABLE_IN_3_32
GDateTime     *ide_application_get_started_at           (IdeApplication           *self);
IDE_AVAILABLE_IN_3_32
gboolean       ide_application_get_command_line_handled (IdeApplication           *self,
                                                         GApplicationCommandLine  *cmdline);
IDE_AVAILABLE_IN_3_32
void           ide_application_set_command_line_handled (IdeApplication           *self,
                                                         GApplicationCommandLine  *cmdline,
                                                         gboolean                  handled);
IDE_AVAILABLE_IN_3_32
void           ide_application_open_project_async       (IdeApplication           *self,
                                                         IdeProjectInfo           *project_info,
                                                         GType                     workspace_type,
                                                         GCancellable             *cancellable,
                                                         GAsyncReadyCallback       callback,
                                                         gpointer                  user_data);
IDE_AVAILABLE_IN_3_32
IdeWorkbench  *ide_application_open_project_finish      (IdeApplication           *self,
                                                         GAsyncResult             *result,
                                                         GError                  **error);
IDE_AVAILABLE_IN_3_32
void           ide_application_set_workspace_type       (IdeApplication           *self,
                                                         GType                     workspace_type);
IDE_AVAILABLE_IN_3_32
void           ide_application_add_workbench            (IdeApplication           *self,
                                                         IdeWorkbench             *workbench);
IDE_AVAILABLE_IN_3_32
void           ide_application_remove_workbench         (IdeApplication           *self,
                                                         IdeWorkbench             *workbench);
IDE_AVAILABLE_IN_3_32
void           ide_application_foreach_workbench        (IdeApplication           *self,
                                                         GFunc                     callback,
                                                         gpointer                  user_data);
IDE_AVAILABLE_IN_3_32
void           ide_application_get_worker_async         (IdeApplication           *self,
                                                         const gchar              *plugin_name,
                                                         GCancellable             *cancellable,
                                                         GAsyncReadyCallback       callback,
                                                         gpointer                  user_data);
IDE_AVAILABLE_IN_3_32
GDBusProxy    *ide_application_get_worker_finish        (IdeApplication           *self,
                                                         GAsyncResult             *result,
                                                         GError                  **error);
IDE_AVAILABLE_IN_3_32
IdeWorkbench  *ide_application_find_workbench_for_file  (IdeApplication           *self,
                                                         GFile                    *file);
IDE_AVAILABLE_IN_3_34
gpointer       ide_application_find_addin_by_module_name (IdeApplication           *self,
                                                          const gchar              *module_name);

G_END_DECLS
