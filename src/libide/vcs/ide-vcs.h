/* ide-vcs.h
 *
 * Copyright 2015-2019 Christian Hergert <christian@hergert.me>
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

#if !defined (IDE_VCS_INSIDE) && !defined (IDE_VCS_COMPILATION)
# error "Only <libide-vcs.h> can be included directly."
#endif

#include <libide-core.h>

#include "ide-vcs-branch.h"
#include "ide-vcs-config.h"

G_BEGIN_DECLS

#define IDE_TYPE_VCS (ide_vcs_get_type())

IDE_AVAILABLE_IN_ALL
G_DECLARE_INTERFACE (IdeVcs, ide_vcs, IDE, VCS, IdeObject)

struct _IdeVcsInterface
{
  GTypeInterface            parent_interface;

  char                   *(*get_display_name)          (IdeVcs               *self);
  GFile                  *(*get_workdir)               (IdeVcs               *self);
  gboolean                (*is_ignored)                (IdeVcs               *self,
                                                        GFile                *file,
                                                        GError              **error);
  gint                    (*get_priority)              (IdeVcs               *self);
  void                    (*changed)                   (IdeVcs               *self);
  IdeVcsConfig           *(*get_config)                (IdeVcs               *self);
  gchar                  *(*get_branch_name)           (IdeVcs               *self);
  void                    (*list_status_async)         (IdeVcs               *self,
                                                        GFile                *directory_or_file,
                                                        gboolean              include_descendants,
                                                        gint                  io_priority,
                                                        GCancellable         *cancellable,
                                                        GAsyncReadyCallback   callback,
                                                        gpointer              user_data);
  GListModel             *(*list_status_finish)        (IdeVcs               *self,
                                                        GAsyncResult         *result,
                                                        GError              **error);
  void                    (*list_branches_async)       (IdeVcs               *self,
                                                        GCancellable         *cancellable,
                                                        GAsyncReadyCallback   callback,
                                                        gpointer              user_data);
  GPtrArray              *(*list_branches_finish)      (IdeVcs               *self,
                                                        GAsyncResult         *result,
                                                        GError              **error);
  void                    (*list_tags_async)           (IdeVcs               *self,
                                                        GCancellable         *cancellable,
                                                        GAsyncReadyCallback   callback,
                                                        gpointer              user_data);
  GPtrArray              *(*list_tags_finish)          (IdeVcs               *self,
                                                        GAsyncResult         *result,
                                                        GError              **error);
  void                    (*switch_branch_async)       (IdeVcs               *self,
                                                        IdeVcsBranch         *branch,
                                                        GCancellable         *cancellable,
                                                        GAsyncReadyCallback   callback,
                                                        gpointer              user_data);
  gboolean                (*switch_branch_finish)      (IdeVcs               *self,
                                                        GAsyncResult         *result,
                                                        GError              **error);
  void                    (*push_branch_async)         (IdeVcs               *self,
                                                        IdeVcsBranch         *branch,
                                                        GCancellable         *cancellable,
                                                        GAsyncReadyCallback   callback,
                                                        gpointer              user_data);
  gboolean                (*push_branch_finish)        (IdeVcs               *self,
                                                        GAsyncResult         *result,
                                                        GError              **error);
  DexFuture              *(*query_ignored)             (IdeVcs               *self,
                                                        GFile                *file);
};

IDE_AVAILABLE_IN_ALL
IdeVcs       *ide_vcs_from_context         (IdeContext           *context);
IDE_AVAILABLE_IN_ALL
IdeVcs       *ide_vcs_ref_from_context     (IdeContext           *context);
IDE_AVAILABLE_IN_ALL
GFile        *ide_vcs_get_workdir          (IdeVcs               *self);
IDE_AVAILABLE_IN_ALL
gboolean      ide_vcs_is_ignored           (IdeVcs               *self,
                                            GFile                *file,
                                            GError              **error);
IDE_AVAILABLE_IN_ALL
gboolean      ide_vcs_path_is_ignored      (IdeVcs               *self,
                                            const gchar          *path,
                                            GError              **error);
IDE_AVAILABLE_IN_ALL
gint          ide_vcs_get_priority         (IdeVcs               *self);
IDE_AVAILABLE_IN_ALL
void          ide_vcs_emit_changed         (IdeVcs               *self);
IDE_AVAILABLE_IN_ALL
IdeVcsConfig *ide_vcs_get_config           (IdeVcs               *self);
IDE_AVAILABLE_IN_ALL
gchar        *ide_vcs_get_branch_name      (IdeVcs               *self);
IDE_AVAILABLE_IN_ALL
void          ide_vcs_list_status_async    (IdeVcs               *self,
                                            GFile                *directory_or_file,
                                            gboolean              include_descendants,
                                            gint                  io_priority,
                                            GCancellable         *cancellable,
                                            GAsyncReadyCallback   callback,
                                            gpointer              user_data);
IDE_AVAILABLE_IN_ALL
GListModel   *ide_vcs_list_status_finish   (IdeVcs               *self,
                                            GAsyncResult         *result,
                                            GError              **error);
IDE_AVAILABLE_IN_ALL
void          ide_vcs_list_branches_async  (IdeVcs               *self,
                                            GCancellable         *cancellable,
                                            GAsyncReadyCallback   callback,
                                            gpointer              user_data);
IDE_AVAILABLE_IN_ALL
GPtrArray    *ide_vcs_list_branches_finish (IdeVcs               *self,
                                            GAsyncResult         *result,
                                            GError              **error);
IDE_AVAILABLE_IN_ALL
void          ide_vcs_list_tags_async      (IdeVcs               *self,
                                            GCancellable         *cancellable,
                                            GAsyncReadyCallback   callback,
                                            gpointer              user_data);
IDE_AVAILABLE_IN_ALL
GPtrArray    *ide_vcs_list_tags_finish     (IdeVcs               *self,
                                            GAsyncResult         *result,
                                            GError              **error);
IDE_AVAILABLE_IN_ALL
void          ide_vcs_switch_branch_async  (IdeVcs               *self,
                                            IdeVcsBranch         *branch,
                                            GCancellable         *cancellable,
                                            GAsyncReadyCallback   callback,
                                            gpointer              user_data);
IDE_AVAILABLE_IN_ALL
gboolean      ide_vcs_switch_branch_finish (IdeVcs               *self,
                                            GAsyncResult         *result,
                                            GError              **error);
IDE_AVAILABLE_IN_ALL
void          ide_vcs_push_branch_async    (IdeVcs               *self,
                                            IdeVcsBranch         *branch,
                                            GCancellable         *cancellable,
                                            GAsyncReadyCallback   callback,
                                            gpointer              user_data);
IDE_AVAILABLE_IN_ALL
gboolean      ide_vcs_push_branch_finish   (IdeVcs               *self,
                                            GAsyncResult         *result,
                                            GError              **error);
IDE_AVAILABLE_IN_ALL
char         *ide_vcs_get_display_name     (IdeVcs               *self);
IDE_AVAILABLE_IN_47
DexFuture    *ide_vcs_query_ignored        (IdeVcs               *self,
                                            GFile                *file);

G_END_DECLS
