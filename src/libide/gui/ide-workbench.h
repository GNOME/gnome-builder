/* ide-workbench.h
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

#include <libide-core.h>
#include <libide-foundry.h>
#include <libide-projects.h>
#include <libide-search.h>
#include <libide-vcs.h>

#include "ide-workspace.h"

G_BEGIN_DECLS

#define IDE_TYPE_WORKBENCH (ide_workbench_get_type())

IDE_AVAILABLE_IN_3_32
G_DECLARE_FINAL_TYPE (IdeWorkbench, ide_workbench, IDE, WORKBENCH, GtkWindowGroup)

IDE_AVAILABLE_IN_3_32
IdeWorkbench    *ide_workbench_new                   (void);
IDE_AVAILABLE_IN_3_32
IdeWorkbench    *ide_workbench_new_for_context       (IdeContext           *context);
IDE_AVAILABLE_IN_3_32
void             ide_workbench_activate              (IdeWorkbench         *self);
IDE_AVAILABLE_IN_3_32
IdeProjectInfo  *ide_workbench_get_project_info      (IdeWorkbench         *self);
IDE_AVAILABLE_IN_3_32
gboolean         ide_workbench_has_project           (IdeWorkbench         *self);
IDE_AVAILABLE_IN_3_32
IdeContext      *ide_workbench_get_context           (IdeWorkbench         *self);
IDE_AVAILABLE_IN_3_32
IdeWorkspace    *ide_workbench_get_current_workspace (IdeWorkbench         *self);
IDE_AVAILABLE_IN_3_32
IdeWorkspace    *ide_workbench_get_workspace_by_type (IdeWorkbench         *self,
                                                      GType                 type);
IDE_AVAILABLE_IN_3_32
IdeSearchEngine *ide_workbench_get_search_engine     (IdeWorkbench         *self);
IDE_AVAILABLE_IN_3_32
IdeWorkbench    *ide_workbench_from_widget           (GtkWidget            *widget);
IDE_AVAILABLE_IN_3_32
void             ide_workbench_add_workspace         (IdeWorkbench         *self,
                                                      IdeWorkspace         *workspace);
IDE_AVAILABLE_IN_3_32
void             ide_workbench_remove_workspace      (IdeWorkbench         *self,
                                                      IdeWorkspace         *workspace);
IDE_AVAILABLE_IN_3_32
void             ide_workbench_focus_workspace       (IdeWorkbench         *self,
                                                      IdeWorkspace         *workspace);
IDE_AVAILABLE_IN_3_32
void             ide_workbench_foreach_workspace     (IdeWorkbench         *self,
                                                      GtkCallback           callback,
                                                      gpointer              user_data);
IDE_AVAILABLE_IN_3_32
void             ide_workbench_foreach_page          (IdeWorkbench         *self,
                                                      GtkCallback           callback,
                                                      gpointer              user_data);
IDE_AVAILABLE_IN_3_32
void             ide_workbench_load_project_async    (IdeWorkbench         *self,
                                                      IdeProjectInfo       *project_info,
                                                      GType                 workspace_type,
                                                      GCancellable         *cancellable,
                                                      GAsyncReadyCallback   callback,
                                                      gpointer              user_data);
IDE_AVAILABLE_IN_3_32
gboolean         ide_workbench_load_project_finish   (IdeWorkbench         *self,
                                                      GAsyncResult         *result,
                                                      GError              **error);
IDE_AVAILABLE_IN_3_32
void             ide_workbench_unload_async          (IdeWorkbench         *self,
                                                      GCancellable         *cancellable,
                                                      GAsyncReadyCallback   callback,
                                                      gpointer              user_data);
IDE_AVAILABLE_IN_3_32
gboolean         ide_workbench_unload_finish         (IdeWorkbench         *self,
                                                      GAsyncResult         *result,
                                                      GError              **error);
IDE_AVAILABLE_IN_3_32
void             ide_workbench_open_async            (IdeWorkbench         *self,
                                                      GFile                *file,
                                                      const gchar          *hint,
                                                      IdeBufferOpenFlags    flags,
                                                      GCancellable         *cancellable,
                                                      GAsyncReadyCallback   callback,
                                                      gpointer              user_data);
IDE_AVAILABLE_IN_3_32
void             ide_workbench_open_at_async         (IdeWorkbench         *self,
                                                      GFile                *file,
                                                      const gchar          *hint,
                                                      gint                  at_line,
                                                      gint                  at_line_offset,
                                                      IdeBufferOpenFlags    flags,
                                                      GCancellable         *cancellable,
                                                      GAsyncReadyCallback   callback,
                                                      gpointer              user_data);
IDE_AVAILABLE_IN_3_32
void             ide_workbench_open_all_async        (IdeWorkbench         *self,
                                                      GFile               **files,
                                                      guint                 n_files,
                                                      const gchar          *hint,
                                                      GCancellable         *cancellable,
                                                      GAsyncReadyCallback   callback,
                                                      gpointer              user_data);
IDE_AVAILABLE_IN_3_32
gboolean         ide_workbench_open_finish           (IdeWorkbench         *self,
                                                      GAsyncResult         *result,
                                                      GError              **error);
IDE_AVAILABLE_IN_3_32
IdeVcs          *ide_workbench_get_vcs               (IdeWorkbench         *self);
IDE_AVAILABLE_IN_3_32
void             ide_workbench_set_vcs               (IdeWorkbench         *self,
                                                      IdeVcs               *vcs);
IDE_AVAILABLE_IN_3_32
IdeVcsMonitor   *ide_workbench_get_vcs_monitor       (IdeWorkbench         *self);
IDE_AVAILABLE_IN_3_32
IdeBuildSystem  *ide_workbench_get_build_system      (IdeWorkbench         *self);
IDE_AVAILABLE_IN_3_32
void             ide_workbench_set_build_system      (IdeWorkbench         *self,
                                                      IdeBuildSystem       *build_system);


G_END_DECLS
