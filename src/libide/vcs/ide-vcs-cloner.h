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

G_BEGIN_DECLS

#define IDE_TYPE_VCS_CLONER (ide_vcs_cloner_get_type())

IDE_AVAILABLE_IN_3_32
G_DECLARE_INTERFACE (IdeVcsCloner, ide_vcs_cloner, IDE, VCS_CLONER, IdeObject)

struct _IdeVcsClonerInterface
{
  GTypeInterface parent_iface;

  gchar    *(*get_title)    (IdeVcsCloner         *self);
  gboolean  (*validate_uri) (IdeVcsCloner         *self,
                             const gchar          *uri,
                             gchar               **errmsg);
  void      (*clone_async)  (IdeVcsCloner         *self,
                             const gchar          *uri,
                             const gchar          *destination,
                             GVariant             *options,
                             IdeNotification      *progress,
                             GCancellable         *cancellable,
                             GAsyncReadyCallback   callback,
                             gpointer              user_data);
  gboolean  (*clone_finish) (IdeVcsCloner         *self,
                             GAsyncResult         *result,
                             GError              **error);
};

IDE_AVAILABLE_IN_3_32
gchar    *ide_vcs_cloner_get_title    (IdeVcsCloner         *self);
IDE_AVAILABLE_IN_3_32
void      ide_vcs_cloner_clone_async  (IdeVcsCloner         *self,
                                       const gchar          *uri,
                                       const gchar          *destination,
                                       GVariant             *options,
                                       IdeNotification      *progress,
                                       GCancellable         *cancellable,
                                       GAsyncReadyCallback   callback,
                                       gpointer              user_data);
IDE_AVAILABLE_IN_3_32
gboolean  ide_vcs_cloner_clone_finish (IdeVcsCloner         *self,
                                       GAsyncResult         *result,
                                       GError              **error);
IDE_AVAILABLE_IN_3_32
gboolean  ide_vcs_cloner_validate_uri (IdeVcsCloner         *self,
                                       const gchar          *uri,
                                       gchar               **errmsg);
IDE_AVAILABLE_IN_3_34
gboolean  ide_vcs_cloner_clone_simple (IdeContext           *context,
                                       const gchar          *module_name,
                                       const gchar          *url,
                                       const gchar          *branch,
                                       const gchar          *destination,
                                       IdeNotification      *notif,
                                       GCancellable         *cancellable,
                                       GError              **error);

G_END_DECLS
