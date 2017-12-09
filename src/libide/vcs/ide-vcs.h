/* ide-vcs.h
 *
 * Copyright © 2015 Christian Hergert <christian@hergert.me>
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
 */

#pragma once

#include <gio/gio.h>

#include "ide-version-macros.h"

#include "ide-object.h"
#include "vcs/ide-vcs-config.h"

G_BEGIN_DECLS

#define IDE_TYPE_VCS (ide_vcs_get_type())

G_DECLARE_INTERFACE (IdeVcs, ide_vcs, IDE, VCS, IdeObject)

struct _IdeVcsInterface
{
  GTypeInterface            parent_interface;

  GFile                  *(*get_working_directory)     (IdeVcs               *self);
  IdeBufferChangeMonitor *(*get_buffer_change_monitor) (IdeVcs               *self,
                                                        IdeBuffer            *buffer);
  gboolean                (*is_ignored)                (IdeVcs               *self,
                                                        GFile                *file,
                                                        GError              **error);
  gint                    (*get_priority)              (IdeVcs               *self);
  void                    (*changed)                   (IdeVcs               *self);
  IdeVcsConfig           *(*get_config)                (IdeVcs               *self);
  gchar                  *(*get_branch_name)           (IdeVcs               *self);
};

IDE_AVAILABLE_IN_ALL
void                    ide_vcs_register_ignored          (const gchar          *pattern);
IDE_AVAILABLE_IN_ALL
IdeBufferChangeMonitor *ide_vcs_get_buffer_change_monitor (IdeVcs               *self,
                                                           IdeBuffer            *buffer);
IDE_AVAILABLE_IN_ALL
GFile                  *ide_vcs_get_working_directory     (IdeVcs               *self);
IDE_AVAILABLE_IN_ALL
void                    ide_vcs_new_async                 (IdeContext           *context,
                                                           int                   io_priority,
                                                           GCancellable         *cancellable,
                                                           GAsyncReadyCallback   callback,
                                                           gpointer              user_data);
IDE_AVAILABLE_IN_ALL
IdeVcs                 *ide_vcs_new_finish                (GAsyncResult         *result,
                                                           GError              **error);
IDE_AVAILABLE_IN_ALL
gboolean                ide_vcs_is_ignored                (IdeVcs               *self,
                                                           GFile                *file,
                                                           GError              **error);
IDE_AVAILABLE_IN_3_28
gboolean                ide_vcs_path_is_ignored           (IdeVcs               *self,
                                                           const gchar          *path,
                                                           GError              **error);
IDE_AVAILABLE_IN_ALL
gint                    ide_vcs_get_priority              (IdeVcs               *self);
IDE_AVAILABLE_IN_ALL
void                    ide_vcs_emit_changed              (IdeVcs               *self);
IDE_AVAILABLE_IN_ALL
IdeVcsConfig           *ide_vcs_get_config                (IdeVcs               *self);
IDE_AVAILABLE_IN_ALL
gchar                  *ide_vcs_get_branch_name           (IdeVcs               *self);

G_END_DECLS
