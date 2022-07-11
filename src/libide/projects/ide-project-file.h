/* ide-project-file.h
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

#if !defined (IDE_PROJECTS_INSIDE) && !defined (IDE_PROJECTS_COMPILATION)
# error "Only <libide-projects.h> can be included directly."
#endif

#include <libide-code.h>

G_BEGIN_DECLS

#define IDE_TYPE_PROJECT_FILE (ide_project_file_get_type())


#define IDE_PROJECT_FILE_ATTRIBUTES \
  G_FILE_ATTRIBUTE_STANDARD_NAME"," \
  G_FILE_ATTRIBUTE_STANDARD_DISPLAY_NAME"," \
  G_FILE_ATTRIBUTE_STANDARD_CONTENT_TYPE"," \
  G_FILE_ATTRIBUTE_STANDARD_SYMBOLIC_ICON"," \
  G_FILE_ATTRIBUTE_STANDARD_TYPE"," \
  G_FILE_ATTRIBUTE_STANDARD_IS_SYMLINK"," \
  G_FILE_ATTRIBUTE_ACCESS_CAN_READ"," \
  G_FILE_ATTRIBUTE_ACCESS_CAN_RENAME"," \
  G_FILE_ATTRIBUTE_ACCESS_CAN_TRASH

IDE_AVAILABLE_IN_ALL
G_DECLARE_DERIVABLE_TYPE (IdeProjectFile, ide_project_file, IDE, PROJECT_FILE, IdeObject)

struct _IdeProjectFileClass
{
  IdeObjectClass parent_class;

  /*< private >*/
  gpointer _reserved[8];
};

IDE_AVAILABLE_IN_ALL
IdeProjectFile *ide_project_file_new                       (GFile                *directory,
                                                            GFileInfo            *info);
IDE_AVAILABLE_IN_ALL
GFile          *ide_project_file_get_directory             (IdeProjectFile       *self);
IDE_AVAILABLE_IN_ALL
GFileInfo      *ide_project_file_get_info                  (IdeProjectFile       *self);
IDE_AVAILABLE_IN_ALL
GFile          *ide_project_file_ref_file                  (IdeProjectFile       *self);
IDE_AVAILABLE_IN_ALL
const gchar    *ide_project_file_get_display_name          (IdeProjectFile       *self);
IDE_AVAILABLE_IN_ALL
const gchar    *ide_project_file_get_name                  (IdeProjectFile       *self);
IDE_AVAILABLE_IN_ALL
gboolean        ide_project_file_is_directory              (IdeProjectFile       *self);
IDE_AVAILABLE_IN_ALL
gboolean        ide_project_file_is_symlink                (IdeProjectFile       *self);
IDE_AVAILABLE_IN_ALL
gint            ide_project_file_compare_directories_first (IdeProjectFile       *a,
                                                            IdeProjectFile       *b);
IDE_AVAILABLE_IN_ALL
gint            ide_project_file_compare                   (IdeProjectFile       *a,
                                                            IdeProjectFile       *b);
IDE_AVAILABLE_IN_ALL
GIcon          *ide_project_file_get_symbolic_icon         (IdeProjectFile       *self);
IDE_AVAILABLE_IN_ALL
IdeProjectFile *ide_project_file_create_child              (IdeProjectFile       *self,
                                                            GFileInfo            *info);
IDE_AVAILABLE_IN_ALL
void            ide_project_file_list_children_async       (IdeProjectFile       *self,
                                                            GCancellable         *cancellable,
                                                            GAsyncReadyCallback   callback,
                                                            gpointer              user_data);
IDE_AVAILABLE_IN_ALL
GPtrArray      *ide_project_file_list_children_finish      (IdeProjectFile       *self,
                                                            GAsyncResult         *result,
                                                            GError              **error);
IDE_AVAILABLE_IN_ALL
void            ide_project_file_trash_async               (IdeProjectFile       *self,
                                                            GCancellable         *cancellable,
                                                            GAsyncReadyCallback   callback,
                                                            gpointer              user_data);
IDE_AVAILABLE_IN_ALL
gboolean        ide_project_file_trash_finish              (IdeProjectFile       *self,
                                                            GAsyncResult         *result,
                                                            GError              **error);

G_END_DECLS
