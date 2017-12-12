/* ide-vcs-file-info.h
 *
 * Copyright © 2017 Christian Hergert <chergert@redhat.com>
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

G_BEGIN_DECLS

#define IDE_TYPE_VCS_FILE_INFO (ide_vcs_file_info_get_type())

G_DECLARE_DERIVABLE_TYPE (IdeVcsFileInfo, ide_vcs_file_info, IDE, VCS_FILE_INFO, GObject)

typedef enum
{
  IDE_VCS_FILE_STATUS_IGNORED = 1,
  IDE_VCS_FILE_STATUS_UNCHANGED,
  IDE_VCS_FILE_STATUS_UNTRACKED,
  IDE_VCS_FILE_STATUS_ADDED,
  IDE_VCS_FILE_STATUS_RENAMED,
  IDE_VCS_FILE_STATUS_DELETED,
  IDE_VCS_FILE_STATUS_CHANGED,
} IdeVcsFileStatus;

struct _IdeVcsFileInfoClass
{
  GObjectClass parent_class;

  /*< private >*/
  gpointer padding[16];
};

IdeVcsFileInfo   *ide_vcs_file_info_new        (GFile            *file);
GFile            *ide_vcs_file_info_get_file   (IdeVcsFileInfo   *self);
IdeVcsFileStatus  ide_vcs_file_info_get_status (IdeVcsFileInfo   *self);
void              ide_vcs_file_info_set_status (IdeVcsFileInfo   *self,
                                                IdeVcsFileStatus  status);

G_END_DECLS
