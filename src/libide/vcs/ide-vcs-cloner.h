/* ide-vcs-cloner.h
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

#include <libide-core.h>

#include "ide-vcs-uri.h"

G_BEGIN_DECLS

#define IDE_TYPE_VCS_CLONER (ide_vcs_cloner_get_type())

IDE_AVAILABLE_IN_ALL
G_DECLARE_INTERFACE (IdeVcsCloner, ide_vcs_cloner, IDE, VCS_CLONER, IdeObject)

struct _IdeVcsClonerInterface
{
  GTypeInterface parent_iface;

  char       *(*get_title)            (IdeVcsCloner         *self);
  gboolean    (*validate_uri)         (IdeVcsCloner         *self,
                                       const char           *uri,
                                       char                **errmsg);
  void        (*clone_async)          (IdeVcsCloner         *self,
                                       const char           *uri,
                                       const char           *destination,
                                       GVariant             *options,
                                       IdeNotification      *progress,
                                       GCancellable         *cancellable,
                                       GAsyncReadyCallback   callback,
                                       gpointer              user_data);
  gboolean    (*clone_finish)         (IdeVcsCloner         *self,
                                       GAsyncResult         *result,
                                       GError              **error);
  void        (*list_branches_async)  (IdeVcsCloner         *self,
                                       IdeVcsUri            *uri,
                                       GCancellable         *cancellable,
                                       GAsyncReadyCallback   callback,
                                       gpointer              user_data);
  GListModel *(*list_branches_finish) (IdeVcsCloner         *self,
                                       GAsyncResult         *result,
                                       GError              **error);
  char       *(*get_directory_name)   (IdeVcsCloner         *self,
                                       IdeVcsUri            *uri);
  void        (*set_pty_fd)           (IdeVcsCloner         *self,
                                       int                   pty_fd);
};

IDE_AVAILABLE_IN_44
void        ide_vcs_cloner_set_pty_fd           (IdeVcsCloner         *self,
                                                 int                   pty_fd);
IDE_AVAILABLE_IN_ALL
char       *ide_vcs_cloner_get_title            (IdeVcsCloner         *self);
IDE_AVAILABLE_IN_ALL
void        ide_vcs_cloner_clone_async          (IdeVcsCloner         *self,
                                                 const char           *uri,
                                                 const char           *destination,
                                                 GVariant             *options,
                                                 IdeNotification      *progress,
                                                 GCancellable         *cancellable,
                                                 GAsyncReadyCallback   callback,
                                                 gpointer              user_data);
IDE_AVAILABLE_IN_ALL
gboolean    ide_vcs_cloner_clone_finish         (IdeVcsCloner         *self,
                                                 GAsyncResult         *result,
                                                 GError              **error);
IDE_AVAILABLE_IN_ALL
void        ide_vcs_cloner_list_branches_async  (IdeVcsCloner         *self,
                                                 IdeVcsUri            *uri,
                                                 GCancellable         *cancellable,
                                                 GAsyncReadyCallback   callback,
                                                 gpointer              user_data);
IDE_AVAILABLE_IN_ALL
GListModel *ide_vcs_cloner_list_branches_finish (IdeVcsCloner         *self,
                                                 GAsyncResult         *result,
                                                 GError              **error);
IDE_AVAILABLE_IN_ALL
char       *ide_vcs_cloner_get_directory_name   (IdeVcsCloner         *self,
                                                 IdeVcsUri            *uri);
IDE_AVAILABLE_IN_ALL
gboolean    ide_vcs_cloner_validate_uri         (IdeVcsCloner         *self,
                                                 const char           *uri,
                                                 char                **errmsg);
IDE_AVAILABLE_IN_ALL
gboolean    ide_vcs_cloner_clone_simple         (IdeContext           *context,
                                                 const char           *module_name,
                                                 const char           *url,
                                                 const char           *branch,
                                                 const char           *destination,
                                                 IdeNotification      *notif,
                                                 GCancellable         *cancellable,
                                                 GError              **error);

G_END_DECLS
