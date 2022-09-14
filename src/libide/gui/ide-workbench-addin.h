/* ide-workbench-addin.h
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

#include "ide-session.h"
#include "ide-workbench.h"
#include "ide-workspace.h"

G_BEGIN_DECLS

#define IDE_TYPE_WORKBENCH_ADDIN (ide_workbench_addin_get_type ())

IDE_AVAILABLE_IN_ALL
G_DECLARE_INTERFACE (IdeWorkbenchAddin, ide_workbench_addin, IDE, WORKBENCH_ADDIN, GObject)

struct _IdeWorkbenchAddinInterface
{
  GTypeInterface parent;

  void          (*load)                  (IdeWorkbenchAddin     *self,
                                          IdeWorkbench          *workbench);
  void          (*unload)                (IdeWorkbenchAddin     *self,
                                          IdeWorkbench          *workbench);
  void          (*load_project_async)    (IdeWorkbenchAddin     *self,
                                          IdeProjectInfo        *project_info,
                                          GCancellable          *cancellable,
                                          GAsyncReadyCallback    callback,
                                          gpointer               user_data);
  gboolean      (*load_project_finish)   (IdeWorkbenchAddin     *self,
                                          GAsyncResult          *result,
                                          GError               **error);
  void          (*unload_project_async)  (IdeWorkbenchAddin     *self,
                                          IdeProjectInfo        *project_info,
                                          GCancellable          *cancellable,
                                          GAsyncReadyCallback    callback,
                                          gpointer               user_data);
  gboolean      (*unload_project_finish) (IdeWorkbenchAddin     *self,
                                          GAsyncResult          *result,
                                          GError               **error);
  void          (*project_loaded)        (IdeWorkbenchAddin     *self,
                                          IdeProjectInfo        *project_info);
  void          (*workspace_added)       (IdeWorkbenchAddin     *self,
                                          IdeWorkspace          *workspace);
  void          (*workspace_removed)     (IdeWorkbenchAddin     *self,
                                          IdeWorkspace          *workspace);
  gboolean      (*can_open)              (IdeWorkbenchAddin     *self,
                                          GFile                 *file,
                                          const gchar           *content_type,
                                          gint                  *priority);
  void          (*open_async)            (IdeWorkbenchAddin     *self,
                                          GFile                 *file,
                                          const gchar           *content_type,
                                          int                    at_line,
                                          int                    at_line_offset,
                                          IdeBufferOpenFlags     flags,
                                          PanelPosition         *position,
                                          GCancellable          *cancellable,
                                          GAsyncReadyCallback    callback,
                                          gpointer               user_data);
  gboolean      (*open_finish)           (IdeWorkbenchAddin     *self,
                                          GAsyncResult          *result,
                                          GError               **error);
  void          (*vcs_changed)           (IdeWorkbenchAddin     *self,
                                          IdeVcs                *vcs);
  GActionGroup *(*ref_action_group)      (IdeWorkbenchAddin     *self);
  void          (*save_session)          (IdeWorkbenchAddin     *self,
                                          IdeSession            *session);
  void          (*restore_session)       (IdeWorkbenchAddin     *self,
                                          IdeSession            *session);
};

IDE_AVAILABLE_IN_ALL
void               ide_workbench_addin_load                  (IdeWorkbenchAddin    *self,
                                                              IdeWorkbench         *workbench);
IDE_AVAILABLE_IN_ALL
void               ide_workbench_addin_unload                (IdeWorkbenchAddin    *self,
                                                              IdeWorkbench         *workbench);
IDE_AVAILABLE_IN_ALL
void               ide_workbench_addin_load_project_async    (IdeWorkbenchAddin    *self,
                                                              IdeProjectInfo       *project_info,
                                                              GCancellable         *cancellable,
                                                              GAsyncReadyCallback   callback,
                                                              gpointer              user_data);
IDE_AVAILABLE_IN_ALL
gboolean           ide_workbench_addin_load_project_finish   (IdeWorkbenchAddin    *self,
                                                              GAsyncResult         *result,
                                                              GError              **error);
IDE_AVAILABLE_IN_ALL
void               ide_workbench_addin_unload_project_async  (IdeWorkbenchAddin    *self,
                                                              IdeProjectInfo       *project_info,
                                                              GCancellable         *cancellable,
                                                              GAsyncReadyCallback   callback,
                                                              gpointer              user_data);
IDE_AVAILABLE_IN_ALL
gboolean           ide_workbench_addin_unload_project_finish (IdeWorkbenchAddin    *self,
                                                              GAsyncResult         *result,
                                                              GError              **error);
IDE_AVAILABLE_IN_ALL
void               ide_workbench_addin_project_loaded        (IdeWorkbenchAddin    *self,
                                                              IdeProjectInfo       *project_info);
IDE_AVAILABLE_IN_ALL
void               ide_workbench_addin_workspace_added       (IdeWorkbenchAddin    *self,
                                                              IdeWorkspace         *workspace);
IDE_AVAILABLE_IN_ALL
void               ide_workbench_addin_workspace_removed     (IdeWorkbenchAddin    *self,
                                                              IdeWorkspace         *workspace);
IDE_AVAILABLE_IN_ALL
gboolean           ide_workbench_addin_can_open              (IdeWorkbenchAddin    *self,
                                                              GFile                *file,
                                                              const gchar          *content_type,
                                                              gint                 *priority);
IDE_AVAILABLE_IN_ALL
void               ide_workbench_addin_open_async            (IdeWorkbenchAddin    *self,
                                                              GFile                *file,
                                                              const gchar          *content_type,
                                                              int                   at_line,
                                                              int                   at_line_offset,
                                                              IdeBufferOpenFlags    flags,
                                                              PanelPosition        *position,
                                                              GCancellable         *cancellable,
                                                              GAsyncReadyCallback   callback,
                                                              gpointer              user_data);
IDE_AVAILABLE_IN_ALL
gboolean           ide_workbench_addin_open_finish           (IdeWorkbenchAddin    *self,
                                                              GAsyncResult         *result,
                                                              GError              **error);
IDE_AVAILABLE_IN_ALL
void               ide_workbench_addin_vcs_changed           (IdeWorkbenchAddin    *self,
                                                              IdeVcs               *vcs);
IDE_AVAILABLE_IN_ALL
GActionGroup      *ide_workbench_addin_ref_action_group      (IdeWorkbenchAddin    *self);
IDE_AVAILABLE_IN_ALL
void               ide_workbench_addin_save_session          (IdeWorkbenchAddin    *self,
                                                              IdeSession           *session);
IDE_AVAILABLE_IN_ALL
void               ide_workbench_addin_restore_session       (IdeWorkbenchAddin    *self,
                                                              IdeSession           *session);
IDE_AVAILABLE_IN_ALL
IdeWorkbenchAddin *ide_workbench_addin_find_by_module_name   (IdeWorkbench         *workbench,
                                                              const gchar          *module_name);

G_END_DECLS
